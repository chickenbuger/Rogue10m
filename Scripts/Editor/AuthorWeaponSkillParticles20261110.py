"""Official MCP: existing10 presentation candidates; future technique mapping pending.
One finite CPU sprite per candidate. Trajectory is a single afterglow card,
NOT continuous ribbon/blade tracking. No gameplay/DA/VFX binding changes.
Run --inspect-only under lease, read skills, then use printed SHA256 and NEW label.
No execution without --lease-granted; no mutation retry/resume.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, time
from InspectWeaponFantasyVFX20261110 import Client, ROOT, object_path, ref
N='NiagaraToolsets.NiagaraToolset_System'
A='editor_toolset.toolsets.asset.AssetTools'
E='EditorToolset.EditorAppToolset'
S='ToolsetRegistry.AgentSkillToolset'
BASE='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110'
EVIDENCE=ROOT/'Feature/doc/evidence/sword-knuckle-20261110'
PLAN=EVIDENCE/'particle-candidate-plan.json'
FLOAT='/Script/Niagara.NiagaraFloat'
VECTOR='/Script/CoreUObject.Vector2f'

def digest(v): return hashlib.sha256(json.dumps(v,sort_keys=True,ensure_ascii=False).encode()).hexdigest()
def package_file(p):
    if not p.startswith('/Game/') or '.' in p or '..' in p: raise ValueError('Unsafe package path')
    return ROOT/'Content'/(p[6:]+'.uasset')
def hashes(paths): return {p:hashlib.sha256(package_file(p).read_bytes()).hexdigest() for p in paths}

def validate_schema(v,s,where='arguments'):
    if isinstance(v,dict) and 'struct' in v and 'value' in v and 'value' in s.get('properties',{}):
        variants=s['properties']['value'].get('oneOf',[])
        chosen=[o for o in variants if o.get('title')==v.get('struct',{}).get('refPath')]
        if variants and len(chosen)!=1: raise ValueError(where+': unknown instanced struct')
        if chosen: validate_schema(v['value'],chosen[0],where+'.value')
    if 'oneOf' in s:
        for option in s['oneOf']:
            try: validate_schema(v,option,where); return
            except (ValueError,TypeError): pass
        raise ValueError(where+': no reflected variant matches')
    kind=s.get('type'); types={'object':dict,'array':list,'string':str,'boolean':bool,'integer':int,'number':(int,float)}
    if kind in types and (not isinstance(v,types[kind]) or kind in ('integer','number') and isinstance(v,bool)): raise TypeError(where+': type drift')
    if isinstance(v,dict):
        props=s.get('properties',{})
        if set(s.get('required',[]))-v.keys(): raise ValueError(where+': missing required arguments')
        if props and set(v)-props.keys(): raise ValueError(where+': unknown arguments')
        for k,item in v.items():
            if k in props: validate_schema(item,props[k],where+'.'+k)
    if isinstance(v,list):
        for item in v: validate_schema(item,s.get('items',{}),where+'[]')
    if 'enum' in s and v not in s['enum']: raise ValueError(where+': invalid enum')

class CheckedClient:
    def __init__(self,url,out):
        self.client=Client(url,out); self.schemas={}; self.sequence=0; self.last_action=None
        self.client.tool('list_toolsets',{},'000-list-toolsets')
        for i,ts in enumerate((N,A,E,S)):
            desc=self.client.tool('describe_toolset',{'toolset_name':ts},f'001-schema-{i}')
            for t in desc['tools']: self.schemas[(ts,t['name'].rsplit('.',1)[-1])]=t['inputSchema']
    def call(self,ts,name,args):
        if (ts,name) not in self.schemas: raise RuntimeError('Missing live tool '+name)
        validate_schema(args,self.schemas[(ts,name)])
        self.sequence+=1; self.last_action={'toolset':ts,'tool':name,'arguments':args}
        # Exactly one dispatch; any timeout/error terminates run and preserves evidence.
        return self.client.tool('call_tool',{'toolset_name':ts,'tool_name':name,'arguments':args},f'{self.sequence:04d}-{name}')
    def niagara(self,name,args): return self.call(N,name,args)

def module_values(rows,module):
    if isinstance(rows,dict): rows=[rows]
    found=[r for r in rows if r.get('moduleName')==module]
    if len(found)!=1: raise RuntimeError('Expected one module '+module)
    return {r['name']:r['value'] for r in found[0]['inputs']}
def numeric_equal(a,b):
    if isinstance(b,dict): return isinstance(a,dict) and all(k in a and numeric_equal(a[k],v) for k,v in b.items())
    if isinstance(b,(int,float)) and not isinstance(b,bool): return isinstance(a,(int,float)) and math.isclose(a,b,rel_tol=1e-5,abs_tol=1e-5)
    return a==b

def snapshot(c,path):
    summary=c.niagara('GetSystemSummary',{'system':{'refPath':object_path(path)}}); es=summary.get('emitters',[])
    if len(es)!=1 or es[0].get('emitterName')!='DirectionalBurst' or es[0].get('simTarget')!='CPUSim' or not es[0].get('bEnabled') or [r.get('refPath') for r in es[0].get('rendererClasses',[])]!=['/Script/Niagara.NiagaraSpriteRendererProperties']: raise RuntimeError('Expected one enabled CPU sprite emitter')
    topo=c.niagara('GetEmitterTopology',{'emitterRef':ref(path,'DirectionalBurst')})
    spawns=[m['moduleName'] for m in topo['emitterUpdateScript']['modules'] if m.get('enabled') and 'Spawn' in m['moduleName']]
    if spawns!=['SpawnBurst_Instantaneous']: raise RuntimeError('Expected one finite burst module')
    inputs=c.niagara('GetEmitterInputValues',{'emitterRef':ref(path,'DirectionalBurst')})
    if module_values(inputs,'SpawnBurst_Instantaneous')['Spawn Count']['value'].get('value')!=1: raise RuntimeError('Expected one-particle burst')
    lifecycle=c.niagara('GetModuleInputValues',{'moduleRef':ref(path,'','SystemUpdateScript','SystemState')})
    if module_values(lifecycle,'SystemState')['Loop Behavior']['value'].get('enumName')!='ENiagara_EmitterStateOptions::NewEnumerator1': raise RuntimeError('System is not finite Once')
    renderer=c.niagara('GetRendererData',{'rendererRef':ref(path,'DirectionalBurst',index=0)})
    emitter=c.niagara('GetEmitterData',{'emitterRef':ref(path,'DirectionalBurst')})
    return {'summary':summary,'topology':topo,'all_inputs':inputs,'lifecycle':lifecycle,'renderer':renderer,'emitter':emitter}

def desired_inputs(s):
    life={'value':s['lifetime_sim_seconds']}; size=dict(zip(('x','y'),s['size_cm']))
    return {'Lifetime Min':(FLOAT,life),'Lifetime Max':(FLOAT,life),'Sprite Size Min':(VECTOR,size),'Sprite Size Max':(VECTOR,size)}
def validate_plan(plan):
    specs=plan['candidates']
    if len(specs)!=30 or len({s['name'] for s in specs})!=30: raise RuntimeError('Expected 30 unique candidates')
    for w in ('GreatSword','Knuckle'):
        for skill in ('Primary01','Primary02','Primary03','Special','Charged'):
            rows=[s for s in specs if s['weapon']==w and s['skill']==skill]
            if {r['phase'] for r in rows}!={'Cast','Trajectory','Impact'}: raise RuntimeError('Missing three-phase skill spec')
    for s in specs:
        if s['source'] not in plan['sources'] or not 0<s['lifetime_sim_seconds']<=.5: raise ValueError('Invalid source/lifetime')
        if len(s['size_cm'])!=2 or not all(0<v<=300 for v in s['size_cm']): raise ValueError('Invalid size')
        if s['local_space']!=(s['phase']!='Impact'): raise ValueError('Invalid phase space')
        if not re.fullmatch(r'NS_[A-Za-z0-9_]+',s['name']): raise ValueError('Unsafe name')
    return specs

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url',default='http://127.0.0.1:8001/mcp'); p.add_argument('--label',required=True)
    p.add_argument('--lease-granted',action='store_true'); p.add_argument('--inspect-only',action='store_true'); p.add_argument('--reviewed-skills-sha256')
    a=p.parse_args()
    if not a.lease_granted: raise RuntimeError('Exclusive broker lease required before any MCP call')
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,47}',a.label): raise ValueError('Fresh simple label required')
    plan=json.loads(PLAN.read_text(encoding='utf-8-sig')); specs=validate_plan(plan)
    dest=BASE+'/SkillCandidates/'+a.label; out=EVIDENCE/'particle-candidates'/a.label
    if out.exists() or package_file(dest+'/placeholder').parent.exists(): raise RuntimeError('Fresh label required; no resume/retry')
    before=hashes(plan['sources']); out.mkdir(parents=True,exist_ok=False)
    manifest={'status':'started','scope':plan['scope'],'activated':False,'preview_pending':True,'continuous_ribbon_implemented':False,'created_paths':[],'candidates':[],'source_hashes_before':before,'plan_sha256':digest(plan),'gameplay_modified':False}
    c=None
    try:
        c=CheckedClient(a.url,out)
        listed=c.call(S,'ListSkills',{})
        if not isinstance(listed,dict): raise RuntimeError('Project skills schema drift')
        skills=c.call(S,'GetSkills',{'skillPaths':list(listed)}) if listed else {}
        sh=digest(skills); manifest['project_skills_sha256']=sh; print('PROJECT_SKILLS_SHA256='+sh,flush=True)
        if a.inspect_only:
            manifest['status']='inspection_only_read_project_skills_before_authoring'; return
        if a.reviewed_skills_sha256!=sh: raise RuntimeError('Read current project skills evidence and pass SHA256 with a fresh label; no mutation yet')
        for s in specs:
            if c.call(A,'exists',{'path':dest+'/'+s['name']}) is not False: raise RuntimeError('Destination exists or ambiguous')
        sources={path:snapshot(c,path) for path in plan['sources']}
        for s in specs:
            path=dest+'/'+s['name']
            created=c.niagara('CreateNiagaraSystem',{'assetName':s['name'],'assetPath':dest,'templateSystem':{'refPath':object_path(s['source'])}})
            if not isinstance(created,dict) or created.get('refPath')!=object_path(path): raise RuntimeError('Uncertain create; inspect, never retry blindly')
            manifest['created_paths'].append(path)
            record=dict(s,asset=path,compile=None,issues=None,all_inputs=None,source_sha256=before[s['source']],activated=False,preview_pending=True,saved=False)
            manifest['candidates'].append(record)
            for name,(kind,value) in desired_inputs(s).items():
                c.niagara('SetStackInputData',{'stackInputRef':ref(path,'DirectionalBurst','ParticleSpawnScript','InitializeParticle',names=[name]),'inputData':{'struct':{'refPath':kind},'value':value}})
            c.niagara('SetEmitterData',{'emitter':ref(path,'DirectionalBurst'),'emitterData':{'propertyValues':json.dumps({'bLocalSpace':s['local_space']})}})
            if c.call(E,'OpenEditorForAsset',{'assetPath':path}) is False: raise RuntimeError('Open editor/compile trigger failed')
            for _ in range(40):
                state=c.niagara('GetSystemCompileState',{'system':{'refPath':object_path(path)}})
                if not state.get('bIsCompiling') and not state.get('bIsStale'): break
                time.sleep(.5)
            issues=c.niagara('GetStackIssues',{'system':{'refPath':object_path(path)}})
            record.update(compile=state,issues=issues)
            if state.get('bHasErrors',True) or state.get('bIsStale',True) or state.get('bIsCompiling',True) or state.get('aggregateStatus') not in ('UpToDate','UpToDateWithWarnings') or issues.get('numErrors',1)!=0: raise RuntimeError('Compile/stack gate failed; NOT saved')
            final=snapshot(c,path); actual=module_values(final['all_inputs'],'InitializeParticle')
            for name,(kind,value) in desired_inputs(s).items():
                if actual[name]['struct']['refPath']!=kind or not numeric_equal(actual[name]['value'],value): raise RuntimeError('Readback failed '+name)
            record.update(readback=final,all_inputs=final['all_inputs'])
            source=sources[s['source']]
            if actual['Color']!=module_values(source['all_inputs'],'InitializeParticle')['Color']: raise RuntimeError('Source color changed')
            if json.loads(final['renderer']['propertyValues'])['Material']!=json.loads(source['renderer']['propertyValues'])['Material']: raise RuntimeError('Source material changed')
            if json.loads(final['emitter']['propertyValues'])['bLocalSpace']!=s['local_space']: raise RuntimeError('Space readback failed')
            tags={'Rogue10m.Source':s['source'],'Rogue10m.SourceSHA256':before[s['source']],'Rogue10m.Status':'Unactivated candidate; rendered preview pending','Rogue10m.Skill':s['weapon']+'/'+s['skill'],'Rogue10m.Phase':s['phase'],'Rogue10m.TrajectoryLimit':'Single burst card; not continuous ribbon/blade tracking','Rogue10m.Authoring':'UE5.8 official MCP; one CPU finite one-sprite burst'}
            if c.call(A,'update_metadata_tags',{'asset_path':path,'set_tags':tags}) is False: raise RuntimeError('Metadata failed')
            if c.call(A,'save_assets',{'asset_paths':[path]}) is not True: raise RuntimeError('Scoped save failed/ambiguous; do not retry')
            if not package_file(path).is_file(): raise RuntimeError('Saved package missing')
            record.update(saved=True,metadata=tags)
            if hashes(plan['sources'])!=before: raise RuntimeError('Source changed; stop without automatic restoration')
            (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
        manifest['status']='30_candidates_saved_compile_checked_rendered_preview_pending'
    except Exception as exc:
        manifest.update(status='stopped_no_automatic_retry',error=str(exc),last_action=c.last_action if c else None,uncertain_mutation_outcome_requires_read_only_inspection=True)
        raise
    finally:
        manifest['source_hashes_after']=hashes(plan['sources']); manifest['sources_preserved']=manifest['source_hashes_after']==before
        (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')

if __name__=='__main__': main()
