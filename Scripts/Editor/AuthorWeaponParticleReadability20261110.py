"""Author ONE isolated gain8 MIC + GreatSword Primary01 Cast NS prototype via official MCP.
Requires exclusive lease and reviewed project skills. Never retries mutations.
No original/candidate/DA/Magic edits; no gameplay activation; visual approval pending.
"""
from __future__ import annotations
import argparse, json, math, re, time
from pathlib import Path
from AuthorWeaponSkillParticles20261110 import (CheckedClient, A, E, S, BASE, EVIDENCE,
    ROOT, object_path, ref, digest, hashes, package_file, snapshot, numeric_equal, validate_schema)
M='editor_toolset.toolsets.material_instance.MaterialInstanceTools'
DEFAULT_MANIFEST=EVIDENCE/'particle-candidates/SkillParticlesA/manifest.json'
PARENT=BASE+'/M_GreatSwordHeavy_Acceptance'
MAGIC='/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01'

def validate_manifest(m):
    rows=m['candidates']
    if len(rows)!=30 or len({r['asset'] for r in rows})!=30: raise RuntimeError('Expected original 30 candidates')
    expected={(w,s,p) for w in ('GreatSword','Knuckle') for s in ('Primary01','Primary02','Primary03','Special','Charged') for p in ('Cast','Trajectory','Impact')}
    if {(r['weapon'],r['skill'],r['phase']) for r in rows}!=expected: raise RuntimeError('Manifest scope drift')
    if any(not r['saved'] or r['activated'] or r['compile']['bHasErrors'] or r['issues']['numErrors'] for r in rows): raise RuntimeError('Original candidate gate failed')
    return next(r for r in rows if (r['weapon'],r['skill'],r['phase'])==('GreatSword','Primary01','Cast'))

def protected_paths(m):
    sources=set(m['source_hashes_before'])
    if len(sources)!=4: raise RuntimeError('Expected four original NS')
    materials={p.rsplit('/',1)[0]+'/M_'+p.rsplit('/',1)[1][3:] for p in sources}
    magic_files=list(package_file(MAGIC+'/placeholder').parent.glob('*.uasset'))
    if len(magic_files)!=8: raise RuntimeError('Expected eight Magic packages; review changed scope')
    magic={'/Game/'+f.relative_to(ROOT/'Content').with_suffix('').as_posix() for f in magic_files}
    paths=sources|materials|{r['asset'] for r in m['candidates']}|{r['existing_skill_asset'] for r in m['candidates']}|magic
    if len(paths)!=56: raise RuntimeError('Expected 56 protected packages')
    return sorted(paths)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url',required=True); p.add_argument('--label',required=True)
    p.add_argument('--manifest',type=Path,default=DEFAULT_MANIFEST)
    p.add_argument('--lease-granted',action='store_true'); p.add_argument('--inspect-only',action='store_true')
    p.add_argument('--reviewed-skills-sha256'); a=p.parse_args()
    if not a.lease_granted: raise RuntimeError('Exclusive editor lease required')
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,47}',a.label): raise RuntimeError('Simple fresh label required')
    m=json.loads(a.manifest.read_text(encoding='utf-8-sig')); source=validate_manifest(m)
    protected=protected_paths(m); before=hashes(protected)
    if any(before[path]!=value for path,value in m['source_hashes_before'].items()): raise RuntimeError('Original NS hash mismatch')
    dest=BASE+'/ReadabilityCandidates/'+a.label; out=EVIDENCE/'particle-candidates'/a.label
    mic=dest+'/MI_GreatSword_Primary01_Cast_Gain8'; ns=dest+'/NS_GreatSword_Primary01_Cast_Gain8'
    if out.exists() or package_file(ns).parent.exists(): raise RuntimeError('Fresh label only; no resume')
    out.mkdir(parents=True,exist_ok=False)
    report={'status':'started','scope':'one isolated GreatSword Primary01 Cast gain8 prototype',
        'source_manifest':str(a.manifest.resolve()),'source_manifest_sha256':digest(m),'source_asset':source['asset'],
        'source_candidate':source,'parent_material':PARENT,'rgb_gain':8,'alpha_preserved':True,
        'activated':False,'preview_pending':True,'gameplay_modified':False,'created_paths':[],
        'protected_hashes_before':before,'saved':False}
    c=None
    def write(): (out/'manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    try:
        c=CheckedClient(a.url,out)
        desc=c.client.tool('describe_toolset',{'toolset_name':M},'002-material-instance-schema')
        for t in desc['tools']: c.schemas[(M,t['name'].rsplit('.',1)[-1])]=t['inputSchema']
        listed=c.call(S,'ListSkills',{}); skills=c.call(S,'GetSkills',{'skillPaths':list(listed)}) if listed else {}
        sh=digest(skills); report['project_skills_sha256']=sh; print('PROJECT_SKILLS_SHA256='+sh,flush=True)
        if a.inspect_only: report['status']='inspection_only_read_skills_before_authoring'; return
        if sh!=a.reviewed_skills_sha256: raise RuntimeError('Read project skills and provide current SHA256; no mutation yet')
        for path in (mic,ns):
            if c.call(A,'exists',{'path':path}) is not False: raise RuntimeError('Destination exists or ambiguous')
        original=snapshot(c,source['asset']); report['source_readback']=original
        renderer=json.loads(original['renderer']['propertyValues'])
        if renderer['Material']!={'refPath':object_path(PARENT)}: raise RuntimeError('Unexpected source material')
        rs=c.niagara('GetRendererSchema',{'rendererClass':{'refPath':'/Script/Niagara.NiagaraSpriteRendererProperties'}})
        patch={'Material':{'refPath':object_path(mic)}}
        renderer_schema=json.loads(rs['propertySchema'])
        # UE5.8 returns a flat lowerCamel property map, unlike GetRendererData's native names.
        renderer_properties=renderer_schema.get('properties',renderer_schema)
        material_schema=renderer_properties.get('material',renderer_properties.get('Material'))
        if not isinstance(material_schema,dict) or material_schema.get('type')!='object' or 'refPath' not in material_schema.get('properties',{}): raise RuntimeError('Renderer Material reference schema drift')
        validate_schema(patch['Material'],material_schema)
        params=c.call(M,'list_parameters',{'material':{'refPath':object_path(PARENT)}})
        if not any(v['name']=='CueTint' and v['type']=='Vector' for v in params): raise RuntimeError('CueTint vector missing')
        made=c.call(M,'create',{'folder_path':dest,'asset_name':mic.rsplit('/',1)[1],'parent':{'refPath':object_path(PARENT)}})
        if made!={'refPath':object_path(mic)}: raise RuntimeError('Uncertain MIC creation; no retry')
        report['created_paths'].append(mic); write()
        inherited=c.call(M,'get_vector_parameter',{'instance':made,'name':'CueTint'})
        if set(inherited)!={'r','g','b','a'} or not all(math.isfinite(v) and v>=0 for v in inherited.values()): raise RuntimeError('Invalid inherited tint')
        tint={k:v*(8 if k!='a' else 1) for k,v in inherited.items()}
        report.update(inherited_cue_tint=inherited,requested_cue_tint=tint)
        c.call(M,'set_vector_parameter',{'instance':made,'name':'CueTint','value':tint})
        actual=c.call(M,'get_vector_parameter',{'instance':made,'name':'CueTint'})
        if not numeric_equal(actual,tint): raise RuntimeError('CueTint readback failed')
        report['actual_cue_tint']=actual
        made_ns=c.niagara('CreateNiagaraSystem',{'assetName':ns.rsplit('/',1)[1],'assetPath':dest,'templateSystem':{'refPath':object_path(source['asset'])}})
        if made_ns!={'refPath':object_path(ns)}: raise RuntimeError('Uncertain NS creation; no retry')
        report['created_paths'].append(ns); write()
        c.niagara('SetRendererData',{'renderer':ref(ns,'DirectionalBurst',index=0),'rendererData':{'propertyValues':json.dumps(patch)}})
        if c.call(E,'OpenEditorForAsset',{'assetPath':ns}) is False: raise RuntimeError('Open editor failed')
        for _ in range(40):
            state=c.niagara('GetSystemCompileState',{'system':{'refPath':object_path(ns)}})
            if not state.get('bIsCompiling') and not state.get('bIsStale'): break
            time.sleep(.5)
        issues=c.niagara('GetStackIssues',{'system':{'refPath':object_path(ns)}})
        report.update(compile=state,issues=issues)
        if state.get('bHasErrors',True) or state.get('bIsStale',True) or state.get('bIsCompiling',True) or state.get('aggregateStatus') not in ('UpToDate','UpToDateWithWarnings') or issues.get('numErrors',1): raise RuntimeError('Compile/stack gate failed')
        final=snapshot(c,ns); final_renderer=json.loads(final['renderer']['propertyValues'])
        if final_renderer!={**renderer,**patch}: raise RuntimeError('Renderer changed beyond Material')
        for key in ('all_inputs','lifecycle','emitter'):
            if final[key]!=original[key]: raise RuntimeError('Unrequested candidate change: '+key)
        if snapshot(c,source['asset'])!=original: raise RuntimeError('Original candidate live state changed')
        report.update(readback=final,material_asset=mic,asset=ns)
        if hashes(protected)!=before: raise RuntimeError('Protected package changed before save')
        for path in (mic,ns):
            tags={'Rogue10m.Status':'Isolated gain8 prototype; NOT activated; visual approval pending','Rogue10m.Source':source['asset'],'Rogue10m.ReadabilityGain':'8 RGB; inherited alpha preserved'}
            if c.call(A,'update_metadata_tags',{'asset_path':path,'set_tags':tags}) is False: raise RuntimeError('Metadata failed')
        if c.call(A,'save_assets',{'asset_paths':[mic,ns]}) is not True: raise RuntimeError('Scoped save uncertain; no retry')
        report['created_hashes']=hashes([mic,ns]); report['saved']=True
        report['status']='one_prototype_saved_compile_checked_visual_preview_pending'
    except Exception as exc:
        report.update(status='stopped_no_automatic_retry',error=str(exc),last_action=c.last_action if c else None,uncertain_mutation_outcome_requires_read_only_inspection=True)
        raise
    finally:
        report['protected_hashes_after']=hashes(protected)
        report['protected_assets_unchanged']=report['protected_hashes_after']==before
        if not report['protected_assets_unchanged']: report['status']='failed_protected_package_changed'
        write()

if __name__=='__main__': main()
