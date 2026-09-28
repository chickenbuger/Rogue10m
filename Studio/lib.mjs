import { readFile, readdir, realpath, mkdir, rename, writeFile, open, unlink } from 'node:fs/promises';
import path from 'node:path';
import { createHash, randomUUID } from 'node:crypto';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const exec = promisify(execFile);
export const revision = text => createHash('sha256').update(text).digest('hex');
const clean = text => text.replace(/[`*]/g, '').trim();
export function parseSprints(text) {
  const headings = [...text.matchAll(/^#{2,3}\s+(Sprint#(\d+)-(\d+))\s*(.*)$/gm)];
  const grouped = new Map();
  for (const h of headings) {
    const rest = text.slice(h.index + h[0].length);
    const end = rest.search(/^#{1,3}\s/m);
    const body = rest.slice(0, end < 0 ? undefined : end).trim();
    const date = h[4].match(/20\d{2}-\d{2}-\d{2}/)?.[0] || body.match(/20\d{2}-\d{2}-\d{2}/)?.[0] || '';
    const title = clean(h[4]).replace(/^[-—–]\s*/, '').replace(/\s*\(20\d{2}.*\)$/, '').trim() || h[1];
    const old = grouped.get(h[1]);
    if (old) { old.text += '\n\n' + h[0] + '\n' + body; if (date > old.date) old.date = date; }
    else grouped.set(h[1], { id: h[1], sprint: Number(h[2]), work: Number(h[3]), title, date, text: body, source: 'Docs/SprintChangeLog.md' });
  }
  return [...grouped.values()].map(item => {
    const field = name => {
      const matches = [...item.text.matchAll(new RegExp('^- (?:' + name + ')\\s*[:：]\\s*(.*(?:\\n[ \\t]+.*)*)', 'gm'))];
      return matches.length ? clean(matches.at(-1)[1]) : '';
    };
    return { ...item, goal: field('목표|목표·변경'), changes: field('주요 변경|주요 기록'), validation: field('검증|검증·상태'), status: field('상태|검증·상태'), category: /수정|왜곡|안정화|오류|버그/.test(item.title) ? '수정' : /개선|보완|refinement|polish/.test(item.title) ? '개선' : /정리|Obsidian|하네스|스튜디오|분석|참고/.test(item.title) ? '도구·기록' : '기능' };
  }).sort((a,b) => b.sprint-a.sprint || b.work-a.work);
}

export async function readDocument(root, name) {
  if (!/^(Docs\/SprintChangeLog\.md|DevLog\/\d{8}\.txt|Feature\/(architect|doc)\/[^/\\]+\.md)$/.test(name)) throw Object.assign(new Error('허용되지 않은 문서입니다.'), { status: 403 });
  const resolvedRoot = await realpath(root);
  const target = await realpath(path.join(root, name));
  const relative = path.relative(resolvedRoot, target);
  if (relative.startsWith('..') || path.isAbsolute(relative)) throw Object.assign(new Error('프로젝트 밖의 문서입니다.'), { status: 403 });
  return (await readFile(target, 'utf8')).replace(/^\uFEFF/, '');
}

async function documents(root, directory, extension) {
  const names = (await readdir(path.join(root, directory))).filter(n => n.endsWith(extension) && /^\d{4}/.test(n)).sort().reverse();
  return Promise.all(names.map(async name => {
    const source = directory + '/' + name;
    const text = await readDocument(root, source);
    const date = extension === '.txt' ? name.slice(0,8).replace(/(\d{4})(\d{2})(\d{2})/, '$1-$2-$3') : name.slice(0,10);
    return { source, date, title: text.match(/^#\s+(.+)$/m)?.[1] || date + ' 개발 일지', text };
  }));
}

export async function snapshot(root) {
  const [changelog, journals, plans, results, branchResult] = await Promise.all([
    readDocument(root, 'Docs/SprintChangeLog.md'), documents(root, 'DevLog', '.txt'),
    documents(root, 'Feature/architect', '.md'), documents(root, 'Feature/doc', '.md'),
    exec('git', ['branch', '--show-current'], { cwd: root, timeout: 5000, windowsHide: true }).catch(() => ({ stdout: '' }))
  ]);
  const entries = parseSprints(changelog);
  const resultNames = new Set(results.map(d => path.basename(d.source)));
  return { project: 'Rogue10m', engine: 'UE 5.8', branch: branchResult.stdout.trim() || '확인 불가', updatedAt: new Date().toISOString(), entries, journals, plans, results,
    unmatchedPlans: plans.filter(d => !resultNames.has(path.basename(d.source))).map(d => ({ ...d, note: '같은 이름의 결과 문서 없음 · 진행 상태 확인 필요' })) };
}

export function validateTasks(tasks) {
  if (!Array.isArray(tasks) || tasks.length > 1000) throw new Error('작업은 최대 1,000개까지 저장할 수 있습니다.');
  const ids = new Set();
  const checked = tasks.map(t => {
    if (!t || typeof t !== 'object' || typeof t.id !== 'string' || !/^[a-zA-Z0-9-]{1,80}$/.test(t.id) || ids.has(t.id)) throw new Error('작업 ID가 잘못되었습니다.');
    ids.add(t.id);
    for (const [key, max] of [['title',160], ['notes',5000], ['date',10]]) if (typeof t[key] !== 'string' || t[key].length > max) throw new Error('작업 입력 길이를 확인하세요.');
    if (!t.title.trim()) throw new Error('작업 제목을 입력하세요.');
    if (!['예정','진행 중','검증 대기','완료'].includes(t.status) || !['기능','개선','수정','검증','기록'].includes(t.category) || !['높음','보통','낮음'].includes(t.priority)) throw new Error('작업 분류가 잘못되었습니다.');
    if (t.date && (!/^\d{4}-\d{2}-\d{2}$/.test(t.date) || !Number.isFinite(Date.parse(t.date)) || new Date(t.date).toISOString().slice(0,10) !== t.date)) throw new Error('유효한 날짜를 입력하세요.');
    const extra = {};
    if (t.autoRun !== undefined) { if (typeof t.autoRun !== 'boolean') throw new Error('자동 검토 설정을 확인하세요.'); extra.autoRun = t.autoRun; }
    if (t.startAfter !== undefined) { if (typeof t.startAfter !== 'string' || (t.startAfter && (!/^\d{4}-\d{2}-\d{2}T/.test(t.startAfter) || !Number.isFinite(Date.parse(t.startAfter))))) throw new Error('시작 가능 시각을 확인하세요.'); extra.startAfter = t.startAfter ? new Date(t.startAfter).toISOString() : ''; }
    if (t.dependsOn !== undefined) { if (!Array.isArray(t.dependsOn) || t.dependsOn.length > 100 || t.dependsOn.some(id=>typeof id !== 'string' || id === t.id)) throw new Error('선행 작업을 확인하세요.'); extra.dependsOn = [...new Set(t.dependsOn)]; }
    return { id:t.id, title:t.title.trim(), notes:t.notes, date:t.date, status:t.status, category:t.category, priority:t.priority, ...extra };
  });
  const map = new Map(checked.map(t=>[t.id,t]));
  const visited = new Set();
  const visit = (id, stack = new Set()) => { if(stack.has(id)) throw new Error('선행 작업이 서로를 기다리는 순환 관계입니다.'); if(visited.has(id))return; const next = new Set(stack).add(id); for(const dep of map.get(id).dependsOn || []) { if(!map.has(dep)) throw new Error('존재하지 않는 선행 작업입니다.'); visit(dep,next); } visited.add(id); };
  checked.forEach(t=>visit(t.id));
  return checked;
}

export async function loadTasks(file) {
  let raw;
  try { raw = await readFile(file, 'utf8'); } catch (error) { if (error.code === 'ENOENT') raw = '[]'; else throw error; }
  try { return { tasks: validateTasks(JSON.parse(raw)), revision: revision(raw) }; }
  catch { throw Object.assign(new Error('일정 파일을 읽을 수 없습니다. 원본은 보존했습니다. Studio/data/tasks.json을 확인하세요.'), { status: 500 }); }
}

// One serialized write queue prevents two tabs from overwriting the same revision.
let queue = Promise.resolve();
export function saveTasks(file, tasks, expectedRevision) {
  const action = queue.then(() => withFileLock(file, async () => {
    const checked = validateTasks(tasks);
    const previous = await loadTasks(file);
    if (previous.revision !== expectedRevision) throw Object.assign(new Error('다른 창에서 일정이 변경되었습니다. 새로고침 후 다시 수정하세요.'), { status: 409 });
    let execution;
    try { execution = JSON.parse(await readFile(path.join(path.dirname(file),'execution.json'),'utf8')); } catch(error) { if(error.code !== 'ENOENT') throw new Error('실행 상태 파일을 확인하세요. 일정은 보존했습니다.'); }
    for(const old of previous.tasks) {
      const record=execution?.records?.[old.id];
      const replacement=checked.find(t=>t.id===old.id);
      if(record && ['reviewing','running','verifying'].includes(record.state) && JSON.stringify(old)!==JSON.stringify(replacement)) throw Object.assign(new Error('실행 중인 작업은 변경하거나 삭제할 수 없습니다. 먼저 중단을 요청하세요.'),{status:409});
    }
    await mkdir(path.dirname(file), { recursive: true });
    const raw = JSON.stringify(checked, null, 2) + '\n';
    const temp = file + '.' + randomUUID() + '.tmp';
    await writeFile(temp, raw, { flag: 'wx' });
    await rename(temp, file);
    return { tasks: checked, revision: revision(raw) };
  }));
  queue = action.catch(() => {});
  return action;
}

export async function withFileLock(taskFile, action) {
  await mkdir(path.dirname(taskFile), {recursive:true});
  const lock = path.join(path.dirname(taskFile),'studio.lock');
  let handle;
  for(let attempt=0;attempt<60;attempt++) {
    try { handle=await open(lock,'wx'); await handle.writeFile(String(process.pid)); break; }
    catch(error) {
      if(error.code!=='EEXIST') throw error;
      // Never steal a lock on a timer or race another process to remove it.
      await new Promise(resolve=>setTimeout(resolve,50));
    }
  }
  if(!handle) throw Object.assign(new Error('다른 저장 작업이 진행 중입니다. 잠시 후 다시 시도하세요.'),{status:409});
  try {return await action();} finally {await handle.close();await unlink(lock);}
}

export async function atomicJson(file, value) {
  await mkdir(path.dirname(file),{recursive:true});
  const temp=file+'.'+randomUUID()+'.tmp';
  await writeFile(temp,JSON.stringify(value,null,2)+'\n',{flag:'wx'});
  await rename(temp,file);
}
