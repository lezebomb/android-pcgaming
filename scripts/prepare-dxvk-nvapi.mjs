// Pinned upstream release; cached locally for an app-private interoperability check.
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
const directory='.tooling/game-tests/dxvk-nvapi', name='dxvk-nvapi-v0.9.2.tar.gz';
const expected='60c284223530d643c446c263f1e1a96c6de7b5ff21796219646da734d97a70d6';
const target=`${directory}/${name}`;
fs.mkdirSync(directory,{recursive:true});
const valid=()=>fs.existsSync(target) && fs.statSync(target).size===4926359
  && createHash('sha256').update(fs.readFileSync(target)).digest('hex')===expected;
if(!valid()) {
  const response=await fetch(`https://github.com/jp7677/dxvk-nvapi/releases/download/v0.9.2/${name}`,{signal:AbortSignal.timeout(60000)});
  if(!response.ok) throw new Error(`Upstream release: ${response.status}`);
  const bytes=Buffer.from(await response.arrayBuffer());
  if(bytes.length!==4926359 || createHash('sha256').update(bytes).digest('hex')!==expected) throw new Error('Release integrity mismatch');
  fs.writeFileSync(target,bytes);
}
const entries=execFileSync('tar.exe',['-tzf',target],{encoding:'utf8'}).trim().split(/\r?\n/);
if(entries.some(entry=>entry.startsWith('/') || entry.includes('..') || entry.includes('\\') || entry.includes(':'))) throw new Error('Unsafe archive member');
const extracted=`${directory}/extracted`;fs.mkdirSync(extracted,{recursive:true});
execFileSync('tar.exe',['-xzf',target,'-C',extracted]);
for(const name of ['README.md','LICENSE']) {
  const response=await fetch(`https://raw.githubusercontent.com/jp7677/dxvk-nvapi/v0.9.2/${name}`,{signal:AbortSignal.timeout(20000)});
  if(!response.ok)throw new Error(`Source ${name}: ${response.status}`);
  fs.writeFileSync(`${directory}/${name}`,await response.text());
}
console.log(`Verified ${name} SHA256 ${expected}; ${entries.length} members extracted.`);
