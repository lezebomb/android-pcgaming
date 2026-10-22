// Stage the verified official installer in our application only. Does not run it.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg = 'org.dimensity.gaming.dev';
const source = '.tooling/game-tests/superposition/Unigine_Superposition-1.1.exe';
const metadata = JSON.parse(fs.readFileSync('.tooling/game-tests/superposition/download.json', 'utf8'));
if (fs.statSync(source).size !== metadata.bytes || metadata.sha256 !== 'de5dbf4f7ad284b709c5db318dbedc1b025a41e50326595a444ee9cbd4a17182')
  throw new Error('Use the previously verified official installer.');
const root = 'files/imagefs/home/xuser-1/.wine/drive_c';
const target = `${root}/DGSmoke/Unigine_Superposition-1.1.exe`;
const temp = '/data/local/tmp/dimensity-superposition-installer.exe';
const call = (args, options = {}) => execFileSync(adb, args, {timeout: 30000, windowsHide: true, ...options});
let size = 0;
try { size = Number(call(['exec-out','run-as',pkg,'stat','-c','%s',target], {encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim()); } catch {}
if (size !== metadata.bytes) {
  call(['push','-Z',source,temp], {timeout: 300000,stdio:'inherit'});
  try {
    call(['shell','run-as',pkg,'cp',temp,target], {timeout: 120000});
    const digest = call(['exec-out','run-as',pkg,'sha256sum',target], {timeout: 120000,encoding:'utf8'}).split(/\s/)[0];
    if (digest !== metadata.sha256) throw new Error('Installer transfer checksum mismatch.');
  } finally { call(['shell','rm','-f',temp]); }
}
const desktop = `[Desktop Entry]\nType=Application\nName=Superposition official installer\nExec=wine C:/DGSmoke/Unigine_Superposition-1.1.exe\n[Extra Data]\ncontainer_id=1\nexecutablePath=C:/DGSmoke/Unigine_Superposition-1.1.exe\nexecArgs=/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /DIR=C:\\Games\\DGSuperposition /LOG=C:\\DGSmoke\\superposition-install.log\nworkingDirectory=C:/DGSmoke\nenvVars=WINEDEBUG=-all\n`;
const shortcut = `${root}/users/xuser/Desktop/DG-SuperpositionInstall.desktop`;
call(['exec-in','run-as',pkg,'sh','-c',`cat > '${shortcut}'`], {input:Buffer.from(desktop)});
fs.mkdirSync('artifacts/superposition', {recursive:true});
fs.writeFileSync('artifacts/superposition/DG-SuperpositionInstall.desktop', desktop);
console.log('Official installer staged and transfer verified. Open the saved shortcut to install inside this app.');
