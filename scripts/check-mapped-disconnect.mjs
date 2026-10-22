// One new removal/reconnect case, using the production wildcard strategy profile.
import fs from 'node:fs';
import {spawn,execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
import {requireRuntimeFocus} from './lib/runtime-focus.mjs';
const adb=process.env.ADB??'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe',pkg='org.dimensity.gaming.dev';
const root='files/imagefs/home/xuser-1/.wine/drive_c',output='artifacts/controller-check';
const call=(args,options={})=>execFileSync(adb,args,{timeout:15000,windowsHide:true,...options});
const delay=ms=>new Promise(r=>setTimeout(r,ms));
const read=()=>call(['exec-out','run-as',pkg,'cat',`${root}/DGSmoke/mapped-disconnect.txt`],{encoding:'utf8'});
async function until(marker) {
  const end=Date.now()+15000;
  while(Date.now()<end){try{if(read().includes(marker))return;}catch{}await delay(150);}
  throw Error('Missing mapped transition: '+marker);
}
async function attach() {
  const child=spawn(process.execPath,['scripts/virtual-gamepad.mjs','HA2C3HFV'],
    {env:{...process.env,ADB:adb},stdio:['pipe','pipe','pipe'],windowsHide:true});
  child.stdout.resume();child.stderr.resume();await delay(2000);return child;
}
const saved=`${root}/users/xuser/Desktop/DG-MappedDisconnect.desktop`;
const desktop=`[Desktop Entry]\nType=Application\nName=Mapped disconnect check\nExec=wine C:/DGSmoke/DG-MappedDisconnect.exe\n[Extra Data]\ncontainer_id=1\nexecutablePath=C:/DGSmoke/DG-MappedDisconnect.exe\nworkingDirectory=C:/DGSmoke\ncontrolsProfile=1000\nenvVars=WINEDEBUG=-all\n`;
call(['shell','run-as',pkg,'touch','files/diagnostics/gamepad-trace.enabled']);
await stagePrivate(adb,pkg,fs.readFileSync('artifacts/DG-MappedDisconnect.exe'),`${root}/DGSmoke/DG-MappedDisconnect.exe`);
await stagePrivate(adb,pkg,Buffer.from(desktop),saved);
call(['shell','run-as',pkg,'rm','-f',`${root}/DGSmoke/mapped-disconnect.txt`]);
call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]);
let hid;
try {
  await until('ready=1');await requireRuntimeFocus(call,pkg);
  fs.writeFileSync(`${output}/mapped-disconnect-window.png`,call(['exec-out','screencap','-p'],{maxBuffer:16000000}));
  hid=await attach();hid.stdin.write('down a\n');await until('mapped_press=1');
  // No up/neutral report: remove the UHID endpoint itself.
  const gone=new Promise(resolve=>hid.once('exit',resolve));hid.stdin.end('disconnect\n');
  await Promise.race([gone,delay(3000)]);await until('released_after_disconnect=1');
  hid=await attach();await requireRuntimeFocus(call,pkg);
  hid.stdin.write('down a\n');await until('reconnected_press=1');
  hid.stdin.write('up a\n');await until('complete=1');
  const result=read();console.log(result);if(!result.includes('mapped_disconnect_result=0'))process.exitCode=1;
} finally {
  try {fs.writeFileSync(`${output}/mapped-disconnect.txt`,read());}catch{}
  try {fs.writeFileSync(`${output}/mapped-disconnect-trace.txt`,call(['exec-out','run-as',pkg,'cat','files/diagnostics/gamepad-trace.txt']));}catch{}
  call(['shell','run-as',pkg,'rm','-f','files/diagnostics/gamepad-trace.enabled']);
  if(hid&&!hid.stdin.destroyed){const gone=new Promise(r=>hid.once('exit',r));hid.stdin.end('neutral\nquit\n');await Promise.race([gone,delay(3000)]);}
  call(['shell','run-as',pkg,'rm','-f',saved]);
}
