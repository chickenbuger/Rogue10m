// Optional UI QA; pass the path of an existing Playwright package. No product dependency.
import { createRequire } from 'node:module';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import os from 'node:os';
import assert from 'node:assert/strict';
import { createStudioServer } from '../server.mjs';
const require = createRequire(import.meta.url);
const { chromium } = require(process.argv[2] || 'playwright');
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const temp = await mkdtemp(path.join(os.tmpdir(),'rogue-studio-ui-'));
for(const dir of ['Docs','DevLog','Feature/architect','Feature/doc'])await mkdir(path.join(temp,dir),{recursive:true});
await writeFile(path.join(temp,'Docs/SprintChangeLog.md'),'## Sprint#4-47 UI 안정화 수정\n- 목표: 임시 데이터 브라우저 검증\n- 검증: 독립 HTTP 서버\n- 상태: 완료\n');
await writeFile(path.join(temp,'DevLog/20260928.txt'),'# 2026-09-28 테스트 개발 기록\n'+ '격리된 브라우저에서 개발 기록과 검색, 문서 열람을 검증합니다. '.repeat(8));
for(const dir of ['architect','doc'])await writeFile(path.join(temp,`Feature/${dir}/2026-09-28_ui-fixture.md`),'# UI 테스트 문서\n임시 산출물을 확인합니다.\n');
const server = createStudioServer({root:temp,taskFile:path.join(temp,'tasks.json')});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const url=`http://127.0.0.1:${server.address().port}`;
let browser;
try {
  browser=await chromium.launch({channel:'msedge',headless:true});
  const page=await browser.newPage({viewport:{width:1440,height:1100},deviceScaleFactor:1,reducedMotion:'reduce'});
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto(url);await page.locator('.metrics').waitFor();
  const evidence=path.join(root,'Feature/doc/evidence/studio-agent-workflow-20260928');await mkdir(evidence,{recursive:true});
  await page.screenshot({path:path.join(evidence,'workspace-desktop.png'),fullPage:true});
  await page.locator('#new-task').click();await page.locator('#task-title').fill('UI 검증용 <태그>');await page.locator('#task-date').fill('2026-10-02');await page.locator('#task-notes').fill('<script>alert(1)</script>');await page.locator('#save-task').click();await page.locator('#task-dialog').waitFor({state:'hidden'});
  await page.locator('[data-view="schedule"]').click();await page.getByRole('heading',{name:'UI 검증용 <태그>'}).waitFor();
  await page.locator('.task-card').click();assert.equal(await page.locator('#task-status').isDisabled(),true);await page.locator('#task-notes').fill('내용 수정 확인 <script>alert(1)</script>');await page.locator('#save-task').click();await page.locator('#task-dialog').waitFor({state:'hidden'});
  await page.reload();await page.locator('.task-card').waitFor();assert.equal(await page.locator('.column').filter({has:page.locator('.column-head .badge',{hasText:'예정'})}).locator('.task-card').count(),1);assert.match(await page.locator('.task-card').textContent(),/내용 수정 확인 <script>/);
  const downloadPromise=page.waitForEvent('download');await page.locator('#export-tasks').click();const download=await downloadPromise;assert.match(download.suggestedFilename(),/rogue10m-studio/);
  await page.locator('[data-view="changes"]').click();await page.locator('[data-filter="수정"]').click();await page.getByRole('textbox',{name:'변경 기록 검색'}).fill('안정화');assert.equal(await page.locator('tbody tr').count(),1);await page.locator('[data-entry]').click();await page.locator('#document-dialog').waitFor();assert.match(await page.locator('#document-body').textContent(),/검증/);await page.locator('.close-document').click();
  await page.locator('[data-view="journals"]').click();await page.locator('.journal-card').first().click();await page.locator('#document-dialog').waitFor();assert.ok((await page.locator('#document-body').textContent()).length>100);await page.locator('.close-document').click();
  await page.locator('[data-view="sources"]').click();await page.locator('.source-item').first().click();await page.locator('#document-dialog').waitFor();await page.locator('.close-document').click();
  await page.locator('[data-view="schedule"]').click();await page.locator('.task-card').click();page.once('dialog',dialog=>dialog.accept());await page.locator('#delete-task').click();await page.locator('#task-dialog').waitFor({state:'hidden'});assert.equal(await page.locator('.task-card').count(),0);
  await page.setViewportSize({width:390,height:844});
  for (const name of ['overview','schedule','changes','journals','sources']) {
    await page.locator(`[data-view="${name}"]`).click();
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`Mobile overflow: ${name}`);
    if(name==='overview')await page.screenshot({path:path.join(evidence,'workspace-mobile.png'),fullPage:true});
  }
  await page.locator('#new-task').click();assert.ok(await page.locator('#task-title').isVisible());await page.keyboard.press('Escape');
  await page.setViewportSize({width:1280,height:900});await page.locator('[data-view="overview"]').click();await page.evaluate(()=>document.documentElement.style.fontSize='200%');assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'200% root text overflow');
  assert.deepEqual(errors,[]);console.log('PASS: 5 views, create/edit/reload/delete, AI-managed state, escaped text, backup, search, source dialogs, 390px mobile, 200% root text, no browser errors. Isolated fixtures only.');
} finally {if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));assert.equal(path.dirname(path.resolve(temp)),path.resolve(os.tmpdir()));assert.ok(path.basename(temp).startsWith('rogue-studio-ui-'));await rm(temp,{recursive:true,force:true});}
