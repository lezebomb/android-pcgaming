// Launches the original free benchmark or its original engine with public options.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';

const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev';
const root='files/imagefs/home/xuser-1/.wine/drive_c';
const diagnostic=process.argv.includes('--diagnose');
const characterCreation=process.argv.includes('--character-creation');
const engine=characterCreation||process.argv.includes('--engine');
const rendererArgument=process.argv.filter(value=>value.startsWith('--wined3d-renderer='));
if(rendererArgument.length>1)throw new Error('Choose one WineD3D auxiliary renderer.');
const renderer=rendererArgument[0]?.split('=')[1];
if(renderer&&!['gl','vulkan','gdi'].includes(renderer))throw new Error('Expected gl, vulkan or gdi.');
const astc=process.argv.includes('--astc-textures');
const compute=astc||process.argv.includes('--compute-textures');
const graphicsConfig=compute?'vulkanVersion=1.3;version=System;blacklistedExtensions=;maxDeviceMemory=0;presentMode=fifo;syncFrame=0;disablePresentWait=0;astcTranscode='+Number(astc)+';etc2Transcode=0;resourceType=auto;bcnEmulation=auto;bcnEmulationType=compute;bcnEmulationCache=1;bcnQualityPreset=fast;gpuName=Device':'';
const executable=diagnostic?'C:/DGSmoke/DG-FFXIVLaunch.exe':engine?
  'C:/Games/DGFFXIVBench/game/ffxiv_dx11.exe':'C:/Games/DGFFXIVBench/ffxiv-dawntrail-bench.exe';
// Public option names: doitsujin/ffxiv-benchmark-launcher, ffxiv-benchmark.py.
// Independent low-cost diagnostic settings; no upstream implementation is copied.
const engineArguments=[
  'SYS.Language=1','SYS.Fps=1','SYS.ScreenMode=0','SYS.ScreenWidth=960','SYS.ScreenHeight=540',
  'SYS.FullScreenWidth=960','SYS.FullScreenHeight=540','SYS.GraphicsRezoScale=50',
  'SYS.GraphicsRezoUpscaleType=0','SYS.DynamicRezoType=0','SYS.LodType_DX11=1',
  'SYS.AntiAliasing_DX11=0','SYS.ReflectionType_DX11=0','SYS.GrassQuality_DX11=0',
  'SYS.ShadowLOD_DX11=1','SYS.ShadowBgLOD=1','SYS.ShadowVisibilityTypeSelf_DX11=0',
  'SYS.ShadowVisibilityTypeOther_DX11=0','SYS.TextureRezoType=0','SYS.SSAO_DX11=0',
  'SYS.Glare_DX11=0','SYS.DepthOfField_DX11=0','SYS.Tessellation_DX11=0',
  ...(characterCreation?['Bench.CharacterCreation=1']:[])
].join(' ');
const saved=`${root}/users/xuser/Desktop/DG-FFXIVBenchmark.desktop`;
const call=args=>execFileSync(adb,args,{timeout:20000,encoding:'utf8',windowsHide:true});
call(['exec-out','run-as',pkg,'stat',`${root}/Games/DGFFXIVBench/${engine?'game/ffxiv_dx11.exe':'ffxiv-dawntrail-bench.exe'}`]);
if(diagnostic) await stagePrivate(adb,pkg,fs.readFileSync('artifacts/DG-FFXIVLaunch.exe'),`${root}/DGSmoke/DG-FFXIVLaunch.exe`);
const desktop=`[Desktop Entry]
Type=Application
Name=FINAL FANTASY XIV official benchmark
Exec=wine ${executable}
[Extra Data]
container_id=1
executablePath=${executable}
${engine?'execArgs='+(diagnostic?'--engine ':'')+engineArguments+'\n':''}\
workingDirectory=C:/Games/DGFFXIVBench
screenSize=${process.argv.includes('--tall-desktop')?'1280x960':'1280x800'}
dxwrapper=dxvk
${compute?'graphicsDriverConfig='+graphicsConfig+'\n':''}\
dxwrapperConfig=version=1.14.1-sarek,framerate=30,async=0,asyncCache=0,vkd3dVersion=None,vkd3dLevel=auto,maliCompat=${process.argv.includes('--without-mali-layer')?'0':'1'},ddrawrapper=none,dxvkConfig=1
envVars=WINEDEBUG=${diagnostic?'+seh,+loaddll':'-all'} DXVK_LOG_LEVEL=info DXVK_LOG_PATH=C:/Games/DGFFXIVBench${renderer?' WINE_D3D_CONFIG=renderer='+renderer:''}
controlsProfile=${process.argv.includes('--mapped-controls')?'1000':'0'}
${process.argv.includes('--legacy-query')?'legacyGraphicsQuery=gdi\n':''}\
simTouchScreen=1
`;
await stagePrivate(adb,pkg,Buffer.from(desktop),saved);
fs.mkdirSync('artifacts/ffxiv',{recursive:true});
fs.writeFileSync('artifacts/ffxiv/DG-FFXIVBenchmark.desktop',desktop);
console.log(call(['shell','am','start','--activity-single-top','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`]).trim());
