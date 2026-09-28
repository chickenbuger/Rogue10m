import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { parseSprints, validateTasks, loadTasks, saveTasks, readDocument } from '../lib.mjs';
import { createStudioServer } from '../server.mjs';

const task = { id:'test-1', title:'애니메이션 검증', notes:'<script>alert(1)</script>', date:'2026-10-02', status:'예정', category:'검증', priority:'높음' };
test('Sprint 보완 기록을 병합하고 최종 상태 및 다중 행 검증을 보존한다', () => {
  const result = parseSprints('## Sprint#4-42 - 안정화 (2026-09-28)\n- 상태: 개발 중\n- 검증:\n  - 빌드 성공\n  - 게임 확인\n### Sprint#4-42 보완 (2026-09-29)\n- 상태: 지정범위 QA완료. 네트워크 제외.\n## Sprint#4-43 - 그림자\n- 목표: 머리 복원');
  assert.equal(result.length,2);assert.equal(result[0].id,'Sprint#4-43');assert.equal(result[1].date,'2026-09-29');
  assert.equal(result[1].category,'수정');assert.match(result[1].status,/네트워크 제외/);assert.match(result[1].validation,/게임 확인/);
});
test('날짜 미정 허용, 잘못된 날짜와 중복 ID 거절',()=>{
  assert.equal(validateTasks([{...task,date:''}])[0].date,'');
  assert.throws(()=>validateTasks([{...task,date:'2026-02-30'}]));assert.throws(()=>validateTasks([task,task]));assert.throws(()=>validateTasks([{...task,status:'대충 완료'}]));
});
test('파일 재조회, 동시 저장 충돌 및 손상 파일 보존',async t=>{
  const root=await mkdtemp(path.join(os.tmpdir(),'rogue-studio-'));t.after(()=>rm(root,{recursive:true,force:true}));const file=path.join(root,'tasks.json');
  const initial=await loadTasks(file);await saveTasks(file,[task],initial.revision);assert.deepEqual((await loadTasks(file)).tasks,[task]);
  const stored=await loadTasks(file);const results=await Promise.allSettled([saveTasks(file,[],stored.revision),saveTasks(file,[{...task,title:'수정'}],stored.revision)]);
  assert.equal(results.filter(x=>x.status==='fulfilled').length,1);assert.equal(results.find(x=>x.status==='rejected').reason.status,409);
  await writeFile(file,'broken');await assert.rejects(saveTasks(file,[],stored.revision));assert.equal(await readFile(file,'utf8'),'broken');
});
test('로컬 HTTP 조회·저장·재시작, 경로 접근과 외부 쓰기 차단',async t=>{
  const root=await mkdtemp(path.join(os.tmpdir(),'rogue-studio-http-'));t.after(()=>rm(root,{recursive:true,force:true}));
  for(const d of ['Docs','DevLog','Feature/architect','Feature/doc'])await mkdir(path.join(root,d),{recursive:true});
  await writeFile(path.join(root,'Docs/SprintChangeLog.md'),'## Sprint#4-42 - 안정화 (2026-09-28)\n- 상태: 검증 완료');
  await writeFile(path.join(root,'DevLog/20260928.txt'),'## 개발 기록\n테스트 내용');
  await writeFile(path.join(root,'Feature/architect/2026-09-28_shadow.md'),'# 그림자 설계');
  const taskFile=path.join(root,'data/tasks.json');const server=createStudioServer({root,taskFile});await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));t.after(()=>new Promise(resolve=>server.close(resolve)));
  const base=`http://127.0.0.1:${server.address().port}`;
  const snapshot=await (await fetch(base+'/api/snapshot')).json();assert.equal(snapshot.entries.length,1);assert.equal(snapshot.unmatchedPlans.length,1);assert.equal(snapshot.journals[0].date,'2026-09-28');
  assert.equal((await fetch(base+'/')).status,200);
  assert.equal((await fetch(base+'/api/document?path=../AGENTS.md')).status,403);
  assert.equal((await fetch(base+'/api/document?path=Docs/SprintChangeLog.md')).status,200);
  await assert.rejects(readDocument(root,'Feature/doc/../../AGENTS.md'));
  const stored=await (await fetch(base+'/api/tasks')).json();const opts={method:'PUT',headers:{'Content-Type':'application/json','X-Studio-Request':'1'},body:JSON.stringify({tasks:[task],revision:stored.revision})};
  assert.equal((await fetch(base+'/api/tasks',{...opts,headers:{...opts.headers,Origin:'https://example.com'}})).status,403);
  assert.equal((await fetch(base+'/api/tasks',{...opts,headers:{'Content-Type':'application/json'}})).status,415);
  assert.equal((await fetch(base+'/api/tasks',opts)).status,200);assert.equal((await fetch(base+'/api/tasks',opts)).status,409);
  assert.equal((await fetch(base+'/api/tasks',{...opts,body:'null'})).status,400);
  const restarted=createStudioServer({root,taskFile});await new Promise(resolve=>restarted.listen(0,'127.0.0.1',resolve));t.after(()=>new Promise(resolve=>restarted.close(resolve)));
  const persisted=await (await fetch(`http://127.0.0.1:${restarted.address().port}/api/tasks`)).json();assert.deepEqual(persisted.tasks,[task]);
});
