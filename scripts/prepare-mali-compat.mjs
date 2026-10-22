// Fetch only the pinned Android source/dependencies; never run upstream scripts.
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';

const revision = '22960fb33a91226a9ab615288b0846ef7eac456b';
const destination = path.resolve('.tooling/mali-compat-source');
const response = await fetch(`https://api.github.com/repos/leegao/compat_layer/git/trees/${revision}?recursive=1`);
if (!response.ok) throw new Error(`Source index: HTTP ${response.status}`);
const tree = await response.json();
if (tree.sha !== revision || tree.truncated) throw new Error('Incomplete or unexpected source index');
const selected = tree.tree.filter(entry => entry.type === 'blob' && (
  ['CMakeLists.txt', 'LICENSE', 'README.md', 'libdxvk_mali_compat_layer.json.in',
    'libdxvk_mali_compat_layer32.json.in'].includes(entry.path) ||
  /^src\/(?!spirv-tools\/)/.test(entry.path) ||
  /^src\/spirv-tools\/include\//.test(entry.path) ||
  /^src\/spirv-tools\/android-arm64\/libSPIRV-Tools(?:-opt)?\.a$/.test(entry.path)
));

function blobHash(bytes) {
  return crypto.createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
}
let fetched = 0;
for (const entry of selected) {
  const target = path.resolve(destination, entry.path);
  if (!target.startsWith(destination + path.sep)) throw new Error('Invalid source path');
  const cached = await fs.readFile(target).catch(() => null);
  if (cached && blobHash(cached) === entry.sha) continue;
  const result = await fetch(`https://raw.githubusercontent.com/leegao/compat_layer/${revision}/${entry.path}`);
  if (!result.ok) throw new Error(`${entry.path}: HTTP ${result.status}`);
  const bytes = Buffer.from(await result.arrayBuffer());
  if (bytes.length !== entry.size || blobHash(bytes) !== entry.sha) throw new Error(`Source integrity: ${entry.path}`);
  await fs.mkdir(path.dirname(target), { recursive: true });
  await fs.writeFile(target, bytes);
  fetched++;
}
await fs.writeFile(path.join(destination, 'dimensity-source-lock.json'), JSON.stringify({
  repository: 'https://github.com/leegao/compat_layer', revision,
  files: selected.map(({ path, sha, size }) => ({ path, sha, size }))
}, null, 2) + '\n');
console.log(`Mali compat source ${revision}: ${selected.length} verified files, ${fetched} downloaded`);
