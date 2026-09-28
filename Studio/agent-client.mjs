import path from 'node:path';
import os from 'node:os';
import { mkdir, readFile, writeFile, readdir, stat } from 'node:fs/promises';
import { spawn, execFile } from 'node:child_process';
import { promisify } from 'node:util';

const exec = promisify(execFile);
export async function resolveCodex() {
  if (process.env.STUDIO_CODEX_PATH) return process.env.STUDIO_CODEX_PATH;
  const base = path.join(process.env.LOCALAPPDATA || path.join(os.homedir(),'AppData/Local'),'OpenAI/Codex/bin');
  try {
    const candidates = await Promise.all((await readdir(base)).map(async entry => {
      const file = path.join(base,entry,'codex.exe');
      try { return {file,time:(await stat(file)).mtimeMs}; } catch { return null; }
    }));
    const latest = candidates.filter(Boolean).sort((a,b)=>b.time-a.time)[0];
    if (latest) return latest.file;
  } catch { /* PATH fallback for non-desktop installations. */ }
  return process.platform === 'win32' ? 'codex.exe' : 'codex';
}

// Validate locally as well: the model's output contract is not a trust boundary.
export function validateSchema(value, schema, field = 'result') {
  if (schema.enum && !schema.enum.includes(value)) throw new Error(`${field}: 허용되지 않는 값`);
  if (schema.type === 'object') {
    if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${field}: 객체 필요`);
    for (const key of schema.required || []) if (!(key in value)) throw new Error(`${field}.${key}: 누락`);
    for (const key of Object.keys(value)) {
      if (!schema.properties[key]) throw new Error(`${field}.${key}: 알 수 없는 필드`);
      validateSchema(value[key],schema.properties[key],`${field}.${key}`);
    }
  } else if (schema.type === 'array') {
    if (!Array.isArray(value) || value.length > (schema.maxItems ?? 100) || value.length < (schema.minItems ?? 0)) throw new Error(`${field}: 배열 길이 오류`);
    value.forEach((item,i)=>validateSchema(item,schema.items,`${field}[${i}]`));
  } else if (schema.type === 'string') {
    if (typeof value !== 'string' || value.length > (schema.maxLength ?? 20000) || value.length < (schema.minLength ?? 0)) throw new Error(`${field}: 문자열 길이 오류`);
  } else if (schema.type === 'boolean' && typeof value !== 'boolean') throw new Error(`${field}: boolean 필요`);
}

export function createCodexClient({ executable, timeoutMs = 45 * 60 * 1000 } = {}) {
  return { async run({root,runDir,id,prompt,schema,sandbox,signal,onEvent = ()=>{}}) {
    if (signal?.aborted) throw Object.assign(new Error('중단 요청'),{name:'AbortError'});
    await mkdir(runDir,{recursive:true});
    const schemaFile=path.join(runDir,`${id}.schema.json`), outputFile=path.join(runDir,`${id}.result.json`);
    await writeFile(schemaFile,JSON.stringify(schema));
    const command=executable || await resolveCodex();
    const args=['exec','--json','--color','never','--sandbox',sandbox,'-c','approval_policy="never"','--cd',root,'--output-schema',schemaFile,'--output-last-message',outputFile,'-'];
    const child=spawn(command,args,{cwd:root,windowsHide:true,stdio:['pipe','pipe','pipe'],detached:process.platform!=='win32'});
    let buffer='',stderr='',sessionId='',streamFailure='',eventQueue=Promise.resolve(),closed=false,termination,exitCode=null;
    const commands=[];
    const emit=event=>{eventQueue=eventQueue.then(()=>onEvent(event));eventQueue.catch(()=>{});};
    const terminate=()=> {
      if (termination || closed || !child.pid) return;
      termination=(async()=>{
        if (process.platform==='win32') await exec('taskkill',['/PID',String(child.pid),'/T','/F'],{windowsHide:true}).catch(()=>{});
        else {try {process.kill(-child.pid,'SIGTERM');} catch {} }
      })();
    };
    signal?.addEventListener('abort',terminate,{once:true});
    if(signal?.aborted)terminate();
    let timedOut=false;
    const timer=setTimeout(()=>{timedOut=true;terminate();},timeoutMs);timer.unref();
    const parse=line=>{
      let event;try {event=JSON.parse(line);} catch {return;}
      if(event.type==='thread.started') {sessionId=String(event.thread_id||'').slice(0,200);emit({type:'session',sessionId});}
      if(event.type==='turn.failed'||event.type==='error') streamFailure=String(event.error?.message||event.message||'Codex 실행 오류').slice(0,2000);
      const item=event.item;
      if(item?.type==='command_execution' && event.type==='item.completed') {
        const evidence={command:String(item.command||'').slice(0,2000),exitCode:item.exit_code,output:String(item.aggregated_output||'').slice(-6000)};
        commands.push(evidence);if(commands.length>200)commands.shift();
        emit({type:'command',summary:`명령 검증 · 종료 코드 ${evidence.exitCode ?? '미확인'}`});
      } else if(item?.type==='file_change' && event.type==='item.completed') emit({type:'files',summary:'파일 변경 기록 수신'});
      else if(item?.type==='agent_message' && event.type==='item.completed') emit({type:'message',summary:String(item.text||'').slice(0,1200)});
    };
    child.stdout.setEncoding('utf8');child.stderr.setEncoding('utf8');
    child.stdout.on('data',chunk=>{
      buffer+=chunk;
      if(buffer.length>2_000_000) {streamFailure='에이전트 출력 한도 초과';terminate();return;}
      let newline;while((newline=buffer.indexOf('\n'))>=0){parse(buffer.slice(0,newline));buffer=buffer.slice(newline+1);}
    });
    child.stderr.on('data',chunk=>{stderr=(stderr+chunk).slice(-6000);});
    child.stdin.on('error',()=>{});
    child.stdin.end(prompt);
    try {
      const code=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('close',code=>{closed=true;exitCode=code;resolve(code);});});
      if(buffer.trim())parse(buffer);await eventQueue;await termination;
      if(signal?.aborted)throw Object.assign(new Error('중단 요청 · 실행 프로세스 종료 확인'),{name:'AbortError'});
      if(timedOut)throw new Error('에이전트 실행 시간 한도 초과');
      if(code!==0 || streamFailure)throw new Error((streamFailure||stderr||`Codex 종료 코드 ${code}`).slice(-2000));
      const result=JSON.parse(await readFile(outputFile,'utf8'));validateSchema(result,schema);
      return {result,commands,sessionId};
    } finally {
      clearTimeout(timer);signal?.removeEventListener('abort',terminate);
      await writeFile(path.join(runDir,`${id}.evidence.json`),JSON.stringify({sessionId,exitCode,commands,error:streamFailure,stderr,aborted:!!signal?.aborted,timedOut},null,2));
    }
  }};
}
