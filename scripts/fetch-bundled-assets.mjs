import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {pipeline} from 'node:stream/promises';
import {Readable} from 'node:stream';
const root=path.resolve(import.meta.dirname,'..');
const index=JSON.parse(fs.readFileSync(path.join(root,'docs/runtime-assets.json'),'utf8'));
const digest=async file=>{
  const result=createHash('sha256');
  for await(const chunk of fs.createReadStream(file))result.update(chunk);
  return result.digest('hex');
};
for(const asset of index.assets){
  if(asset.kind!=='pinned-url')continue;
  if(!/^app\/(src\/main\/(assets|jniLibs)|libs)\//.test(asset.path)||asset.path.split('/').includes('..'))
    throw Error('Invalid runtime destination');
  const destination=path.resolve(root,asset.path);
  if(fs.existsSync(destination)&&await digest(destination)===asset.sha256)continue;
  fs.mkdirSync(path.dirname(destination),{recursive:true});
  const response=await fetch(asset.url,{signal:AbortSignal.timeout(300000)});
  if(!response.ok)throw Error(`Runtime download failed (${response.status}): ${asset.path}`);
  const temporary=destination+'.part';
  try{
    await pipeline(Readable.fromWeb(response.body),fs.createWriteStream(temporary));
    if(fs.statSync(temporary).size!==asset.size||await digest(temporary)!==asset.sha256)
      throw Error('Runtime integrity mismatch: '+asset.path);
    fs.renameSync(temporary,destination);
  }finally{fs.rmSync(temporary,{force:true});}
  console.log('Restored '+asset.path);
}
console.log('Pinned baseline assets ready. Fetch the three large runtimes and build the optional graphics libraries separately.');
