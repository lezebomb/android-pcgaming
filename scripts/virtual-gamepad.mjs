// Temporary Android UHID gamepad. EOF/disconnect removes the device.
// Protocol: Android Open Source Project, frameworks/base/cmds/hid.
import { spawn, execFileSync } from 'node:child_process';
import { createInterface } from 'node:readline';
import path from 'node:path';

const adb = process.env.ADB || (process.env.ANDROID_HOME
  ? path.join(process.env.ANDROID_HOME, 'platform-tools', 'adb.exe') : 'adb');
const serial = process.argv[2] || execFileSync(adb, ['get-serialno'], { encoding: 'utf8' }).trim();
if (!serial || serial === 'unknown') throw new Error('Connect one authorized device or pass its serial.');
const child = spawn(adb, ['-s', serial, 'shell', 'hid', '-'], { stdio: ['pipe', 'pipe', 'pipe'] });
child.stdout.pipe(process.stdout);
child.stderr.pipe(process.stderr);
child.on('error', error => { console.error(error.message); process.exitCode = 1; });
child.on('close', code => { console.log(`HID connection closed (${code}).`); process.exit(code || 0); });
const send = object => child.stdin.write(JSON.stringify({ id: 1, ...object }) + '\n');
// 16 buttons, a null-capable hat, four centered stick axes, two triggers.
const descriptor = [
  0x05,1,0x09,5,0xa1,1,0x85,1,
  0x05,9,0x19,1,0x29,16,0x15,0,0x25,1,0x75,1,0x95,16,0x81,2,
  0x05,1,0x09,0x39,0x15,0,0x25,7,0x35,0,0x46,0x3b,1,0x65,0x14,0x75,4,0x95,1,0x81,0x42,
  0x65,0,0x75,4,0x95,1,0x81,3,
  0x09,0x30,0x09,0x31,0x09,0x32,0x09,0x35,0x15,0,0x26,255,0,0x35,0,0x46,255,0,0x75,8,0x95,4,0x81,2,
  0x05,2,0x09,0xc5,0x09,0xc4,0x15,0,0x26,255,0,0x75,8,0x95,2,0x81,2,0xc0
];
send({ command: 'register', name: 'Dimensity Virtual Gamepad', vid: 0x1209, pid: 0xd601, bus: 'usb', descriptor });
let buttons = 0, hat = 8, axes = [128,128,128,128,0,0];
const report = () => send({ command: 'report', report: [1, buttons & 255, buttons >>> 8, hat, ...axes] });
const neutral = () => { buttons = 0; hat = 8; axes = [128,128,128,128,0,0]; report(); };
const buttonBits = { a:0,b:1,x:3,y:4,lb:6,rb:7,back:10,start:11,ls:13,rs:14 };
const axisIndexes = { lx:0,ly:1,rx:2,ry:3,lt:4,rt:5 };
let queuedUntil = 0, closing = false, disconnecting = false;
const delay = duration => {
  send({ command: 'delay', duration });
  queuedUntil = Math.max(performance.now(), queuedUntil) + duration;
};
const timer = setTimeout(() => { neutral(); console.log('Ready: down/up or tap button [30..1000 ms]; axis lx|ly|rx|ry -1..1; axis lt|rt 0..1; hat 0..8; wait 1..5000 ms; neutral; quit'); }, 1500);
const input = createInterface({ input: process.stdin });
input.on('line', line => {
  if (closing) return;
  const [command, name, value] = line.trim().toLowerCase().split(/\s+/);
  if (command === 'quit') { input.close(); return; }
  // Explicit diagnostic unplug while held. Normal quit/EOF still releases first.
  if (command === 'disconnect') { disconnecting = true; input.close(); return; }
  if (command === 'tap' && Object.hasOwn(buttonBits, name)) {
    const duration = value === undefined ? 120 : Number(value);
    if (!Number.isFinite(duration) || duration < 30 || duration > 1000) { console.log('Tap duration must be 30..1000 ms.'); return; }
    const mask = 1 << buttonBits[name]; buttons |= mask; report();
    delay(Math.round(duration)); buttons &= ~mask; report(); return;
  }
  if (command === 'wait' && Number.isInteger(Number(name)) && Number(name) >= 1 && Number(name) <= 5000) {
    delay(Number(name)); return;
  }
  if (command === 'sweep') {
    // Queue a short deterministic sweep inside hid; keep stdin open until it finishes.
    const pause = () => delay(180);
    for (const bit of Object.values(buttonBits)) {
      buttons = 1 << bit; report(); pause(); neutral(); pause();
    }
    for (const direction of [0,2,4,6]) { hat = direction; report(); pause(); neutral(); pause(); }
    for (let i = 0; i < axes.length; i++) {
      for (const extreme of (i < 4 ? [0,255] : [255])) {
        axes[i] = extreme; report(); pause(); neutral(); pause();
      }
    }
    console.log('Queued 9-second button, D-pad, stick and trigger sweep.');
    return;
  }
  if (command === 'neutral') { neutral(); return; }
  if ((command === 'down' || command === 'up') && Object.hasOwn(buttonBits, name)) {
    const mask = 1 << buttonBits[name]; buttons = command === 'down' ? buttons | mask : buttons & ~mask;
  } else if (command === 'axis' && Object.hasOwn(axisIndexes, name) && Number.isFinite(Number(value))) {
    const index = axisIndexes[name], n = Math.max(index < 4 ? -1 : 0, Math.min(1, Number(value)));
    axes[index] = Math.round(index < 4 ? (n + 1) * 127.5 : n * 255);
  } else if (command === 'hat' && Number.isInteger(Number(name)) && Number(name) >= 0 && Number(name) <= 8) {
    hat = Number(name);
  } else { console.log('Unknown input command.'); return; }
  report();
});
input.on('close', () => {
  closing = true; clearTimeout(timer);
  if (!child.stdin.writableEnded) {
    if (!disconnecting) neutral();
    // Let the last scheduled release arrive before EOF unregisters the UHID device.
    setTimeout(() => { if (!child.stdin.writableEnded) child.stdin.end(); }, Math.max(0, queuedUntil - performance.now()) + 50);
  }
});
process.on('SIGINT', () => { closing = true; neutral(); child.stdin.end(); input.close(); });
