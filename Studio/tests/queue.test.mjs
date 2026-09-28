import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { pathToFileURL } from 'node:url';
import { loadTasks, saveTasks, validateTasks } from '../lib.mjs';
import { queueCommand, queueView, eligible, effectiveStatus } from '../queue.mjs';
import { createStudioServer } from '../server.mjs';
const worker={worker:true};
const base={id:'a',title:'문서 작성',notes:'완료 조건',date:'2026-09-28',status:'예정',category:'기록',priority:'보통'};
async function fixture(t,tasks=[base]) {const root=await mkdtemp(path.join(os.tmpdir(),'studio-queue-'));t.after(()=>rm(root,{recursive:true,force:true}));const file=path.join(root,'tasks.json');const s=await loadTasks(file);await saveTasks(file,tasks,s.revision);await queueCommand(file,'pause',{paused:false});return file;}
test('레거시 시작시각 무시, 선행 작업·수동 제외 및 우선순위',()=>{
  const tasks=[base,{...base,id:'b',priority:'높음'},{...base,id:'c',autoRun:false},{...base,id:'d',startAfter:'2099-01-01T00:00:00Z'},{...base,id:'e',dependsOn:['a']}];
  assert.deepEqual(eligible(tasks,{records:{}}).map(t=>t.id),['b','a','d']);
  assert.deepEqual(eligible(tasks,{records:{a:{state:'completed'}}}).map(t=>t.id),['b','d','e']);
  assert.throws(()=>validateTasks([{...base,dependsOn:['missing']}]));
  assert.throws(()=>validateTasks([{...base,dependsOn:['b']},{...base,id:'b',dependsOn:['a']}]));
});
test('claim 중복 방지, 실행 중 편집 차단, 소유권 및 완료 근거',async t=>{
  const file=await fixture(t);const run=await queueCommand(file,'claim',{taskId:'a'},worker);
  await assert.rejects(queueCommand(file,'claim',{taskId:'a'},worker),/이미 실행/);
  const stored=await loadTasks(file);await assert.rejects(saveTasks(file,[],stored.revision),/실행 중/);
  await assert.rejects(queueCommand(file,'progress',{taskId:'a',token:'wrong',state:'running',message:'x'},worker),/소유권/);
  const input={taskId:'a',token:run.token,message:'기획 문서와 검증 완료'};
  await assert.rejects(queueCommand(file,'finish',{...input,state:'completed'},worker),/검증 단계/);
  await queueCommand(file,'progress',{...input,state:'running'},worker);
  await queueCommand(file,'progress',{...input,state:'verifying'},worker);
  await assert.rejects(queueCommand(file,'progress',{...input,state:'running'},worker),/이전 실행 단계/);
  await queueCommand(file,'finish',{...input,state:'completed',validation:'원문 대조 완료',artifacts:['Studio/README.md']},worker);
  const view=await queueView(file);assert.equal(view.records.a.token,undefined);assert.equal(effectiveStatus(base,view.records.a),'완료');assert.equal(view.ready.length,0);assert.equal(view.records.a.events.length,4);
});
test('전체·개별 중단, 종료 후 명시적 재검토 및 서버 재시작 보존',async t=>{
  const file=await fixture(t);let run=await queueCommand(file,'claim',{taskId:'a'},worker);
  await queueCommand(file,'pause',{paused:true});
  await assert.rejects(queueCommand(file,'progress',{taskId:'a',token:run.token,state:'running',message:'x'},worker),/중단 요청/);
  await queueCommand(file,'finish',{taskId:'a',token:run.token,state:'stopped',message:'작업 보존 후 중단'},worker);
  await queueCommand(file,'pause',{paused:false});assert.equal((await queueView(file)).ready.length,0);
  await queueCommand(file,'retry',{taskId:'a'});run=await queueCommand(file,'claim',{taskId:'a'},worker);assert.equal(run.attempt,2);
  await queueCommand(file,'stop',{taskId:'a'});assert.equal((await queueView(file)).records.a.stopRequested,true);
  await queueCommand(file,'finish',{taskId:'a',token:run.token,state:'blocked',message:'추가 요구사항 필요'},worker);
  assert.equal((await queueView(file)).records.a.state,'blocked');assert.equal((await queueView(file)).ready.length,0);
});
test('별도 두 프로세스의 claim도 한 번만 성공',async t=>{
  const file=await fixture(t);const moduleUrl=pathToFileURL(path.resolve('Studio/queue.mjs')).href;
  const code=`import {queueCommand} from ${JSON.stringify(moduleUrl)};await queueCommand(${JSON.stringify(file)},'claim',{taskId:'a'},{worker:true});`;
  const exec=promisify(execFile);const results=await Promise.allSettled([exec(process.execPath,['--input-type=module','-e',code],{windowsHide:true}),exec(process.execPath,['--input-type=module','-e',code],{windowsHide:true})]);
  assert.equal(results.filter(r=>r.status==='fulfilled').length,1);assert.equal((await queueView(file)).records.a.attempt,1);
});
test('HTTP는 제어만 허용하고 worker token과 원본 입력을 숨긴다',async t=>{
  const file=await fixture(t);await queueCommand(file,'claim',{taskId:'a'},worker);
  const server=createStudioServer({taskFile:file});await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));t.after(()=>new Promise(resolve=>server.close(resolve)));
  const url=`http://127.0.0.1:${server.address().port}/api/execution`;
  const view=await(await fetch(url)).json();assert.equal(view.records.a.token,undefined);assert.equal(view.records.a.input,undefined);
  const options={method:'POST',headers:{'Content-Type':'application/json','X-Studio-Request':'1'}};
  assert.equal((await fetch(url,{...options,body:JSON.stringify({action:'claim',taskId:'a'})})).status,400);
  assert.equal((await fetch(url,{...options,headers:{...options.headers,Origin:'https://external.example'},body:JSON.stringify({action:'stop',taskId:'a'})})).status,403);
  assert.equal((await fetch(url,{...options,body:JSON.stringify({action:'stop',taskId:'a'})})).status,200);
});

test('검증 종료 순간 중단과 잘못된 runId는 완료할 수 없다',async t=>{
  const file=await fixture(t);const run=await queueCommand(file,'claim',{taskId:'a'},worker);
  const input={taskId:'a',token:run.token,runId:run.runId,message:'검증'};
  await assert.rejects(queueCommand(file,'progress',{...input,runId:'other',state:'running'},worker),/소유권/);
  await queueCommand(file,'progress',{...input,state:'verifying'},worker);
  await queueCommand(file,'stop',{taskId:'a'});
  await assert.rejects(queueCommand(file,'finish',{...input,state:'completed',validation:'실제 검증',artifacts:['Studio/README.md']},worker),/중단 요청/);
  assert.equal((await queueView(file)).records.a.state,'verifying');
});

test('HTTP 저장과 재개가 즉시 dispatcher를 깨우며 읽기는 실행하지 않는다',async t=>{
  const file=await fixture(t);let kicks=0;
  const server=createStudioServer({taskFile:file,dispatcher:{kick(){kicks++;}}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));t.after(()=>new Promise(resolve=>server.close(resolve)));
  const url=`http://127.0.0.1:${server.address().port}`;
  const options={headers:{'Content-Type':'application/json','X-Studio-Request':'1'}};
  const current=await(await fetch(`${url}/api/tasks`)).json();assert.equal(kicks,0);
  assert.equal((await fetch(`${url}/api/tasks`,{...options,method:'PUT',body:JSON.stringify(current)})).status,200);assert.equal(kicks,1);
  assert.equal((await fetch(`${url}/api/execution`,{...options,method:'POST',body:JSON.stringify({action:'pause',paused:false})})).status,200);assert.equal(kicks,2);
});
