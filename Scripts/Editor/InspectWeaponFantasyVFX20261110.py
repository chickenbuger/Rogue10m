"""Read-only official UE5.8 MCP inventory. Requires the broker's serial UE lease."""
from __future__ import annotations
import argparse, json, urllib.parse, urllib.request
from pathlib import Path
TOOLSET='NiagaraToolsets.NiagaraToolset_System'
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Feature/doc/evidence/sword-knuckle-20261110/vfx-source-inspection'
SOURCES=[
 '/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Cast',
 '/Game/Rogue10m/VFX/Character/Combat/Knuckle/NS_Knuckle_Impact',
 '/Game/Rogue10m/VFX/Character/Combat/GreatSword/NS_GreatSword_Cast',
 '/Game/Rogue10m/VFX/Character/Combat/GreatSword/NS_GreatSword_Charge',
 '/Game/Rogue10m/VFX/Character/Combat/GreatSword/NS_GreatSword_Impact',
 '/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst',
 '/Game/Rogue10m/VFX/Monsters/NS_Monster_IronShell']
def object_path(path):
 return path if '.' in path.rsplit('/',1)[-1] else path+'.'+path.rsplit('/',1)[-1]
def ref(path,emitter='',script='',module='',index=-1,names=()):
 return {'system':{'refPath':object_path(path)},'emitterName':emitter,'scriptName':script,'moduleName':module,'rendererIndex':index,'inputNameStack':list(names)}
class Client:
 def __init__(self,url,out=OUT):
  if urllib.parse.urlparse(url).hostname not in ('127.0.0.1','localhost','::1'):raise RuntimeError('Only verified loopback editor allowed')
  self.url=url;self.out=out;self.id=0
  self.headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
  self.opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
  init=self.rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'weapon-fantasy-vfx','version':'1'}})
  self.headers['MCP-Protocol-Version']=init['result']['protocolVersion'];self.rpc('notifications/initialized',None,False)
 def rpc(self,method,params,identified=True):
  self.id+=1;p={'jsonrpc':'2.0','method':method}
  if params is not None:p['params']=params
  if identified:p['id']=self.id
  with self.opener.open(urllib.request.Request(self.url,json.dumps(p).encode(),headers=self.headers),timeout=90) as response:
   session=response.headers.get('Mcp-Session-Id')
   if session:self.headers['Mcp-Session-Id']=session
   raw=response.read().decode()
   if not raw.strip():return {}
   if 'text/event-stream' in response.headers.get('Content-Type',''):
    answer=next((json.loads(line[5:]) for line in raw.splitlines() if line.startswith('data:') and json.loads(line[5:]).get('id')==self.id),None)
    if answer is None:raise RuntimeError('No matching MCP response')
    return answer
   return json.loads(raw)
 def tool(self,name,args,label):
  result=self.rpc('tools/call',{'name':name,'arguments':args})
  self.out.mkdir(parents=True,exist_ok=True);(self.out/(label+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
  if 'error' in result or result.get('result',{}).get('isError'):raise RuntimeError('Failed '+label+': '+str(result))
  body=result['result']
  for entry in body.get('content',[]):
   if entry.get('type')=='text':
    try:body=json.loads(entry['text']);break
    except ValueError:pass
  return body.get('returnValue',body) if isinstance(body,dict) else body
 def niagara(self,name,args,label):return self.tool('call_tool',{'toolset_name':TOOLSET,'tool_name':name,'arguments':args},label)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--url',default='http://127.0.0.1:8001/mcp');p.add_argument('--lease-granted',action='store_true');a=p.parse_args()
 if not a.lease_granted:raise RuntimeError('Broker must grant serial UE lease first')
 c=Client(a.url);c.tool('list_toolsets',{},'00-toolsets');c.tool('describe_toolset',{'toolset_name':TOOLSET},'01-niagara-schema')
 c.niagara('GetModuleSchemaFromAsset',{'moduleAsset':{'refPath':'/Niagara/Modules/Update/Camera/CameraOffset.CameraOffset'}},'01-camera-offset-schema')
 rows=[]
 for index,path in enumerate(SOURCES):
  label=f'{index+2:02d}-'+path.rsplit('/',1)[-1]
  summary=c.niagara('GetSystemSummary',{'system':{'refPath':object_path(path)}},label+'-summary')
  c.niagara('GetSystemData',{'system':{'refPath':object_path(path)}},label+'-data')
  for emitter in summary.get('emitters',[]):
   name=emitter['emitterName'];key=label+'-'+name
   c.niagara('GetEmitterTopology',{'emitterRef':ref(path,name)},key+'-topology')
   c.niagara('GetEmitterInputValues',{'emitterRef':ref(path,name)},key+'-inputs')
   c.niagara('GetEmitterData',{'emitterRef':ref(path,name)},key+'-data')
  rows.append({'source':path,'summary':summary});print('INSPECTED',path,flush=True)
 (OUT/'summary.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_FANTASY_VFX_SOURCES_INSPECTED')
if __name__=='__main__':main()


