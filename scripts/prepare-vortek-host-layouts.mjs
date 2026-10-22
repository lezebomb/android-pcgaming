import fs from 'node:fs';
import {createHash} from 'node:crypto';
const file='app/src/main/cpp/vortekrenderer/include/vortek_serializer.h';
const originalHash='2761a1cad4c6812004e57c210b57507800b64b28ba33824b936d9daa37461253';
const expression='*(VkImageLayout*)(inputBuffer + bufferOffset)';
const mapped=`vt_hostImageLayout(${expression})`;
const input=fs.readFileSync(file,'utf8').replaceAll('\r\n','\n');
const original=input.replaceAll(mapped,expression);
if(createHash('sha256').update(original).digest('hex')!==originalHash)
  throw Error('Unrecognized Vortek wire serializer; layout mapping not applied');
const count=original.split('= '+expression+';').length-1;
if(!count)throw Error('No host layout fields found');
fs.writeFileSync(file,original.replaceAll('= '+expression+';','= '+mapped+';'));
console.log(`Mapped ${count} host layout reads; byte layout and client protocol unchanged.`);
