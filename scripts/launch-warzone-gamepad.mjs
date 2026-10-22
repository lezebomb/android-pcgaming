// Launch one bounded real campaign session with the portable keyboard/mouse mapping.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev', root='files/imagefs/home/xuser-1/.wine/drive_c';
const call=args=>execFileSync(adb,args,{timeout:20000,encoding:'utf8'});
const executable='DG-WarzoneGamepad.exe', saved=`${root}/users/xuser/Desktop/DG-WarzoneGamepad.desktop`;
call(['exec-out','run-as',pkg,'rm','-f',`${root}/DGSmoke/warzone-play.stop`,`${root}/DGSmoke/warzone-dialog.txt`]);
await stagePrivate(adb,pkg,fs.readFileSync(`artifacts/${executable}`),`${root}/DGSmoke/${executable}`);
let desktop=`[Desktop Entry]\nType=Application\nName=Warzone gamepad short session\nExec=wine C:/DGSmoke/${executable}\n[Extra Data]\ncontainer_id=1\ncontrolsProfile=1000\nexecutablePath=C:/DGSmoke/${executable}\nenvVars=SDL_JOYSTICK_WGI=0 SDL_JOYSTICK_RAWINPUT=0 SDL_JOYSTICK_GAMEINPUT=0 SDL_XINPUT_ENABLED=1 WINEDEBUG=-all\n`;
if (process.argv.includes('--vortek')) desktop += 'vulkanBackend=vortek\n';
const extraArgs=[];
if (process.argv.includes('--directx')) extraArgs.push('--directx');
if (process.argv.includes('--long-play')) extraArgs.push('--long-play');
if(extraArgs.length) desktop += 'execArgs='+extraArgs.join(' ')+'\n';
await stagePrivate(adb,pkg,Buffer.from(desktop),saved);
call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]);
console.log(`Started the original game via bounded helper (max ${process.argv.includes('--long-play')?8:3} minutes); no gameplay pass implied.`);
