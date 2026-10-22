import fs from 'node:fs';
import {patchMemoryWire} from './lib/vortek-memory-wire.mjs';
import {createHash} from 'node:crypto';
const file='app/src/main/cpp/vortekrenderer/include/vortek_serializer.h';
const originalHash='2761a1cad4c6812004e57c210b57507800b64b28ba33824b936d9daa37461253';
const expression='*(VkImageLayout*)(inputBuffer + bufferOffset)';
const mapped=`vt_hostImageLayout(${expression})`;
const input=fs.readFileSync(file,'utf8').replaceAll('\r\n','\n');
const original=input.replaceAll(mapped,expression);
if(createHash('sha256').update(original).digest('hex')!==originalHash && createHash('sha256').update(original).digest('hex')!=='91681f0ebe619db601ccedb68661b9d9361ac92a56e56cc4287b72d250dd93bb')
  throw Error('Unrecognized Vortek wire serializer; layout mapping not applied');
const count=original.split('= '+expression+';').length-1;
if(!count)throw Error('No host layout fields found');
fs.writeFileSync(file,patchMemoryWire(original).replaceAll('= '+expression+';','= '+mapped+';'));
console.log(`Mapped ${count} host layout reads; paired memory serializer retained.`);
