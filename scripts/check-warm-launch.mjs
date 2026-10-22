// One new lifecycle case only: dispatch while the existing library is in front.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
const adb='D:/Collect/.tooling/android-sdk/platform-tools/adb.exe', pkg='org.dimensity.gaming.dev';
const root='files/imagefs/home/xuser-1/.wine/drive_c';
const desktop=fs.readFileSync('artifacts/launcher/DG-AutoLaunchCheck.desktop','utf8')
  .replace('execArgs=--label "A B" -sample one','execArgs=--label warm-entry-v018');
const saved=`${root}/users/xuser/Desktop/DG-WarmEntry.desktop`;
const call=(args,options={})=>execFileSync(adb,args,{timeout:20000,...options});
call(['exec-in','run-as',pkg,'sh','-c',`cat > '${saved}'`],{input:Buffer.from(desktop)});
call(['exec-out','run-as',pkg,'stat','-c','%s',saved]);
console.log(call(['shell','am','start','--activity-single-top','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
  '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',`/data/user/0/${pkg}/${saved}`],{encoding:'utf8'}).trim());
await new Promise(resolve=>setTimeout(resolve,4000));
const trace=call(['exec-out','run-as',pkg,'cat','files/diagnostics/launch-entry.json'],{encoding:'utf8'});
const result=call(['exec-out','run-as',pkg,'cat',`${root}/DGSmoke/direct-launch.txt`],{encoding:'utf8'});
fs.writeFileSync('artifacts/launcher/warm-entry-trace.json',trace);
fs.writeFileSync('artifacts/launcher/warm-entry-result.txt',result);
console.log(trace.trim());
console.log(result.slice(-450));
if (!result.includes('arg_2=warm-entry-v018')) process.exitCode=1;
