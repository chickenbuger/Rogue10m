import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import { mkdtemp, mkdir, writeFile, readFile, rm } from 'node:fs/promises';
import { createDispatcher, planBatches, hasCommandEvidence } from '../dispatcher.mjs';
import { loadTasks, saveTasks } from '../lib.mjs';
import { queueCommand, loadExecution } from '../queue.mjs';

const task={id:'a',title:'문서 작성',notes:'관련 문서 두 개를 작성하고 검증',date:'',status:'예정',category:'기록',priority:'보통'};
const packet=(id,file,dependsOn=[])=>({id,title:id,instructions:'문서 작성 및 실제 내용 검증',files:[file],dependsOn});
const plan={outcome:'passed',summary:'독립 문서 작업 두 개',packets:[packet('one','Feature/doc/one.md'),packet('two','Feature/doc/two.md')]};
const passed={outcome:'passed',summary:'실행 완료',validation:'실제 내용 검증 완료',artifacts:[],checks:[]};
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
test('실제 CLI wrapper 안의 검증 명령을 대조하고 실패 명령은 제외한다',()=>{
  assert.equal(hasCommandEvidence('node check.mjs',[{command:'pwsh.exe -Command "node check.mjs"',exitCode:0}]),true);
  assert.equal(hasCommandEvidence('node check.mjs',[{command:'pwsh.exe -Command "node check.mjs"',exitCode:1}]),false);
  assert.equal(hasCommandEvidence('node unknown.mjs',[{command:'node check.mjs',exitCode:0}]),false);
  assert.equal(hasCommandEvidence('Scripts/BuildEditor.ps1',[{command:'Write-Output Scripts/BuildEditor.ps1',exitCode:0}]),false);
  assert.equal(hasCommandEvidence('node check.mjs',[{command:'pwsh.exe -Command "node check.mjs; Write-Output done"',exitCode:0}]),false);
  assert.equal(hasCommandEvidence('node check.mjs; Write-Output done',[{command:'node check.mjs; Write-Output done',exitCode:0}]),false);
});
async function fixture(t,tasks=[task]) {
  const root=await mkdtemp(path.join(os.tmpdir(),'studio-dispatcher-'));
  t.after(async()=>{assert.ok(root.startsWith(path.join(os.tmpdir(),'studio-dispatcher-')));await rm(root,{recursive:true,force:true});});
  await mkdir(path.join(root,'Studio'),{recursive:true});await writeFile(path.join(root,'Studio/EXECUTION.md'),'Local execution instructions');await writeFile(path.join(root,'AGENTS.md'),'Preserve unrelated changes.');
  const taskFile=path.join(root,'Studio/data/tasks.json');const store=await loadTasks(taskFile);await saveTasks(taskFile,tasks,store.revision);
  return {root,taskFile};
}
function fakeClient(root,{builderEvidence=true,reviewPass=true,onRun,selectedPlan=plan}={}) {
  const calls=[];let simultaneous=0,maxParallel=0;
  return {calls,get maxParallel(){return maxParallel;},async run(request){
    const {id,role,signal,onEvent}=request;calls.push(id);await onRun?.(request);
    if(signal.aborted)throw Object.assign(new Error('aborted'),{name:'AbortError'});
    await onEvent({type:'session',sessionId:'test-session-'+id});
    if(role==='planner')return {result:selectedPlan,commands:[]};
    if(role==='implementer'){
      simultaneous++;maxParallel=Math.max(maxParallel,simultaneous);await delay(200);simultaneous--;
      const p=selectedPlan.packets.find(p=>'implement-'+p.id===id),file=p.files[0];await mkdir(path.dirname(path.join(root,file)),{recursive:true});await writeFile(path.join(root,file),'Implementation');
      return {result:{...passed,artifacts:[file]},commands:[]};
    }
    if(role==='builder')return {result:{...passed,checks:['node check.mjs']},commands:builderEvidence?[{command:'node check.mjs',exitCode:0,output:'verified'}]:[]};
    if(role==='reviewer')return {result:{...passed,outcome:reviewPass?'passed':'blocked',summary:reviewPass?'검토 완료':'검토 실패'},commands:[]};
    const date=new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Seoul'}).format(new Date()).replaceAll('-','');
    const artifacts=['Feature/doc/result.md',`DevLog/${date}.txt`,'Docs/SprintChangeLog.md'];
    for(const file of artifacts){await mkdir(path.dirname(path.join(root,file)),{recursive:true});await writeFile(path.join(root,file),'검증 결과 기록');}
    return {result:{...passed,artifacts},commands:[]};
  }};
}
function dispatcher(f,client){return createDispatcher({...f,client,prepareBranch:async()=>{}});}
test('독립 packet 병렬 실행과 완료 전 실제 builder 근거·독립 리뷰·문서 순서',async t=>{
  const f=await fixture(t),client=fakeClient(f.root),d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();
  const state=await loadExecution(f.taskFile);assert.equal(state.records.a.state,'completed');assert.equal(client.maxParallel,2);
  assert.equal(client.calls[0],'architect');
  assert.deepEqual(client.calls.slice(1,3).sort(),['implement-one','implement-two']);
  assert.deepEqual(client.calls.slice(3),['builder','reviewer','writer','exit-reviewer']);
  assert.equal(state.records.a.agents.length,7);assert.equal(state.records.a.plan.packets.length,2);assert.ok(state.records.a.artifacts.includes('Docs/SprintChangeLog.md'));
});
test('겹치는 파일 및 선행 packet은 직렬 실행한다',async t=>{
  const overlap={...plan,packets:[packet('one','Feature/doc/shared.md'),packet('two','Feature/doc/shared.md')]};
  assert.equal(planBatches(overlap).length,2);assert.equal(planBatches({...plan,packets:[packet('one','a'),packet('two','b',['one'])]}).length,2);
  assert.throws(()=>planBatches({...plan,packets:[packet('one','../outside')]}));assert.throws(()=>planBatches({...plan,packets:[packet('one','DevLog/20260928.txt')]}));
  const f=await fixture(t),client=fakeClient(f.root,{selectedPlan:overlap}),d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();assert.equal(client.maxParallel,1);
});
test('builder 성공 주장에 실제 명령 증거가 없으면 완료하지 않는다',async t=>{
  const f=await fixture(t),client=fakeClient(f.root,{builderEvidence:false}),d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();
  const state=await loadExecution(f.taskFile);assert.equal(state.records.a.state,'blocked');assert.equal(state.paused,true);assert.ok(!client.calls.includes('writer'));
  assert.equal(state.engine.status,'blocked');assert.match(state.engine.message,/명령 근거/);
  d.kick();await d.waitForIdle();const afterKick=await loadExecution(f.taskFile);
  assert.equal(afterKick.engine.status,'blocked');assert.equal(afterKick.engine.message,state.engine.message);
});
test('독립 검토가 실패하면 writer와 completed로 넘어가지 않는다',async t=>{
  const f=await fixture(t),client=fakeClient(f.root,{reviewPass:false}),d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();assert.equal((await loadExecution(f.taskFile)).records.a.state,'blocked');assert.ok(!client.calls.includes('writer'));
});
test('실행 중 저장/kick된 작업을 같은 drain에서 이어 처리한다',async t=>{
  const f=await fixture(t);let d,added=false;
  const client=fakeClient(f.root,{onRun:async r=>{if(r.role==='implementer'&&!added){added=true;const s=await loadTasks(f.taskFile);await saveTasks(f.taskFile,[...s.tasks,{...task,id:'b'}],s.revision);d.kick();}}});
  d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();const state=await loadExecution(f.taskFile);assert.equal(state.records.a.state,'completed');assert.equal(state.records.b.state,'completed');
});
test('중단 요청은 child 종료를 기다리고 stopped로 보존한다',async t=>{
  const f=await fixture(t);let started,exited=false;const startedPromise=new Promise(resolve=>{started=resolve;});
  const client={async run({signal}){started();await new Promise(resolve=>signal.addEventListener('abort',resolve,{once:true}));await delay(25);exited=true;throw Object.assign(new Error('aborted'),{name:'AbortError'});}};
  const d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await startedPromise;await queueCommand(f.taskFile,'stop',{taskId:'a'});await d.waitForIdle();assert.equal(exited,true);assert.equal((await loadExecution(f.taskFile)).records.a.state,'stopped');
});
test('존재하는 dispatcher 잠금은 삭제하거나 반복 실행하지 않는다',async t=>{
  const f=await fixture(t);const lock=path.join(path.dirname(f.taskFile),'dispatcher.lock');await writeFile(lock,'existing-owner');const client=fakeClient(f.root),d=dispatcher(f,client);t.after(()=>d.stop());d.kick();await d.waitForIdle();assert.equal(client.calls.length,0);assert.equal(await readFile(lock,'utf8'),'existing-owner');assert.equal(d.status().status,'blocked');
});
