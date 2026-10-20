"""Author finite species bursts through the official UE Niagara toolset, serially.
Use only against this feature's isolated, idle Editor. No original assets are deleted.
"""
import argparse,json,urllib.request,datetime
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Feature/doc/evidence/monster-effects-chase-20261011/authoring'
TOOLSET='NiagaraToolsets.NiagaraToolset_System'
TEMPLATE='/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst'
FXROOT='/Game/Rogue10m/VFX/Monsters'
# Species, RGB, count, size min/max, lifetime min/max, speed, cone angle.
CONFIGS=[
 ('VerdantSlime',(0.18,2.8,0.12),18,(10,10),(18,18),(.35,.65),240,85),
 ('Sporeling',(1.5,2.3,.28),32,(14,14),(25,25),(.5,.9),140,130),
 ('CrimsonSlime',(3.8,.2,.035),25,(5,12),(9,22),(.3,.65),400,70),
 ('IronShell',(3.2,2.1,.8),20,(2,13),(4,24),(.2,.45),600,95),
 ('ThornWarden',(.18,2.1,.65),14,(3,24),(5,40),(.25,.55),450,50),
 ('Azathor',(1.5,.12,3.6),30,(8,20),(18,35),(.35,.7),350,100),
 ('AzathorWave',(1.6,.18,4),60,(10,28),(20,50),(.65,1.1),850,22),
 ('AzathorZone',(1.25,.06,3.7),70,(12,12),(24,24),(.45,.9),220,175),
]
class Client:
 def __init__(self,url):
  if url not in ('http://127.0.0.1:8002/mcp',):raise RuntimeError('Only the isolated feature Editor endpoint is allowed')
  self.url=url;self.id=0;self.headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
  self.opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
  d=self.rpc('initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'monster-fx-authoring','version':'1'}},True)
  self.headers['MCP-Protocol-Version']=d['result']['protocolVersion'];self.rpc('notifications/initialized',None,False)
 def rpc(self,method,params,identified=True):
  p={'jsonrpc':'2.0','method':method}
  if params is not None:p['params']=params
  if identified:self.id+=1;p['id']=self.id
  req=urllib.request.Request(self.url,json.dumps(p).encode(),headers=self.headers)
  with self.opener.open(req,timeout=120) as r:
   session=r.headers.get('Mcp-Session-Id')
   if session:self.headers['Mcp-Session-Id']=session
   body=r.read().decode()
  return json.loads(body) if body.strip() else {}
 def tool(self,name,args,label=None):
  d=self.rpc('tools/call',{'name':'call_tool','arguments':{'toolset_name':TOOLSET,'tool_name':name,'arguments':args}})
  OUT.mkdir(parents=True,exist_ok=True)
  (OUT/(f'{self.id:03d}-{label or name}.json')).write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
  if 'error' in d or d.get('result',{}).get('isError'):raise RuntimeError(d)
  body=json.loads(d['result']['content'][0]['text'])
  return body.get('returnValue',body)
def ref(system,script='',module='',input_name=None,emitter='DirectionalBurst'):
 return {'system':{'refPath':system},'emitterName':emitter,'scriptName':script,'moduleName':module,'rendererIndex':-1,'inputNameStack':[input_name] if input_name else []}
def value(kind,data):return {'struct':{'refPath':kind},'value':data}
def set_input(c,system,script,module,name,kind,data):
 return c.tool('SetStackInputData',{'stackInputRef':ref(system,script,module,name),'inputData':value(kind,data)})
def main():
 global OUT
 p=argparse.ArgumentParser();p.add_argument('--inspect',action='store_true');p.add_argument('--resume-first',action='store_true');a=p.parse_args()
 OUT=OUT/datetime.datetime.now().strftime('%H%M%S')
 c=Client('http://127.0.0.1:8002/mcp')
 if a.inspect:
  for emitter,script,module in [('','SystemUpdateScript','SystemState'),('DirectionalBurst','EmitterUpdateScript','EmitterState'),('DirectionalBurst','ParticleSpawnScript','AddVelocityInCone')]:
   d=c.tool('GetModuleInputValues',{'moduleRef':ref(TEMPLATE,script,module,emitter=emitter)});print(module,json.dumps(d,ensure_ascii=False))
  print('renderer',c.tool('GetRendererData',{'rendererRef':{**ref(TEMPLATE),'rendererIndex':0}}));return
 for name,color,count,size_min,size_max,lifetime,speed,cone in CONFIGS:
  asset_name='NS_Monster_'+name
  system=FXROOT+'/'+asset_name+'.'+asset_name
  if a.resume_first and name=='VerdantSlime':
   summary=c.tool('GetSystemSummary',{'system':{'refPath':system}})
   if [e['emitterName'] for e in summary['emitters']]!=['DirectionalBurst']:raise RuntimeError('Unexpected partial asset; inspect before resuming')
  else:
   created=c.tool('CreateNiagaraSystem',{'assetName':asset_name,'assetPath':FXROOT,'templateSystem':{'refPath':TEMPLATE}},'create-'+name)
   if not isinstance(created,dict) or created.get('refPath')!=system:
    raise RuntimeError('Creation returned an unexpected path; no existing system will be edited: '+str(created))
   c.tool('RemoveEmitter',{'emitterToRemove':ref(system,emitter='LocationBasedRibbon')})
   c.tool('RemoveModule',{'moduleToRemove':ref(system,'ParticleUpdateScript','GenerateLocationEvent')})
  for module,name_in,kind,data in [
   ('SpawnBurst_Instantaneous','Spawn Count','/Script/Niagara.NiagaraInt32',{'value':count})]:
   set_input(c,system,'EmitterUpdateScript',module,name_in,kind,data)
  for input_name,kind,data in [
   ('Color','/Script/CoreUObject.LinearColor',dict(zip(('r','g','b','a'),(*color,1)))),
   ('Lifetime Min','/Script/Niagara.NiagaraFloat',{'value':lifetime[0]}),
   ('Lifetime Max','/Script/Niagara.NiagaraFloat',{'value':lifetime[1]}),
   ('Sprite Size Min','/Script/CoreUObject.Vector2f',dict(zip(('x','y'),size_min))),
   ('Sprite Size Max','/Script/CoreUObject.Vector2f',dict(zip(('x','y'),size_max)))]:
   set_input(c,system,'ParticleSpawnScript','InitializeParticle',input_name,kind,data)
  set_input(c,system,'ParticleSpawnScript','AddVelocityInCone','Velocity Strength','/Script/Niagara.NiagaraFloat',{'value':speed})
  set_input(c,system,'ParticleSpawnScript','AddVelocityInCone','Cone Angle','/Script/Niagara.NiagaraFloat',{'value':cone})
  set_input(c,system,'ParticleSpawnScript','AddVelocityInCone','Cone Axis','/Script/CoreUObject.Vector3f',{'x':1,'y':0,'z':.15})
  # Asset checks are collected after opening each system's Editor in a later stage.
  print('AUTHORED',name,flush=True)
if __name__=='__main__':main()
