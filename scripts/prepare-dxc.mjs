// Official host-side compiler for our own Shader Model 6 diagnostics.
import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';

const url = 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/dxc_2026_09_29.zip';
const expected = 'ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1';
const bytes = 32287592;
const directory = path.resolve('.tooling/game-tests/dxc');
const archive = path.join(directory, 'dxc_2026_09_29.zip');
fs.mkdirSync(directory, {recursive: true});
const valid = file => {
  if (!fs.existsSync(file) || fs.statSync(file).size !== bytes) return false;
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex') === expected;
};
if (!valid(archive)) {
  const response = await fetch(url, {signal: AbortSignal.timeout(90000)});
  if (!response.ok) throw new Error(`Official DXC download failed: ${response.status}`);
  const temporary = archive + '.part';
  fs.writeFileSync(temporary, Buffer.from(await response.arrayBuffer()));
  if (!valid(temporary)) throw new Error('DXC asset size or SHA-256 mismatch');
  fs.renameSync(temporary, archive);
}
console.log(JSON.stringify({archive, bytes, sha256: expected, source: url}));
