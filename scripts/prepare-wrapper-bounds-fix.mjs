// Reproducible minimal patch of our bundled open-source graphics wrapper.
// Original remains unchanged; reject any other build instead of guessing offsets.
import fs from 'node:fs';
import {createHash} from 'node:crypto';
const original = '.tooling/wrapper-analysis/libvulkan_wrapper.so';
const output = 'artifacts/wrapper-bounds';
const expected = '3b4c285550847a0caa6d110614298d6c0c57feefd9c96460713ad707a3c8304e';
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const bytes = fs.readFileSync(original);
if (hash(bytes) !== expected) throw new Error('Unrecognized wrapper; no patch applied.');
const patches = [
  {offset:0x110b04, before:0x7100183f, after:0x7100603f, assembly:'cmp w1, #24'},
  {offset:0x110b24, before:0x2a0103f3, after:0x53027c33, assembly:'lsr w19, w1, #2'},
];
for (const patch of patches) {
  if (bytes.readUInt32LE(patch.offset) !== patch.before) throw new Error('Unexpected instruction.');
  bytes.writeUInt32LE(patch.after, patch.offset);
}
fs.mkdirSync(output,{recursive:true});
fs.writeFileSync(`${output}/libvulkan_wrapper_dimensity.so`,bytes);
fs.writeFileSync(`${output}/bounds-fix.json`,JSON.stringify({
  description:'Convert byte length to word count before SPIR-V traversal; retain transformation.',
  source:'https://github.com/GunaCharanTeja/gamenative-wrapper',
  sourceCommit:'b8663654f7eacad900d2a23958422c64878fdf79',
  originalSha256:expected, patchedSha256:hash(bytes), bytes:bytes.length,
  originalBuildIdRetained:true, patches
},null,2));
console.log(`Prepared ${bytes.length} bytes, SHA-256 ${hash(bytes)}`);
