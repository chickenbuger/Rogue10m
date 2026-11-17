"""Scoped material authoring and guarded VFX-only activation for two existing skills.
UE5.8 editor Python. Broker lease required. Default execution creates materials only.
No native gameplay, animation, camera, first-person tuning, vendor asset or save-game edits.
"""
from __future__ import annotations
import json,re
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
DEST='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110'
OUT=ROOT/'Feature/doc/evidence/sword-knuckle-20261110/vfx-authoring'
VERSION='WeaponFantasy20261110_v1'
CONFIGS=[
 {'id':'KnuckleFlurry','skill':'/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary03','mapping':'Existing six-hit HookFlurry / focused flurry presentation, not the new three-hit K01',
  'cast':'NS_KnuckleFlurry_Acceptance','impact':'NS_KnuckleFlurry_ConfirmedImpact'},
 {'id':'GreatSwordHeavy','skill':'/Game/DataAsset/AttackSkill/ClassCombat/GreatSword/DA_Attack_GreatSword_Charged','mapping':'G09 Paewangcham heavy-cut presentation, not G07 ground rupture',
  'cast':'NS_GreatSwordHeavy_Acceptance','impact':'NS_GreatSwordHeavy_ConfirmedImpact'}]
# Pure UV masks: no external textures, depth reads, lights, decals or body-painted circles.
# Every visible layer is within one camera-facing sprite; Impact is target-bounds feedback,
# not an exact contact-normal marker and not an active damage or ground-area indicator.
MASKS={
 'M_KnuckleFlurry_Acceptance':('float2 p=(UV-.5)*2; float stroke=exp(-p.y*p.y*65)*saturate(1-abs(p.x)); return stroke*pow(saturate(1-Age),2)*.65;',(.95,.87,.68)),
 'M_KnuckleFlurry_ConfirmedImpact':('float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x); float t=saturate(Age); float core=Core*pow(saturate(1-t*2),2); float ring=exp(-pow((r-lerp(.22,.83,t))*36,2))*step(.35,abs(sin(a*3+.4)))*pow(1-t,1.5); float rays=pow(abs(cos(a*5+.2)),48)*exp(-pow((r-.62)*12,2))*pow(1-t,3)*.35; return saturate(core+ring*.7+rays);',(1.0,.71,.25)),
 'M_GreatSwordHeavy_Acceptance':('float2 p=(UV-.5)*2; float flash=exp(-p.x*p.x*40)*exp(-p.y*p.y*4); return flash*pow(saturate(1-Age),3)*.6;',(1.0,.61,.20)),
 'M_GreatSwordHeavy_ConfirmedImpact':('float2 p=(UV-.5)*2; float r=length(p); float a=atan2(p.y,p.x); float t=saturate(Age); float core=Core*pow(saturate(1-t*2.1),2); float crescent=exp(-pow((length(p-float2(-.18,0))-lerp(.3,.8,t))*32,2))*smoothstep(-.1,.5,p.x)*pow(1-t,1.4); float shards=pow(abs(cos(a*4+.25)),56)*exp(-pow((r-.65)*10,2))*pow(1-t,2.5)*.4; return saturate(core+crescent*.75+shards);',(1.0,.42,.085)),
}

def require(path):
 obj=unreal.load_asset(path)
 if not obj:raise RuntimeError('Missing '+path)
 return obj

def frozen(value):
 if isinstance(value,unreal.Object):return value.get_path_name()
 if hasattr(value,'export_text'):return value.export_text()
 if isinstance(value,(str,int,float,bool)) or value is None:return value
 try:return [frozen(v) for v in value]
 except TypeError:return str(value)

def snake(name):
 if len(name)>1 and name[0]=='b' and name[1].isupper():name=name[1:]
 return re.sub(r'(?<!^)(?=[A-Z])','_',name).lower()

def snapshot(skill):
 text=(ROOT/'Source/Rogue10m/Data/Rogue10mAttackSkillData.h').read_text(encoding='utf-8',errors='replace')
 # Match all public reflected member declarations after the skill class, including arrays.
 text=text.split('class ROGUE10M_API URogue10mAttackSkillData',1)[-1]
 names=re.findall(r'^\s*(?:TObjectPtr<[^>]+>|TSoftObjectPtr<[^>]+>|TSubclassOf<[^>]+>|TArray<[^;]+>|[A-Za-z_]\w*)\s+(\w+)\s*(?:=[^;]*)?;',text,re.M)
 result={}
 for native in names:
  name=snake(native)
  try:result[name]=frozen(skill.get_editor_property(name))
  except Exception:pass
 if len(result)<65:raise RuntimeError('Insufficient reflected guard coverage: '+str(len(result)))
 for key in ['attack_montage','hit_start_delay_seconds','damage','hit_count','resource_costs','input_slot','first_person_effect_offset','first_person_cast_scale_multiplier']:
  if key not in result:raise RuntimeError('Missing protected guard '+key)
 return result

def save(obj):
 if not obj.get_path_name().startswith(DEST+'/') and obj.get_path_name().split('.',1)[0] not in [c['skill'] for c in CONFIGS]:raise RuntimeError('Out-of-scope save rejected '+obj.get_path_name())
 if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('Save failed '+obj.get_path_name())

def custom_input(name):
 value=unreal.CustomInput();value.set_editor_property('input_name',name);return value

def connect_expression(*args):
 if not unreal.MaterialEditingLibrary.connect_material_expressions(*args):raise RuntimeError('Material expression connection failed '+str(args[1::2]))

def connect_property(*args):
 if not unreal.MaterialEditingLibrary.connect_material_property(*args):raise RuntimeError('Material output connection failed '+str(args[1:]))

def materials(repair_incomplete=False):
 lib=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
 unreal.EditorAssetLibrary.make_directory(DEST);rows=[]
 for name,(code,color) in MASKS.items():
  path=DEST+'/'+name
  if unreal.EditorAssetLibrary.does_asset_exist(path):
   mat=require(path)
   if unreal.EditorAssetLibrary.get_metadata_tag(mat,'Rogue10m.WeaponFantasyVersion')==VERSION:
    rows.append(path);continue
   if not repair_incomplete:raise RuntimeError('Existing incomplete material requires diagnosed explicit resume '+path)
   lib.delete_all_material_expressions(mat)
  else:mat=tools.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
  if not mat:raise RuntimeError('Material creation failed '+path)
  mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_ADDITIVE,'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,'two_sided':True})
  uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-700,0)
  age=lib.create_material_expression(mat,unreal.MaterialExpressionParticleRelativeTime,-700,180)
  custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-400,0)
  custom.set_editor_properties({'code':code,'output_type':unreal.CustomMaterialOutputType.CMOT_FLOAT1,'description':'Finite bounded weapon fantasy mask','inputs':[custom_input(n) for n in (['UV','Age','Core'] if 'ConfirmedImpact' in name else ['UV','Age'])]})
  connect_expression(uv,'',custom,'UV');connect_expression(age,'',custom,'Age')
  if 'ConfirmedImpact' in name:
   radial=lib.create_material_expression(mat,unreal.MaterialExpressionMaterialFunctionCall,-700,-220)
   if not radial.set_material_function(require('/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential')):raise RuntimeError('Cannot reuse engine radial gradient')
   connect_expression(uv,'',radial,'UVs')
   for input_name,value in [('Radius',.11),('Density',3.)]:
    e=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-950,-250);e.set_editor_property('r',value);connect_expression(e,'',radial,input_name)
   connect_expression(radial,'',custom,'Core')
  tint=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-400,300);tint.set_editor_properties({'parameter_name':'CueTint','group':'Weapon Fantasy','default_value':unreal.LinearColor(*color,1)})
  particles=lib.create_material_expression(mat,unreal.MaterialExpressionParticleColor,-400,450)
  multiply=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-100,300)
  connect_expression(tint,'',multiply,'A');connect_expression(particles,'RGB',multiply,'B')
  connect_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  connect_property(custom,'',unreal.MaterialProperty.MP_OPACITY)
  lib.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
  lib.recompile_material(mat)
  unreal.EditorAssetLibrary.set_metadata_tag(mat,'Rogue10m.WeaponFantasyVersion',VERSION)
  unreal.EditorAssetLibrary.set_metadata_tag(mat,'Rogue10m.Source','Original procedural UV shaping for local Niagara derivatives; no external input')
  save(mat);rows.append(path)
 OUT.mkdir(parents=True,exist_ok=True);(OUT/'materials.json').write_text(json.dumps({'materials':rows,'first_person_qa':False},indent=2),encoding='utf-8')
 print('RESULT=WEAPON_FANTASY_MATERIALS_PREPARED materials='+str(len(rows)))

def activate():
 # Call only after all four systems passed official Niagara compile/stack inspection.
 # Only effect references are changed. Every tuning field, including all FP budgets, is frozen.
 OUT.mkdir(parents=True,exist_ok=True);report=[]
 for cfg in CONFIGS:
  skill=require(cfg['skill']);before=snapshot(skill)
  updates={key+'_effect':require(DEST+'/'+cfg[key]) for key in ('cast','impact')}
  if not before.get('enable_attack_effects'):raise RuntimeError('Existing skill effects disabled, explicit scope needed')
  for effect in updates.values():
   if not isinstance(effect,unreal.NiagaraSystem):raise RuntimeError('Not Niagara '+effect.get_path_name())
   if unreal.EditorAssetLibrary.get_metadata_tag(effect,'Rogue10m.WeaponFantasyVersion')!=VERSION:raise RuntimeError('Unvalidated derivative '+effect.get_path_name())
  skill.set_editor_properties(updates);after=snapshot(skill)
  changed={key for key in before if before[key]!=after[key]}
  if changed-{'cast_effect','impact_effect'}:
   skill.set_editor_properties({key:require(before[key]) if key.endswith('_effect') and before[key] else before[key] for key in changed & {'cast_effect','impact_effect'}})
   raise RuntimeError('Protected field changed '+str(changed))
  save(skill);report.append({'skill':cfg['skill'],'mapping':cfg['mapping'],'changed_fields':sorted(changed),'protected_field_count':len(before)-2,'before':before,'after':after,'first_person_qa':False})
 (OUT/'activation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_FANTASY_VFX_ACTIVATED skills=2 changed=cast_effect,impact_effect protected=all_other_fields')

def finalize_and_activate():
 """Save only the four compile-gated candidate systems, then bind two VFX references."""
 evidence=ROOT/'Feature/doc/evidence/sword-knuckle-20261110/vfx-authoring-mcp/validated-systems.json'
 rows=json.loads(evidence.read_text(encoding='utf-8-sig'))
 expected={DEST+'/'+cfg[key] for cfg in CONFIGS for key in ('cast','impact')}
 if len(rows)!=4 or {r['asset'] for r in rows}!=expected:raise RuntimeError('Missing four-system validation gate')
 for row in rows:
  if row['compile'].get('bHasErrors') or row['compile'].get('bIsStale') or row['issues'].get('numErrors',1):raise RuntimeError('Bad validated system '+row['asset'])
  obj=require(row['asset'])
  if not isinstance(obj,unreal.NiagaraSystem):raise RuntimeError('Bad type '+row['asset'])
  unreal.EditorAssetLibrary.set_metadata_tag(obj,'Rogue10m.WeaponFantasyVersion',VERSION)
  unreal.EditorAssetLibrary.set_metadata_tag(obj,'Rogue10m.Source',row['source'])
  unreal.EditorAssetLibrary.set_metadata_tag(obj,'Rogue10m.PhaseContract','Accepted-start hand cue' if 'Acceptance' in row['asset'] else 'Actual health-loss target-bounds cue; not exact surface contact')
  save(obj)
 activate()

def reconcile_reviewed_motion(skill_path, expected, motion):
 """Accept only the separately recorded two-field sword motion activation over this exact baseline."""
 result=dict(expected)
 if '/GreatSword/' not in skill_path:return result,False
 name=skill_path.rsplit('_',1)[-1]
 row=next((r for r in motion.get('skills',[]) if r.get('name')==name),None)
 if row is None:return result,False
 before,after=row.get('before',{}),row.get('after',{})
 allowed={'attack_montage','hit_start_delay_seconds'}
 if set(before)!=set(after):raise RuntimeError('Motion snapshot keys changed')
 changed={k for k in before if before[k]!=after[k]}
 if changed-allowed:raise RuntimeError('Unexpected motion protected fields '+str(changed-allowed))
 if any(before.get(k)!=v for k,v in expected.items()):raise RuntimeError('Motion activation baseline differs from VFX baseline')
 for k in allowed:result[k]=after[k]
 return result,True

def validate():
 """Cold readback of complete reflected snapshots; does not modify assets."""
 report=json.loads((OUT/'activation.json').read_text(encoding='utf-8-sig'))
 checks=[]
 motion_file=ROOT/'Feature/doc/evidence/sword-knuckle-20261110/greatsword-activation.json'
 motion=json.loads(motion_file.read_text(encoding='utf-8-sig')) if motion_file.exists() else {}
 for row in report:
  now=snapshot(require(row['skill']))
  expected,reconciled=reconcile_reviewed_motion(row['skill'],row['after'],motion)
  differences={k for k in expected if now.get(k)!=expected[k]}
  if differences:raise RuntimeError('Cold reflected state mismatch '+row['skill']+' '+str(differences))
  for key in ('cast_effect','impact_effect'):
   obj=require(now[key]);assert isinstance(obj,unreal.NiagaraSystem)
   if unreal.EditorAssetLibrary.get_metadata_tag(obj,'Rogue10m.WeaponFantasyVersion')!=VERSION:raise RuntimeError('Missing provenance '+now[key])
  checks.append({'skill':row['skill'],'reflected_fields_preserved':len(now)-2,'changed_fields':row['changed_fields'],'first_person_fields_preserved':True,'first_person_qa':False,'reviewed_motion_reconciled':reconciled})
 (OUT/'cold-validation.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_FANTASY_VFX_COLD_PASSED skills=2')
if __name__=='__main__':materials()





