// Observe an already GUI-launched free benchmark. Never relaunch or change it.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
const adb=process.env.ADB??'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev',directory='artifacts/superposition/scene';
const call=(args,options={})=>execFileSync(adb,args,{timeout:15000,windowsHide:true,maxBuffer:24*1024*1024,...options});
const uid=call(['shell','run-as',pkg,'id','-u'],{encoding:'utf8'}).trim();
const rows=()=>call(['shell','ps','-A','-o','UID,PID,NAME'],{encoding:'utf8'}).split(/\r?\n/);
const engine=()=>rows().map(r=>r.trim().split(/\s+/)).find(r=>r[0]===uid&&/^C:\\Games\\DGSuperposition\\bin\\superposition\.exe$/i.test(r[2]??''));
const pid=engine()?.[1];
if(!pid)throw Error('No existing engine in the development app UID.');
fs.mkdirSync(directory,{recursive:true});
const read=file=>call(['exec-out','run-as',pkg,'cat',file]);
const logPath='files/imagefs/home/xuser-1/.wine/drive_c/users/xuser/Superposition/log.html';
fs.writeFileSync(directory+'/command-line.txt',read('/proc/'+pid+'/cmdline'));
let previous='',result='observation_limit';
const started=Date.now();
for(let sample=0;sample<27;sample++) {
  const log=read(logPath);fs.writeFileSync(directory+'/engine-log.html',log);
  const text=log.toString();
  const phase=text.includes('Benchmark stopped')?'stopped':text.includes('Benchmark running')?'running':'loading';
  const elapsed=Math.round((Date.now()-started)/1000);
  // A loading image, a scene candidate, and the returned launcher distinguish
  // actual output from startup/exit logs; captures are ignored local artifacts.
  if(phase!==previous||sample%2===0) {
    fs.writeFileSync(`${directory}/${String(elapsed).padStart(3,'0')}-${phase}.png`,call(['exec-out','screencap','-p']));
    if(phase!==previous)console.log(`engine ${pid}: ${phase}, elapsed ${elapsed}s`);
  }
  previous=phase;
  if(engine()?.[1]!==pid){result='engine_exit';break;}
  await new Promise(r=>setTimeout(r,5000));
}
fs.writeFileSync(directory+'/result.json',JSON.stringify({pid,result,lastPhase:previous,elapsedMs:Date.now()-started},null,2)+'\n');
fs.writeFileSync(directory+'/wine-errors.log',read('files/diagnostics/wine-errors.log'));
console.log(result+'; screenshots and logs saved locally. No gameplay verdict inferred.');
