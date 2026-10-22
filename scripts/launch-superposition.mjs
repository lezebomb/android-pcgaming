// Normal free-edition launcher UI. No paid CLI benchmark automation.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg = 'org.dimensity.gaming.dev';
const root = 'files/imagefs/home/xuser-1/.wine/drive_c';
const diagnostic = process.argv.includes('--diagnose');
const engine = process.argv.includes('--engine');
if (process.argv.includes('--sarek') && process.argv.includes('--legacy-dxvk')) throw new Error('Choose one DXVK version.');
if (diagnostic && engine) throw new Error('Choose launcher diagnosis or the normal engine EXE.');
const executable = diagnostic ? 'C:/DGSmoke/DG-SuperpositionLaunch.exe' : engine ? 'C:/Games/DGSuperposition/bin/superposition.exe' : 'C:/Games/DGSuperposition/Superposition.exe';
let desktop = `[Desktop Entry]\nType=Application\nName=Superposition\nExec=wine ${executable}\n[Extra Data]\ncontainer_id=1\nexecutablePath=${executable}\nworkingDirectory=C:/Games/DGSuperposition\nscreenSize=1280x800\ndxwrapper=dxvk\ndxwrapperConfig=version=v2.3.1-stripped,framerate=30,async=0,asyncCache=0,vkd3dVersion=2.14.1,vkd3dLevel=auto,maliCompat=1,ddrawrapper=none,dxvkConfig=1\nenvVars=WINEDEBUG=-all QV4_FORCE_INTERPRETER=1 QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic DXVK_LOG_LEVEL=info DXVK_LOG_PATH=C:/Games/DGSuperposition\n`;
if (process.argv.includes('--observe-gui')) {
  if (!diagnostic) throw new Error('--observe-gui requires the bounded launcher helper.');
  desktop += 'execArgs=--observe-gui\n';
}
if(process.argv.includes('--native-identity') && process.argv.includes('--launcher-identity')) throw new Error('Choose one identity scope.');
desktop += (process.argv.includes('--native-identity') || process.argv.includes('--launcher-identity') ? '' : 'adapterIdentity=nvidia\n')+'controlsProfile=0\nsimTouchScreen=1\n';
if(process.argv.includes('--launcher-identity')) desktop=desktop.replace('envVars=WINEDEBUG=',
  'envVars=DXVK_CONFIG=[launcher.exe];dxgi.customVendorId=10de;dxgi.hideNvidiaGpu=False WINEDEBUG=');
if (process.argv.includes('--vortek')) desktop += 'vulkanBackend=vortek\n';
if (process.argv.includes('--legacy-dxvk')) desktop=desktop.replace('version=v2.3.1-stripped,', 'version=1.10.3,');
if (process.argv.includes('--sarek')) desktop=desktop.replace('version=v2.3.1-stripped,', 'version=1.14.1-sarek,').replace('vkd3dVersion=2.14.1,', 'vkd3dVersion=None,');
if (process.argv.includes('--seh')) desktop=desktop.replace('WINEDEBUG=-all', 'WINEDEBUG=+seh');
if (process.argv.includes('--window-events')) {
  if (process.argv.includes('--seh') || process.argv.includes('--shader-diagnosis'))
    throw new Error('Choose one bounded diagnostic trace.');
  desktop=desktop.replace('WINEDEBUG=-all','WINEDEBUG=+msg,+event,+seh,+loaddll');
}
if (process.argv.includes('--original-wrapper')) desktop=desktop.replace('WINEDEBUG=-all', 'WINEDEBUG=-all DIMENSITY_WRAPPER_BOUNDS=0');
if (process.argv.includes('--shader-diagnosis')) desktop=desktop.replace('WINEDEBUG=-all',
  `WINEDEBUG=+seh DXVK_LOG_LEVEL=warn COMPAT_CAPTURE_SHADER_INPUTS=/data/user/0/${pkg}/files/diagnostics/shader-inputs`);
const saved = `${root}/users/xuser/Desktop/DG-Superposition.desktop`;
const call = args => execFileSync(adb, args, {timeout:20000,encoding:'utf8'});
if (process.argv.includes('--shader-diagnosis')) call(['shell','run-as',pkg,'mkdir','-p','files/diagnostics/shader-inputs']);
if (diagnostic) await stagePrivate(adb,pkg,fs.readFileSync('artifacts/DG-SuperpositionLaunch.exe'),`${root}/DGSmoke/DG-SuperpositionLaunch.exe`);
await stagePrivate(adb,pkg,Buffer.from(desktop),saved);
fs.mkdirSync('artifacts/superposition', {recursive:true});
fs.writeFileSync('artifacts/superposition/DG-Superposition.desktop',desktop);
if (!process.argv.includes('--stage-only')) console.log(call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]).trim());
