import fs from 'node:fs';
import {execFileSync} from 'node:child_process';
const adb = process.env.ADB ?? 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const file = process.argv[2];
if (!file || !/^artifacts\/[a-zA-Z0-9/._-]+\.png$/.test(file) || file.includes('..')) throw new Error('Give a workspace artifact PNG path.');
const call = args => execFileSync(adb, args, {timeout:20000,encoding:'utf8'});
call(['shell','uiautomator','dump','/data/local/tmp/dimensity-runtime.xml']);
let xml = call(['exec-out','cat','/data/local/tmp/dimensity-runtime.xml']);
// This unrelated OS dialog may obscure our runtime. Only choose Wait, never stop Settings.
if (xml.includes('text="设置没有响应"')) {
  const node = [...xml.matchAll(/<node\b[^>]*>/g)].map(x=>x[0]).find(x=>x.includes('text="等待"'));
  const m = node?.match(/bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"/);
  if (m) call(['shell','input','tap',String((+m[1]+ +m[3])/2),String((+m[2]+ +m[4])/2)]);
  call(['shell','uiautomator','dump','/data/local/tmp/dimensity-runtime.xml']);
  xml = call(['exec-out','cat','/data/local/tmp/dimensity-runtime.xml']);
}
if (xml.includes('Dimensity Gaming') && xml.includes('尝试读取应用列表')) {
  const node = [...xml.matchAll(/<node\b[^>]*>/g)].map(x=>x[0]).find(x=>x.includes('text="拒绝"'));
  const m = node?.match(/bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"/);
  if (m) call(['shell','input','tap',String((+m[1]+ +m[3])/2),String((+m[2]+ +m[4])/2)]);
}
if (process.argv.includes('--escape')) {
  call(['shell','input','keyevent','111']);
  await new Promise(resolve=>setTimeout(resolve,1000));
}
call(['shell','screencap','-p','/data/local/tmp/dimensity-runtime.png']);
call(['pull','/data/local/tmp/dimensity-runtime.png',file]);
fs.writeFileSync(file.replace('.png','.xml'), xml);
console.log(file);
