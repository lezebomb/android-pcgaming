// Observe one GUI-launched engine. Do not launch games or sweep compatibility paths.
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe',pkg='org.dimensity.gaming.dev';
const call=(args,options={})=>execFileSync(adb,args,{timeout:15000,windowsHide:true,maxBuffer:17*1024*1024,...options});
const privateRead=file=>call(['exec-out','run-as',pkg,'cat',file]);
const directory='artifacts/superposition',begin=Date.now();
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let pid=null,firstSeen=null,samples=[],reason='wait_timeout';
console.log('Waiting for one app-private Superposition engine (no relaunch).');
while(Date.now()-begin<120000 || (firstSeen!==null && Date.now()-firstSeen<120000)) {
  if(!pid) {
    const processes=call(['shell','ps','-A'],{encoding:'utf8'});
    const match=processes.split(/\r?\n/).find(line=>/^u0_a335\s/.test(line)&&/C:\\Games\\DGSuperposition\\bin\\superposition\.exe\s*$/i.test(line));
    pid=match?.trim().split(/\s+/)[1] ?? null;
    if(pid) {firstSeen=Date.now();console.log(`Capturing native mappings for PID ${pid}.`);}
  }
  if(pid) {
    const status=privateRead(`/proc/${pid}/status`).toString();
    if(!status.includes('Name:') || /^State:\s+Z/m.test(status)) {reason='engine_exit';break;}
    const maps=privateRead(`/proc/${pid}/maps`);
    if(maps.toString().includes('libc.so'))fs.writeFileSync(`${directory}/engine-native-maps.txt`,maps);
    samples.push({elapsed_ms:Date.now()-begin,pid,status:status.split(/\r?\n/).filter(line=>/^(VmRSS|VmSize|Threads|Name):/.test(line))});
    reason='observe_timeout';
  }
  await delay(1000);
}
fs.writeFileSync(`${directory}/engine-native-samples.json`,JSON.stringify({pid,reason,samples},null,2));
await delay(500); // Let the app's pipe reader flush final critical records.
for(const name of ['wine.log','wine.log.previous','wine-errors.log','wine-errors.log.previous']) {
  try {
    call(['shell','run-as',pkg,'test','-f',`files/diagnostics/${name}`]);
    fs.writeFileSync(`${directory}/engine-native-${name}`,privateRead(`files/diagnostics/${name}`));
  } catch {}
}
if(pid) {
  const inputs=[];
  let listing='';try{listing=call(['shell','run-as',pkg,'ls','files/diagnostics/shader-inputs'],{encoding:'utf8'});}catch{}
  const destination=`${directory}/shader-inputs`;fs.mkdirSync(destination,{recursive:true});
  for(const name of listing.split(/\s+/).filter(name=>new RegExp(`^last-input-${pid}-[0-9]+\\.spv$`).test(name)).slice(0,64)) {
    const bytes=privateRead(`files/diagnostics/shader-inputs/${name}`);
    fs.writeFileSync(`${destination}/${name}`,bytes);
    inputs.push({name,bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')});
  }
  fs.writeFileSync(`${directory}/shader-input-manifest.json`,JSON.stringify({pid,inputs},null,2));
}
console.log(`Saved single engine session: ${reason}.`);