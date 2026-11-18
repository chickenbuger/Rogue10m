"""UE editor Python: guarded two-skill activation/validation/rollback.
Call activate(manifest_path, lease_granted=True) exactly once after authoring gates.
No original Niagara/material edits. Only refs and cast/impact scale may change.
All dilation/FP/charge, combat, motion, damage, unlock and input fields are frozen.
"""
from __future__ import annotations
import hashlib, json, math, shutil, sys
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
if ROOT != Path(r'D:\Project\Rogue10m'): raise RuntimeError('Wrong project')
sys.path.insert(0,str(ROOT/'Scripts/Editor'))
from PrepareWeaponFantasyVFX20261110 import snapshot, require, reconcile_reviewed_motion, CONFIGS as ORIGINAL_CONFIGS
BASE='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110'
VERSION='ActiveWeaponReadability20261110_v1'
EVIDENCE=ROOT/'Feature/doc/evidence/sword-knuckle-20261110'
ALLOWED={'cast_effect','impact_effect','cast_effect_scale','impact_effect_scale'}
EXPECTED={
 '/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03':{'cast_effect_scale':.20,'impact_effect_scale':.30},
 '/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Charged':{'cast_effect_scale':.25,'impact_effect_scale':.36},
}

def load(path): return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def write(path, value): Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def package(path):
 if not path.startswith('/Game/') or '.' in path or '..' in path: raise RuntimeError('Unsafe package path')
 return ROOT/'Content'/(path[6:]+'.uasset')
def objectpath(path): return path+'.'+path.rsplit('/',1)[1]
def hashes(paths): return {p:sha(package(p)) for p in paths}
def stable_check(expected,actual):
 if set(expected)!=set(actual): raise RuntimeError('Reflected field coverage changed')
 diffs=[k for k in expected if expected[k]!=actual[k]]
 if diffs: raise RuntimeError('Protected reflected state mismatch: '+str(diffs))
def gate_manifest(path):
 path=Path(path).resolve(); report=load(path)
 if report.get('status')!='four_active_derivatives_saved_compile_checked_NOT_bound' or not report.get('saved') or report.get('activated') or not report.get('protected_assets_unchanged'): raise RuntimeError('Authoring gate failed')
 rows=report['derivatives']
 if len(rows)!=4 or {(r['skill'],r['phase']) for r in rows}!={(s,p) for s in EXPECTED for p in ('cast','impact')}: raise RuntimeError('Wrong exact four derivatives')
 for row in rows:
  if not row.get('saved') or row['compile'].get('bHasErrors',True) or row['compile'].get('bIsStale',True) or row['compile'].get('bIsCompiling',True) or row['issues'].get('numErrors',1): raise RuntimeError('Compile gate failed')
  if row['requested_scale']!=EXPECTED[row['skill']][row['phase']+'_effect_scale']: raise RuntimeError('Scale scope drift')
  for key in ('asset','material_asset'):
   if not row[key].startswith(BASE+'/ActiveReadability/'+report['label']+'/'): raise RuntimeError('Destination scope drift')
   obj=require(row[key])
   if unreal.EditorAssetLibrary.get_metadata_tag(obj,'Rogue10m.ReadabilityVersion')!=VERSION: raise RuntimeError('Missing derivative provenance')
  if not isinstance(require(row['asset']),unreal.NiagaraSystem): raise RuntimeError('Expected Niagara')
 if hashes(report['created_hashes'])!=report['created_hashes']: raise RuntimeError('Derivative package hash changed after authoring')
 return path,report

def baseline():
 rows=load(EVIDENCE/'vfx-authoring/activation.json')
 if len(rows)!=2 or {r['skill'] for r in rows}!=set(EXPECTED): raise RuntimeError('Original baseline scope changed')
 motion=load(EVIDENCE/'greatsword-activation.json')
 result={}
 for r in rows:
  expected,reconciled=reconcile_reviewed_motion(r['skill'],r['after'],motion)
  if '/GreatSword/' in r['skill'] and not reconciled: raise RuntimeError('Reviewed sword motion reconcile required')
  now=snapshot(require(r['skill'])); stable_check(expected,now)
  if len(now)!=len(r['after']) or len(now)<67: raise RuntimeError('Incomplete reflected guard')
  result[r['skill']]={'before':now,'reviewed_motion_reconciled':reconciled}
 return result

def plan(manifest_path):
 path,report=gate_manifest(manifest_path)
 if hashes(report['protected_hashes_before'])!=report['protected_hashes_before']: raise RuntimeError('Protected package drift before activation')
 rows=baseline()
 for skill,row in rows.items():
  updates=dict(EXPECTED[skill])
  for phase in ('cast','impact'):
   updates[phase+'_effect']=objectpath(next(r['asset'] for r in report['derivatives'] if r['skill']==skill and r['phase']==phase))
  row['requested_updates']=updates
  # Verify existing FP budgets, without changing any FP field or claiming FP visual QA.
  for phase,limit in [('cast',.0601),('impact',.1801)]:
   effective=updates[phase+'_effect_scale']*row['before']['first_person_'+phase+'_scale_multiplier']
   if not math.isfinite(effective) or effective>limit: raise RuntimeError('First-person numeric scale budget exceeded')
 return path,report,rows

def apply(skill,updates):
 require(skill).set_editor_properties({k:require(v) if k.endswith('_effect') else v for k,v in updates.items()})
def save(skill):
 if skill not in EXPECTED: raise RuntimeError('Out-of-scope save')
 if not unreal.EditorAssetLibrary.save_loaded_asset(require(skill),False): raise RuntimeError('Save failed or uncertain; inspect before retry')

def activate(manifest_path,lease_granted=False):
 if not lease_granted: raise RuntimeError('Root exclusive lease required')
 path,authored,rows=plan(manifest_path); out=path.parent/'activation.json'; backup=path.parent/'backup'
 if out.exists() or backup.exists(): raise RuntimeError('Fresh activation only; inspect prior result, do not rerun')
 backup.mkdir(exist_ok=False)
 for skill,row in rows.items():
  file=backup/(skill.rsplit('/',1)[1]+'.uasset.before')
  shutil.copy2(package(skill),file)
  if sha(file)!=sha(package(skill)): raise RuntimeError('Backup verification failed')
  row['backup_file']=str(file); row['backup_sha256']=sha(file)
 guard={k:v for k,v in authored['protected_hashes_before'].items() if k not in EXPECTED}
 result={'status':'backed_up_before_mutation','manifest_path':str(path),'manifest_sha256':sha(path),
  'allowed_fields':sorted(ALLOWED),'protected_hashes_before':guard,'skills':rows,
  'save_attempts':[],'saved_skills':[],'visual_approval':False,'first_person_visual_QA':False}
 write(out,result)
 try:
  for skill,row in rows.items():
   apply(skill,row['requested_updates']); after=snapshot(require(skill))
   if set(after)!=set(row['before']): raise RuntimeError('Snapshot field coverage drift')
   changed={k for k in after if after[k]!=row['before'][k]}
   if changed!=ALLOWED: raise RuntimeError('Unexpected changed fields '+str(changed))
   for k,v in row['requested_updates'].items():
    if isinstance(v,float):
     if not math.isclose(after[k],v,rel_tol=1e-6,abs_tol=1e-7): raise RuntimeError('Scale readback mismatch')
    elif after[k]!=v: raise RuntimeError('Effect readback mismatch')
   row.update(after=after,changed_fields=sorted(changed),frozen_field_count=len(after)-len(ALLOWED))
  # Both assets pass all reflected guards BEFORE either package is saved.
  if hashes(guard)!=guard: raise RuntimeError('Protected package drift before saves')
  result['status']='live_readback_passed_before_scoped_saves'; write(out,result)
  for skill in rows:
   result['save_attempts'].append(skill); write(out,result); save(skill)
   result['saved_skills'].append(skill); write(out,result)
  if hashes(guard)!=guard: raise RuntimeError('Protected package drift after saves')
  result.update(status='two_skills_activated_readback_passed_visual_QA_pending',active_package_hashes=hashes(EXPECTED),derivative_hashes=authored['created_hashes'])
 except Exception as exc:
  result.update(status='stopped_inspect_before_any_retry_or_rollback',error=str(exc),uncertain_save_outcome=bool(result['save_attempts']))
  raise
 finally:
  result['protected_hashes_after']=hashes(guard); result['protected_assets_unchanged']=result['protected_hashes_after']==guard
  write(out,result)
 print('RESULT=ACTIVE_VFX_BOUND skills=2 changed=refs_and_cast_impact_scales_only pending=rendered_runtime_QA report='+str(out))

def validate(activation_path):
 data=load(activation_path)
 if data['status']!='two_skills_activated_readback_passed_visual_QA_pending': raise RuntimeError('Incomplete activation')
 for skill,row in data['skills'].items(): stable_check(row['after'],snapshot(require(skill)))
 if hashes(data['protected_hashes_before'])!=data['protected_hashes_before']: raise RuntimeError('Protected package drift')
 if hashes(data['derivative_hashes'])!=data['derivative_hashes']: raise RuntimeError('Derivative package drift')
 if hashes(data['active_package_hashes'])!=data['active_package_hashes']: raise RuntimeError('Active package drift')
 print('RESULT=ACTIVE_VFX_COLD_READBACK_PASSED skills=2 protected_fields=all_except_four_allowed visual_QA=pending')

def rollback(activation_path,lease_granted=False):
 if not lease_granted: raise RuntimeError('Root exclusive lease required')
 path=Path(activation_path).resolve(); data=load(path); target=path.parent/'rollback.json'
 if target.exists(): raise RuntimeError('Rollback already attempted; inspect, no retry')
 if set(data['skills'])!=set(EXPECTED): raise RuntimeError('Wrong rollback scope')
 checks={}
 for skill,row in data['skills'].items():
  if sha(row['backup_file'])!=row['backup_sha256']: raise RuntimeError('Backup mismatch')
  current=snapshot(require(skill))
  if current!=row['before'] and current!=row.get('after'): raise RuntimeError('Current state differs from recorded states; do not clobber changes')
  checks[skill]=current
 guard=data['protected_hashes_before']
 if hashes(guard)!=guard: raise RuntimeError('Protected package drift before rollback')
 report={'status':'started','activation':str(path),'before':checks,'save_attempts':[]}
 write(target,report)
 try:
  for skill,row in data['skills'].items():
   if checks[skill]==row['before']: continue
   apply(skill,{k:row['before'][k] for k in ALLOWED})
   stable_check(row['before'],snapshot(require(skill)))
  for skill in data['skills']:
   report['save_attempts'].append(skill);write(target,report);save(skill)
  if hashes(guard)!=guard: raise RuntimeError('Protected package drift after rollback')
  report['status']='two_skill_reflected_baselines_restored_derivatives_retained'
 except Exception as exc:
  report.update(status='stopped_inspect_outcome',error=str(exc));raise
 finally: write(target,report)
 print('RESULT=ACTIVE_VFX_ROLLBACK_PASSED original_fields_restored derivatives_retained=true')
