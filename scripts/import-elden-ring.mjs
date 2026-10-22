// Development import of the user's owned Steam installation. Does not launch games,
// install Steam, copy credentials/saves, change source files, or disable protection.
import fs from 'node:fs/promises';
import { createReadStream } from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawn, execFileSync } from 'node:child_process';
import { pipeline } from 'node:stream/promises';
import { Transform } from 'node:stream';

const source = path.resolve(process.env.ELDEN_RING_SOURCE || 'D:/SteamLibrary/steamapps/common/ELDEN RING/Game');
const adb = process.env.ADB || 'D:/Collect/.tooling/android-sdk/platform-tools/adb.exe';
const serial = process.env.ANDROID_SERIAL || 'HA2C3HFV';
const packageName = 'org.dimensity.gaming.dev';
const destination = 'files/imagefs/home/xuser-1/.wine/drive_c/Steam/steamapps/common/ELDEN RING/Game';
const stateFile = path.resolve('artifacts/elden-ring/import-state.json');
const blockSize = 1024 * 1024;
const sampleSize = 64 * 1024;
const topFiles = /^(?:Data[0-3]\.b[dh]t|Data[0-3]\.bhd|amd_ags_x64\.dll|bink2w64\.dll|eldenring\.exe|eossdk-win64-shipping\.dll|oo2core_6_win64\.dll|regulation\.bin|start_protected_game\.exe|steam_api64\.dll)$/i;
const topDirectories = new Set(['EasyAntiCheat', 'movie', 'sd']);
const quote = value => "'" + String(value).replaceAll("'", "'\\''") + "'";
// adb shell joins arguments into shell text; exec-in/out already escape each argv.
const adbArgs = (command, args) => ['-s', serial, command, ...(command === 'shell' ? args.map(quote) : args)];
const remoteArgs = args => ['run-as', packageName, ...args];

function textCommand(args, allowFailure = false) {
  try {
    return execFileSync(adb, adbArgs('shell', remoteArgs(args)), {
      encoding: 'utf8', maxBuffer: 256 * 1024, windowsHide: true, timeout: 30000,
      stdio: ['ignore', 'pipe', 'pipe']
    }).trim();
  } catch (error) {
    if (allowFailure) return null;
    throw error;
  }
}
function remoteSize(file) {
  const value = textCommand(['stat', '-c', '%s', file], true);
  return value !== null && /^\d+$/.test(value) ? Number(value) : null;
}
function remoteSample(file, offset, count) {
  return execFileSync(adb, adbArgs('exec-out', remoteArgs([
    'dd', `if=${file}`, 'bs=1', `skip=${offset}`, `count=${count}`, 'status=none'
  ])), { maxBuffer: sampleSize * 2, windowsHide: true, timeout: 30000, stdio: ['ignore', 'pipe', 'pipe'] });
}
async function sourceSample(file, offset, count) {
  const handle = await fs.open(file, 'r');
  try {
    const buffer = Buffer.alloc(count);
    const { bytesRead } = await handle.read(buffer, 0, count, offset);
    return buffer.subarray(0, bytesRead);
  } finally { await handle.close(); }
}
async function samePartial(file, remote, length) {
  const count = Math.min(sampleSize, length);
  for (const offset of new Set([0, Math.max(0, length - count)])) {
    const local = await sourceSample(file, offset, count);
    if (!local.equals(remoteSample(remote, offset, count))) return false;
  }
  return true;
}
async function hashSource(file) {
  const hash = crypto.createHash('sha256');
  for await (const bytes of createReadStream(file, { highWaterMark: blockSize })) hash.update(bytes);
  return hash.digest('hex');
}
async function remoteHash(file) {
  const child = spawn(adb, adbArgs('shell', remoteArgs(['sha256sum', file])), { windowsHide: true });
  let output = '', errors = '';
  child.stdout.on('data', bytes => { output += bytes; });
  child.stderr.on('data', bytes => { errors += bytes; });
  await new Promise((resolve, reject) => {
    child.on('error', reject);
    child.on('close', code => code === 0 ? resolve() : reject(new Error(`Device checksum failed (${code}): ${errors}`)));
  });
  const match = output.match(/^([0-9a-f]{64})\s/i);
  if (!match) throw new Error('Invalid device checksum result');
  return match[1].toLowerCase();
}
async function runAdb(args, label, progress = null) {
  const child = activeChild = spawn(adb, ['-s', serial, ...args], { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
  let output = '';
  const collect = bytes => { output = (output + String(bytes)).slice(-4096); };
  child.stdout.on('data', collect); child.stderr.on('data', collect);
  const started = Date.now();
  const timer = progress ? setInterval(() => {
    const size = remoteSize(progress.file);
    if (size !== null) console.log(`${progress.name}: ${(size / 1024 ** 2).toFixed(0)} / ${(progress.total / 1024 ** 2).toFixed(0)} MiB staged, ${(size / 1024 ** 2 / ((Date.now() - started) / 1000)).toFixed(1)} MiB/s`);
  }, 30000) : null;
  try {
    await new Promise((resolve, reject) => {
      child.on('error', reject);
      child.on('close', code => code === 0 ? resolve() : reject(new Error(`${label} (${code}): ${output}`)));
    });
  } finally {
    if (timer) clearInterval(timer);
    activeChild = null;
  }
  return output.trim();
}
async function walk(directory, relative = '') {
  const result = [];
  for (const entry of await fs.readdir(directory, { withFileTypes: true })) {
    if (entry.isSymbolicLink()) throw new Error(`Symbolic link is not an import source: ${entry.name}`);
    const rel = relative ? `${relative}/${entry.name}` : entry.name;
    if (!relative && !(entry.isDirectory() ? topDirectories.has(entry.name) : topFiles.test(entry.name))) continue;
    if (entry.isDirectory()) result.push(...await walk(path.join(directory, entry.name), rel));
    else if (entry.isFile()) {
      const stats = await fs.stat(path.join(source, rel), { bigint: true });
      result.push({ name: rel, size: Number(stats.size), modifiedNs: String(stats.mtimeNs) });
    }
  }
  return result.sort((a, b) => a.size - b.size || a.name.localeCompare(b.name));
}

await fs.mkdir(path.dirname(stateFile), { recursive: true });
const files = await walk(source);
for (const required of ['eldenring.exe', 'steam_api64.dll', 'start_protected_game.exe', 'Data0.bdt', 'Data1.bdt', 'Data2.bdt', 'Data3.bdt']) {
  if (!files.some(file => file.name === required)) throw new Error(`Incomplete Steam source: ${required}`);
}
const old = await fs.readFile(stateFile, 'utf8').then(JSON.parse).catch(error => {
  if (error.code === 'ENOENT') return null;
  throw error;
});
if (old && (old.source !== source || old.destination !== destination || old.serial !== serial))
  throw new Error('Import checkpoint belongs to a different source/device');
if (old?.hostProcessId && old.hostProcessId !== process.pid) {
  try {
    process.kill(old.hostProcessId, 0);
    throw new Error(`Import process ${old.hostProcessId} is still alive; refusing a concurrent import`);
  } catch (error) {
    if (error.code !== 'ESRCH') throw error;
  }
}
const state = old || { source, destination, serial, startedAt: new Date().toISOString(), files: {} };
state.hostProcessId = process.pid;
async function save() {
  state.updatedAt = new Date().toISOString();
  await fs.writeFile(stateFile + '.part', JSON.stringify(state, null, 2) + '\n');
  await fs.rename(stateFile + '.part', stateFile);
}
const missingBytes = files.reduce((sum, file) => {
  const entry = state.files[file.name];
  if (entry && (entry.size !== file.size || entry.modifiedNs !== file.modifiedNs))
    throw new Error(`Source changed during import; refusing to overwrite: ${file.name}`);
  return sum + (entry?.status === 'verified' ? 0 : Math.max(0, file.size - (remoteSize(`${destination}/${file.name}.part`) || 0)));
}, 0);
const disk = textCommand(['df', '-k', 'files']).split(/\r?\n/).at(-1).trim().split(/\s+/);
if (Number(disk[3]) * 1024 < missingBytes + 2 * 1024 ** 3) throw new Error('Insufficient device space (2 GiB reserve)');
console.log(`Steam game import: ${files.length} files, ${(files.reduce((sum, file) => sum + file.size, 0) / 1024 ** 3).toFixed(2)} GiB; credentials/saves/unlisted extras excluded`);
await save();

let activeChild;
process.on('SIGINT', () => { activeChild?.kill(); console.error('Interrupted; partial files and checkpoint retained.'); process.exit(130); });
for (const file of files) {
  const local = path.join(source, file.name);
  const target = `${destination}/${file.name}`;
  const partial = target + '.part';
  const existing = state.files[file.name];
  if (existing?.status === 'verified' && remoteSize(target) === file.size) {
    console.log(`Already verified: ${file.name}`); continue;
  }
  if (remoteSize(target) !== null) {
    if (!existing || !existing.sha256 || await remoteHash(target) !== existing.sha256)
      throw new Error(`Destination already exists without matching import checkpoint: ${file.name}`);
    state.files[file.name] = { ...file, sha256: existing.sha256, status: 'verified' };
    await save();
    if (/^\/data\/local\/tmp\/dimensity-elden-[0-9a-f]{12}\.part$/.test(existing.stage || ''))
      await runAdb(['shell', 'rm', '-f', quote(existing.stage)], 'Staging cleanup failed');
    continue;
  }
  textCommand(['mkdir', '-p', path.posix.dirname(target)]);
  if (file.size >= 256 * 1024 ** 2) {
    // Native adb sync is substantially faster than Windows exec-in pipes.
    // Stage one file, then copy privately; verified finished files are retained.
    const stage = '/data/local/tmp/dimensity-elden-' + crypto.createHash('sha256').update(file.name).digest('hex').slice(0, 12) + '.part';
    state.files[file.name] = { ...file, status: 'staging', stage };
    await save();
    console.log(`Native push: ${file.name} (${file.size} bytes)`);
    const free = Number(textCommand(['df', '-k', 'files']).split(/\r?\n/).at(-1).trim().split(/\s+/)[3]) * 1024;
    const required = Math.max(0, file.size - (remoteSize(stage) || 0))
      + Math.max(0, file.size - (remoteSize(partial) || 0)) + 2 * 1024 ** 3;
    if (free < required) throw new Error(`Insufficient space for one staged file and private copy: ${file.name}`);
    if (remoteSize(stage) !== file.size) {
      const summary = await runAdb(['push', '-Z', local, stage], 'Native push failed', { file: stage, total: file.size, name: file.name });
      console.log(summary.split(/\r?\n/).at(-1));
    }
    if (remoteSize(stage) !== file.size) throw new Error(`Staged size mismatch: ${file.name}`);
    console.log(`Commit private copy: ${file.name}`);
    await runAdb(['shell', ...remoteArgs(['cp', stage, partial]).map(quote)], 'Private copy failed');
    const current = await fs.stat(local, { bigint: true });
    if (Number(current.size) !== file.size || String(current.mtimeNs) !== file.modifiedNs)
      throw new Error(`Source modified while transferring: ${file.name}`);
    const sha256 = await hashSource(local);
    state.files[file.name] = { ...state.files[file.name], sha256, status: 'verifying' };
    await save();
    console.log(`Verify device SHA256: ${file.name}`);
    if (remoteSize(partial) !== file.size || await remoteHash(partial) !== sha256)
      throw new Error(`Private copy verification mismatch: ${file.name}`);
    textCommand(['mv', partial, target]);
    await runAdb(['shell', 'rm', '-f', quote(stage)], 'Staging cleanup failed');
    state.files[file.name] = { ...file, sha256, status: 'verified' };
    await save();
    console.log(`Verified: ${file.name}`);
    continue;
  }
  let offset = remoteSize(partial) || 0;
  if (offset > file.size || (offset && !existing))
    throw new Error(`Partial file does not match checkpoint/source: ${file.name}`);
  offset = Math.floor(offset / blockSize) * blockSize;
  if (offset && !await samePartial(local, partial, offset))
    throw new Error(`Partial prefix does not match source: ${file.name}`);
  if (offset) textCommand(['truncate', '-s', String(offset), partial]);
  state.files[file.name] = { ...file, status: 'copying', resumedBytes: offset };
  await save();
  const hash = crypto.createHash('sha256');
  let copied = offset, lastReport = Date.now();
  const started = Date.now();
  console.log(`Copy: ${file.name} (${file.size} bytes, resume ${offset})`);
  const args = ['dd', `of=${partial}`, `bs=${blockSize}`, 'status=none'];
  if (offset) args.push(`seek=${offset / blockSize}`, 'conv=notrunc');
  // exec-in preserves binary stdin on Windows. It may finish before the remote
  // writer drains its final buffered bytes; wait for size below before hashing.
  const child = activeChild = spawn(adb, adbArgs('exec-in', remoteArgs(args)), { windowsHide: true });
  let stderr = '';
  child.stderr.on('data', bytes => { stderr += bytes; });
  child.stdout.resume();
  const completion = new Promise((resolve, reject) => {
    child.on('error', reject);
    child.on('close', code => code === 0 ? resolve() : reject(new Error(`Transfer failed (${code}): ${stderr}`)));
  });
  const counter = new Transform({ transform(bytes, encoding, next) {
    if (!offset) hash.update(bytes);
    copied += bytes.length;
    if (Date.now() - lastReport >= 15000) {
      console.log(`${file.name}: ${(copied / 1024 ** 2).toFixed(0)} / ${(file.size / 1024 ** 2).toFixed(0)} MiB, ${((copied - offset) / 1024 ** 2 / ((Date.now() - started) / 1000)).toFixed(1)} MiB/s`);
      lastReport = Date.now();
    }
    next(null, bytes);
  }});
  await Promise.all([completion, pipeline(createReadStream(local, { start: offset, highWaterMark: blockSize }), counter, child.stdin)]);
  activeChild = null;
  let transferredSize = remoteSize(partial);
  const deadline = Date.now() + 15000;
  while (transferredSize !== file.size && Date.now() < deadline) {
    await new Promise(resolve => setTimeout(resolve, 250));
    transferredSize = remoteSize(partial);
  }
  if (transferredSize !== file.size)
    throw new Error(`Transferred file size mismatch: ${file.name}, ${transferredSize} / ${file.size}`);
  const current = await fs.stat(local, { bigint: true });
  if (Number(current.size) !== file.size || String(current.mtimeNs) !== file.modifiedNs)
    throw new Error(`Source modified while transferring: ${file.name}`);
  const sha256 = offset ? await hashSource(local) : hash.digest('hex');
  state.files[file.name] = { ...state.files[file.name], sha256, status: 'verifying' };
  await save();
  console.log(`Verify device SHA256: ${file.name}`);
  if (await remoteHash(partial) !== sha256) throw new Error(`Transferred checksum mismatch: ${file.name}`);
  textCommand(['mv', partial, target]);
  state.files[file.name] = { ...file, sha256, status: 'verified' };
  await save();
  console.log(`Verified: ${file.name}`);
}
state.completedAt = new Date().toISOString();
await save();
console.log('Import complete, every file verified; game not launched.');
