// Click a position observed in a current runtime screenshot, then capture the result.
import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
import {requireRuntimeFocus} from './lib/runtime-focus.mjs';
const [x,y,output]=process.argv.slice(2),nx=Number(x),ny=Number(y);
if(!Number.isFinite(nx)||!Number.isFinite(ny)||nx<0||nx>1||ny<0||ny>1
    ||!/^artifacts\/[a-zA-Z0-9/._-]+\.png$/.test(output)||output.includes('..')) throw new Error('Expected normalized observed x/y and an artifact PNG.');
const adb=process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const call=(args,options={})=>execFileSync(adb,args,{timeout:20000,windowsHide:true,maxBuffer:16000000,...options});
await requireRuntimeFocus(call,'org.dimensity.gaming.dev');
const before=call(['exec-out','screencap','-p']);
const width=before.readUInt32BE(16),height=before.readUInt32BE(20);
// simTouchScreen=1 must already be selected in this game's saved shortcut.
const px=String(Math.round(nx*width)),py=String(Math.round(ny*height));
call(['shell','input','swipe',px,py,px,py,'120']);
await new Promise(resolve=>setTimeout(resolve,1200));
fs.writeFileSync(output,call(['exec-out','screencap','-p']));
console.log(output);
