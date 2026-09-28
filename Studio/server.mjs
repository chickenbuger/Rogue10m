import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readFile } from 'node:fs/promises';
import { snapshot, readDocument, loadTasks, saveTasks } from './lib.mjs';
import { queueView, queueCommand } from './queue.mjs';

const directory = path.dirname(fileURLToPath(import.meta.url));
export function createStudioServer({ root = path.resolve(directory, '..'), taskFile = path.join(directory, 'data/tasks.json'), dispatcher = null } = {}) {
  const server = http.createServer(async (req, res) => {
    const send = (status, payload, type = 'application/json; charset=utf-8') => {
      res.writeHead(status, { 'Content-Type':type, 'Cache-Control':'no-store', 'X-Content-Type-Options':'nosniff', 'Content-Security-Policy':"default-src 'self'; style-src 'self'; script-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'" });
      res.end(type.startsWith('application/json') ? JSON.stringify(payload) : payload);
    };
    try {
      const host = `127.0.0.1:${server.address().port}`;
      if (req.headers.host !== host && req.headers.host !== `localhost:${server.address().port}`) return send(403,{ error:'허용되지 않은 호스트입니다.' });
      const url = new URL(req.url, `http://${host}`);
      if (req.headers.origin && req.headers.origin !== `http://${req.headers.host}`) return send(403, { error:'외부 사이트에서의 접근은 허용하지 않습니다.' });
      if (req.method === 'GET' && url.pathname === '/api/snapshot') return send(200, await snapshot(root));
      if (req.method === 'GET' && url.pathname === '/api/document') return send(200, { text: await readDocument(root, url.searchParams.get('path') || '') });
      if (req.method === 'GET' && url.pathname === '/api/tasks') return send(200, await loadTasks(taskFile));
      if (req.method === 'GET' && url.pathname === '/api/execution') return send(200, await queueView(taskFile));
      if (req.method === 'POST' && url.pathname === '/api/execution') {
        if (req.headers['content-type'] !== 'application/json' || req.headers['x-studio-request'] !== '1') return send(415,{error:'지원하지 않는 요청 형식입니다.'});
        const chunks=[];let bytes=0;
        for await(const chunk of req){bytes+=chunk.length;if(bytes>10000)return send(413,{error:'요청이 너무 큽니다.'});chunks.push(chunk);}
        let input;try{input=JSON.parse(Buffer.concat(chunks).toString('utf8'));}catch{return send(400,{error:'입력 내용을 읽을 수 없습니다.'});}
        if(!input || !['pause','stop','retry'].includes(input.action))return send(400,{error:'허용되지 않는 제어 명령입니다.'});
        try{await queueCommand(taskFile,input.action,input);dispatcher?.kick();return send(200,await queueView(taskFile));}catch(error){return send(error.status||400,{error:error.message});}
      }
      if (req.method === 'PUT' && url.pathname === '/api/tasks') {
        if (req.headers['content-type'] !== 'application/json' || req.headers['x-studio-request'] !== '1') return send(415,{ error:'지원하지 않는 요청 형식입니다.' });
        const chunks = []; let bytes = 0;
        for await (const chunk of req) { bytes += chunk.length; if (bytes > 2_000_000) return send(413,{ error:'저장할 내용이 너무 큽니다.' }); chunks.push(chunk); }
        let body; try { body = JSON.parse(Buffer.concat(chunks).toString('utf8')); } catch { return send(400,{error:'입력 내용을 읽을 수 없습니다.'}); }
        if (!body || typeof body !== 'object' || Array.isArray(body)) return send(400,{error:'작업 목록을 확인하세요.'});
        try { const saved=await saveTasks(taskFile, body.tasks, body.revision);dispatcher?.kick();return send(200,saved); } catch (error) { return send(error.status || 400, { error: error.message }); }
      }
      const assets = { '/':'index.html', '/app.js':'app.js', '/style.css':'style.css', '/favicon.svg':'favicon.svg' };
      if (req.method === 'GET' && assets[url.pathname]) {
        const file = assets[url.pathname];
        const type = file.endsWith('.html') ? 'text/html' : file.endsWith('.js') ? 'text/javascript' : file.endsWith('.svg') ? 'image/svg+xml' : 'text/css';
        return send(200, await readFile(path.join(directory,'public',file)), type + '; charset=utf-8');
      }
      send(404,{error:'페이지를 찾을 수 없습니다.'});
    } catch (error) { send(error.status || 500,{error: error.code === 'ENOENT' ? '원본 문서를 찾을 수 없습니다.' : error.message || '데이터를 읽을 수 없습니다.'}); }
  });
  return server;
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const port = Number(process.env.STUDIO_PORT || 4317);
  const {createDispatcher}=await import('./dispatcher.mjs');
  const dispatcher=createDispatcher({root:path.resolve(directory,'..'),taskFile:path.join(directory,'data/tasks.json')});
  const server = createStudioServer({dispatcher});
  server.on('error', error => { console.error(error.code === 'EADDRINUSE' ? `포트 ${port}가 사용 중입니다. 기존 스튜디오를 확인하거나 STUDIO_PORT를 변경하세요.` : error.message); process.exitCode = 1; });
  server.listen(port,'127.0.0.1', () => {console.log(`Rogue10m Studio ready: http://127.0.0.1:${port}`);dispatcher.kick();});
  let closing=false;
  for(const signal of ['SIGINT','SIGTERM'])process.on(signal,async()=>{if(closing)return;closing=true;await dispatcher.stop();server.close(()=>process.exit(0));});
}
