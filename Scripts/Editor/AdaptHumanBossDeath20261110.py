"""Preserve the crouched-back candidate and derive a clearer owned side-collapse montage."""
import sys,json,hashlib
from pathlib import Path
import unreal
PROJECT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(PROJECT/'Scripts/Editor'))
import PrepareHumanBossAnimations20261110 as prep
ROOT=prep.ROOT
SOURCE='/Game/CombatMagicAnims/Demo/Mannequins/Anims/Death/MM_Death_Right_01'
NAME='A_HumanBoss_Death_Right'
original=prep.require(SOURCE)
source_file=PROJECT/'Content'/(SOURCE[6:]+'.uasset')
sha=hashlib.sha256(source_file.read_bytes()).hexdigest()
if not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/'+NAME):
    inp=unreal.IKRetargetBatchOperationInputs()
    inp.assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(SOURCE)]
    inp.source_mesh=prep.require('/Game/CombatMagicAnims/Demo/Mannequins/Meshes/SKM_Manny_Simple')
    inp.target_mesh=prep.require(prep.TARGET)
    inp.ik_retarget_asset=prep.require(ROOT+'/RTG_Magic_HumanBoss')
    inp.search=original.get_name();inp.replace=NAME;inp.target_path=ROOT
    inp.use_source_path=False;inp.include_referenced_assets=False;inp.overwrite_existing_files=False
    unreal.IKRetargetBatchOperation.run_batch_retarget(inp)
clip=prep.require(ROOT+'/'+NAME)
clip.set_editor_property('enable_root_motion',False)
clip.set_editor_property('force_root_lock',True)
unreal.EditorAssetLibrary.set_metadata_tag(clip,'Rogue10m.Source',SOURCE)
prep.save(clip)
report=prep.pose_report(clip,inp.target_mesh if 'inp' in globals() else prep.require(prep.TARGET))
montage=unreal.load_asset(ROOT+'/AM_HumanBoss_Death_Right')
if not montage:
    factory=unreal.AnimMontageFactory();factory.set_editor_property('target_skeleton',clip.get_editor_property('skeleton'));factory.set_editor_property('source_animation',clip)
    montage=unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_HumanBoss_Death_Right',ROOT,unreal.AnimMontage,factory)
if not montage: raise RuntimeError('Death montage creation failed')
for field,seconds in [('blend_in',.08),('blend_out',.1)]:
    blend=montage.get_editor_property(field);blend.set_editor_property('blend_time',seconds);montage.set_editor_property(field,blend)
prep.save(montage)
profile=prep.require(ROOT+'/DA_HumanBoss_AttackProfile')
profile.set_editor_property('death_animation',clip);profile.set_editor_property('death_montage',montage);prep.save(profile)
if hashlib.sha256(source_file.read_bytes()).hexdigest()!=sha: raise RuntimeError('Original modified')
report.update({'source':SOURCE,'original_sha256':sha,'montage':montage.get_path_name(),'status':'activated_pending_actual_lifecycle_capture','style':'short side collapse and despawn, not persistent full-floor corpse'})
(prep.OUT/'boss-death-adaptation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('RESULT=HUMAN_BOSS_DEATH_ADAPTED')
