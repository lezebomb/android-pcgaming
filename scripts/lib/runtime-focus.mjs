// Wait only on the known unrelated Settings ANR. Never stop it or alter its data.
export async function requireRuntimeFocus(call, packageName) {
  let state='';
  for (let attempt=0;attempt<3;attempt++) {
    state=call(['shell','dumpsys','window'],{encoding:'utf8'});
    const current=state.split(/\r?\n/).find(line=>line.includes('mCurrentFocus=')) ?? '';
    if (current.includes(packageName)) return current.trim();
    if (!current.includes('Application Not Responding: com.android.settings')) break;
    call(['shell','uiautomator','dump','/data/local/tmp/dimensity-focus.xml']);
    const xml=call(['exec-out','cat','/data/local/tmp/dimensity-focus.xml'],{encoding:'utf8'});
    if (!xml.includes('text="设置没有响应"')) break;
    const node=[...xml.matchAll(/<node\b[^>]*>/g)].map(x=>x[0]).find(x=>x.includes('text="等待"'));
    const bounds=node?.match(/bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"/);
    if (!bounds) break;
    call(['shell','input','tap',String((+bounds[1]+ +bounds[3])/2),String((+bounds[2]+ +bounds[4])/2)]);
    await new Promise(resolve=>setTimeout(resolve,75));
  }
  throw new Error('Runtime lacks Android input focus: '+state.split(/\r?\n/).find(line=>line.includes('mCurrentFocus=')));
}
