"""Fresh gain8 MI/NS derivatives for the four ALREADY ACTIVE weapon VFX.
Root editor lease only. No original/candidate/Magic/DA mutation. No mutation retries.
Inspect with one label, review saved tool schemas/skills, author with another label.
"""
from __future__ import annotations
import argparse, json, math, re, sys, time
from pathlib import Path
ROOT = Path(r'D:\Project\Rogue10m')
sys.path.insert(0, str(ROOT/'Scripts/Editor'))
from AuthorWeaponSkillParticles20261110 import (CheckedClient, A, E, S, BASE, EVIDENCE,
    object_path, ref, digest, hashes, package_file, snapshot, numeric_equal, validate_schema)
from AuthorWeaponParticleReadability20261110 import protected_paths, validate_manifest
M = 'editor_toolset.toolsets.material_instance.MaterialInstanceTools'
ORIGINAL_MANIFEST = EVIDENCE/'particle-candidates/SkillParticlesA/manifest.json'
PROTOTYPE_MANIFEST = EVIDENCE/'particle-candidates/ReadabilityGain8B/manifest.json'
CONFIGS = (
    ('KnuckleFlurry', 'Knuckle', 'Primary03', .20, .30),
    ('GreatSwordHeavy', 'GreatSword', 'Charged', .25, .36),
)
STATUS = 'four_active_derivatives_saved_compile_checked_NOT_bound'
VERSION = 'ActiveWeaponReadability20261110_v1'

def protected_inventory(m):
    proto = json.loads(PROTOTYPE_MANIFEST.read_text(encoding='utf-8-sig'))
    paths = set(protected_paths(m)) | {proto['asset'], proto['material_asset'], '/Game/Rogue10m/VFX/Monsters/NS_Monster_IronShell'}
    if len(paths) != 59: raise RuntimeError('Expected 59 protected packages; review scope')
    return sorted(paths)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url', required=True); p.add_argument('--label', required=True)
    p.add_argument('--lease-granted', action='store_true'); p.add_argument('--inspect-only', action='store_true')
    p.add_argument('--reviewed-skills-sha256'); a=p.parse_args()
    if not a.lease_granted: raise RuntimeError('Exclusive root editor lease required')
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,47}', a.label): raise RuntimeError('Fresh simple label required')
    m=json.loads(ORIGINAL_MANIFEST.read_text(encoding='utf-8-sig')); validate_manifest(m)
    protected=protected_inventory(m); before=hashes(protected)
    if any(before[k]!=v for k,v in m['source_hashes_before'].items()): raise RuntimeError('Original source hash changed')
    dest=BASE+'/ActiveReadability/'+a.label; out=EVIDENCE/'active-readability'/a.label
    if out.exists() or package_file(dest+'/placeholder').parent.exists(): raise RuntimeError('Fresh label only, no resume')
    out.mkdir(parents=True, exist_ok=False)
    report={'status':'started','version':VERSION,'label':a.label,'scope':'four derivatives for two existing active skills only',
        'activated':False,'visual_approval':False,'rgb_gain':8,'alpha_preserved':True,
        'dilation_first_person_charge_and_all_gameplay_fields_frozen':True,
        'created_paths':[],'derivatives':[],'protected_hashes_before':before,
        'original_manifest_sha256':digest(m),'created_hashes':{},'saved':False}
    c=None
    def write(): (out/'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    try:
        c=CheckedClient(a.url,out)
        desc=c.client.tool('describe_toolset', {'toolset_name':M}, '002-material-instance-schema')
        for t in desc['tools']: c.schemas[(M,t['name'].rsplit('.',1)[-1])]=t['inputSchema']
        listed=c.call(S,'ListSkills',{}); skills=c.call(S,'GetSkills',{'skillPaths':list(listed)}) if listed else {}
        sh=digest(skills); report['project_skills_sha256']=sh; print('PROJECT_SKILLS_SHA256='+sh,flush=True)
        rs=c.niagara('GetRendererSchema',{'rendererClass':{'refPath':'/Script/Niagara.NiagaraSpriteRendererProperties'}})
        schema=json.loads(rs['propertySchema']); properties=schema.get('properties',schema)
        material_schema=properties.get('material',properties.get('Material'))
        if not isinstance(material_schema,dict) or material_schema.get('type')!='object' or 'refPath' not in material_schema.get('properties',{}): raise RuntimeError('Material reference schema drift')
        if a.inspect_only: report['status']='inspection_only_read_schemas_and_skills'; return
        if sh!=a.reviewed_skills_sha256: raise RuntimeError('Review current skills and provide digest; no mutation yet')
        specs=[]
        for identity,weapon,variant,castscale,impactscale in CONFIGS:
            for phase,suffix,scale in [('cast','Acceptance',castscale),('impact','ConfirmedImpact',impactscale)]:
                name=identity+'_'+suffix
                specs.append({'identity':identity,'weapon':weapon,'variant':variant,'phase':phase,
                    'skill':'/Game/DataAsset/AttackSkill/ClassCombat/'+weapon+'/DA_Attack_'+weapon+'_'+variant,
                    'source':BASE+'/NS_'+name,'parent_material':BASE+'/M_'+name,'asset':dest+'/NS_'+name+'_Gain8',
                    'material_asset':dest+'/MI_'+name+'_Gain8','requested_scale':scale})
        for r in specs:
            for path in (r['material_asset'],r['asset']):
                if c.call(A,'exists',{'path':path}) is not False: raise RuntimeError('Destination exists or uncertain')
        # Read all four baselines before any mutation, not one by one after edits.
        sources={r['source']:snapshot(c,r['source']) for r in specs}
        for r in specs:
            renderer=json.loads(sources[r['source']]['renderer']['propertyValues'])
            if renderer.get('Material')!={'refPath':object_path(r['parent_material'])}: raise RuntimeError('Unexpected parent material')
            params=c.call(M,'list_parameters',{'material':{'refPath':object_path(r['parent_material'])}})
            if not any(v['name']=='CueTint' and v['type']=='Vector' for v in params): raise RuntimeError('CueTint missing')
        report['source_readbacks']=sources; write()
        for r in specs:
            mic=r['material_asset']; ns=r['asset']; patch={'Material':{'refPath':object_path(mic)}}
            validate_schema(patch['Material'],material_schema)
            made=c.call(M,'create',{'folder_path':dest,'asset_name':mic.rsplit('/',1)[1],'parent':{'refPath':object_path(r['parent_material'])}})
            if made!={'refPath':object_path(mic)}: raise RuntimeError('MIC creation uncertain; inspect, no retry')
            report['created_paths'].append(mic); write()
            tint=c.call(M,'get_vector_parameter',{'instance':made,'name':'CueTint'})
            if set(tint)!={'r','g','b','a'} or not all(math.isfinite(v) and v>=0 for v in tint.values()): raise RuntimeError('Invalid inherited tint')
            desired={k:v*(8 if k!='a' else 1) for k,v in tint.items()}
            c.call(M,'set_vector_parameter',{'instance':made,'name':'CueTint','value':desired})
            actual=c.call(M,'get_vector_parameter',{'instance':made,'name':'CueTint'})
            if not numeric_equal(actual,desired) or set(actual)!=set(desired): raise RuntimeError('Tint readback mismatch')
            made_ns=c.niagara('CreateNiagaraSystem',{'assetName':ns.rsplit('/',1)[1],'assetPath':dest,'templateSystem':{'refPath':object_path(r['source'])}})
            if made_ns!={'refPath':object_path(ns)}: raise RuntimeError('NS creation uncertain; inspect, no retry')
            report['created_paths'].append(ns); write()
            c.niagara('SetRendererData',{'renderer':ref(ns,'DirectionalBurst',index=0),'rendererData':{'propertyValues':json.dumps(patch)}})
            if c.call(E,'OpenEditorForAsset',{'assetPath':ns}) is False: raise RuntimeError('Open editor failed')
            for _ in range(40):
                state=c.niagara('GetSystemCompileState',{'system':{'refPath':object_path(ns)}})
                if not state.get('bIsCompiling') and not state.get('bIsStale'): break
                time.sleep(.5)
            issues=c.niagara('GetStackIssues',{'system':{'refPath':object_path(ns)}})
            if state.get('bHasErrors',True) or state.get('bIsStale',True) or state.get('bIsCompiling',True) or state.get('aggregateStatus') not in ('UpToDate','UpToDateWithWarnings') or issues.get('numErrors',1): raise RuntimeError('Compile/stack gate failed')
            final=snapshot(c,ns); original=sources[r['source']]
            if json.loads(final['renderer']['propertyValues'])!={**json.loads(original['renderer']['propertyValues']),**patch}: raise RuntimeError('Renderer changed beyond material')
            for key in ('all_inputs','lifecycle','emitter'):
                if final[key]!=original[key]: raise RuntimeError('Unrequested Niagara change: '+key)
            if hashes(protected)!=before: raise RuntimeError('Protected package changed')
            tags={'Rogue10m.ReadabilityVersion':VERSION,'Rogue10m.Source':r['source'],
                'Rogue10m.Status':'Prepared derivative; actual-game visual acceptance pending',
                'Rogue10m.ReadabilityGain':'8 linear RGB only; inherited alpha preserved',
                'Rogue10m.Phase':r['phase'],'Rogue10m.Skill':r['skill']}
            for path in (mic,ns):
                if c.call(A,'update_metadata_tags',{'asset_path':path,'set_tags':tags}) is False: raise RuntimeError('Metadata failed')
            if c.call(A,'save_assets',{'asset_paths':[mic,ns]}) is not True: raise RuntimeError('Save uncertain; no retry')
            r.update(compile=state,issues=issues,readback=final,inherited_cue_tint=tint,actual_cue_tint=actual,saved=True)
            report['derivatives'].append(r); report['created_hashes'].update(hashes([mic,ns])); write()
            print('SAVED='+ns,flush=True)
        for source,baseline in sources.items():
            if snapshot(c,source)!=baseline: raise RuntimeError('Original live source changed')
        if hashes(protected)!=before: raise RuntimeError('Protected package changed after saves')
        report.update(status=STATUS,saved=True)
    except Exception as exc:
        report.update(status='stopped_no_automatic_retry',error=str(exc),last_action=c.last_action if c else None,uncertain_outcome_requires_inspection=True)
        raise
    finally:
        report['protected_hashes_after']=hashes(protected)
        report['protected_assets_unchanged']=report['protected_hashes_after']==before
        if not report['protected_assets_unchanged']: report['status']='failed_protected_package_changed'
        write()
    print('RESULT=ACTIVE_VFX_DERIVATIVES_PREPARED manifest='+str(out/'manifest.json'),flush=True)
if __name__=='__main__': main()
