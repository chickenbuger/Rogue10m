const $ = selector => document.querySelector(selector);
const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const localDate = (date = new Date()) => `${date.getFullYear()}-${String(date.getMonth()+1).padStart(2,'0')}-${String(date.getDate()).padStart(2,'0')}`;
const labels = { overview:['스튜디오 홈','오늘의 개발, 한곳에서 이어가세요.','YOUR DEVELOPMENT WORKSPACE'], schedule:['개발 작업','예정에 등록하면 AI가 나누어 진행하고, 검증 결과를 남깁니다.','YOUR AI DEVELOPMENT TEAM'], changes:['개선 · 수정','어떤 변화가 있었는지, 근거와 함께 확인하세요.','PROJECT CHANGELOG'], journals:['개발 기록','하루의 고민과 결과가 쌓이는 곳.','DEVELOPMENT JOURNAL'], sources:['설계 · 결과 문서','아이디어에서 검증까지, 작업의 맥락을 이어갑니다.','PROJECT LIBRARY'] };
let data, tasks = [], rev, execution, view = 'overview', filter = '전체', query = '', limit = 30, scheduleMode = '상태별', sourceMode = '결과 문서', saving = false, connected = true, polling = false;
const agentRoles = {planner:'설계',implementer:'구현',builder:'빌드·검증',reviewer:'독립 리뷰',writer:'문서화'};
const agentStates = {queued:'대기',running:'진행 중',completed:'완료',failed:'실패',blocked:'확인 필요',stopped:'중단'};
const executionLabels = {queued:'에이전트 배정 대기',reviewing:'요구사항 검토',running:'개발 중',verifying:'검증 중',completed:'실행 완료',blocked:'확인 필요',failed:'실행 실패',stopped:'중단됨'};
const runningStates = ['reviewing','running','verifying'];
function taskStatus(t) { const state=execution?.records?.[t.id]?.state;return state==='completed'?'완료':state==='verifying'?'검증 대기':['reviewing','running'].includes(state)?'진행 중':t.status; }
const dateTime = value => value ? new Date(value).toLocaleString('ko-KR',{month:'numeric',day:'numeric',hour:'2-digit',minute:'2-digit'}) : '아직 확인하지 않음';
const statusColors = { '예정':'', '진행 중':'green', '검증 대기':'orange', '완료':'blue', '수정':'orange', '개선':'green', '기능':'blue' };
const badge = (text, color = statusColors[text] || '') => `<span class="badge ${color}">${escape(text)}</span>`;
const goButton = (target, text='전체 보기 ↗') => `<button class="text-button" data-go="${target}">${text}</button>`;
const docButton = (source,title,cls='row-title') => `<button type="button" class="${cls}" data-doc="${escape(source)}" data-title="${escape(title)}">${escape(title)}</button>`;
const empty = (text, action='') => `<div class="empty"><div class="empty-symbol">▱</div>${text}${action}</div>`;
const addButton = '<br><button class="secondary" data-new>첫 작업 등록하기 ＋</button>';
function toast(message) { $('#toast').textContent = message; $('#toast').hidden = false; clearTimeout(toast.timer); toast.timer = setTimeout(() => $('#toast').hidden = true, 3500); }
async function api(url, options) { const response = await fetch(url, options); const result = await response.json(); if (!response.ok) throw new Error(result.error || '요청을 완료하지 못했습니다.'); return result; }
function connectionState(ok, error) {
  connected=ok;
  $('#error').hidden=ok;
  if(!ok) $('#error').textContent=`스튜디오 서버에 연결할 수 없습니다. 표시된 내용은 마지막 수신 상태입니다. OpenPersonalStudio.bat으로 서버를 실행하면 자동으로 다시 연결합니다. ${error?.message||''}`;
}
async function refresh() {
  $('#refresh').disabled = true;
  try {
    const [snapshot, stored] = await Promise.all([api('/api/snapshot'), api('/api/execution')]);
    data = snapshot; execution=stored; tasks = stored.tasks; rev = stored.taskRevision;
    connectionState(true);
    $('#sync-time').textContent = `원본 확인 ${new Date(data.updatedAt).toLocaleTimeString('ko-KR',{hour:'2-digit',minute:'2-digit'})} · PC에 저장`;
    render();
  } catch (error) { connectionState(false,error); if (!data) $('#content').innerHTML = empty('기록을 불러오지 못했습니다. 위 안내를 확인하세요.'); else render(); }
  finally { $('#refresh').disabled = false; }
}
function navigate(next) { if (!labels[next]) return; view=next; filter='전체'; query=''; limit=30; history.replaceState(null,'',`#${view}`); render(); }
function render() {
  if (!data) return;
  $('#breadcrumb').textContent = labels[view][0]; $('#page-title').innerHTML = `${labels[view][0]}<span class="accent">.</span>`; $('#subtitle').textContent = labels[view][1]; $('#eyebrow').textContent = labels[view][2];
  document.querySelectorAll('[data-view]').forEach(button => {button.classList.toggle('active',button.dataset.view===view); if(button.dataset.view===view) button.setAttribute('aria-current','page'); else button.removeAttribute('aria-current');});
  $('#task-count').textContent = tasks.filter(t=>taskStatus(t)!=='완료').length;
  $('#content').innerHTML = ({overview:overview, schedule:schedule, changes:changes, journals:journals, sources:sources})[view]();
}
function metric(label, value, note, suffix='') { return `<article class="metric"><div class="metric-label">${label}<span>↗</span></div><div class="metric-value">${escape(value)}<small>${suffix}</small></div><p>${escape(note)}</p></article>`; }
function activeTasks() { const rank = {'높음':0,'보통':1,'낮음':2}; return tasks.filter(t=>taskStatus(t)!=='완료').sort((a,b)=>(a.date||'9999').localeCompare(b.date||'9999') || rank[a.priority]-rank[b.priority]); }
function week() {
  const today = localDate(), start = new Date(); start.setDate(start.getDate() - (start.getDay()+6)%7);
  return `<div class="week">${['월','화','수','목','금','토','일'].map((name,i)=>{const d=new Date(start); d.setDate(d.getDate()+i);const key=localDate(d), count=tasks.filter(t=>t.date===key).length;return `<div class="day ${key===today?'today':''}"><span>${name}</span><strong>${d.getDate()}</strong><span class="day-count">${count?count+'건':'—'}</span></div>`;}).join('')}</div>`;
}
function miniTask(task) { return `<button class="task-mini" data-task="${escape(task.id)}"><span class="task-check">${taskStatus(task)==='완료'?'✓':'□'}</span><div class="task-mini-content"><h3>${escape(task.title)}</h3><p>${escape(task.date||'날짜 미정')} · ${escape(task.category)}</p></div>${badge(taskStatus(task))}</button>`; }
function overview() {
  const current = data.entries.find(e=>data.branch.startsWith(e.id+'-'));
  const latest = data.entries[0];
  const sprint = data.branch.match(/Sprint#(\d+)/)?.[1] || latest?.sprint || '—';
  const open = activeTasks(), overdue = open.filter(t=>t.date&&t.date<localDate()).length;
  const recent = data.entries.slice(0,5);
  return `<div class="metrics">${metric('현재 스프린트',String(sprint).padStart(2,'0'),'현재 작업 브랜치 기준','SPRINT')}${metric('진행 중인 작업',tasks.filter(t=>taskStatus(t)==='진행 중').length,'직접 등록한 일정 기준','개')}${metric('확인이 필요한 일정',overdue,overdue?'목표일이 지난 미완료 작업':'기한을 넘긴 등록 작업이 없습니다','개')}${metric('누적 개발 기록',data.journals.length,'DevLog 일자별 기록','일')}</div>
  <div class="overview-grid"><section class="panel"><div class="panel-head"><div><h2>진행 중인 프로젝트</h2><p>Rogue10m · 10분 로그라이크 액션 RPG</p></div>${badge('개발 작업실','green')}</div><div class="sprint-focus"><div class="sprint-meta"><span class="sprint-number">SPRINT ${escape(sprint)} / CURRENT WORK</span>${badge('브랜치 기준')}</div><h3>${escape(current?.title || '작업 기록 연결 대기')}</h3><p>${escape(current?.goal || '현재 브랜치의 작업 기록이 아직 등록되지 않았습니다. 개발 일정에서 다음 작업을 관리하세요.')}</p><div class="branch">⑂ ${escape(data.branch)}</div></div><div class="panel-head"><h2>다음 할 일 <span class="muted">${open.length.toString().padStart(2,'0')}</span></h2>${goButton('schedule','일정 열기 ↗')}</div>${open.length?open.slice(0,3).map(miniTask).join(''):empty('다음 목표를 정해볼까요?<br>아직 등록된 개발 일정이 없습니다.',addButton)}</section>
  <div class="side-stack"><section class="panel"><div class="panel-head"><h2>이번 주</h2><span class="muted">${new Date().getMonth()+1}월</span></div>${week()}<div class="note-box">${tasks.filter(t=>{const d=new Date(t.date+'T12:00:00');const start=new Date();start.setHours(0,0,0,0);start.setDate(start.getDate()-(start.getDay()+6)%7);return d>=start&&d<new Date(start.getTime()+7*86400000);}).length}개의 등록 일정 ${goButton('schedule','일정 관리 ↗')}</div></section><section class="panel"><div class="panel-head"><h2>확인할 기록</h2>${badge('문서 기준')}</div><div class="note-box">${data.unmatchedPlans.slice(0,2).map(d=>`<div class="notice-row">${docButton(d.source,d.title)}<small>${escape(d.note)}</small></div>`).join('') || '모든 설계 문서에 같은 이름의 결과 문서가 있습니다.'}<div class="notice-row"><small>문서 연결 상태를 표시합니다.<br>실제 게임의 완료 여부는 원문의 검증 범위를 확인하세요.</small></div></div></section></div></div>
  <section class="panel recent-panel"><div class="panel-head"><div><h2>최근 변경 사항</h2><p>작업 번호순 · 저장소에 기록된 변경과 검증</p></div>${goButton('changes')}</div>${recordTable(recent)}</section>`;
}
function recordTable(entries) {
  if (!entries.length) return empty('조건에 맞는 변경 기록이 없습니다.');
  return `<div class="table-wrap"><table class="records"><thead><tr><th>작업</th><th>구분</th><th class="status-col">기록된 상태</th><th>기록일</th></tr></thead><tbody>${entries.map(e=>`<tr><td class="title-cell"><button class="row-title" data-entry="${escape(e.id)}">${escape(e.title)}</button><div class="row-meta">${escape(e.id)}</div></td><td>${badge(e.category)}</td><td class="status-col"><div class="row-status" title="${escape(e.status||'상태 미기록')}">${escape(e.status||'상태 미기록')}</div></td><td class="date-cell">${escape(e.date||'미기록')}</td></tr>`).join('')}</tbody></table></div>`;
}
function taskCard(t) {
  const run=execution?.records?.[t.id];
  const queueLabel=run?executionLabels[run.state]:(t.dependsOn||[]).some(id=>{const dep=tasks.find(x=>x.id===id);return dep&&taskStatus(dep)!=='완료';})?'선행 작업 대기':t.status==='예정'?'에이전트 배정 대기':taskStatus(t);
  const agents=run?.agents||[],active=agents.filter(a=>a.state==='running');
  return `<button class="task-card" data-task="${escape(t.id)}"><div class="task-card-meta">${badge(t.category)}<span>${t.priority==='높음'?'↑ 높음':escape(t.priority)}</span></div><h3>${escape(t.title)}</h3>${t.notes?`<p>${escape(t.notes.slice(0,95))}${t.notes.length>95?'…':''}</p>`:''}<div class="execution-tag">${badge(queueLabel,['blocked','failed'].includes(run?.state)?'orange':runningStates.includes(run?.state)?'green':'')}</div>${agents.length?`<div class="agent-card-status">${active.length?escape(active.map(a=>agentRoles[a.role]||a.role).join(' · '))+' 담당 작업 중':`에이전트 ${agents.filter(a=>a.state==='completed').length}/${agents.length} 완료`}</div>`:''}<div class="task-card-meta"><span class="${t.date&&t.date<localDate()&&taskStatus(t)!=='완료'?'overdue':''}">${escape(t.date||'목표일 없음')}</span><span>담당 · 결과 ↗</span></div></button>`;
}
function executionPanel() {
  if(!execution)return '';
  const current=tasks.find(t=>t.id===execution.activeId),engine=execution.engine;
  const status=!connected?'offline':engine?.status||'offline';
  const title=!connected?'스튜디오 연결 끊김':execution.paused?'AI 작업 일시정지':({ready:'AI 팀이 다음 작업을 기다립니다',busy:'AI 팀이 작업을 진행하고 있습니다',blocked:'AI 실행을 위해 확인이 필요합니다',offline:'AI 실행 엔진 연결 대기'})[status];
  return `<section class="panel execution-panel"><div><p class="eyebrow">AI DEVELOPMENT TEAM</p><h2>${title}</h2><p>${escape(!connected?'서버에 다시 연결되면 실제 진행 상태를 확인합니다.':engine?.message||'실행 엔진의 연결 상태를 확인하고 있습니다.')} · 처리 가능 ${execution.ready?.length||0}개</p>${current?`<p>현재 작업: <button class="text-button" data-task="${escape(current.id)}">${escape(current.title)} ↗</button></p>`:''}<div class="workflow-path" aria-label="작업 진행 순서">${['예정','진행 중','검증 대기','완료'].map(s=>badge(s)).join('<span aria-hidden="true">→</span>')}</div><small>예정 등록 시 AI가 역할을 나누어 처리합니다. 검증 결과와 문서가 준비되면 완료로 이동합니다.<br>PC와 스튜디오 서버가 켜져 있어야 합니다. 중단 요청은 안전한 작업 지점에서 반영됩니다.</small>${execution.stale?'<p class="execution-warning">작업 진행 갱신이 지연되고 있습니다. 담당 에이전트 기록을 확인하세요.</p>':''}</div><button type="button" class="secondary" data-execution-action="pause" data-paused="${!execution.paused}" ${!connected?'disabled':''}>${execution.paused?'AI 작업 재개':'AI 작업 일시정지'}</button></section>`;
}
function schedule() {
  const done=tasks.filter(t=>taskStatus(t)==='완료').length;
  const matching = tasks.filter(t=>[t.title,t.notes,t.category].join(' ').toLowerCase().includes(query.toLowerCase()));
  const toolbar=`<div class="toolbar"><div class="tabs">${['상태별','날짜순'].map(x=>`<button class="tab ${scheduleMode===x?'active':''}" data-schedule-mode="${x}">${x}</button>`).join('')}<button class="tab" id="export-tasks">일정 백업 ↓</button></div><input class="search" data-search aria-label="작업 검색" placeholder="작업 검색…" value="${escape(query)}"></div>`;
  return `${executionPanel()}${toolbar}<p class="schedule-summary">등록 작업 <strong>${tasks.length}개</strong> · 완료 ${done}개${tasks.length?' · 완료율 '+Math.round(done/tasks.length*100)+'%':''} <span class="footer-line">/</span> 목표일은 참고 기한입니다. 예정에 넣으면 우선순위와 선행 작업에 따라 처리합니다.</p>${scheduleMode==='상태별'?`<div class="board">${['예정','진행 중','검증 대기','완료'].map(status=>{const list=matching.filter(t=>taskStatus(t)===status).sort((a,b)=>(a.date||'9999').localeCompare(b.date||'9999'));return `<section class="column"><div class="column-head">${badge(status)}<span>${list.length}</span></div>${list.length?list.map(taskCard).join(''):empty(query?'검색 결과 없음':'등록된 작업 없음')}</section>`;}).join('')}</div>`:`<section class="panel">${matching.length?matching.sort((a,b)=>(a.date||'9999').localeCompare(b.date||'9999')).map(miniTask).join(''):empty('등록된 작업이 없습니다.',addButton)}</section>`}`;
}
function changes() {
  const rows=data.entries.filter(e=>(filter==='전체'||e.category===filter)&&[e.title,e.id,e.text].join(' ').toLowerCase().includes(query.toLowerCase()));
  return `<div class="toolbar"><div class="tabs">${['전체','기능','개선','수정','도구·기록'].map(t=>`<button class="tab ${filter===t?'active':''}" data-filter="${t}">${t}</button>`).join('')}</div><input class="search" data-search aria-label="변경 기록 검색" placeholder="작업명, 키워드로 검색…" value="${escape(query)}"></div><p class="schedule-summary">${rows.length}개의 작업 · 제목 기준 자동 분류 · 원문에서 실제 상태와 검증 범위 확인</p><section class="panel">${recordTable(rows.slice(0,limit))}</section>${rows.length>limit?'<div class="empty"><button class="secondary" data-more>이전 기록 더 보기</button></div>':''}`;
}
function journals() {
  const list=data.journals.filter(d=>(d.text+d.date).toLowerCase().includes(query.toLowerCase()));
  return `<div class="toolbar"><span class="muted">${list.length}일의 개발 기록</span><input class="search" data-search aria-label="개발 일지 검색" placeholder="날짜 또는 내용 검색…" value="${escape(query)}"></div><div class="journal-grid">${list.slice(0,limit).map(d=>`<button class="journal-card" data-doc="${escape(d.source)}" data-title="${d.date} 개발 일지"><small>${d.date.replaceAll('-',' / ')}</small><h3>${escape(d.text.match(/^#{1,2}\s+(.+)$/m)?.[1]?.replace(/^20\d{2}-\d{2}-\d{2}\s*[—|–-]?\s*/,'') || '개발 일지')}</h3><p>${escape(d.text.split('\n').filter(s=>s.trim()&&!s.startsWith('#')).slice(0,4).join(' ').slice(0,240))}</p><span class="text-button">기록 읽기 ↗</span></button>`).join('')}</div>${!list.length?empty('검색 결과가 없습니다.'):''}${list.length>limit?'<div class="empty"><button class="secondary" data-more>이전 기록 더 보기</button></div>':''}`;
}
function sources() {
  const list=(sourceMode==='결과 문서'?data.results:data.plans).filter(d=>(d.title+d.text).toLowerCase().includes(query.toLowerCase()));
  return `<div class="toolbar"><div class="tabs">${['결과 문서','설계 문서'].map(x=>`<button class="tab ${sourceMode===x?'active':''}" data-source-mode="${x}">${x}</button>`).join('')}</div><input class="search" data-search aria-label="문서 검색" placeholder="문서 제목, 내용 검색…" value="${escape(query)}"></div><p class="schedule-summary">${list.length}개의 문서 · 원문은 읽기 전용입니다.</p><div class="source-list">${list.slice(0,limit).map(d=>`<button class="source-item" data-doc="${escape(d.source)}" data-title="${escape(d.title)}"><div><h3>${escape(d.title)}</h3><p>${escape(d.source)}</p></div><span>↗</span></button>`).join('')}</div>${!list.length?empty('검색 결과가 없습니다.'):''}${list.length>limit?'<div class="empty"><button class="secondary" data-more>문서 더 보기</button></div>':''}`;
}
function editTask(id) {
  const task=tasks.find(t=>t.id===id); $('#task-form').reset(); $('#task-id').value=task?.id||''; $('#task-dialog-title').textContent=task?'작업 수정':'새 작업'; $('#delete-task').hidden=!task; $('#form-error').hidden=true;
  for(const key of ['title','notes','date','category','status','priority']) if(task) $('#task-'+key).value=task[key];
  $('#task-depends-on').innerHTML=tasks.filter(t=>t.id!==id).map(t=>`<option value="${escape(t.id)}" ${(task?.dependsOn||[]).includes(t.id)?'selected':''}>${escape(t.title)}</option>`).join('');
  syncTaskLock(id);
  renderTaskExecution(id);
  $('#task-dialog').showModal(); if(!$('#task-title').disabled)$('#task-title').focus();
}
function syncTaskLock(id) {
  const active=runningStates.includes(execution?.records?.[id]?.state),completed=execution?.records?.[id]?.state==='completed';
  for(const field of $('#task-form').querySelectorAll('input,select,textarea'))field.disabled=active;
  $('#save-task').disabled=active||saving||!connected;$('#delete-task').disabled=active||saving||!connected;
  $('#task-status').disabled=true;
  const task=tasks.find(t=>t.id===id);$('#task-status').value=task?taskStatus(task):'예정';
  $('#task-lock-note').textContent=active?'AI가 작업 중입니다. 내용을 바꾸려면 먼저 중단을 요청하세요.':completed?'AI가 검증과 결과 기록을 완료했습니다. 새 개발 요청은 새 작업으로 등록하세요.':'예정으로 저장하면 AI가 검토를 시작합니다. 진행 상태는 담당 에이전트가 갱신합니다.';
}
function agentRoster(agents=[]) {
  if(!agents.length)return '<p class="muted">작업 범위를 검토하면 역할별 담당 에이전트가 표시됩니다.</p>';
  return `<div class="agent-roster">${agents.map(a=>`<article class="agent-row ${a.state==='running'?'is-running':''}"><div class="agent-row-head"><strong>${escape(agentRoles[a.role]||a.role)}</strong>${badge(agentStates[a.state]||a.state,a.state==='running'?'green':a.state==='completed'?'blue':['failed','blocked'].includes(a.state)?'orange':'')}</div><h4>${escape(a.title||agentRoles[a.role]||'담당 작업')}</h4>${a.summary?`<p>${escape(a.summary)}</p>`:''}<small>${a.startedAt?'시작 '+escape(dateTime(a.startedAt)):'배정 대기'}${a.finishedAt?' · 종료 '+escape(dateTime(a.finishedAt)):''}</small></article>`).join('')}</div>`;
}
function renderTaskExecution(id) {
  const r=execution?.records?.[id];const target=$('#task-execution');
  if(!r){target.innerHTML='';return;}
  const events=r.events||[],showEvents=target.querySelector('details')?.open;
  target.innerHTML=`<div class="execution-detail"><h3>${escape(executionLabels[r.state]||r.state)} · ${r.attempt||1}회차</h3><p>${escape(r.summary||events.at(-1)?.message||'')}</p>${r.stopRequested?'<p class="execution-warning">중단 요청됨 · 다음 안전한 지점에서 종료합니다.</p>':''}${r.plan?.summary?`<h3>작업 분배 계획</h3><p>${escape(r.plan.summary)}</p>`:''}<h3>담당 에이전트</h3>${agentRoster(r.agents)}${r.validation?`<h3>검증 결과</h3><p>${escape(r.validation)}</p>`:r.state==='verifying'?'<p class="execution-warning">검증과 독립 리뷰가 끝나면 결과를 기록합니다.</p>':''}${r.artifacts?.length?`<h3>산출물</h3><ul>${r.artifacts.map(p=>`<li>${/^(Feature\/(architect|doc)\/[^/]+\.md|Docs\/SprintChangeLog\.md|DevLog\/\d{8}\.txt)$/.test(p)?docButton(p,p):escape(p)}</li>`).join('')}</ul>`:''}<details ${showEvents?'open':''}><summary>진행 기록 ${events.length}건</summary><ol>${events.slice().reverse().map(e=>`<li><small>${escape(dateTime(e.at))} · ${escape(executionLabels[e.state]||e.state)}</small><p>${escape(e.message)}</p></li>`).join('')}</ol></details>${runningStates.includes(r.state)?`<button type="button" class="secondary" data-execution-action="stop" data-task-id="${escape(id)}" ${r.stopRequested||!connected?'disabled':''}>중단 요청</button>`:['blocked','failed','stopped'].includes(r.state)?`<button type="button" class="secondary" data-execution-action="retry" data-task-id="${escape(id)}" ${!connected?'disabled':''}>다시 검토</button><small>내용을 수정했다면 먼저 저장한 뒤 다시 검토하세요.</small>`:''}</div>`;
}
async function persist(next) {
  if(saving) return false; saving=true; $('#save-task').disabled=true; $('#delete-task').disabled=true;
  try {const saved=await api('/api/tasks',{method:'PUT',headers:{'Content-Type':'application/json','X-Studio-Request':'1'},body:JSON.stringify({tasks:next,revision:rev})}); tasks=saved.tasks;rev=saved.revision;render();return true;}
  catch(error){$('#form-error').textContent=error.message;$('#form-error').hidden=false;return false;}
  finally{saving=false;syncTaskLock($('#task-id').value);}
}
function showDocument(title, source, text) { $('#document-title').textContent=title;$('#document-source').textContent=source;$('#document-body').textContent=text;$('#document-dialog').showModal(); }
document.addEventListener('click', async event=>{
  const b=event.target.closest('button');if(!b)return;
  if(b.dataset.executionAction){b.disabled=true;try{execution=await api('/api/execution',{method:'POST',headers:{'Content-Type':'application/json','X-Studio-Request':'1'},body:JSON.stringify({action:b.dataset.executionAction,taskId:b.dataset.taskId,paused:b.dataset.paused==='true'})});tasks=execution.tasks;rev=execution.taskRevision;render();if($('#task-dialog').open){syncTaskLock($('#task-id').value);renderTaskExecution($('#task-id').value);}toast('실행 요청을 반영했습니다.');}catch(error){toast(error.message);}finally{b.disabled=false;}return;}
  if(b.dataset.view)navigate(b.dataset.view); if(b.dataset.go)navigate(b.dataset.go);
  if(b.hasAttribute('data-new'))editTask(); if(b.dataset.task)editTask(b.dataset.task);
  if(b.dataset.filter){filter=b.dataset.filter;limit=30;render();}
  if(b.dataset.scheduleMode){scheduleMode=b.dataset.scheduleMode;render();}
  if(b.dataset.sourceMode){sourceMode=b.dataset.sourceMode;limit=30;render();}
  if(b.hasAttribute('data-more')){limit+=30;render();}
  if(b.classList.contains('close-dialog')&&!saving)$('#task-dialog').close();
  if(b.classList.contains('close-document'))$('#document-dialog').close();
  if(b.dataset.entry){const entry=data.entries.find(e=>e.id===b.dataset.entry);showDocument(entry.title,entry.source+' · '+entry.id,entry.text);}
  if(b.dataset.doc){try{const result=await api('/api/document?path='+encodeURIComponent(b.dataset.doc));showDocument(b.dataset.title,b.dataset.doc,result.text);}catch(error){toast(error.message);}}
  if(b.id==='export-tasks'){const blob=new Blob([JSON.stringify({version:1,exportedAt:new Date().toISOString(),tasks},null,2)],{type:'application/json'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download=`rogue10m-studio-${localDate()}.json`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);toast('일정 백업을 다운로드했습니다.');}
});
document.addEventListener('input',event=>{if(!event.target.matches('[data-search]'))return;query=event.target.value;const pos=event.target.selectionStart;render();const input=$('[data-search]');input.focus();input.setSelectionRange(pos,pos);});
$('#task-form').addEventListener('submit',async event=>{event.preventDefault();if(saving||!connected)return;const id=$('#task-id').value;if(runningStates.includes(execution?.records?.[id]?.state))return;const original=tasks.find(t=>t.id===id),task={id:id||crypto.randomUUID(),status:original?.status||'예정'};for(const key of ['title','notes','date','category','priority'])task[key]=$('#task-'+key).value;task.autoRun=true;task.startAfter='';task.dependsOn=[...$('#task-depends-on').selectedOptions].map(o=>o.value);const next=tasks.some(t=>t.id===task.id)?tasks.map(t=>t.id===task.id?task:t):[...tasks,task];if(await persist(next)){$('#task-dialog').close();toast(task.status==='예정'?'작업을 저장했습니다. AI 처리 대기열에 반영했습니다.':'작업 내용을 저장했습니다.');}});
$('#delete-task').addEventListener('click',async()=>{if(saving||!confirm('이 작업을 삭제할까요?'))return;if(await persist(tasks.filter(t=>t.id!==$('#task-id').value))){$('#task-dialog').close();toast('작업을 삭제했습니다.');}});
$('#task-dialog').addEventListener('cancel',event=>{if(saving)event.preventDefault();});
$('#new-task').addEventListener('click',()=>{if(!data){toast('프로젝트 기록을 먼저 불러와 주세요.');return;}editTask();});
$('#refresh').addEventListener('click',refresh);
$('.brand').addEventListener('click',event=>{event.preventDefault();navigate('overview');});
$('#today').textContent=new Date().toLocaleDateString('ko-KR',{month:'long',day:'numeric',weekday:'short'});
if(labels[location.hash.slice(1)])view=location.hash.slice(1);
await refresh();
setInterval(async()=>{
  if(saving||document.hidden||polling)return;
  polling=true;
  try{const next=await api('/api/execution');const changed=JSON.stringify(next)!==JSON.stringify(execution)||!connected;execution=next;connectionState(true);
    if(!data){await refresh();return;}
    const dialogOpen=$('#task-dialog').open;
    // Keep the revision captured when editing: concurrent task edits must still fail safely.
    if(!dialogOpen){tasks=next.tasks;rev=next.taskRevision;}
    if(changed&&dialogOpen){syncTaskLock($('#task-id').value);renderTaskExecution($('#task-id').value);}
    if(changed&&!document.activeElement?.matches('[data-search]'))render();
  }catch(error){const wasConnected=connected;connectionState(false,error);if(wasConnected){render();if($('#task-dialog').open){syncTaskLock($('#task-id').value);renderTaskExecution($('#task-id').value);}}}
  finally{polling=false;}
},2000);
