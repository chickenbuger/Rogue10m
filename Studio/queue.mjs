import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readFile, realpath } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import { loadTasks, withFileLock, atomicJson, revision } from './lib.mjs';

export const ACTIVE = ['reviewing','running','verifying'];
const TERMINAL = ['completed','blocked','failed','stopped'];
const fileFor = taskFile => path.join(path.dirname(taskFile),'execution.json');
export const fingerprint = task => revision(JSON.stringify({title:task.title,notes:task.notes,category:task.category}));
const defaultState = () => ({version:1,paused:false,automation:null,lastCheckedAt:null,records:{}});
export async function loadExecution(taskFile) {
  try {const state=JSON.parse(await readFile(fileFor(taskFile),'utf8'));if(state.version!==1 || typeof state.paused!=='boolean' || !state.records || Array.isArray(state.records))throw new Error('invalid');return state;}
  catch(error) {if(error.code==='ENOENT')return defaultState();throw new Error('실행 상태 파일을 읽을 수 없습니다. 원본을 보존했습니다.');}
}
export function effectiveStatus(task, record) {
  if(record?.state==='completed')return '완료';
  if(record?.state==='verifying')return '검증 대기';
  if(record && ['reviewing','running'].includes(record.state))return '진행 중';
  return task.status;
}
export function eligible(tasks, state, now = Date.now()) {
  const rank={'높음':0,'보통':1,'낮음':2};
  return tasks.filter(t=>t.autoRun!==false && t.status==='예정'
    && (!state.records[t.id] || state.records[t.id].state==='queued')
    && (t.dependsOn||[]).every(id=>{const dep=tasks.find(x=>x.id===id);return dep && effectiveStatus(dep,state.records[id])==='완료';}))
    .sort((a,b)=>rank[a.priority]-rank[b.priority] || (a.date||'9999').localeCompare(b.date||'9999') || a.id.localeCompare(b.id));
}
export async function queueView(taskFile, {privateView=false} = {}) {
  const [store,state]=await Promise.all([loadTasks(taskFile),loadExecution(taskFile)]);
  const active=Object.values(state.records).find(r=>ACTIVE.includes(r.state));
  const records=JSON.parse(JSON.stringify(state.records));
  if(!privateView) for(const r of Object.values(records)) {delete r.token;delete r.input;}
  return {...state,records,activeId:active?.taskId||null,stale:!!active && Date.now()-Date.parse(active.updatedAt)>30*60*1000,
    ready:eligible(store.tasks,state).map(t=>t.id),tasks:store.tasks,taskRevision:store.revision};
}
function log(record,message,now) {record.events.push({at:now,state:record.state,message});record.events=record.events.slice(-200);record.updatedAt=now;}
export async function queueCommand(taskFile, action, input={}, {worker=false,root:projectRoot=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..')}={}) {
  return withFileLock(taskFile,async()=>{
    const state=await loadExecution(taskFile);const store=await loadTasks(taskFile);const now=new Date().toISOString();
    const task=store.tasks.find(t=>t.id===input.taskId);
    let record=state.records[input.taskId];let result={};
    if(['poll','claim','progress','finish','connect','agent','engine','plan'].includes(action)&&!worker)throw Object.assign(new Error('작업자 전용 명령입니다.'),{status:403});
    if(action==='poll') {state.lastCheckedAt=now;result={paused:state.paused,active:Object.values(state.records).find(r=>ACTIVE.includes(r.state))||null,eligible:eligible(store.tasks,state),automation:state.automation};}
    else if(action==='engine') {
      const engine=input.engine;
      if(!engine || !['ready','busy','blocked','offline'].includes(engine.status))throw new Error('엔진 상태를 확인하세요.');
      state.engine={mode:'agents',status:engine.status,message:String(engine.message||'').slice(0,2000),updatedAt:now};state.automation=null;
    }
    else if(action==='connect') {if(typeof input.automationId!=='string'||input.automationId.length>120)throw new Error('자동화 식별자를 확인하세요.');state.automation={id:input.automationId,intervalMinutes:5,connectedAt:now};}
    else if(action==='pause') {if(typeof input.paused!=='boolean')throw new Error('일시정지 값을 확인하세요.');state.paused=input.paused;if(input.paused)for(const r of Object.values(state.records))if(ACTIVE.includes(r.state)){r.stopRequested=true;log(r,'전체 일시정지 요청 · 안전한 중단 지점에서 종료',now);}}
    else if(action==='claim') {
      if(state.paused)throw new Error('자동 실행이 일시정지되어 있습니다.');
      if(Object.values(state.records).some(r=>ACTIVE.includes(r.state)))throw Object.assign(new Error('이미 실행 중인 작업이 있습니다. 먼저 기존 실행을 확인하세요.'),{status:409});
      if(!task || !eligible(store.tasks,state).some(t=>t.id===task.id))throw new Error('아직 실행할 수 없는 작업입니다.');
      if(input.fingerprint && input.fingerprint!==fingerprint(task))throw new Error('검토 후 작업 내용이 변경되었습니다.');
      record={taskId:task.id,runId:randomUUID(),token:randomUUID(),attempt:(record?.attempt||0)+1,state:'reviewing',input:task,fingerprint:fingerprint(task),startedAt:now,updatedAt:now,stopRequested:false,events:record?.events||[],agents:[],summary:'',validation:'',artifacts:[]};
      log(record,'요구사항과 프로젝트 상태 검토 시작',now);state.records[task.id]=record;result=record;
    } else if(action==='stop') {
      if(!record || !ACTIVE.includes(record.state))throw new Error('현재 실행 중인 작업이 아닙니다.');record.stopRequested=true;log(record,'사용자가 중단 요청 · 강제 종료나 변경 취소는 하지 않음',now);
    } else if(action==='retry') {
      if(!task)throw new Error('작업을 찾을 수 없습니다.');
      if(record && ACTIVE.includes(record.state))throw new Error('실행 중에는 다시 요청할 수 없습니다.');
      if(task.status!=='예정')throw new Error('일정 상태를 예정으로 바꾼 뒤 다시 검토하세요.');
      if(record?.state==='completed')throw new Error('완료한 작업은 복제하거나 새 작업으로 등록하세요.');
      if(record){record.state='queued';record.stopRequested=false;record.summary='';log(record,'내용 보완 후 다시 검토 요청',now);}
    } else if(['progress','finish','agent','plan'].includes(action)) {
      if(!record || !ACTIVE.includes(record.state) || input.token!==record.token || (input.runId!==undefined && input.runId!==record.runId))throw Object.assign(new Error('실행 소유권이 일치하지 않습니다.'),{status:409});
      if(!task || fingerprint(task)!==record.fingerprint)throw new Error('실행 도중 작업 요구사항이 변경되었습니다.');
      if(action==='agent') {
        const a=input.agent;
        if(!a || !/^[a-zA-Z0-9_-]{1,80}$/.test(a.id) || !['planner','implementer','builder','reviewer','writer'].includes(a.role) || !['queued','running','completed','failed','blocked','stopped'].includes(a.state))throw new Error('에이전트 기록을 확인하세요.');
        record.agents||=[];const previous=record.agents.find(x=>x.id===a.id);
        if(!previous && record.agents.length>=30)throw new Error('에이전트 기록 한도를 초과했습니다.');
        const item={id:a.id,role:a.role,state:a.state,title:String(a.title||a.role).slice(0,160),summary:String(a.summary||'').slice(0,2000)};
        for(const key of ['startedAt','finishedAt','sessionId'])if(a[key])item[key]=String(a[key]).slice(0,120);
        if(previous)Object.assign(previous,item);else record.agents.push(item);
        record.updatedAt=now;await atomicJson(fileFor(taskFile),state);return {state:record.state};
      }
      if(action==='plan') {
        if(!input.plan || JSON.stringify(input.plan).length>20000)throw new Error('작업 배분 계획을 확인하세요.');
        record.plan=input.plan;record.updatedAt=now;await atomicJson(fileFor(taskFile),state);return {state:record.state};
      }
      if(typeof input.message!=='string'||!input.message.trim()||input.message.length>10000)throw new Error('진행 내용을 입력하세요.');
      if(action==='progress') {
        if(!ACTIVE.includes(input.state))throw new Error('잘못된 실행 단계입니다.');
        if(ACTIVE.indexOf(input.state)<ACTIVE.indexOf(record.state))throw new Error('이전 실행 단계로 되돌릴 수 없습니다.');
        if(record.stopRequested||state.paused)throw new Error('중단 요청됨: finish stopped로 보존한 변경과 남은 일을 기록하세요.');
        record.state=input.state;
      } else {
        if(!TERMINAL.includes(input.state))throw new Error('잘못된 종료 상태입니다.');
        if(input.state==='completed' && (record.stopRequested||state.paused))throw new Error('중단 요청된 작업은 완료 처리할 수 없습니다.');
        if(input.state==='completed' && (record.state!=='verifying'||typeof input.validation!=='string'||!input.validation.trim()||!Array.isArray(input.artifacts)||!input.artifacts.length))throw new Error('검증 단계, 검증 결과, 산출물이 있어야 완료할 수 있습니다.');
        if(input.artifacts!==undefined && (!Array.isArray(input.artifacts)||input.artifacts.length>100||input.artifacts.some(p=>typeof p!=='string'||p.length>500||p.includes('..')||path.isAbsolute(p))))throw new Error('프로젝트 기준 산출물 경로를 입력하세요.');
        if(input.state==='completed') {
          const root=await realpath(projectRoot);
          for(const artifact of input.artifacts) {const resolved=await realpath(path.join(root,artifact));const relative=path.relative(root,resolved);if(relative.startsWith('..')||path.isAbsolute(relative))throw new Error('프로젝트 밖의 산출물입니다.');}
        }
        record.state=input.state;record.summary=input.message;record.validation=String(input.validation||'').slice(0,10000);record.artifacts=input.artifacts||[];record.finishedAt=now;
      }
      log(record,input.message,now);result={state:record.state,stopRequested:record.stopRequested};
    } else throw new Error('지원하지 않는 대기열 명령입니다.');
    await atomicJson(fileFor(taskFile),state);return result;
  });
}

if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const taskFile=path.join(path.dirname(fileURLToPath(import.meta.url)),'data/tasks.json');
  try {
    const action=process.argv[2]||'view';
    let raw=''; if(process.argv[3]==='-')for await(const chunk of process.stdin)raw+=chunk;
    const input=process.argv[3]?JSON.parse(process.argv[3]==='-'?raw:await readFile(process.argv[3],'utf8')):{};
    const result=action==='view'?await queueView(taskFile,{privateView:true}):await queueCommand(taskFile,action,input,{worker:true});
    console.log(JSON.stringify(result,null,2));
  } catch(error) {console.error(error.message);process.exitCode=1;}
}
