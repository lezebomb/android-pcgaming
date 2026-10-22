// Only the new focus-loss case. Keep A physically held while Android takes focus.
import fs from 'node:fs';
import {spawn,execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe', pkg='org.dimensity.gaming.dev';
const root='files/imagefs/home/xuser-1/.wine/drive_c', output='artifacts/controller-check';
const call=(args,options={})=>execFileSync(adb,args,{timeout:20000,windowsHide:true,...options});
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const focus=()=>call(['shell','dumpsys','window'],{encoding:'utf8'})
  .split(/\r?\n/).filter(line=>/mCurrentFocus|mFocusedApp|mTopFocusedDisplayId/.test(line)).join('\n');
const read=()=>call(['exec-out','run-as',pkg,'cat',`${root}/DGSmoke/focus-input.txt`],{encoding:'utf8',stdio:['ignore','pipe','ignore']});
async function until(marker) {
  const end=Date.now()+8000;
  while(Date.now()<end) { try { if(read().includes(marker)) return; } catch {} await delay(200); }
  throw new Error(`Missing focus transition: ${marker}`);
}
function dump() {
  call(['shell','uiautomator','dump','/data/local/tmp/dimensity-focus.xml']);
  return call(['exec-out','cat','/data/local/tmp/dimensity-focus.xml'],{encoding:'utf8'});
}
function tap(xml,text) {
  const node=[...xml.matchAll(/<node\b[^>]*>/g)].map(x=>x[0]).find(x=>x.includes(`text="${text}"`));
  const b=node?.match(/bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"/);
  if(!b) throw new Error(`Visible focus control missing: ${text}`);
  call(['shell','input','tap',String((+b[1]+ +b[3])/2),String((+b[2]+ +b[4])/2)]);
}
async function requireRuntimeFocus() {
  for (let attempt=0;attempt<3;attempt++) {
    const state=focus();
    if (state.split('\n').some(line=>line.includes('mCurrentFocus=') && line.includes(pkg))) return;
    if (state.includes('Application Not Responding: com.android.settings')) {
      const xml=dump();
      if (xml.includes('text="设置没有响应"')) tap(xml,'等待');
    }
    await delay(100);
  }
  throw new Error('Runtime has no input focus; see focus-window-final.txt.');
}
const saved=`${root}/users/xuser/Desktop/DG-FocusInput.desktop`;
const desktop=`[Desktop Entry]\nType=Application\nName=Focus input check\nExec=wine C:/DGSmoke/DG-FocusInput.exe\n[Extra Data]\ncontainer_id=1\nexecutablePath=C:/DGSmoke/DG-FocusInput.exe\nexecArgs=--focus\nenvVars=WINEDEBUG=-all\n`;
call(['exec-out','run-as',pkg,'mkdir','-p','files/diagnostics']);
call(['exec-out','run-as',pkg,'touch','files/diagnostics/gamepad-trace.enabled']);
await stagePrivate(adb,pkg,fs.readFileSync('artifacts/DG-FocusInput.exe'),`${root}/DGSmoke/DG-FocusInput.exe`);
await stagePrivate(adb,pkg,Buffer.from(desktop),saved);
call(['exec-out','run-as',pkg,'rm','-f',`${root}/DGSmoke/focus-input.txt`]);
call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]);
let hid;
try {
  await until('Dimensity bounded smoke');
  let xml=dump();
  if(xml.includes('text="设置没有响应"')) tap(xml,'等待');
  hid=spawn(process.execPath,['scripts/virtual-gamepad.mjs','HA2C3HFV'],{env:{...process.env,ADB:adb},stdio:['pipe','pipe','pipe'],windowsHide:true});
  hid.stdout.resume(); hid.stderr.resume();
  await delay(2000);
  hid.stdin.write('down a\n');
  await until('held_seen=1');
  call(['shell','input','keyevent','3']);
  await until('neutral_after_hold=1');
  // No up/neutral report was sent until the Windows observer saw neutral.
  fs.writeFileSync(`${output}/focus-held-while-neutral.txt`,read());
  hid.stdin.write('up a\n');
  call(['shell','am','start','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER',
    '-f','0x10200000','-n',`${pkg}/com.winlator.cmod.MainActivity`,'--ez','skip_auto_start','true']);
  await delay(300);
  await requireRuntimeFocus();
  fs.writeFileSync(`${output}/focus-window-resumed.txt`,focus());
  hid.stdin.write('down a\n');
  await until('fresh_press_after_focus=1');
  hid.stdin.write('up a\n');
  await until('complete=1');
  const result=read();
  fs.writeFileSync(`${output}/focus-input.txt`,result);
  console.log(result);
  if(!result.includes('focus_result=0')) process.exitCode=1;
} finally {
  try {
    fs.writeFileSync(`${output}/focus-gamepad-trace.txt`,call(['exec-out','run-as',pkg,'cat','files/diagnostics/gamepad-trace.txt']));
    fs.writeFileSync(`${output}/focus-input-latest.txt`,read());
    fs.writeFileSync(`${output}/focus-event0.bin`,call(['exec-out','run-as',pkg,'cat','files/imagefs/dev/input/event0']));
    fs.writeFileSync(`${output}/focus-window-final.txt`,focus());
  } catch {}
  call(['exec-out','run-as',pkg,'rm','-f','files/diagnostics/gamepad-trace.enabled']);
  if(hid && !hid.stdin.destroyed) {
    hid.stdin.end('neutral\nquit\n');
    await Promise.race([new Promise(resolve=>hid.once('exit',resolve)),delay(4000)]);
  }
}
