// One reversible GUI-launcher check using the upstream open-source NVAPI bridge.
// All added DLL/config files stay in our app's Wine prefix; original EXEs stay intact.
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev',root='files/imagefs/home/xuser-1/.wine/drive_c';
const call=(args,options={})=>execFileSync(adb,args,{timeout:20000,windowsHide:true,...options});
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const dll=fs.readFileSync('.tooling/game-tests/dxvk-nvapi/extracted/x64/nvapi64.dll');
const hash=createHash('sha256').update(dll).digest('hex');
const target=`${root}/Games/DGSuperposition/bin/nvapi64.dll`;
const saved=`${root}/users/xuser/Desktop/DG-SuperpositionNVAPI.desktop`;
const interactive=process.argv.includes('--open');
const identityOnly=process.argv.includes('--identity-only');
const prefix=identityOnly ? 'identity' : 'nvapi';
const cleanup=process.argv.includes('--cleanup');
let existed=false;
try { existed=call(['shell','run-as',pkg,'sha256sum',target],{encoding:'utf8',stdio:['ignore','pipe','ignore']}).startsWith(hash);if(!existed)throw new Error('An unrelated NVAPI DLL is already present.'); }
catch(error) { if(error.status!==1)throw error; }
if(cleanup) {
  const processes=call(['shell','ps','-A'],{encoding:'utf8'});
  if(processes.split(/\r?\n/).some(line=>/u0_a335\b/.test(line)&&/\\(?:launcher|superposition)\.exe\s*$/i.test(line)))
    throw new Error('Close the Superposition windows before cleaning up their DLL.');
  if(existed)call(['exec-out','run-as',pkg,'rm','-f',target]);
  call(['exec-out','run-as',pkg,'rm','-f',saved,`${root}/DGSmoke/superposition-nvapi.conf`]);
  console.log('Removed only the matching temporary DLL and diagnostic shortcut/config.');
  process.exit(0);
}
const desktop=`[Desktop Entry]\nType=Application\nName=Superposition NVAPI diagnostic\nExec=wine C:/DGSmoke/DG-SuperpositionLaunch.exe\n[Extra Data]\ncontainer_id=1\ncontrolsProfile=0\nexecutablePath=C:/DGSmoke/DG-SuperpositionLaunch.exe\nworkingDirectory=C:/Games/DGSuperposition\nscreenSize=1280x800\ndxwrapper=dxvk\ndxwrapperConfig=version=v2.3.1-stripped,framerate=30,async=0,asyncCache=0,vkd3dVersion=2.14.1,vkd3dLevel=auto,maliCompat=1,ddrawrapper=none,dxvkConfig=1\nenvVars=WINEDEBUG=-all WINEDLLOVERRIDES=nvapi64=n DXVK_ENABLE_NVAPI=1 DXVK_NVAPI_ALLOW_OTHER_DRIVERS=1 DXVK_CONFIG_FILE=C:/DGSmoke/superposition-nvapi.conf DXVK_NVAPI_LOG_LEVEL=info DXVK_NVAPI_LOG_PATH=C:/Games/DGSuperposition DXVK_LOG_PATH=C:/Games/DGSuperposition QV4_FORCE_INTERPRETER=1 QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic\n`;
try {
  if(!identityOnly) await stagePrivate(adb,pkg,dll,target);
  await stagePrivate(adb,pkg,Buffer.from('dxgi.customVendorId = 10de\ndxgi.hideNvidiaGpu = False\n'),`${root}/DGSmoke/superposition-nvapi.conf`);
  await stagePrivate(adb,pkg,fs.readFileSync('artifacts/DG-SuperpositionLaunch.exe'),`${root}/DGSmoke/DG-SuperpositionLaunch.exe`);
  let entry=interactive ? desktop.replaceAll('C:/DGSmoke/DG-SuperpositionLaunch.exe','C:/Games/DGSuperposition/Superposition.exe')+'simTouchScreen=1\n' : desktop;
  if(identityOnly) entry=entry.replace('WINEDLLOVERRIDES=nvapi64=n DXVK_ENABLE_NVAPI=1 DXVK_NVAPI_ALLOW_OTHER_DRIVERS=1 ','');
  if(process.argv.includes('--seh')) entry=entry.replace('WINEDEBUG=-all','WINEDEBUG=+seh DXVK_LOG_LEVEL=warn');
  await stagePrivate(adb,pkg,Buffer.from(entry),saved);
  call(['exec-out','run-as',pkg,'rm','-f',`${root}/DGSmoke/superposition-launch.txt`]);
  call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
    '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]);
  if(interactive) {
    await wait(6500);
    fs.writeFileSync(`artifacts/superposition/${prefix}-gui-open.png`,call(['exec-out','screencap','-p'],{maxBuffer:16000000}));
    console.log('Original GUI launched; temporary NVAPI files retained until --cleanup.');
  } else {
  let result='';
  for(let i=0;i<46;i++) {
    await wait(1000);
    try { result=call(['exec-out','run-as',pkg,'cat',`${root}/DGSmoke/superposition-launch.txt`],{encoding:'utf8'}); } catch {}
    if(result.includes('complete=1'))break;
  }
  fs.writeFileSync('artifacts/superposition/launch-nvapi-result.txt',result);
  fs.writeFileSync('artifacts/superposition/launch-nvapi-stderr.txt',call(['exec-out','run-as',pkg,'cat',`${root}/DGSmoke/superposition-launch-stderr.txt`]));
  fs.writeFileSync('artifacts/superposition/launch-nvapi.png',call(['exec-out','screencap','-p'],{maxBuffer:16000000}));
  console.log(result);
  }
} finally {
  // The diagnostic child has a bounded lifetime. Never replace/remove an original DLL.
  if(!interactive) {
    if(!existed)call(['exec-out','run-as',pkg,'rm','-f',target]);
    call(['exec-out','run-as',pkg,'rm','-f',saved,`${root}/DGSmoke/superposition-nvapi.conf`]);
  }
}
