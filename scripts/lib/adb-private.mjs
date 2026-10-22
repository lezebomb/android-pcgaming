import {execFileSync} from 'node:child_process';

// exec-in may return before the device finishes consuming stdin. Never expose a
// partial EXE/shortcut to a concurrent launch. All mutations stay in our app UID.
export async function stagePrivate(adb, packageName, bytes, destination) {
  if (!destination.startsWith('files/') || destination.split('/').includes('..')) throw new Error('Expected an app-private relative file.');
  const quote = value => "'" + value.replaceAll("'", "'\\''") + "'";
  const temporary = `${destination}.part-${process.pid}-${Date.now()}`;
  const call = (args, options={}) => execFileSync(adb, args, {timeout:20000,...options});
  try {
    call(['exec-in','run-as',packageName,'sh','-c',`cat > ${quote(temporary)}`], {input:bytes});
    const deadline=Date.now()+10000;
    for (;;) {
      let size=-1;
      try { size=Number(call(['exec-out','run-as',packageName,'stat','-c','%s',temporary], {encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim()); } catch {}
      if (size===bytes.length) break;
      if (Date.now()>=deadline) throw new Error(`Incomplete staging: ${destination}`);
      await new Promise(resolve=>setTimeout(resolve,100));
    }
    call(['exec-out','run-as',packageName,'mv','-f',temporary,destination]);
  } finally {
    call(['exec-out','run-as',packageName,'rm','-f',temporary]);
  }
}
