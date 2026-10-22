import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {stagePrivate} from './lib/adb-private.mjs';
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev', root=`/data/user/0/${pkg}`;
const call=(args,options={})=>execFileSync(adb,args,{timeout:30000,windowsHide:true,...options});
const lock=JSON.parse(fs.readFileSync('artifacts/wrapper-bounds/bounds-fix.json'));
function remoteHash(file,expected) {
  const result=call(['shell','run-as',pkg,'sha256sum',file],{encoding:'utf8'});
  if (result.split(/\s/)[0]!==expected) throw new Error('App-private wrapper hash mismatch.');
}
remoteHash('files/imagefs/usr/lib/libvulkan_wrapper.so',lock.originalSha256);
call(['shell','run-as',pkg,'mkdir','-p','files/diagnostics/wrapper-bounds']);
for(const [source,name] of [
  ['artifacts/wrapper-bounds/check-bounds','check-bounds'],
  ['artifacts/wrapper-bounds/libvulkan_wrapper_dimensity.so','fixed.so'],
  ['artifacts/superposition/shader-inputs/last-input-21368-21407.spv','input.spv'],
]) await stagePrivate(adb,pkg,fs.readFileSync(source),`files/diagnostics/wrapper-bounds/${name}`);
remoteHash('files/diagnostics/wrapper-bounds/fixed.so',lock.patchedSha256);
remoteHash('files/diagnostics/wrapper-bounds/input.spv','754a38c523f4d3f50f04a36418219a41f4942440feced796d3e166a65c7cdb50');
const native=`${root}/files/diagnostics/wrapper-bounds`,lib=`${root}/files/imagefs/usr/lib`;
try {
  const result=call(['shell','run-as',pkg,'env',`LD_LIBRARY_PATH=${lib}:/system/lib64:/vendor/lib64:/apex/com.android.vndk.v34/lib64`,
    '/system/bin/linker64',`${native}/check-bounds`,`${lib}/libvulkan_wrapper.so`,`${native}/fixed.so`,`${native}/input.spv`],
    {encoding:'utf8',maxBuffer:1024*1024});
  fs.writeFileSync('artifacts/wrapper-bounds/guard-result.txt',result);console.log(result.trim());
} catch(error) {
  fs.writeFileSync('artifacts/wrapper-bounds/guard-result.txt',String(error.stdout??'')+String(error.stderr??''));
  throw error;
}
