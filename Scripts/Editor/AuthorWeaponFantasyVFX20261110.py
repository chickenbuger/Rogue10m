"""Author four finite weapon fantasy Niagara derivatives through official UE5.8 MCP.
Requires a broker-granted serial lease, fresh source inventory and prepared materials.
Does not start the editor, save/activate DataAssets, change vendor sources or retry mutations.
"""
from __future__ import annotations
import argparse,json,hashlib,time
from pathlib import Path
from InspectWeaponFantasyVFX20261110 import Client,ROOT,OUT as INVENTORY,object_path,ref
DEST='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110'
SOURCE='/Game/Rogue10m/VFX/Monsters/NS_Monster_IronShell'
OUT=ROOT/'Feature/doc/evidence/sword-knuckle-20261110/vfx-authoring-mcp'
# Internal units are deliberately sized for the existing unchanged DA scale/dilation.
# One burst / one sprite per component, no spawned ribbons, collisions, lights or decals.
CONFIGS=[
 ('NS_KnuckleFlurry_Acceptance','M_KnuckleFlurry_Acceptance',96,44,.26,True),
 ('NS_KnuckleFlurry_ConfirmedImpact','M_KnuckleFlurry_ConfirmedImpact',118,118,.30,False),
 ('NS_GreatSwordHeavy_Acceptance','M_GreatSwordHeavy_Acceptance',70,140,.27,True),
 ('NS_GreatSwordHeavy_ConfirmedImpact','M_GreatSwordHeavy_ConfirmedImpact',144,144,.35,False)]

def local(kind,value):return {'struct':{'refPath':kind},'value':value}
def set_input(c,path,script,module,name,kind,data,label):
 return c.niagara('SetStackInputData',{'stackInputRef':ref(path,'DirectionalBurst',script,module,names=[name]),'inputData':local(kind,data)},label)

def require_finite_source(summary):
 emitters=summary.get('emitters',[])
 if len(emitters)!=1 or emitters[0].get('emitterName')!='DirectionalBurst' or emitters[0].get('simTarget')!='CPUSim' or not emitters[0].get('bEnabled') or [r.get('refPath') for r in emitters[0].get('rendererClasses',[])]!=['/Script/Niagara.NiagaraSpriteRendererProperties']:raise RuntimeError('Finite local source topology changed; inspect before authoring')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--url',default='http://127.0.0.1:8001/mcp');p.add_argument('--lease-granted',action='store_true');p.add_argument('--resume',action='store_true');p.add_argument('--resume-first',action='store_true');p.add_argument('--resume-existing-count',type=int,default=0);a=p.parse_args()
 if not a.lease_granted:raise RuntimeError('Serial UE broker lease required')
 inventory=json.loads((INVENTORY/'summary.json').read_text(encoding='utf-8-sig'))
 source=next((r for r in inventory if r['source']==SOURCE),None)
 if not source:raise RuntimeError('Fresh source inventory required')
 require_finite_source(source['summary']); source_file=ROOT/'Content/Rogue10m/VFX/Monsters/NS_Monster_IronShell.uasset'; source_hash=hashlib.sha256(source_file.read_bytes()).hexdigest(); c=Client(a.url,OUT)
 c.tool('list_toolsets',{},'00-toolsets');c.tool('describe_toolset',{'toolset_name':'NiagaraToolsets.NiagaraToolset_System'},'01-schema')
 c.niagara('GetRendererSchema',{'rendererClass':{'refPath':'/Script/Niagara.NiagaraSpriteRendererProperties'}},'02-renderer-schema')
 current=c.niagara('GetSystemSummary',{'system':{'refPath':object_path(SOURCE)}},'03-source-now');require_finite_source(current)
 report=[]
 for index,(name,material,width,height,life,local_space) in enumerate(CONFIGS):
  path=DEST+'/'+name;label=name.replace('NS_','')
  existing=a.resume or index<a.resume_existing_count or (a.resume_first and index==0)
  if existing:
   check=c.niagara('GetSystemSummary',{'system':{'refPath':object_path(path)}},label+'-resume-state');require_finite_source(check)
  else:
   created=c.niagara('CreateNiagaraSystem',{'assetName':name,'assetPath':DEST,'templateSystem':{'refPath':object_path(SOURCE)}},label+'-create')
   if not isinstance(created,dict) or created.get('refPath')!=object_path(path):raise RuntimeError('Unexpected create outcome; do not retry '+str(created))
  if not existing:
   set_input(c,path,'EmitterUpdateScript','SpawnBurst_Instantaneous','Spawn Count','/Script/Niagara.NiagaraInt32',{'value':1},label+'-count')
   for parameter,kind,data in [
    ('Color','/Script/CoreUObject.LinearColor',{'r':1.,'g':1.,'b':1.,'a':1.}),
    ('Lifetime Min','/Script/Niagara.NiagaraFloat',{'value':life}),
    ('Lifetime Max','/Script/Niagara.NiagaraFloat',{'value':life}),
    ('Sprite Size Min','/Script/CoreUObject.Vector2f',{'x':width,'y':height}),
    ('Sprite Size Max','/Script/CoreUObject.Vector2f',{'x':width,'y':height})]:
    set_input(c,path,'ParticleSpawnScript','InitializeParticle',parameter,kind,data,label+'-'+parameter.replace(' ','-'))
   set_input(c,path,'ParticleSpawnScript','AddVelocityInCone','Velocity Strength','/Script/Niagara.NiagaraFloat',{'value':0.},label+'-stationary')
   for module in ['GravityForce','Drag','ScaleSpriteSizeBySpeed']:
    c.niagara('SetModuleEnabled',{'moduleRef':ref(path,'DirectionalBurst','ParticleUpdateScript',module),'bEnabled':False},label+'-disable-'+module)
   if 'ConfirmedImpact' in name:
    added=c.niagara('AddModule',{'moduleLocationRef':ref(path,'DirectionalBurst','ParticleUpdateScript'),'moduleAsset':{'refPath':'/Niagara/Modules/Update/Camera/CameraOffset.CameraOffset'}},label+'-camera-offset-add')
    set_input(c,path,'ParticleUpdateScript',added['moduleName'],'Camera Offset Amount','/Script/Niagara.NiagaraFloat',{'value':24.},label+'-camera-offset-set')
   c.niagara('SetEmitterData',{'emitter':ref(path,'DirectionalBurst'),'emitterData':{'propertyValues':json.dumps({'bLocalSpace':local_space})}},label+'-space')
   c.niagara('GetRendererData',{'rendererRef':ref(path,'DirectionalBurst',index=0)},label+'-renderer-before')
   renderer_values={'Material':{'refPath':object_path(DEST+'/'+material)},'Alignment':'Unaligned','FacingMode':'FaceCamera','bCastShadows':False,'SortMode':'None'}
   c.niagara('SetRendererData',{'renderer':ref(path,'DirectionalBurst',index=0),'rendererData':{'propertyValues':json.dumps(renderer_values)}},label+'-renderer-set')
  summary=c.niagara('GetSystemSummary',{'system':{'refPath':object_path(path)}},label+'-summary');require_finite_source(summary)
  lifecycle=c.niagara('GetModuleInputValues',{'moduleRef':ref(path,'','SystemUpdateScript','SystemState')},label+'-lifecycle')
  loop=next((r for r in lifecycle.get('inputs',[]) if r.get('name')=='Loop Behavior'),{})
  if loop.get('value',{}).get('value',{}).get('enumName')!='ENiagara_EmitterStateOptions::NewEnumerator1':raise RuntimeError('System not finite '+str(lifecycle))
  for attempt in range(20):
   state=c.niagara('GetSystemCompileState',{'system':{'refPath':object_path(path)}},label+'-compile-'+str(attempt))
   if not state.get('bIsCompiling') and not state.get('bIsStale'):break
   time.sleep(.5)
  if state.get('bIsCompiling') or state.get('bIsStale'):raise RuntimeError('Compile still in flight after bounded read polling; leave editor open and inspect')
  issues=c.niagara('GetStackIssues',{'system':{'refPath':object_path(path)}},label+'-issues')
  if state.get('bHasErrors') or state.get('bIsStale') or state.get('bIsCompiling') or state.get('aggregateStatus') not in ('UpToDate','UpToDateWithWarnings') or issues.get('numErrors',1)>0:raise RuntimeError('Compile/stack gate failed '+str(state)+' '+str(issues))
  c.niagara('GetEmitterTopology',{'emitterRef':ref(path,'DirectionalBurst')},label+'-final-topology')
  c.niagara('GetEmitterInputValues',{'emitterRef':ref(path,'DirectionalBurst')},label+'-final-inputs')
  renderer=c.niagara('GetRendererData',{'rendererRef':ref(path,'DirectionalBurst',index=0)},label+'-renderer-final')
  report.append({'asset':path,'source':SOURCE,'material':DEST+'/'+material,'count':1,'lifetime_sim_seconds':life,'internal_size_cm':[width,height],'local_space':local_space,'compile':state,'issues':issues,'lifecycle':lifecycle,'renderer':renderer})
  print('AUTHORED',path,flush=True)
 after_hash=hashlib.sha256(source_file.read_bytes()).hexdigest()
 if source_hash!=after_hash:raise RuntimeError('Source package changed unexpectedly')
 (OUT/'source-preservation.json').write_text(json.dumps({'source':SOURCE,'before_sha256':source_hash,'after_sha256':after_hash,'preserved':True},indent=2),encoding='utf-8')
 (OUT/'validated-systems.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_FANTASY_NIAGARA_AUTHORED systems=4 unactivated=true')
if __name__=='__main__':main()




