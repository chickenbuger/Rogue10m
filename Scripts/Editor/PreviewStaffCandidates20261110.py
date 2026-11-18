"""Isolated Staff candidate pose review: 8 PNGs, no gameplay activation or asset/map saves.
Explicit coordinator lease and fresh output required. Human-direct is offline derivative
review; runtime-retarget exercises the existing body ABP on a frozen Manny pose, not combat.
Never publishes source assets or full-pose data. Phase labels are provisional review samples.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, time
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
BASE='/Game/Rogue10m/Animation/Combat/Staff20261110'
HUMAN='/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody'
MANNY='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
ABP='/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M'
BONES=('pelvis','head','hand_r','hand_l','middle_01_r','middle_01_l','foot_r','foot_l')
PHASES=('ready','winding','action','recovery')
STATE={}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def vec(v): return [v.x,v.y,v.z]
def load(p):
    a=unreal.load_asset(p)
    if not a: raise RuntimeError('Missing asset '+p)
    return a

def protect_package(path, protected):
    package=path.split('.')[0]
    if not package.startswith('/Game/') or '..' in package: raise RuntimeError('Unsafe project package')
    stem=ROOT/'Content'/package[6:]
    if not stem.with_suffix('.uasset').is_file(): raise RuntimeError('Unsaved package '+path)
    for ext in ('.uasset','.uexp','.ubulk','.uptnl'):
        file=stem.with_suffix(ext)
        if file.is_file(): protected[str(file)]=sha(file)

def spawn(cls,location=None,rotation=None):
    actor=STATE['actors'].spawn_actor_from_class(cls,location or unreal.Vector(),rotation or unreal.Rotator(),transient=True)
    if not actor: raise RuntimeError('Transient spawn failed')
    STATE['owned'].append(actor)
    return actor

def setup(comp,mesh):
    comp.set_skeletal_mesh_asset(load(mesh)); comp.set_enable_animation(True)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); comp.set_receives_decals(False)
    comp.set_update_animation_in_editor(True); comp.set_component_tick_enabled(True)
    comp.set_editor_property('pause_anims',False)
    comp.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)

def refresh(): unreal.SystemLibrary.execute_console_command(STATE['world'],'Rogue10m.RefreshSwordPreview')
def positions(comp): return {b:comp.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).translation for b in BONES}
def write(): (STATE['out']/'captures.json').write_text(json.dumps(STATE['report'],indent=2),encoding='utf-8')

def stop(error=None):
    s=STATE
    if s.get('callback'): unreal.unregister_slate_post_tick_callback(s.pop('callback'))
    after={p:sha(Path(p)) if Path(p).is_file() else None for p in s['protected']}
    s['report'].update(status='failed' if error else 'captured_visual_review_pending',error=str(error) if error else None,
        protected_sha256_after=after,protected_assets_unchanged=after==s['protected'])
    if after!=s['protected']: s['report'].update(status='failed',error='Protected file hash changed')
    # A pending async screenshot owns the scene. Never destroy actors before its completion.
    pending=s.get('task') and not s['task'].is_task_done()
    s['report']['pending_screenshot_scene_retained']=bool(pending)
    if not pending:
        for actor in reversed(s.get('owned',[])):
            if unreal.SystemLibrary.is_valid(actor): s['actors'].destroy_actor(actor)
    write(); unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def stage_scene():
    s=STATE; unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    s['world']=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    s['actors']=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    floor=spawn(unreal.StaticMeshActor,unreal.Vector(0,0,-7));floor.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(10,10,.1))
    light=spawn(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(pitch=-40,yaw=-35,roll=0));light.light_component.set_editor_property('intensity',2.5)
    fill=spawn(unreal.PointLight,unreal.Vector(200,-220,220));fill.light_component.set_editor_property('intensity',3500.);fill.light_component.set_editor_property('attenuation_radius',1000.)
    pp=spawn(unreal.PostProcessVolume);pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings')
    for key,value in {'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,'auto_exposure_min_brightness':1.,'auto_exposure_max_brightness':1.,'override_auto_exposure_bias':True,'auto_exposure_bias':-1.,'override_bloom_intensity':True,'bloom_intensity':.15}.items():settings.set_editor_property(key,value)
    pp.set_editor_property('settings',settings)
    actor=spawn(unreal.SkeletalMeshActor);actor.set_actor_label('QA_SwordPreview_20261110',False)
    source=actor.skeletal_mesh_component;setup(source,HUMAN if s['mode']=='human-direct' else MANNY)
    body=source
    if s['mode']=='runtime-retarget':
        source.set_visibility(False,False)
        body=unreal.SkeletalMeshComponent(outer=actor,name='QAWeaponPreviewBody');setup(body,HUMAN)
        actor.call_method('FinishAddComponent',args=(body,False,unreal.Transform()))
        if body.get_owner()!=actor or body.get_attach_parent()!=source: raise RuntimeError('Invalid retarget body attachment')
        body.set_visibility(True,False);body.add_tick_prerequisite_component(source)
    s.update(source=source,body=body)
    if s['weapon']:
        spec=s['weapon'];weapon=spawn(unreal.StaticMeshActor);weapon.static_mesh_component.set_static_mesh(load(spec['asset']))
        weapon.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);weapon.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        if body.get_bone_index(spec['bone'])<0: raise RuntimeError('Weapon attachment bone missing')
        if not weapon.attach_to_component(body,spec['bone'],unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.KEEP_WORLD,False): raise RuntimeError('Weapon attach failed')
        tr=unreal.Transform();tr.translation=unreal.Vector(*spec['translation_cm']);tr.rotation=unreal.Quat(*spec['quaternion_xyzw']);tr.scale3d=unreal.Vector(*spec['scale']);weapon.set_actor_relative_transform(tr,False,False)
    camera=unreal.Vector(360,-340,205);look=unreal.Vector(0,0,95)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,look))
    s['report']['camera']={'position':vec(camera),'look_at':vec(look),'view':'third-person front-side, session-only exposure'}
    s['actors'].clear_actor_selection_set()

def stage_pose(row):
    s=STATE;clip=load(row['clip']);source=s['source'];body=s['body']
    source.play_animation(clip,False);source.set_play_rate(0.);source.set_position(row['time_s'],False)
    if s['mode']=='runtime-retarget':
        body.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);refresh();s['reference']=positions(body)
        body.set_anim_instance_class(load(ABP).generated_class())
    refresh();refresh();s.update(clip=clip,stage_started=time.monotonic(),phase='settle')

def verify(row):
    s=STATE;refresh();opts=unreal.AnimPoseEvaluationOptions();opts.optional_skeletal_mesh=s['source'].get_skeletal_mesh_asset();opts.extract_root_motion=False;opts.incorporate_root_motion_into_pose=True
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(s['clip'],row['time_s'],opts)
    actual=positions(s['source']);errors={b:(actual[b]-unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD).translation).length() for b in BONES}
    if any(not math.isfinite(v) or v>1. for v in errors.values()): raise RuntimeError('Actual source pose mismatch/stale '+str(errors))
    result={'actual_source_bone_error_cm':errors,'max_source_pose_error_cm':max(errors.values()),'body_route':s['mode'],'runtime_body_pose_equivalence_verified':False}
    if s['mode']=='runtime-retarget':
        deltas={b:(positions(s['body'])[b]-s['reference'][b]).length() for b in BONES}
        if not s['body'].get_anim_instance() or max(deltas[b] for b in ('hand_l','hand_r'))<1.: raise RuntimeError('Runtime retarget body missing or reference/stale pose')
        result.update(body_reference_delta_cm=deltas,retarget_class=ABP,body_pose_error_to_offline_derivative_not_asserted=True)
    else: result['human_derivative_pose_error_verified']=True
    return result

def step(dt):
    s=STATE
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
        if s['index']==8: stop();return
        row=s['rows'][s['index']]
        if s['phase']=='next':stage_pose(row)
        elif s['phase']=='settle' and time.monotonic()-s['stage_started']>=2.:
            s['pose_check']=verify(row);s['image']=s['out']/(str(s['index']+1).zfill(2)+'_'+row['name']+'_'+row['phase']+'.png')
            s['task']=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(s['image']))
            if not s['task'] or not s['task'].is_valid_task():raise RuntimeError('Invalid screenshot task')
            s.update(phase='capture',capture_started=time.monotonic())
        elif s['phase']=='capture':
            if time.monotonic()-s['capture_started']>90:raise RuntimeError('Screenshot timeout; pending scene retained')
            if s['task'].is_task_done():
                if not s['image'].is_file() or s['image'].stat().st_size<=1024:raise RuntimeError('Empty/missing PNG')
                s['report']['captures'].append(dict(row,**s['pose_check'],image=s['image'].name,bytes=s['image'].stat().st_size,image_sha256=sha(s['image']),screenshot_task_completed=True,weapon_present=bool(s['weapon']),actual_combat=False,visual_review='pending'))
                write();s.update(task=None,index=s['index']+1,phase='next')
    except Exception as exc:stop(exc)

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage-manifest',required=True);p.add_argument('--output',required=True)
    p.add_argument('--lease-granted',action='store_true');p.add_argument('--allow-transient-map',action='store_true');p.add_argument('--mode',choices=['human-direct','runtime-retarget'],default='human-direct')
    p.add_argument('--sample-times-json',help='StaffStrike/StaffShot objects each with ready,winding,action,recovery seconds; review points, not combat contracts')
    p.add_argument('--weapon-spec',help='Optional owned-confirmed static mesh JSON with asset,bone,translation_cm,quaternion_xyzw,scale; never inferred')
    a=p.parse_args(argv)
    if not a.lease_granted or not a.allow_transient_map:raise RuntimeError('Explicit exclusive lease and transient map required')
    if STATE:raise RuntimeError('One run per editor Python session')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():raise RuntimeError('Dirty editor state; no auto-save/discard')
    mp=Path(a.stage_manifest).resolve();m=json.loads(mp.read_text(encoding='utf-8-sig'));out=Path(a.output).resolve()
    if out.exists():raise RuntimeError('Fresh output required')
    if m.get('status')!='staged_unvalidated_not_activated' or m.get('activated') is not False or not m.get('original_files_preserved'):raise RuntimeError('Require completed unactivated preserved staging')
    label=m['label']
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,23}',label) or m['asset_root']!=BASE+'/'+label:raise RuntimeError('Stage path mismatch')
    expected={BASE+'/'+label+'/A_Staff_'+n+'_'+role for n in ('StaffStrike','StaffShot') for role in ('Manny','HumanMale')}
    if {c['asset'].split('.')[0] for c in m['candidates']}!=expected:raise RuntimeError('Require exactly four staged Staff derivatives')
    protected=dict(m['original_files_before_sha256'])
    for file,digest in protected.items():
        if not Path(file).is_file() or sha(Path(file))!=digest:raise RuntimeError('Original/shared stage hash changed '+file)
    for asset in m['created_assets']:protect_package(asset,protected)
    # All shared combat skills/profiles are read-only, including the coordinator's DA17 scope.
    for folder in ('DataAsset/AttackSkill/ClassCombat','DataAsset/SkillProfile'):
        for file in (ROOT/'Content'/folder).rglob('*.uasset'):protected[str(file)]=sha(file)
    weapon=json.loads(Path(a.weapon_spec).read_text(encoding='utf-8-sig')) if a.weapon_spec else None
    if weapon:
        if weapon.get('owned_confirmed') is not True:raise RuntimeError('Weapon ownership must be explicitly confirmed')
        for key,count in (('translation_cm',3),('quaternion_xyzw',4),('scale',3)):
            if len(weapon[key])!=count or not all(isinstance(v,(int,float)) and math.isfinite(v) for v in weapon[key]):raise RuntimeError('Invalid weapon transform')
        if any(v<=0 for v in weapon['scale']) or abs(sum(v*v for v in weapon['quaternion_xyzw'])-1)>1e-3:raise RuntimeError('Invalid weapon scale/quaternion')
        protect_package(weapon['asset'],protected)
    times=json.loads(Path(a.sample_times_json).read_text(encoding='utf-8-sig')) if a.sample_times_json else None
    rows=[]
    for name in ('StaffStrike','StaffShot'):
        clip=BASE+'/'+label+'/A_Staff_'+name+('_HumanMale' if a.mode=='human-direct' else '_Manny');duration=load(clip).get_play_length()
        source_clip=load('/Game/CombatMagicAnims/Animations/AS_'+name)
        if abs(source_clip.get_play_length()-duration)>1e-4:raise RuntimeError('Source/derivative duration mismatch; source time not equivalent')
        points=times[name] if times else dict(zip(PHASES,[duration*f for f in (.05,.3,.5,.85)]))
        if set(points)!=set(PHASES) or any(not math.isfinite(v) or not 0<=v<duration for v in points.values()):raise RuntimeError('Invalid four sample times')
        for phase in PHASES:rows.append(dict(name=name,phase=phase,clip=clip,time_s=points[phase],source_asset='/Game/CombatMagicAnims/Animations/AS_'+name,source_time_s=points[phase],duration_s=duration,sampling='explicit review times' if times else 'provisional normalized samples .05/.30/.50/.85; phase/contact not verified'))
    old_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_map=old_world.get_path_name().split('.')[0] if old_world else ''
    if old_map.startswith('/Game/'):
        file=ROOT/'Content'/(old_map[6:]+'.umap')
        if file.is_file():protected[str(file)]=sha(file)
    out.mkdir(parents=True,exist_ok=False)
    STATE.update(out=out,protected=protected,rows=rows,index=0,phase='next',task=None,owned=[],weapon=weapon,mode=a.mode,
        report=dict(status='started',stage_manifest=str(mp),stage_manifest_sha256=sha(mp),expected_count=8,mode=a.mode,weapon=weapon,weapon_absent_limitation='Unarmed motion review only; staff grip/contact not evaluated' if not weapon else 'Explicit model transform is a visual candidate, not validated grip',actual_combat=False,activated=False,saved_assets_or_maps=False,source_or_full_pose_published=False,visual_review='pending',runtime_body_claim='Frozen existing retarget ABP fixture only' if a.mode=='runtime-retarget' else 'Offline Human derivative; NOT runtime-body equivalence',protected_sha256_before=protected,captures=[]))
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    try:stage_scene();STATE['callback']=unreal.register_slate_post_tick_callback(step);write()
    except Exception as exc:stop(exc);raise

if __name__=='__main__':main()
