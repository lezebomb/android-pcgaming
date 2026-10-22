// Official free interactive 3D workload, cached for compatibility development.
import fs from 'node:fs';
import {Readable, Transform} from 'node:stream';
import {pipeline} from 'node:stream/promises';
import crypto from 'node:crypto';
const directory = '.tooling/game-tests/superposition';
const file = `${directory}/Unigine_Superposition-1.1.exe`;
const record = `${directory}/download.json`;
const url = 'https://assets.unigine.com/d/Unigine_Superposition-1.1.exe';
fs.mkdirSync(directory, {recursive: true});
if (fs.existsSync(file) && fs.existsSync(record)) {
  const metadata = JSON.parse(fs.readFileSync(record, 'utf8'));
  if (metadata.bytes === fs.statSync(file).size) { console.log(JSON.stringify(metadata)); process.exit(0); }
}
const response = await fetch(url, {signal: AbortSignal.timeout(1200000)});
if (!response.ok || !response.body) throw new Error(`Official download failed: ${response.status}`);
const expected = Number(response.headers.get('content-length'));
const hash = crypto.createHash('sha256');
let bytes = 0, last = Date.now();
await pipeline(Readable.fromWeb(response.body), new Transform({transform(chunk, encoding, done) {
  bytes += chunk.length; hash.update(chunk);
  if (Date.now() - last > 30000) { last = Date.now(); console.log(`${Math.round(bytes / 1048576)} MiB downloaded`); }
  done(null, chunk);
}}), fs.createWriteStream(`${file}.part`));
if (expected > 0 && bytes !== expected) throw new Error(`Incomplete download ${bytes}/${expected}`);
const metadata = {url, bytes, sha256: hash.digest('hex'), downloadedAt: new Date().toISOString()};
fs.renameSync(`${file}.part`, file);
fs.writeFileSync(record, JSON.stringify(metadata, null, 2));
console.log(JSON.stringify(metadata));
