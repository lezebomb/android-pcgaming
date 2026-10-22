// Bounded, application-private checks. Run only the new path that needs evidence.
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';

const profiles = {
  shader: {exe: 'DG-D3D12Shader.exe', result: 'd3d12-shader.txt', log: 'vkd3d-shader.log'},
  bc: {exe: 'DG-D3D12BC.exe', result: 'd3d12-bc-textures.txt', log: 'vkd3d-bc.log', pixel: 'texture-ps.dxil'},
  sparse: {exe: 'DG-VulkanSparseBudget.exe', result: 'vulkan-sparse-budget.txt', shaders:false, env:'COMPAT_SPARSE_COMMIT_BUDGET=4096'},
};
const selected = process.argv[2];
if (!Object.hasOwn(profiles, selected)) throw new Error('Choose shader, bc or sparse; do not rerun passed paths without a change.');
const profile = profiles[selected];
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const serial = process.env.ANDROID_SERIAL ?? 'HA2C3HFV';
const pkg = 'org.dimensity.gaming.dev';
const root = 'files/imagefs/home/xuser-1/.wine/drive_c';
const outputDir = path.resolve('artifacts/mali-compat');
const desktopName = profile.exe.replace(/\.exe$/, '.desktop');
const invoke = (args, options = {}) => execFileSync(adb, ['-s', serial, ...args], {timeout: 20000, ...options});
const read = file => invoke(['exec-out', 'run-as', pkg, 'cat', file]);
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
fs.mkdirSync(outputDir, {recursive: true});
async function stage(data, destination) {
  await stagePrivate(adb,pkg,data,destination);
}
const extraEnv = profile.env ?? `VKD3D_DEBUG=info VKD3D_LOG_FILE=C:/DGSmoke/${profile.log}`;
const desktop = `[Desktop Entry]\nType=Application\nName=Dimensity ${selected} diagnostic\nExec=wine C:/DGSmoke/${profile.exe}\n[Extra Data]\ncontainer_id=1\nexecutablePath=C:/DGSmoke/${profile.exe}\ndxwrapper=dxvk\ndxwrapperConfig=version=v2.3.1-stripped,framerate=30,async=0,asyncCache=0,vkd3dVersion=2.14.1,vkd3dLevel=auto,maliCompat=1,ddrawrapper=none,dxvkConfig=1\nscreenSize=960x600\nenvVars=${extraEnv} WINEDEBUG=-all\n`;
const desktopFile = `${root}/users/xuser/Desktop/${desktopName}`;
await stage(fs.readFileSync(`artifacts/${profile.exe}`), `${root}/DGSmoke/${profile.exe}`);
for (const name of profile.shaders === false ? [] : ['gradient-vs.dxil', profile.pixel ?? 'gradient-ps.dxil'])
  await stage(fs.readFileSync(`.tooling/game-tests/dxc/${name}`), `${root}/DGSmoke/${name}`);
await stage(Buffer.from(desktop), desktopFile);
fs.writeFileSync(path.join(outputDir, desktopName), desktop);
invoke(['shell', 'run-as', pkg, 'rm', '-f', `${root}/DGSmoke/${profile.result}`]);
console.log(`Starting one bounded ${selected} check on ${serial}`);
invoke(['shell', 'am', 'start', '-a', 'android.intent.action.VIEW', '-n', `${pkg}/com.winlator.cmod.MainActivity`,
  '--ez', 'launch_game', 'true', '--ei', 'container_id', '1', '--es', 'shortcut_path', `/data/user/0/${pkg}/${desktopFile}`]);
const deadline = Date.now() + 45000;
let result = '';
while (Date.now() < deadline) {
  await delay(1000);
  try { result = read(`${root}/DGSmoke/${profile.result}`).toString('utf8'); }
  catch { continue; }
  if (result.includes('complete=1')) break;
}
fs.writeFileSync(path.join(outputDir, profile.result), result);
for (const [remote, local] of [...(profile.log ? [[`${root}/DGSmoke/${profile.log}`, profile.log]] : []), ['files/diagnostics/wine.log', `${selected}-wine.log`]]) {
  try { fs.writeFileSync(path.join(outputDir, local), read(remote)); } catch { /* optional diagnostics */ }
}
console.log(result.trim());
if (!result.includes('complete=1 result=0')) process.exitCode = 1;
