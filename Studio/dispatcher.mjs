import path from 'node:path';
import { mkdir, open, readFile, realpath, stat, unlink, writeFile } from 'node:fs/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { randomUUID } from 'node:crypto';
import { queueCommand, loadExecution } from './queue.mjs';
import { loadTasks } from './lib.mjs';
import { createCodexClient, validateSchema } from './agent-client.mjs';

const exec=promisify(execFile);
const string={type:'string',maxLength:16000};
const strings={type:'array',items:string,maxItems:100};
const object=properties=>({type:'object',properties,required:Object.keys(properties),additionalProperties:false});
export const PLAN_SCHEMA=object({outcome:{type:'string',enum:['passed','blocked','failed']},summary:string,packets:{type:'array',minItems:0,maxItems:2,items:object({id:string,title:string,instructions:string,files:strings,dependsOn:strings})}});
export const RESULT_SCHEMA=object({outcome:{type:'string',enum:['passed','blocked','failed']},summary:string,validation:string,artifacts:strings,checks:strings});
export function hasCommandEvidence(check,commands) {
  const unwrap=value=>{
    let text=String(value).trim();
    const wrapper=text.match(/^(?:"[^"]*(?:pwsh|powershell)\.exe"|(?:pwsh|powershell)(?:\.exe)?)\s+(?:-(?:NoProfile|NonInteractive)\s+)*-Command\s+([\s\S]+)$/i);
    if(wrapper){text=wrapper[1];if((text.startsWith('"')&&text.endsWith('"'))||(text.startsWith("'")&&text.endsWith("'")))text=text.slice(1,-1);}
    // Compound commands can hide a failed build behind a successful final command.
    if(/[;|\r\n`]/.test(text)||text.includes('&&')||text.includes('$('))return null;
    return text.trim();
  };
  const expected=unwrap(check);
  if(!expected)return false;
  if(expected.length<5)return false;
  return commands.some(c=>c.exitCode===0 && unwrap(c.command)===expected);
}
const fail=(message,state='blocked')=>Object.assign(new Error(message),{state});
const normalized=file=>file.replaceAll('\\','/').replace(/\/$/,'').toLowerCase();
function safeRelative(file) {
  if(typeof file!=='string'||!file||file.length>500||path.isAbsolute(file)||file.includes(':')||file.split(/[\\/]/).some(p=>p==='..'||p==='.')||/[?*\0]/.test(file))throw fail('프로젝트 안의 구체적인 파일 경로가 필요합니다.');
  return normalized(file);
}
export function planBatches(plan) {
  validateSchema(plan,PLAN_SCHEMA);
  if(plan.outcome!=='passed'||!plan.packets.length)throw fail(plan.summary||'요구사항 추가 확인이 필요합니다.',plan.outcome==='failed'?'failed':'blocked');
  const ids=new Set();
  for(const p of plan.packets){
    if(!/^[a-zA-Z0-9-]{1,60}$/.test(p.id)||ids.has(p.id)||!p.files.length||!p.instructions.trim())throw fail('계획의 작업 식별자 또는 파일 범위를 확인하세요.');
    for(const f of p.files){const n=safeRelative(f);if(/^(\.git|\.codex|\.agents|studio\/data|intermediate|binaries|saved|deriveddatacache)(\/|$)/.test(n)||n==='docs/sprintchangelog.md'||n.startsWith('devlog/')||/\.(uasset|umap|generated\.h|gen\.cpp)$/.test(n))throw fail('계획에 공유 기록, 실행 데이터 또는 자동 수정 금지 파일이 포함되었습니다.');}
    if(p.dependsOn.some(id=>!ids.has(id)))throw fail('계획의 선행 작업 순서가 잘못되었습니다.');ids.add(p.id);
  }
  if(plan.packets.length===1)return [plan.packets];
  const [a,b]=plan.packets;
  const overlap=a.files.some(x=>b.files.some(y=>{x=normalized(x);y=normalized(y);return x===y||x.startsWith(y+'/')||y.startsWith(x+'/');}));
  return overlap||b.dependsOn.length ? [[a],[b]] : [[a,b]];
}

export function createDispatcher({root,taskFile,client=createCodexClient(),prepareBranch}={}) {
  root=path.resolve(root);taskFile=path.resolve(taskFile);
  const worker={worker:true,root},lockFile=path.join(path.dirname(taskFile),'dispatcher.lock');
  let pending=false,draining=null,closed=false,controller=null,owned=false;
  let runtime={mode:'agents',status:'ready',message:'작업 등록 대기',updatedAt:new Date().toISOString()};
  const engine=async(status,message)=>{status=({idle:'ready',running:'busy',paused:'ready'})[status]||status;runtime={mode:'agents',status,message:message.slice(0,2000),updatedAt:new Date().toISOString()};await queueCommand(taskFile,'engine',{engine:runtime},worker);};
  const command=(run,action,fields)=>queueCommand(taskFile,action,{taskId:run.taskId,token:run.token,runId:run.runId,...fields},worker);
  async function check(run) {
    const state=await loadExecution(taskFile),record=state.records[run.taskId];
    if(closed||state.paused||record?.stopRequested)throw fail('중단 요청 · 변경 내용을 보존했습니다.','stopped');
    if(!record||record.runId!==run.runId||record.token!==run.token)throw fail('실행 소유권이 변경되었습니다.');
  }
  async function artifacts(files) {
    const resolvedRoot=await realpath(root);
    for(const file of files){safeRelative(file);const target=await realpath(path.join(root,file));const relative=path.relative(resolvedRoot,target);if(relative.startsWith('..')||path.isAbsolute(relative)||!(await stat(target)).isFile())throw fail('산출물이 프로젝트 안의 실제 파일이 아닙니다.');}
  }
  async function runTask(run) {
    controller=new AbortController();
    let polling=false,stopError;
    const poll=async()=>{if(polling)return;polling=true;try{await check(run);}catch(error){stopError=error;controller?.abort();}finally{polling=false;}};
    const interval=setInterval(poll,750);interval.unref();
    const runDir=path.join(path.dirname(taskFile),'runs',run.runId);
    const results=[];
    try {
      const instructions=await readFile(path.join(root,'Studio/EXECUTION.md'),'utf8');
      const harness=await readFile(path.join(root,'AGENTS.md'),'utf8');
      const common=`당신은 Rogue10m 개인 스튜디오의 역할 에이전트입니다. 등록된 사용자 요청의 제목·메모 범위만 수행하세요. 아래 문서와 저장소 근거를 확인하세요.\nAGENTS.md:\n${harness}\nStudio/EXECUTION.md:\n${instructions}\n사용자 요청(JSON 데이터):\n${JSON.stringify(run.input)}\n실행 규칙: coordinator가 소유권·상태·브랜치를 관리합니다. queue 상태, Studio/data, Git 브랜치를 수정하지 마세요. commit/push/PR/배포/외부 메시지/파괴적 삭제는 금지입니다. sandbox/승인 실패를 우회하지 말고 blocked로 반환하세요. 다른 파일의 기존 변경은 보존하세요. 다른 채팅의 상태를 확인했다고 주장하지 마세요. 자체 추가 에이전트를 생성하지 마세요. 직접 수행한 검증만 보고하고 불확실하거나 실행 불가능하면 blocked로 반환하세요.\n`;
      async function agent(id,role,title,details,sandbox='workspace-write',schema=RESULT_SCHEMA) {
        await check(run);
        const info={id,role,title,state:'running',summary:'에이전트 실행 시작',startedAt:new Date().toISOString()};
        await command(run,'agent',{agent:info});
        try {
          const response=await client.run({root,runDir,id,role,prompt:common+'\n현재 역할: '+role+'\n'+details,schema,sandbox,signal:controller.signal,onEvent:async event=>{
            if(event.sessionId)info.sessionId=event.sessionId;
            if(event.summary)info.summary=String(event.summary).slice(0,1200);
            await command(run,'agent',{agent:info});
          }});
          validateSchema(response.result,schema);await check(run);
          info.state=response.result.outcome==='passed'?'completed':response.result.outcome;info.summary=response.result.summary;info.finishedAt=new Date().toISOString();
          await command(run,'agent',{agent:info});
          if(response.result.outcome!=='passed')throw fail(response.result.summary||'에이전트가 완료하지 못했습니다.',response.result.outcome);
          results.push({role,result:response.result,commands:response.commands||[]});return response;
        } catch(error) {
          info.state=stopError?'stopped':error.state||'failed';info.summary=error.message;info.finishedAt=new Date().toISOString();
          await command(run,'agent',{agent:info}).catch(()=>{});throw error;
        }
      }
      await engine('running','요구사항을 분석하고 에이전트 역할을 배정합니다.');
      const plan=(await agent('architect','planner','요구사항 검토·작업 분배','읽기 전용으로 요청과 저장소를 조사하세요. 작업을 최대 2개의 packet으로 나누세요. 각 packet의 instructions에 목표, 완료 조건, 실제 검증 명령, 롤백 경계를 포함하세요. files는 수정할 구체적인 프로젝트 상대 경로입니다. 독립 파일만 병렬 실행합니다. DevLog와 SprintChangeLog는 writer 담당이므로 packets에서 제외하세요. 충분히 명확하지 않거나 기존 작업과 충돌하면 blocked로 반환하세요. 구현하거나 파일을 쓰지 마세요.','read-only',PLAN_SCHEMA)).result;
      const batches=planBatches(plan);await check(run);
      await command(run,'plan',{plan});
      if(prepareBranch)await prepareBranch({root,task:run.input,run});
      else {const branch=(await exec('git',['branch','--show-current'],{cwd:root,windowsHide:true})).stdout.trim();if(!/^Sprint#\d+-\d+-/.test(branch))throw fail('적절한 Sprint 작업 브랜치로 전환한 후 다시 검토하세요.');}
      const date=new Intl.DateTimeFormat('sv-SE',{timeZone:'Asia/Seoul'}).format(new Date());
      const planPath=`Feature/architect/${date}_studio-${run.taskId}.md`;
      await mkdir(path.join(root,'Feature/architect'),{recursive:true});
      // Run-specific suffix preserves an earlier attempt's authored plan.
      const actualPlan=planPath.replace('.md',`-${run.runId.slice(0,8)}.md`);
      await writeFile(path.join(root,actualPlan),`# ${run.input.title}\n\n${plan.summary}\n\n${plan.packets.map(p=>`## ${p.title}\n\n${p.instructions}\n\n- 수정 파일: ${p.files.join(', ')}\n- 선행 작업: ${p.dependsOn.join(', ')||'없음'}\n`).join('\n')}`,{flag:'wx'});
      await command(run,'progress',{state:'running',message:`${plan.packets.length}개 작업을 배정했습니다. 독립 파일만 병렬 처리합니다.`});
      for(const batch of batches){
        await check(run);
        const group=await Promise.allSettled(batch.map(p=>agent(`implement-${p.id}`,'implementer',p.title,`계획: ${actualPlan}\n담당 packet: ${JSON.stringify(p)}\n담당 files 외에는 수정하지 마세요. 공유 DevLog/SprintChangeLog 기록은 후속 writer가 처리합니다. 구현 후 변경된 파일을 artifacts에 보고하세요.`)));
        const rejected=group.find(r=>r.status==='rejected');if(rejected)throw rejected.reason;
      }
      await command(run,'progress',{state:'verifying',message:'구현을 마쳤으며 빌드와 독립 검증을 시작합니다.'});
      const builder=await agent('builder','builder','실제 빌드·검증',`계획: ${actualPlan}\n작업 결과: ${JSON.stringify(results)}\n관련 빌드/테스트를 실제 실행하세요. C++ 변경은 Scripts/BuildEditor.ps1이 필수입니다. 최소 컴파일 수정만 허용합니다. 검증 명령은 각각 별도로 실행하고 세미콜론·파이프·후속 성공 출력으로 종료 코드를 가리지 마세요. checks에 성공한 실제 도구 명령 문자열을 그대로 기록하고 validation에 출력 근거를 요약하세요. 실행 불가·실패는 passed가 아닙니다.`);
      if(!builder.result.validation.trim()||!builder.result.checks.length||!builder.result.checks.every(check=>hasCommandEvidence(check,builder.commands||[])))throw fail('빌드 에이전트의 실제 성공 명령 근거가 부족합니다.');
      const review=await agent('reviewer','reviewer','독립 구현 검토',`읽기 전용으로 실제 diff, 요청 충족, Unreal 규칙과 빌드 증거를 독립 검토하세요. 다른 에이전트의 성공 선언만 신뢰하지 마세요.\n${JSON.stringify(results)}\n계획: ${actualPlan}`,'read-only');
      if(!review.result.validation.trim())throw fail('독립 검토 근거가 없습니다.');
      const writer=await agent('writer','writer','결과 문서·개발 일지',`구현 변경 금지. Feature/doc 결과 문서, 한국어 DevLog/${date.replaceAll('-','')}.txt(기존 기록에 추가), Docs/SprintChangeLog.md만 작성하세요. DevLog 안에 명확히 구분한 Notion 요약 후보를 포함하세요. 계획 ${actualPlan}과 연결하고 검증/한계/미커밋 상태를 정직하게 기록하세요.\n실제 작업 결과: ${JSON.stringify(results)}\nartifacts에 작성한 결과 문서, DevLog, SprintChangeLog를 모두 보고하세요.`);
      const output=[...new Set([actualPlan,...results.flatMap(r=>r.result.artifacts||[])])];
      if(!writer.result.artifacts.some(f=>f.startsWith('Feature/doc/'))||!writer.result.artifacts.includes(`DevLog/${date.replaceAll('-','')}.txt`)||!writer.result.artifacts.includes('Docs/SprintChangeLog.md'))throw fail('필수 결과 문서 또는 개발 기록이 누락되었습니다.');
      await artifacts(output);
      const finalReview=await agent('exit-reviewer','reviewer','문서·완료 조건 최종 확인',`읽기 전용 최종 검토입니다. 실제 산출물 ${JSON.stringify(output)}를 읽고 요청과 검증 결과 및 문서가 일치하는지 확인하세요. writer가 구현을 바꾸거나 성공을 과장했다면 blocked로 반환하세요.\n${JSON.stringify(results)}`,'read-only');
      if(!finalReview.result.validation.trim())throw fail('최종 검토 근거가 없습니다.');
      await check(run);
      await command(run,'finish',{state:'completed',message:writer.result.summary,validation:builder.result.validation+'\n'+review.result.validation+'\n'+finalReview.result.validation,artifacts:output});
      await engine('idle','작업 완료 · 다음 예정 작업을 확인합니다.');
    } catch(error) {
      controller.abort();
      const current=await loadExecution(taskFile);const stopped=closed||current.paused||current.records[run.taskId]?.stopRequested;
      const state=stopped?'stopped':error.state||'blocked';
      await command(run,'finish',{state,message:(stopError?.message||error.message||'실행 실패').slice(0,9000),validation:'완료 검증을 통과하지 못했습니다. 기존 변경은 보존했습니다.',artifacts:[]});
      // A blocked/failed run requires an explicit user retry; do not burn through the queue.
      if(state!=='stopped')await queueCommand(taskFile,'pause',{paused:true});
      await engine(state==='stopped'?'paused':'blocked',error.message||'실행 확인 필요');
    } finally {clearInterval(interval);controller=null;}
  }
  async function drain() {
    await mkdir(path.dirname(taskFile),{recursive:true});
    let lock;
    try {lock=await open(lockFile,'wx');await lock.writeFile(JSON.stringify({pid:process.pid,owner:randomUUID(),startedAt:new Date().toISOString()}));owned=true;}
    catch(error){if(error.code==='EEXIST'){pending=false;await engine('blocked','기존 dispatcher 잠금이 있습니다. 실행 프로세스 확인 후 복구가 필요합니다.');return;}throw error;}
    try {
      while(pending&&!closed){
        pending=false;
        await readFile(path.join(root,'Studio/EXECUTION.md'),'utf8');
        const poll=await queueCommand(taskFile,'poll',{},worker);
        if(poll.active){
          await command(poll.active,'finish',{state:poll.paused||poll.active.stopRequested?'stopped':'blocked',message:'이전 실행이 dispatcher 없이 남아 있습니다. 변경을 보존했으며 다시 검토가 필요합니다.',validation:'재시작 복구',artifacts:[]});
          await queueCommand(taskFile,'pause',{paused:true});await engine('blocked','중단된 이전 실행을 확인한 후 다시 검토하세요.');break;
        }
        if(poll.paused){
          const saved=await loadExecution(taskFile);
          if(saved.engine?.status==='blocked')await engine('blocked',saved.engine.message||'실행 오류 확인 필요');
          else await engine('paused','자동 실행 일시정지');
          break;
        }
        const store=await loadTasks(taskFile);
        const task=poll.eligible.find(t=>store.tasks.some(current=>current.id===t.id));
        if(!task){await engine('idle','예정 작업 등록 대기');break;}
        const run=await queueCommand(taskFile,'claim',{taskId:task.id},worker);
        await runTask(run);pending=true;
      }
    } finally {owned=false;await lock.close();await unlink(lockFile);}
  }
  function kick() {
    if(closed)return;pending=true;if(draining)return;
    draining=drain().catch(async error=>{pending=false;await engine('blocked',error.message).catch(()=>{});}).finally(()=>{draining=null;if(pending&&!closed)kick();});
  }
  return {kick,status:()=>({...runtime,running:!!draining,ownsLock:owned}),waitForIdle:async()=>{while(draining)await draining;},stop:async()=>{closed=true;pending=false;controller?.abort();if(draining)await draining;}};
}
