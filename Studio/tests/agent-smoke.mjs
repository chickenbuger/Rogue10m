// Explicit opt-in: invokes the real logged-in Codex CLI in an isolated fixture.
import path from 'node:path';
import {randomUUID} from 'node:crypto';
import {mkdir,writeFile} from 'node:fs/promises';
import {createDispatcher} from '../dispatcher.mjs';
import {saveTasks,loadTasks} from '../lib.mjs';
import {queueView} from '../queue.mjs';

if(!process.argv.includes('--real-agent'))throw new Error('실제 모델 실행은 --real-agent 인자가 필요합니다.');
const root=path.resolve('Studio/data',`smoke-${randomUUID()}`);
await mkdir(path.join(root,'Studio'),{recursive:true});
await writeFile(path.join(root,'AGENTS.md'),'This is an isolated dispatcher smoke fixture. Work only inside this root. Read input.txt. Never access or modify the parent repository. No Git commands. Architect/implementer/builder/reviewer/writer are separate roles. Writer owns DevLog and Docs/SprintChangeLog.md. Preserve existing files.');
await writeFile(path.join(root,'Studio/EXECUTION.md'),'Registered task only. Do not claim other tasks or edit queue state. Approval policy never: sandbox errors must return outcome blocked. Do not escalate or change sandbox settings. No commits, publishing, messaging, or deletion.');
await writeFile(path.join(root,'input.txt'),'studio-agent-smoke: 7 + 5 = 12\n');
const taskFile=path.join(root,'Studio/data/tasks.json');
await saveTasks(taskFile,[{id:'smoke',title:'격리 에이전트 실행 검증',notes:'먼저 input.txt를 실제 도구로 읽으세요. 읽을 수 없으면 즉시 blocked로 반환하세요. 읽기에 성공하면 그 파일의 수식 결과 12를 Feature/doc/smoke-result.md에 기록하고 실제 명령으로 검증하세요. 이 격리 폴더 밖으로 접근하거나 수정하지 마세요.',date:'',status:'예정',category:'검증',priority:'보통'}],(await loadTasks(taskFile)).revision);
const dispatcher=createDispatcher({root,taskFile,prepareBranch:async()=>{}});
dispatcher.kick();await dispatcher.waitForIdle();await dispatcher.stop();
const view=await queueView(taskFile);
console.log(JSON.stringify({root,paused:view.paused,engine:view.engine,record:view.records.smoke},null,2));
