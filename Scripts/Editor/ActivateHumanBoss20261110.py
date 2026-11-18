"""Activate the validated project-owned humanoid boss without altering gameplay tuning or maps."""
from pathlib import Path
import json, math
import unreal
PROJECT=Path(unreal.Paths.project_dir()).resolve()
ROOT='/Game/Rogue10m/Characters/Boss/AzathorHuman20261110'
DATA='/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor'
MESH='/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody'
OUT=PROJECT/'Feature/doc/evidence/humanoid-boss-npc-20261110'
def require(path):
    obj=unreal.load_asset(path)
    if not obj: raise RuntimeError('Missing '+path)
    return obj
def path(obj): return obj.get_path_name() if obj else None
def save(obj):
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError('Save failed '+path(obj))
def main():
    data=require(DATA); old=data.get_editor_property('attack_pattern_profile')
    report={'old_profile':path(old),'old_mesh':str(data.get_editor_property('skeletal_mesh')),
      'old_animation_class':path(data.get_editor_property('animation_blueprint_class')),
      'old_mesh_location':str(data.get_editor_property('mesh_relative_location')),
      'old_mesh_rotation':str(data.get_editor_property('mesh_relative_rotation')),
      'old_mesh_scale':str(data.get_editor_property('mesh_relative_scale'))}
    # Preserve the first rollback evidence across idempotent reruns.
    prior=OUT/'boss-activation.json'
    if prior.exists():
        first=json.loads(prior.read_text(encoding='utf-8-sig'))
        for key in list(report):
            if key in first: report[key]=first[key]
    original_report=OUT/'boss-activation-original.json'
    if not original_report.exists():
        original_report.write_text(json.dumps(report,indent=2),encoding='utf-8')
    profile_path=ROOT+'/DA_HumanBoss_AttackProfile'
    profile=unreal.load_asset(profile_path)
    if not profile:
        profile=unreal.EditorAssetLibrary.duplicate_asset(path(old).split('.')[0],profile_path)
    if not profile: raise RuntimeError('Profile copy failed')
    clips={'idle':'Idle','movement':'Walk','attack':'Hook_Raw','frontal_wave':'Wave_Raw','abyss_zone':'Zone_Raw','hit':'Hit','death':('Death_Right' if unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/A_HumanBoss_Death_Right') else 'Death')}
    montage_paths={}
    for key,name in clips.items():
        clip=require(ROOT+'/A_HumanBoss_'+name)
        profile.set_editor_property(key+'_animation',clip)
        if key in ('idle','movement'): continue
        montage_path=ROOT+'/AM_HumanBoss_'+('Death_Right' if key=='death' and name=='Death_Right' else key.title().replace('_',''))
        montage=unreal.load_asset(montage_path)
        if not montage:
            factory=unreal.AnimMontageFactory()
            factory.set_editor_property('target_skeleton',clip.get_editor_property('skeleton'))
            factory.set_editor_property('source_animation',clip)
            montage=unreal.AssetToolsHelpers.get_asset_tools().create_asset(montage_path.rsplit('/',1)[1],ROOT,unreal.AnimMontage,factory)
        if not montage: raise RuntimeError('Montage creation failed '+key)
        for field,seconds in [('blend_in',0.08),('blend_out',0.1)]:
            blend=montage.get_editor_property(field)
            blend.set_editor_property('blend_time',seconds)
            montage.set_editor_property(field,blend)
        unreal.EditorAssetLibrary.set_metadata_tag(montage,'Rogue10m.Source',path(clip))
        unreal.EditorAssetLibrary.set_metadata_tag(montage,'Rogue10m.Slot','DefaultSlot')
        save(montage)
        profile.set_editor_property(key+'_montage',montage)
        montage_paths[key]=path(montage)
    profile.set_editor_property('use_humanoid_anim_instance',True)
    profile.set_editor_property('attack_contact_time_seconds',0.5)
    profile.set_editor_property('frontal_wave_contact_time_seconds',2.2333333)
    profile.set_editor_property('abyss_zone_contact_time_seconds',3.1666667)
    save(profile)
    data.set_editor_property('skeletal_mesh',require(MESH))
    data.set_editor_property('animation_blueprint_class',unreal.Rogue10mHumanoidAnimInstance.static_class())
    data.set_editor_property('mesh_relative_location',unreal.Vector(x=0,y=0,z=-90))
    data.set_editor_property('mesh_relative_rotation',unreal.Rotator(pitch=0,yaw=-90,roll=0))
    data.set_editor_property('mesh_relative_scale',unreal.Vector(x=1,y=1,z=1))
    data.set_editor_property('attack_pattern_profile',profile)
    save(data)
    report.update({'status':'activated_pending_runtime_validation','profile':path(profile),'mesh':MESH,'montages':montage_paths,
      'contact_source_seconds':{'attack':0.5,'frontal_wave':2.2333333,'abyss_zone':3.1666667},
      'gameplay_tuning_changed':False,'map_saved':False})
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'boss-activation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('RESULT=HUMAN_BOSS_ACTIVATED')
if __name__=='__main__': main()
