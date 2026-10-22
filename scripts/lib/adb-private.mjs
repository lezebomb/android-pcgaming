import {execFileSync, spawn} from 'node:child_process';
import fs from 'node:fs';
import {pipeline} from 'node:stream/promises';

// exec-in may return before the device finishes consuming stdin. Never expose a
// partial EXE/shortcut to a concurrent launch. All mutations stay in our app UID.
export async function stagePrivate(adb, packageName, bytes, destination) {
  if (!destination.startsWith('files/') || destination.split('/').includes('..')) throw new Error('Expected an app-private relative file.');
  const quote = value => "'" + value.replaceAll("'", "'\\''") + "'";
  const temporary = `${destination}.part-${process.pid}-${Date.now()}`;
  const call = (args, options={}) => execFileSync(adb, args, {timeout:20000,...options});
  try {
    call(['exec-in','run-as',packageName,'sh','-c',`cat > ${quote(temporary)}`], {input:bytes});
    const deadline=Date.now()+10000;
    for (;;) {
      let size=-1;
      try { size=Number(call(['exec-out','run-as',packageName,'stat','-c','%s',temporary], {encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim()); } catch {}
      if (size===bytes.length) break;
      if (Date.now()>=deadline) throw new Error(`Incomplete staging: ${destination}`);
      await new Promise(resolve=>setTimeout(resolve,100));
    }
    call(['exec-out','run-as',packageName,'mv','-f',temporary,destination]);
  } finally {
    call(['exec-out','run-as',packageName,'rm','-f',temporary]);
  }
}

// Large game files must not be materialized as a Node Buffer. Stream into a
// private temporary file, then verify the device's size before exposing it.
export async function stagePrivateFile(adb, packageName, source, destination, onProgress) {
  if (!destination.startsWith('files/') || destination.split('/').includes('..'))
    throw new Error('Expected an app-private relative file.');
  const info=fs.statSync(source);
  if (!info.isFile() || !Number.isSafeInteger(info.size)) throw new Error('Expected a regular source file.');
  const quote=value=>"'"+value.replaceAll("'", "'\\''")+"'";
  const temporary=`${destination}.part-${process.pid}-${Date.now()}`;
  const call=(args,options={})=>execFileSync(adb,args,{timeout:20000,windowsHide:true,...options});
  const child=spawn(adb,['exec-in','run-as',packageName,'sh','-c',`cat > ${quote(temporary)}`],
    {stdio:['pipe','ignore','pipe'],windowsHide:true,timeout:1800000});
  let diagnostic='',sent=0;
  child.stderr.on('data',bytes=>{diagnostic=(diagnostic+bytes.toString()).slice(-4096);});
  const exited=new Promise((resolve,reject)=>{
    child.once('error',reject);
    child.once('close',(code,signal)=>code===0?resolve():reject(new Error(`Private transfer failed (${code??signal}): ${diagnostic}`)));
  });
  const input=fs.createReadStream(source);
  if(onProgress)input.on('data',bytes=>onProgress(sent+=bytes.length,info.size));
  try {
    await Promise.all([pipeline(input,child.stdin),exited]);
    const after=fs.statSync(source);
    if(after.size!==info.size||after.mtimeMs!==info.mtimeMs)throw new Error('Source changed during transfer.');
    const deadline=Date.now()+20000;
    for(;;) {
      let size=-1;
      try{size=Number(call(['exec-out','run-as',packageName,'stat','-c','%s',temporary],{encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim());}catch{}
      if(size===info.size)break;
      if(Date.now()>=deadline)throw new Error(`Incomplete private transfer: ${destination}`);
      await new Promise(resolve=>setTimeout(resolve,250));
    }
    call(['exec-out','run-as',packageName,'mv','-f',temporary,destination]);
  } finally {
    input.destroy();
    if(child.exitCode===null)child.kill();
    try{call(['exec-out','run-as',packageName,'rm','-f',temporary],{stdio:'ignore'});}catch{}
  }
}
