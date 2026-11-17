import sys,json,time,hashlib
from pathlib import Path
P=Path(r'D:/Project/Rogue10m');sys.path.insert(0,str(P/'Scripts/Editor'))
from InspectWeaponFantasyVFX20261110 import Client,object_path,ref
O=P/'Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1/mcp-vfx';c=Client('http://127.0.0.1:8001/mcp',O)
A='editor_toolset.toolsets.asset.AssetTools';N='NiagaraToolsets.NiagaraToolset_System';E='EditorToolset.EditorAppToolset'
def call(ts,name,args,label):return c.tool('call_tool',{'toolset_name':ts,'tool_name':name,'arguments':args},label)
source='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/NS_GreatSwordHeavy_Acceptance';dest='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/ResumeSword1/NS_GreatSwordSlash_Candidate'
sf=P/'Content/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/NS_GreatSwordHeavy_Acceptance.uasset';before=hashlib.sha256(sf.read_bytes()).hexdigest()
summary=call(N,'GetSystemSummary',{'system':{'refPath':object_path(source)}},'source-summary')
if len(summary.get('emitters',[]))!=1:raise RuntimeError('Expected finite one-emitter source')
exists=call(A,'exists',{'path':dest},'candidate-before')
if exists:raise RuntimeError('Candidate exists; inspect without mutation retry')
created=call(N,'CreateNiagaraSystem',{'assetName':'NS_GreatSwordSlash_Candidate','assetPath':dest.rsplit('/',1)[0],'templateSystem':{'refPath':object_path(source)}},'create')
if created.get('refPath')!=object_path(dest):raise RuntimeError('Uncertain create outcome; inspect before retry')
for label,kind,value in [('Lifetime Min','/Script/Niagara.NiagaraFloat',{'value':.18}),('Lifetime Max','/Script/Niagara.NiagaraFloat',{'value':.18}),('Sprite Size Min','/Script/CoreUObject.Vector2f',{'x':110.,'y':34.}),('Sprite Size Max','/Script/CoreUObject.Vector2f',{'x':110.,'y':34.})]:
 call(N,'SetStackInputData',{'stackInputRef':ref(dest,'DirectionalBurst','ParticleSpawnScript','InitializeParticle',names=[label]),'inputData':{'struct':{'refPath':kind},'value':value}},label.replace(' ','-'))
call(E,'OpenEditorForAsset',{'assetPath':dest},'open-editor')
for i in range(20):
 state=call(N,'GetSystemCompileState',{'system':{'refPath':object_path(dest)}},'compile-'+str(i))
 if not state.get('bIsCompiling') and not state.get('bIsStale'):break
 time.sleep(.5)
issues=call(N,'GetStackIssues',{'system':{'refPath':object_path(dest)}},'stack-issues')
if state.get('bHasErrors') or state.get('bIsCompiling') or state.get('bIsStale') or issues.get('numErrors',1)>0:raise RuntimeError('Compile gate failed; do not save')
inputs=call(N,'GetEmitterInputValues',{'emitterRef':ref(dest,'DirectionalBurst')},'final-inputs')
call(A,'update_metadata_tags',{'asset_path':dest,'set_tags':{'Rogue10m.Source':source,'Rogue10m.Status':'Unactivated slash cue candidate; needs rendered motion timing review','Rogue10m.Authoring':'UE5.8 official MCP; finite one-sprite local-space prototype'}},'metadata')
if not call(A,'save_assets',{'asset_paths':[dest]},'save'):raise RuntimeError('Save failed')
after=hashlib.sha256(sf.read_bytes()).hexdigest()
if before!=after:raise RuntimeError('Source preservation failed')
(O/'summary.json').write_text(json.dumps({'created':dest,'source':source,'source_preserved':True,'compile':state,'issues':issues,'lifetime_sim_seconds':.18,'size_cm':[110,34],'activated':False,'requires':'actual swing/contact rendered review'},indent=2),encoding='utf-8')
print('RESULT=MCP_GREATSWORD_SLASH_CANDIDATE_SAVED unactivated=true source_preserved=true')