"""Prepare or serially bind a single Staff VFX pilot using official Unreal MCP.

Default prepare mode is entirely offline. Live modes require the coordinator's
exclusive editor lease. No editor/build is launched, no gameplay C++ is changed,
and only one explicitly scoped DataAsset may be written or saved. Failures never
trigger automatic mutation retry or rollback.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Scripts'))
from MagicSkillVfxBinding20261111 import build_binding_plan,audit_transition,validate_plan,normalize_object_ref,REQUIRED_SNAPSHOT_FIELDS,REFERENCE_FIELDS
A='editor_toolset.toolsets.asset.AssetTools'
O='editor_toolset.toolsets.object.ObjectTools'
N='NiagaraToolsets.NiagaraToolset_System'
E='EditorToolset.EditorAppToolset'
S='ToolsetRegistry.AgentSkillToolset'
EVIDENCE=ROOT/'Feature/doc/evidence/magic-skill-vfx-binding-20261111'
CANONICAL_TO_NATIVE={field:field.split('_')[0]+''.join(part.title() for part in field.split('_')[1:])
                     for field in REQUIRED_SNAPSHOT_FIELDS|REFERENCE_FIELDS}
CANONICAL_TO_NATIVE['enable_attack_effects']='bEnableAttackEffects'


def audit_view(properties):
    inverse={native:canonical for canonical,native in CANONICAL_TO_NATIVE.items()}
    result={inverse.get(key,key):value for key,value in properties.items()}
    if len(result)!=len(properties):raise ValueError('Colliding native/canonical property aliases')
    return result


def native_updates(updates):
    return {CANONICAL_TO_NATIVE[key]:({'refPath':value} if key in REFERENCE_FIELDS and value else value)
            for key,value in updates.items()}


def audit_native(before,after,updates):return audit_transition(audit_view(before),audit_view(after),updates)


def decode(value):
    for _ in range(5):
        if not isinstance(value,str):return value
        try:value=json.loads(value)
        except ValueError:return value
    raise ValueError('Excess nested MCP JSON')


def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,ensure_ascii=False,allow_nan=False).encode('utf-8')).hexdigest()
def file_sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def object_path(path):return path+'.'+path.rsplit('/',1)[-1]


def package_file(path):
    if not re.fullmatch(r'/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+',path):raise ValueError('Unsafe package path')
    result=ROOT/'Content'/(path[6:]+'.uasset')
    if not result.resolve().is_relative_to((ROOT/'Content').resolve()):raise ValueError('Package escapes project')
    return result


def fingerprints(plan):
    packages=[r['asset'] for r in plan['targets']]+[plan['profile_asset'],plan['character_asset']]+list(plan['candidate_assets'])
    packages+=['/Game/Rogue10m/VFX/Character/Combat/MagicCandidates20261110/MagicMcp01/'+p.rsplit('/',1)[-1].replace('NS_','M_',1) for p in plan['candidate_assets']]
    paths=[package_file(p) for p in packages]
    paths += [ROOT/p for p in ['Rogue10m.uproject','Config/DefaultGame.ini',
        'Source/Rogue10m/Components/Rogue10mCombatComponent.cpp','Source/Rogue10m/Components/Rogue10mCombatComponent.h',
        'Source/Rogue10m/Data/Rogue10mAttackSkillData.h','Scripts/MagicSkillVfxBinding20261111.py',
        'Scripts/Editor/BindMagicSkillVfxMCP20261111.py']]
    return {p.relative_to(ROOT).as_posix():file_sha(p) for p in paths}


class NativeClient:
    def __init__(self,url,out):
        from InspectWeaponFantasyVFX20261110 import Client
        from AuthorWeaponSkillParticles20261110 import validate_schema
        self.client=Client(url,out);self.schemas={};self.sequence=0;self.last_action=None;self.validate=validate_schema
        self.client.tool('list_toolsets',{},'000-toolsets')
        for i,ts in enumerate([A,O,N,E,S]):
            desc=decode(self.client.tool('describe_toolset',{'toolset_name':ts},f'001-schema-{i}'))
            for tool in desc['tools']:self.schemas[(ts,tool['name'].rsplit('.',1)[-1])]=tool['inputSchema']
    def call(self,ts,name,args):
        if (ts,name) not in self.schemas:raise RuntimeError('Missing native tool '+name)
        self.validate(args,self.schemas[(ts,name)])
        self.sequence+=1;self.last_action={'toolset':ts,'tool':name,'arguments':args}
        return decode(self.client.tool('call_tool',{'toolset_name':ts,'tool_name':name,'arguments':args},f'{self.sequence:04d}-{name}'))


def all_properties(c,path):
    ref={'refPath':object_path(path)}
    fields=c.call(O,'list_properties',{'instance':ref})
    if not isinstance(fields,dict) or not fields:raise RuntimeError('No reflected property map '+path)
    values=c.call(O,'get_properties',{'instance':ref,'properties':sorted(fields)})
    if not isinstance(values,dict) or set(values)!=set(fields):raise RuntimeError('Incomplete full-property snapshot '+path)
    return values


def contains_reference(value,path):
    if isinstance(value,dict):return any(contains_reference(v,path) for v in value.values())
    if isinstance(value,list):return any(contains_reference(v,path) for v in value)
    return isinstance(value,str) and value in (path,object_path(path))


def normalized_guid(value):
    if isinstance(value,str):
        cleaned=re.sub(r'[-{}\s]','',value)
        if re.fullmatch('[0-9A-Fa-f]{32}',cleaned):return cleaned.lower()
    if isinstance(value,dict) and {k.lower() for k in value}=={'a','b','c','d'}:
        lowered={k.lower():v for k,v in value.items()}
        return ''.join(f'{int(lowered[k])&0xffffffff:08x}' for k in 'abcd')
    raise RuntimeError('Unknown reflected project GUID representation')


def project_identity(c):
    ini=(ROOT/'Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
    match=re.search(r'\[/Script/EngineSettings.GeneralProjectSettings\]\s*\nProjectID=([0-9A-Fa-f]{32})',ini)
    if not match:raise RuntimeError('Project GUID missing from local settings')
    ref={'refPath':'/Script/EngineSettings.Default__GeneralProjectSettings'}
    fields=c.call(O,'list_properties',{'instance':ref})
    keys=[key for key in fields if key.lower()=='projectid']
    if len(keys)!=1:raise RuntimeError('No unique live project ID field')
    values=c.call(O,'get_properties',{'instance':ref,'properties':keys})
    if normalized_guid(values[keys[0]])!=match.group(1).lower():raise RuntimeError('Wrong live project')
    return {'project_guid':match.group(1).lower(),'local_project_root':str(ROOT)}


def inspect_state(c,plan):
    if c.call(E,'IsPIERunning',{}) is not False:raise RuntimeError('Stop PIE before binding inspection')
    skills=c.call(S,'ListSkills',{})
    if not isinstance(skills,dict):raise RuntimeError('Project skill schema drift')
    skill_text=c.call(S,'GetSkills',{'skillPaths':list(skills)}) if skills else {}
    states={}
    classes={}
    for row in plan['targets']+[
            {'asset':plan['profile_asset'],'role':'profile'},
            {'asset':plan['character_asset'],'role':'character'}]:
        path=row['asset']
        if c.call(A,'is_dirty',{'asset_path':path}) is not False:raise RuntimeError('Unsaved target/protected asset '+path)
        klass=c.call(A,'get_asset_class',{'asset_path':path})
        expected={'pilot':'Rogue10mAttackSkillData','protected':'Rogue10mAttackSkillData',
                  'profile':'Rogue10mWeaponSkillProfileDataAsset','character':'Rogue10mCharacterDataAsset'}[row['role']]
        if not isinstance(klass,str) or klass.rsplit('.',1)[-1]!=expected:raise RuntimeError('Unexpected class '+path+': '+str(klass))
        if row['role']=='pilot' and c.call(A,'can_edit_asset',{'asset_path':path}) is not True:raise RuntimeError('Pilot asset locked')
        states[path]=all_properties(c,path);classes[path]=klass
    profile=states[plan['profile_asset']]
    if profile.get('weaponType')!='Staff':raise RuntimeError('Profile is not Staff')
    if not contains_reference(profile.get('skillTreeSkills'),plan['pilot_asset']):raise RuntimeError('Pilot absent from Staff tree')
    bindings=profile.get('defaultSkillBindings')
    # Official reflected structs use lowerCamel; accept the legacy native alias
    # only when exactly one Primary key exists, never a nested/ambiguous match.
    primary_keys=[key for key in bindings if isinstance(key,str) and key.lower()=='primary'] if isinstance(bindings,dict) else []
    if len(primary_keys)!=1 or primary_keys[0] not in ('primary','Primary'):raise RuntimeError('Pilot is not the Staff default Primary binding: missing/ambiguous key')
    try:primary_ref=normalize_object_ref(bindings[primary_keys[0]])
    except ValueError as exc:raise RuntimeError('Pilot is not the Staff default Primary binding: invalid reference') from exc
    if primary_ref!=object_path(plan['pilot_asset']):raise RuntimeError('Pilot is not the Staff default Primary binding')
    if not contains_reference(states[plan['character_asset']].get('weaponSkillProfiles'),plan['profile_asset']):raise RuntimeError('Default character does not reference Staff profile')
    compiles={}
    for path in plan['candidate_assets']:
        if c.call(A,'is_dirty',{'asset_path':path}) is not False:raise RuntimeError('Candidate is unsaved '+path)
        if c.call(A,'get_asset_class',{'asset_path':path})!='NiagaraSystem':raise RuntimeError('Candidate is not Niagara')
        ref={'system':{'refPath':object_path(path)}}
        compile_state=c.call(N,'GetSystemCompileState',ref)
        issues=c.call(N,'GetStackIssues',ref)
        if any(compile_state.get(k,True) for k in ['bIsCompiling','bIsStale','bHasErrors']) or compile_state.get('aggregateStatus')!='UpToDate' or issues.get('numErrors',1)!=0 or issues.get('numWarnings',1)!=0:raise RuntimeError('Candidate compile gate failed '+path)
        compiles[path]={'compile':compile_state,'issues':issues}
    pilot=states[plan['pilot_asset']]
    updates=next(row['updates'] for row in plan['targets'] if row['role']=='pilot')
    if set(native_updates(updates))-pilot.keys():raise RuntimeError('Missing reflected pilot tuning fields')
    return {'properties':states,'classes':classes,'candidate_compiles':compiles,
            'project_skills':skill_text,'project_skills_sha256':digest(skill_text)}


def equivalent(a,b):
    if isinstance(a,dict) and set(a)=={'refPath'}:a=a['refPath']
    if isinstance(b,dict) and set(b)=={'refPath'}:b=b['refPath']
    if isinstance(a,str) and isinstance(b,str) and a.startswith('/Game/') and b.startswith('/Game/'):return a.split('.',1)[0]==b.split('.',1)[0]
    if isinstance(a,(float,int)) and not isinstance(a,bool) and isinstance(b,(float,int)) and not isinstance(b,bool):return math.isclose(a,b,abs_tol=1e-6,rel_tol=1e-6)
    return type(a)==type(b) and a==b


def apply_inspected_state(c,plan,baseline,report):
    """Single mutation dispatch at most. Full-property audit precedes scoped save."""
    current=inspect_state(c,plan)
    if current['properties']!=baseline['properties']:raise RuntimeError('Stale property snapshot; inspect again')
    if current['project_skills_sha256']!=baseline['project_skills_sha256']:raise RuntimeError('Project skills changed; inspect and review again')
    pilot=plan['pilot_asset'];before=current['properties'][pilot]
    updates=next(row['updates'] for row in plan['targets'] if row['role']=='pilot')
    native=native_updates(updates)
    # Reject known forbidden transitions BEFORE touching the live object.
    proposed=copy.deepcopy(before);proposed.update(native)
    audit_native(before,proposed,updates)
    remaining={k:v for k,v in native.items() if not equivalent(before[k],v)}
    report.update(mutation_attempted=False,asset_saved=False,already_matches=not remaining)
    if remaining:
        report['mutation_attempted']=True
        report['data_asset_connected']=None
        if c.call(O,'set_properties',{'instance':{'refPath':object_path(pilot)},'values':json.dumps(remaining,allow_nan=False)}) is not True:raise RuntimeError('Setter returned failure/ambiguous; read state before any retry')
    after={path:all_properties(c,path) for path in current['properties']}
    for row in plan['targets']:audit_native(current['properties'][row['asset']],after[row['asset']],row['updates'])
    for path in [plan['profile_asset'],plan['character_asset']]:
        if after[path]!=current['properties'][path]:raise RuntimeError('Protected profile/character changed')
    report['properties_after']=after
    report['nonvisual_fields_preserved']=True
    report['data_asset_connected_in_memory']=True
    if remaining:
        report['save_attempted']=True
        if c.call(A,'save_assets',{'asset_paths':[pilot]}) is not True:raise RuntimeError('Scoped save failed/ambiguous; inspect before any retry')
        report['asset_saved']=True
        report['pilot_saved_sha256']=file_sha(package_file(pilot))
    if c.call(A,'is_dirty',{'asset_path':pilot}) is not False:raise RuntimeError('Pilot remains dirty after scoped save')
    if all_properties(c,pilot)!=after[pilot]:raise RuntimeError('Properties changed after save')
    report.update(status='pilot_visual_binding_saved_same_session_readback_pending_gameplay_qa',
                  data_asset_connected=True,runtime_gameplay_validated=False,cold_reload_validated=False)
    return report


def validate_applied_artifact(plan,inspect_sha,applied,current_hashes):
    if applied.get('phase')!='apply' or applied.get('status')!='pilot_visual_binding_saved_same_session_readback_pending_gameplay_qa' or applied.get('data_asset_connected') is not True:
        raise RuntimeError('Successful apply report required for pinned readback')
    if applied.get('plan')!=plan or applied.get('inspect_snapshot_sha256')!=inspect_sha:
        raise RuntimeError('Apply report does not belong to this inspected plan')
    if applied.get('fingerprints_after')!=current_hashes:
        raise RuntimeError('Saved artifact changed after apply; cached editor values cannot verify it')


def validate_final_fingerprints(before,after,pilot_key,saved_hash=None):
    if set(before)!=set(after):raise RuntimeError('Tracked file scope changed')
    for key,value in before.items():
        expected=saved_hash if key==pilot_key and saved_hash else value
        if after[key]!=expected:raise RuntimeError('Tracked/protected file changed during operation: '+key)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--phase',choices=['prepare','inspect','apply','verify'],default='prepare')
    p.add_argument('--label',required=True);p.add_argument('--url',default='http://127.0.0.1:8001/mcp')
    p.add_argument('--lease-granted',action='store_true');p.add_argument('--snapshot',type=Path)
    p.add_argument('--snapshot-sha256')
    p.add_argument('--applied-report',type=Path);p.add_argument('--applied-report-sha256')
    a=p.parse_args()
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,47}',a.label):raise ValueError('Fresh simple evidence label required')
    if a.phase!='prepare' and not a.lease_granted:raise RuntimeError('Coordinator exclusive editor lease required; no calls made')
    plan=build_binding_plan(ROOT);validate_plan(plan)
    hashes=fingerprints(plan)
    out=EVIDENCE/a.label
    if out.exists():raise RuntimeError('Preserve evidence: use a fresh label')
    out.mkdir(parents=True)
    report={'phase':a.phase,'plan':plan,'project_root':str(ROOT),'fingerprints_before':hashes,
            'data_asset_connected':False,'runtime_gameplay_validated':False,'cold_reload_validated':False,
            'editor_or_build_launched':False,'mutation_attempted':False,'asset_saved':False}
    c=None
    try:
        if a.phase=='prepare':
            verified=json.loads((ROOT/'Feature/doc/evidence/magic-particle-candidates-20261110/MagicMcp01/final-verification.json').read_text(encoding='utf-8'))
            for row in verified['assets']:
                if file_sha(ROOT/row['path'])!=row['sha256']:raise RuntimeError('Candidate changed after prior render review')
            report.update(status='implementation_prepared_offline_no_editor_calls',mcp_dispatched=False,previous_candidate_review_sha256=file_sha(ROOT/'Feature/doc/evidence/magic-particle-candidates-20261110/MagicMcp01/final-verification.json'))
        else:
            c=NativeClient(a.url,out/'mcp')
            report['identity']=project_identity(c);report['mcp_dispatched']=True
            if a.phase=='inspect':
                report['live']=inspect_state(c,plan)
                report.update(status='fresh_live_readonly_snapshot_review_project_skills_before_apply',snapshot_sha256=digest(report['live']))
            else:
                if not a.snapshot or not a.snapshot_sha256 or file_sha(a.snapshot)!=a.snapshot_sha256:raise RuntimeError('Reviewed snapshot file and exact SHA256 required')
                snapshot=json.loads(a.snapshot.read_text(encoding='utf-8'))
                expected=snapshot.get('fingerprints_before',{})
                allowed={package_file(plan['pilot_asset']).relative_to(ROOT).as_posix()} if a.phase=='verify' else set()
                same_files=set(expected)==set(hashes) and all(hashes[k]==v for k,v in expected.items() if k not in allowed)
                if snapshot.get('phase')!='inspect' or snapshot.get('status')!='fresh_live_readonly_snapshot_review_project_skills_before_apply' or snapshot.get('plan')!=plan or not same_files:raise RuntimeError('Snapshot plan/files changed; inspect again')
                report['inspect_snapshot_sha256']=a.snapshot_sha256
                if a.phase=='apply':apply_inspected_state(c,plan,snapshot['live'],report)
                else:
                    if not a.applied_report or not a.applied_report_sha256 or file_sha(a.applied_report)!=a.applied_report_sha256:raise RuntimeError('Successful applied report and exact SHA256 required for verify')
                    applied=json.loads(a.applied_report.read_text(encoding='utf-8'))
                    validate_applied_artifact(plan,a.snapshot_sha256,applied,hashes)
                    report['applied_report_sha256']=a.applied_report_sha256
                    live=inspect_state(c,plan)
                    for row in plan['targets']:audit_native(snapshot['live']['properties'][row['asset']],live['properties'][row['asset']],row['updates'])
                    for path in [plan['profile_asset'],plan['character_asset']]:
                        if live['properties'][path]!=snapshot['live']['properties'][path]:raise RuntimeError('Protected profile/character changed')
                    report.update(status='pilot_binding_readback_verified_gameplay_qa_not_performed',data_asset_connected=True,live=live)
        report['fingerprints_after']=fingerprints(plan)
        pilot_key=package_file(plan['pilot_asset']).relative_to(ROOT).as_posix()
        validate_final_fingerprints(hashes,report['fingerprints_after'],pilot_key,report.get('pilot_saved_sha256') if report['asset_saved'] else None)
        report['protected_files_preserved']=True
    except Exception as exc:
        report.update(status='stopped_no_automatic_retry',error=str(exc),last_action=c.last_action if c else None,
                      uncertain_mutation_outcome_requires_readonly_inspection=report['mutation_attempted'])
        raise
    finally:
        (out/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    print('RESULT='+report['status']+' evidence='+str(out),flush=True)


if __name__=='__main__':main()
