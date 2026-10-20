"""Import two animation-only FBXs on the unchanged accepted Arin skeleton."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(PROJECT/'Scripts/Editor'))
import ApplyReferenceArin as model

EVIDENCE = PROJECT/'Feature/doc/evidence/arin-walk-talk-20261009'
SOURCE = PROJECT/'Art/NPC/Arin/Animation'
ROOT = model.ROOT+'/Animations'
CLIPS = {'Walk':1.2, 'Talk':3.0}

def path(obj):
    return obj.get_path_name() if obj else None

def write(name, value):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')

def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()

def validate(clip, skeleton, seconds):
    if not isinstance(clip,unreal.AnimSequence) or clip.get_editor_property('skeleton')!=skeleton:
        raise RuntimeError('Animation skeleton differs: '+str(clip))
    if abs(clip.get_editor_property('sequence_length')-seconds)>.0001:
        raise RuntimeError('Animation duration differs: '+path(clip))
    if clip.get_editor_property('enable_root_motion'):
        raise RuntimeError('In-place clip unexpectedly enables root motion')
    data_model=clip.get_editor_property('data_model_interface')
    rate=data_model.get_frame_rate()
    if rate.numerator!=30 or rate.denominator!=1:
        raise RuntimeError('Clip source sample rate differs from 30fps')
    return {'path':path(clip),'skeleton':path(skeleton),'seconds':clip.get_editor_property('sequence_length'),
            'root_motion':False,'sample_rate':[rate.numerator,rate.denominator],
            'number_of_frames':data_model.get_number_of_frames()}

def import_clip(key, skeleton):
    name='A_Arin_'+key
    ui=unreal.FbxImportUI()
    ui.set_editor_properties({'automated_import_should_detect_type':False,
        'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION,
        'original_import_type':unreal.FBXImportType.FBXIT_ANIMATION,
        'import_as_skeletal':True,'import_mesh':False,'import_animations':True,
        'import_materials':False,'import_textures':False,'create_physics_asset':False,
        'skeleton':skeleton,'override_animation_name':name})
    ui.get_editor_property('anim_sequence_import_data').set_editor_properties({
        'use_default_sample_rate':True,'remove_redundant_keys':False,
        'animation_length':unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        'import_uniform_scale':1.,'convert_scene':True,'convert_scene_unit':True})
    task=unreal.AssetImportTask()
    task.set_editor_properties({'filename':str(SOURCE/(name+'.fbx')),'destination_path':ROOT,
        'destination_name':name,'automated':True,'replace_existing':False,'save':False,
        'options':ui,'factory':unreal.FbxFactory()})
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    clip=model._require(ROOT+'/'+name,unreal.AnimSequence)
    clip.set_editor_property('enable_root_motion',False)
    unreal.EditorAssetLibrary.set_metadata_tag(clip,'Arin.AnimationFBXSHA256',digest(SOURCE/(name+'.fbx')))
    return clip

def main(validate_only=False):
    data,bp,cdo,actor=model._guide_context()
    before=model._guide_state(data,bp,cdo,actor)
    checkpoint=EVIDENCE/'before-animation-guide-application.json'
    if checkpoint.exists():
        frozen=json.loads(checkpoint.read_text(encoding='utf-8'))
        model._verify_preserved(frozen,before)
        if frozen['appearance']!=before['appearance']:
            raise RuntimeError('Accepted mesh/Idle DataAsset references changed')
    elif validate_only:
        raise RuntimeError('Missing original guide checkpoint')
    else:
        write(checkpoint.name,before)
    build=json.loads((EVIDENCE/'motion-build.json').read_text(encoding='utf-8'))
    if digest(PROJECT/'Art/NPC/Arin/ReferenceRefined/Arin.blend')!=build['source_sha256']:
        raise RuntimeError('Accepted source changed')
    for key in CLIPS:
        name='A_Arin_'+key+'.fbx'
        if digest(SOURCE/name)!=build['files'][name]['sha256']:
            raise RuntimeError('Export differs from reviewed motion build: '+name)
    skeleton=model._require(model.SKELETON_PATH,unreal.Skeleton)
    expected={ROOT+'/A_Arin_'+key for key in CLIPS}
    existing={p.split('.')[0] for p in unreal.EditorAssetLibrary.list_assets(ROOT,recursive=True,include_folder=False)}
    if existing and existing!=expected:
        raise RuntimeError('Unexpected/partial owned animation scope: '+str(existing))
    if validate_only and existing!=expected:
        raise RuntimeError('Missing saved animations')
    clips={}
    for key,seconds in CLIPS.items():
        clip=model._require(ROOT+'/A_Arin_'+key,unreal.AnimSequence) if existing else import_clip(key,skeleton)
        validate(clip,skeleton,seconds)
        if unreal.EditorAssetLibrary.get_metadata_tag(clip,'Arin.AnimationFBXSHA256')!=digest(SOURCE/('A_Arin_'+key+'.fbx')):
            raise RuntimeError('Existing animation provenance differs')
        clips[key]=clip
    observed={p.split('.')[0] for p in unreal.EditorAssetLibrary.list_assets(ROOT,recursive=True,include_folder=False)}
    if observed!=expected:
        raise RuntimeError('Animation import created unexpected assets: '+str(observed))
    if not validate_only:
        for clip in clips.values():
            if not unreal.EditorAssetLibrary.save_loaded_asset(clip,False):
                raise RuntimeError('Animation save failed')
        data.modify()
        data.set_editor_properties({'guide_walk_animation':clips['Walk'],'guide_talk_animation':clips['Talk'],
            'guide_walk_reference_speed':110.,'guide_minimum_walk_speed':10.})
        if not unreal.EditorAssetLibrary.save_loaded_asset(data,False):
            raise RuntimeError('Guide data save failed')
    for key in CLIPS:
        if data.get_editor_property('guide_'+key.lower()+'_animation')!=clips[key]:
            raise RuntimeError('Guide clip assignment differs')
    components={}
    for name,owner in [('cdo',cdo),('placed_actor',actor)]:
        component=owner.get_editor_property('guide_animation')
        if not isinstance(component,unreal.Rogue10mTutorialAnimationComponent):
            raise RuntimeError('Missing native animation component on '+name)
        components[name]=path(component)
    after=model._guide_state(data,bp,cdo,actor)
    model._verify_preserved(before,after)
    if before['appearance']!=after['appearance'] or before['cdo_component']!=after['cdo_component'] or before['actor_component']!=after['actor_component']:
        raise RuntimeError('Existing appearance/Idle component data changed')
    report={'status':'passed','validate_only':validate_only,'clips':{k:validate(v,skeleton,CLIPS[k]) for k,v in clips.items()},
        'components':components,'guide_data':model.DATA_PATH,'reference_speed_cm_s':data.get_editor_property('guide_walk_reference_speed'),
        'minimum_speed_cm_s':data.get_editor_property('guide_minimum_walk_speed'),'protected_guide_preserved':True,
        'saved_packages':sorted(expected|{model.DATA_PATH}) if not validate_only else [],
        'source_preserved':True,'mixamo_downloaded_motion':False}
    write('ue-reloaded-validation.json' if validate_only else 'ue-application.json',report)
    unreal.log('ARIN_WALK_TALK_'+('RELOADED' if validate_only else 'APPLIED')+'_PASS')
    return report

if __name__=='__main__':
    main()
