import { createWriteStream, createReadStream, existsSync, renameSync, rmSync, mkdirSync, writeFileSync } from 'node:fs';
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';

// The release digest detects corruption; it is not a license or an authenticity audit.
const root = resolve(import.meta.dirname, '..');
const requested = new Set(process.argv.slice(2));
const response = await fetch('https://api.github.com/repos/GunaCharanTeja/Winlator-Extras/releases/tags/Sd');
if (!response.ok) throw new Error(`GitHub ${response.status}`);
const release = await response.json();
const names = requested.size ? requested : new Set(['imagefs.tzst', 'proton-10-arm64ec.tzst', 'proton-10-arm64ec_container_pattern.tzst']);
mkdirSync(resolve(root, 'artifacts'), { recursive: true });
const records = [];
const pinned = {
  'imagefs.tzst': '0c8c6c1c7653766323378f3294be6618392075bd354d50b2416a534a6e231620',
  'proton-10-arm64ec.tzst': 'd00ff472fb2ee9fe1bfe1a6e7b28d012e7f854b1ed10596aaa702295e8410548',
  'proton-10-arm64ec_container_pattern.tzst': '514fb9e0301d49194d904ea62d9d065649c014da1601a0a133dfaff756e1d79c'
};
for (const name of names) {
  const asset = release.assets.find(a => a.name === name);
  if (!asset?.digest?.startsWith('sha256:')) throw new Error(`No pinned digest for ${name}`);
  const dest = resolve(root, 'app/src/main/assets', name);
  const expected = asset.digest.substring(7);
  if (pinned[name] && pinned[name] !== expected) throw new Error(`Upstream asset changed: ${name}; review before updating the pin`);
  const digest = async path => {
    const hash = createHash('sha256');
    for await (const chunk of createReadStream(path)) hash.update(chunk);
    return hash.digest('hex');
  };
  if (!existsSync(dest) || await digest(dest) !== expected) {
    console.log(`Downloading ${name}: ${Math.round(asset.size / 1048576)} MiB`);
    const download = await fetch(asset.browser_download_url, { signal: AbortSignal.timeout(600000) });
    if (!download.ok) throw new Error(`Download ${download.status}: ${name}`);
    const temp = dest + '.part';
    await pipeline(Readable.fromWeb(download.body), createWriteStream(temp));
    if (await digest(temp) !== expected) { rmSync(temp); throw new Error(`SHA-256 mismatch: ${name}`); }
    renameSync(temp, dest);
  }
  records.push({ name, size: asset.size, sha256: expected, url: asset.browser_download_url });
  console.log(`Verified ${name}`);
}
writeFileSync(resolve(root, 'artifacts/runtime-downloads.json'), JSON.stringify(records, null, 2) + '\n');
