// Read-only NDK native backtrace of one existing launcher owned by this app UID.
import fs from 'node:fs';
import {spawn,execFileSync} from 'node:child_process';
const adb=process.env.ADB??'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev',root=`/data/user/0/${pkg}`;
const ndk='D:/Collect/.tooling/android-sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64';
const call=(args,options={})=>execFileSync(adb,args,{timeout:12000,encoding:'utf8',windowsHide:true,...options});
const row=call(['shell','ps','-A']).split(/\r?\n/).find(x=>/^u0_a335\s/.test(x)&&/launcher\.exe$/.test(x));
const pid=row?.trim().split(/\s+/)[1];
if(!pid||!/^\d+$/.test(pid))throw Error('No existing app-private launcher');
const port='50389';if(call(['forward','--list']).includes(`tcp:${port} `))throw Error('Diagnostic port in use');
let serverOutput='',trace='';
const server=spawn(adb,['shell',`run-as ${pkg} sh -c 'echo $$ > files/diagnostics/lldb-server.pid; exec ${root}/files/diagnostics/lldb-server gdbserver 127.0.0.1:${port} --attach ${pid}'`],
  {stdio:['ignore','pipe','pipe'],windowsHide:true});
server.stdout.on('data',x=>serverOutput+=x);server.stderr.on('data',x=>serverOutput+=x);
try {
  await new Promise(r=>setTimeout(r,700));
  call(['forward',`tcp:${port}`,`tcp:${port}`]);
  trace=execFileSync(`${ndk}/bin/lldb.exe`,['--no-lldbinit','--batch',
    '-o',`gdb-remote 127.0.0.1:${port}`,'-o','thread backtrace all -c 24','-o','process detach'],
    {timeout:25000,encoding:'utf8',windowsHide:true,maxBuffer:4000000,
      env:{...process.env,PYTHONHOME:`${ndk}/python3`,PATH:`${ndk}/python3;${process.env.PATH}`}});
} catch(error) {trace=String(error.stdout??'')+'\n'+String(error.stderr??'');}
finally {
  await new Promise(r=>setTimeout(r,100));
  try {
    const serverPid=call(['shell','run-as',pkg,'cat','files/diagnostics/lldb-server.pid']).trim();
    if(/^\d+$/.test(serverPid)&&call(['shell','run-as',pkg,'cat',`/proc/${serverPid}/cmdline`]).includes(`gdbserver 127.0.0.1:${port}`))
      call(['shell','run-as',pkg,'kill','-TERM',serverPid]);
  } catch{}
  if(server.exitCode===null)server.kill();
  // LLDB can leave a signal-stopped FEX guest after detach. Resume only the
  // same app-owned launcher PID, never an unrelated/reused process.
  try {
    const current=call(['shell','ps','-A']).split(/\r?\n/).find(x=>/^u0_a335\s/.test(x)
      &&x.trim().split(/\s+/)[1]===pid&&/launcher\.exe$/.test(x));
    if(current)call(['shell','run-as',pkg,'kill','-CONT',pid]);
  } catch{}
  call(['forward','--remove',`tcp:${port}`]);
  fs.writeFileSync('artifacts/superposition/launcher-wait-native-stack.txt',trace);
  fs.writeFileSync('artifacts/superposition/launcher-wait-debugger.txt',serverOutput);
  console.log(serverOutput.trim());console.log(trace.slice(-3800));
}
