// Normal free-edition launcher UI. No paid CLI benchmark automation.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg = 'org.dimensity.gaming.dev';
const root = 'files/imagefs/home/xuser-1/.wine/drive_c';
const diagnostic = process.argv.includes('--diagnose');
const engine = process.argv.includes('--engine');
if (diagnostic && engine) throw new Error('Choose launcher diagnosis or the normal engine EXE.');
const executable = diagnostic ? 'C:/DGSmoke/DG-SuperpositionLaunch.exe' : engine ? 'C:/Games/DGSuperposition/bin/superposition.exe' : 'C:/Games/DGSuperposition/Superposition.exe';
let desktop = `[Desktop Entry]\nType=Application\nName=Superposition\nExec=wine ${executable}\n[Extra Data]\ncontainer_id=1\nexecutablePath=${executable}\nworkingDirectory=C:/Games/DGSuperposition\nscreenSize=1280x800\ndxwrapper=dxvk\ndxwrapperConfig=version=v2.3.1-stripped,framerate=30,async=0,asyncCache=0,vkd3dVersion=2.14.1,vkd3dLevel=auto,maliCompat=1,ddrawrapper=none,dxvkConfig=1\nenvVars=WINEDEBUG=-all QV4_FORCE_INTERPRETER=1 QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic DXVK_LOG_LEVEL=info DXVK_LOG_PATH=C:/Games/DGSuperposition\n`;
desktop += 'adapterIdentity=nvidia\ncontrolsProfile=0\nsimTouchScreen=1\n';
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
