// Observe one authorized saved-game intent in our debuggable app. No heap dump.
import fs from 'node:fs';
import {spawn,execFileSync} from 'node:child_process';
const adb=process.env.ADB??'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const pkg='org.dimensity.gaming.dev';
const call=args=>execFileSync(adb,args,{timeout:15000,encoding:'utf8',windowsHide:true});
const pid=call(['shell','pidof',pkg]).trim();
const port='50388';
if(call(['forward','--list']).includes(`tcp:${port} `))throw Error('Diagnostic port in use');
call(['forward',`tcp:${port}`,`jdwp:${pid}`]);
let transcript='',triggered=false,inspected=false;
const child=spawn('D:/Collect/.tooling/jdk/current/bin/jdb.exe',[
  '-J-Duser.language=en','-J-Duser.country=US','-J-Dfile.encoding=UTF-8',
  '-connect',`com.sun.jdi.SocketAttach:hostname=127.0.0.1,port=${port}`
],{stdio:['pipe','pipe','pipe'],windowsHide:true});
const receive=chunk=>{
  transcript+=chunk.toString();
  if(!triggered&&transcript.includes('Initializing jdb')){
    triggered=true;child.stdin.write('stop in com.winlator.cmod.MainActivity.onNewIntent\n');
    setTimeout(()=>{
      try {console.log(call(['shell','am','start','-a','android.intent.action.VIEW','-n',`${pkg}/com.winlator.cmod.MainActivity`,
        '--ez','launch_game','true','--ei','container_id','1','--es','shortcut_path',
        `/data/user/0/${pkg}/files/imagefs/home/xuser-1/.wine/drive_c/users/xuser/Desktop/DG-Superposition.desktop`]));}
      catch(error){transcript+='\nIntent dispatch failed: '+error.message;}
    },500);
  }
  if(!inspected&&transcript.includes('Breakpoint hit:')) {
    inspected=true;child.stdin.write('where\nprint this.launchEntryHandled\nprint intent.getBooleanExtra("launch_game",false)\ncont\nexit\n');
  }
};
child.stdout.on('data',receive);child.stderr.on('data',receive);
try {
  await Promise.race([new Promise(resolve=>child.once('exit',resolve)),new Promise(resolve=>setTimeout(resolve,10000))]);
} finally {
  if(child.exitCode===null){child.stdin.end('resume\nexit\n');await new Promise(resolve=>setTimeout(resolve,300));if(child.exitCode===null)child.kill();}
  call(['forward','--remove',`tcp:${port}`]);
  fs.writeFileSync('artifacts/launcher/entry-breakpoint.txt',transcript);
  console.log(transcript.slice(-6000));
}
