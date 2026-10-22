// Attach only to an existing engine in this app UID; never launch/relaunch it.
import fs from 'node:fs';
import {spawn,execFileSync} from 'node:child_process';
const adb=process.env.ADB??'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe',pkg='org.dimensity.gaming.dev';
const root='/data/user/0/'+pkg,port='50390',out='artifacts/superposition';
const ndk='D:/Collect/.tooling/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64';
const call=(args,options={})=>execFileSync(adb,args,{timeout:12000,encoding:'utf8',windowsHide:true,...options});
const uid=call(['shell','run-as',pkg,'id','-u']).trim();
const processes=()=>call(['shell','ps','-A','-o','UID,PID,NAME']).split(/\r?\n/);
const isEngine=row=>row.trim().split(/\s+/)[0]===uid&&/C:\\Games\\DGSuperposition\\bin\\superposition\.exe$/i.test(row);
const row=processes().find(isEngine),pid=row?.trim().split(/\s+/)[1];
if(!pid||!/^\d+$/.test(pid))throw Error('No existing app-private Superposition engine.');
if(call(['forward','--list']).includes('tcp:'+port+' '))throw Error('Diagnostic port already in use.');
fs.mkdirSync(out,{recursive:true});
const maps=call(['exec-out','run-as',pkg,'cat','/proc/'+pid+'/maps']);
fs.writeFileSync(out+'/abort-engine-maps.txt',maps);
// FEX's private loader may not publish Android images to LLDB. Use the
// actual mapped libc plus its own exported abort offset for this device.
const libcRow=maps.split(/\r?\n/).map(line=>line.match(/^([0-9a-f]+)-[0-9a-f]+\s+\S+\s+0+\s+\S+\s+\d+\s+(\/.*bionic\/libc\.so)$/)).find(Boolean);
if(!libcRow)throw Error('No mapped Android libc; refusing an inferred breakpoint.');
const libcFile='D:/games-Android/.tooling/abort-libc.so';
fs.writeFileSync(libcFile,call(['exec-out','run-as',pkg,'cat',libcRow[2]],{encoding:null,maxBuffer:12*1024*1024}));
const symbols=execFileSync(ndk+'/bin/llvm-readelf.exe',['--dyn-syms',libcFile],{encoding:'utf8',timeout:10000,windowsHide:true});
const symbol=symbols.match(/^\s*\d+:\s+([0-9a-f]+)\s+\d+\s+FUNC[^\n]*\babort@@LIBC\s*$/mi);
if(!symbol)throw Error('Cannot resolve native abort on this device.');
const abortAddress='0x'+(BigInt('0x'+libcRow[1])+BigInt('0x'+symbol[1])).toString(16);
let serverOutput='',trace='';
const server=spawn(adb,['shell',`run-as ${pkg} sh -c 'echo $$ > files/diagnostics/lldb-abort.pid; exec ${root}/files/diagnostics/lldb-server gdbserver 127.0.0.1:${port} --attach ${pid}'`],{stdio:['ignore','pipe','pipe'],windowsHide:true});
server.stdout.on('data',b=>serverOutput+=b);server.stderr.on('data',b=>serverOutput+=b);
try {
  await new Promise(r=>setTimeout(r,700));
  call(['forward','tcp:'+port,'tcp:'+port]);
  console.log('Observing one engine abort; maximum 150 seconds.');
  trace=execFileSync(ndk+'/bin/lldb.exe',['--no-lldbinit','--batch',
    '-o','platform select remote-android',
    '-o','target create --arch aarch64-linux-android --no-dependents '+libcFile,
    '-o','gdb-remote 127.0.0.1:'+port,
    '-o','target modules load --file '+libcFile+' --slide 0x'+libcRow[1],
    '-o','process handle SIGSEGV SIGBUS SIGSYS SIGILL SIGUSR1 SIGUSR2 -s false -n false -p true',
    '-o','breakpoint set --address '+abortAddress,
    '-o','continue',
    '-o','thread backtrace all -c 20',
    '-o','register read pc sp x29 x30',
    '-o','image list -o -f',
    '-o','process detach'],{timeout:150000,encoding:'utf8',windowsHide:true,maxBuffer:6000000,
    env:{...process.env,PYTHONHOME:ndk+'/python3',PATH:ndk+'/python3;'+process.env.PATH}});
} catch(error) {trace=String(error.stdout??'')+'\n'+String(error.stderr??error.message);}
finally {
  try {const spid=call(['shell','run-as',pkg,'cat','files/diagnostics/lldb-abort.pid']).trim();
    if(/^\d+$/.test(spid)&&call(['shell','run-as',pkg,'cat','/proc/'+spid+'/cmdline']).includes('gdbserver 127.0.0.1:'+port))
      call(['shell','run-as',pkg,'kill','-TERM',spid]);
  }catch{}
  if(server.exitCode===null)server.kill();
  try {const current=processes().find(r=>isEngine(r)&&r.trim().split(/\s+/)[1]===pid);
    if(current)call(['shell','run-as',pkg,'kill','-CONT',pid]);
  }catch{}
  try{call(['forward','--remove','tcp:'+port]);}catch{}
  fs.writeFileSync(out+'/engine-abort-stack.txt',trace);fs.writeFileSync(out+'/engine-abort-debugger.txt',serverOutput);
  console.log(serverOutput.trim());console.log(trace.slice(-5000));
}
if(!trace.includes('stop reason = breakpoint 1.1')) {
  console.error('No native abort breakpoint captured; saved output is diagnostic only.');
  process.exitCode=1;
}
