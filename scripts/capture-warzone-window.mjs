import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg = 'org.dimensity.gaming.dev';
const base = 'files/imagefs/home/xuser-1/.wine/drive_c';
const output = 'artifacts/mali-compat';
const campaign = process.argv.includes('--campaign');
const executable = campaign ? 'DG-WarzoneCampaign.exe' : 'DG-WarzoneDialog.exe';
const shortcut = executable.replace('.exe', '.desktop');
const call = (args, options = {}) => execFileSync(adb, args, {timeout: 20000, maxBuffer:16*1024*1024, ...options});
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
async function stage(source, destination) {
  const bytes = Buffer.isBuffer(source) ? source : fs.readFileSync(source);
  call(['exec-in','run-as',pkg,'sh','-c',`cat > '${destination}'`], {input: bytes});
  for (let i=0; i<50; i++) {
    const size = Number(call(['exec-out','run-as',pkg,'stat','-c','%s',destination], {encoding:'utf8'}).trim());
    if (size === bytes.length) return;
    await wait(100);
  }
  throw new Error(`Incomplete staging ${source}`);
}
function dump() {
  call(['shell','uiautomator','dump','/data/local/tmp/dimensity-window-capture.xml']);
  return call(['exec-out','cat','/data/local/tmp/dimensity-window-capture.xml'], {encoding:'utf8'});
}
function tapText(xml, text) {
  const node = [...xml.matchAll(/<node\b[^>]*>/g)].map(x=>x[0]).find(x=>x.includes(`text="${text}"`));
  const match = node?.match(/bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"/);
  if (!match) throw new Error(`Visible control not found: ${text}`);
  call(['shell','input','tap',String(Math.floor((Number(match[1])+Number(match[3]))/2)),String(Math.floor((Number(match[2])+Number(match[4]))/2))]);
}
function screenshot(name) {
  call(['shell','screencap','-p','/data/local/tmp/dimensity-window-capture.png']);
  fs.writeFileSync(`${output}/${name}.png`, call(['exec-out','cat','/data/local/tmp/dimensity-window-capture.png']));
}
await stage(`artifacts/${executable}`, `${base}/DGSmoke/${executable}`);
if (campaign) {
  const desktop = fs.readFileSync(`${output}/DG-WarzoneDialog.desktop`, 'utf8').replaceAll('DG-WarzoneDialog.exe', executable);
  await stage(Buffer.from(desktop), `${base}/users/xuser/Desktop/${shortcut}`);
}
await stage('artifacts/dg-x11-window-protocol', 'files/dg-x11-window-protocol');
call(['shell','run-as',pkg,'rm','-f',`${base}/DGSmoke/warzone-dialog.txt`,'files/diagnostics/windows.json']);
call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${base}/users/xuser/Desktop/${shortcut}`]);
await wait(4000);
const appPid = call(['shell','pidof',pkg], {encoding:'utf8'}).trim();
let xml = dump();
if (xml.includes('text="设置没有响应"')) tapText(xml, '等待');
call(['shell','input','keyevent','4']);
xml = dump();
tapText(xml, 'Logs');
await wait(500);
fs.writeFileSync(`${output}/warzone-windows-dialog.json`, call(['exec-out','run-as',pkg,'cat','files/diagnostics/windows.json']));
call(['shell','input','keyevent','4']);
screenshot('warzone-before-dialog-dismiss');
if (process.argv.includes('--protocol')) try {
  const result = call(['exec-out','run-as',pkg,'/system/bin/linker64',`/data/user/0/${pkg}/files/dg-x11-window-protocol`,
    `/data/user/0/${pkg}/files/imagefs/usr/tmp/.X11-unix/X0`], {encoding:'utf8'});
  fs.writeFileSync(`${output}/x11-window-protocol.txt`, result);
  console.log(result.trim());
} catch (failure) {
  fs.writeFileSync(`${output}/x11-window-protocol.txt`, String(failure.stdout ?? '') + '\n' + String(failure.stderr ?? failure));
  console.log('Protocol check failed; evidence saved.');
}
await wait(6000);
screenshot('warzone-after-dialog-dismiss');
fs.writeFileSync(`${output}/warzone-dialog-capture.txt`, call(['exec-out','run-as',pkg,'cat',`${base}/DGSmoke/warzone-dialog.txt`]));
const finalPid = call(['shell','pidof',pkg], {encoding:'utf8'}).trim();
if (!appPid || finalPid !== appPid) throw new Error('Application exited or restarted during capture; inspect crash evidence.');
console.log('Captured window metadata and screenshots; application stayed alive. Inspect images before judging the game.');
