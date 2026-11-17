"""Render isolated third-person candidate poses without changing any live DataAsset or saved map."""
from pathlib import Path
import json,time,uuid,runpy,unreal
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'DevLog/20261110_SwordKnuckle'/('PreviewCuts_'+uuid.uuid4().hex[:8])
FOLDER='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
STATE={}
def begin():
 unreal.EditorPythonScripting.set_keep_python_script_alive(True)
 OUT.mkdir(parents=True,exist_ok=True)
 # Read-only current skill/profile state for legal runtime fixture planning.
 helpers=runpy.run_path(str(ROOT/'Scripts/Editor/PrepareWeaponFantasyVFX20261110.py'))
 snapshot,frozen=helpers['snapshot'],helpers['frozen']
 metadata={'skills':{},'profiles':{}}
 for weapon in ['GreatSword','Knuckle']:
  for variant in ['Primary01','Primary02','Primary03','Special','Charged']:
   path='/Game/DataAsset/AttackSkill/ClassCombat/'+weapon+'/DA_Attack_'+weapon+'_'+variant
   obj=unreal.load_asset(path)
   if not obj:raise RuntimeError('Missing skill metadata '+path)
   metadata['skills'][path]=snapshot(obj)
  path='/Game/DataAsset/SkillProfile/DA_SkillProfile_Combat_'+weapon
  profile=unreal.load_asset(path)
  if not profile:raise RuntimeError('Missing profile metadata '+path)
  metadata['profiles'][path]={'default_bindings':{str(k):frozen(v) for k,v in profile.get_editor_property('default_skill_bindings').items()},'tree':frozen(profile.get_editor_property('skill_tree_skills')),'initial':frozen(profile.get_editor_property('initially_unlocked_skills'))}
 (OUT/'skill-profile-metadata.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_CURRENT_METADATA_CAPTURED path='+str(OUT/'skill-profile-metadata.json'))
 runpy.run_path(str(ROOT/'Scripts/Editor/StageGreatSwordDerivatives20261110.py'))['main']()
 runpy.run_path(str(ROOT/'Scripts/Editor/ValidateWeaponAssets20261110.py'))['main']()
 runpy.run_path(str(ROOT/'Scripts/Editor/PrepareWeaponFantasyVFX20261110.py'))['validate']()
 unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)
 # Leave user editor settings untouched; settle by elapsed time rather than foreground FPS.
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 floor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-7),transient=True);floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(15,15,.1))
 light=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(pitch=-45,yaw=-35,roll=0),transient=True);light.light_component.set_editor_property('intensity',5.)
 actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector(),transient=True)
 fill=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(200,-200,220),transient=True);fill.light_component.set_editor_property('intensity',15000.);fill.light_component.set_editor_property('attenuation_radius',1200.)
 sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,500),transient=True);sky.light_component.set_editor_property('intensity',1.)
 body=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,0),unreal.Rotator(pitch=0,yaw=-90,roll=0),transient=True)
 body.set_actor_label('QA_SwordPreview_20261110')
 comp=body.skeletal_mesh_component;comp.set_skeletal_mesh_asset(unreal.load_asset('/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody'))
 comp.set_enable_animation(True);comp.set_editor_property('pause_anims',False)
 comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);comp.set_receives_decals(False);comp.set_update_animation_in_editor(True);comp.set_component_tick_enabled(True);comp.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
 sword=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(),transient=True);sword.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/Weapons/Starter20261110/SM_StarterGreatSword'))
 sword.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
 sword.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);sword.static_mesh_component.set_receives_decals(False)
 if not sword.attach_to_component(comp,'hand_r',unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.KEEP_WORLD,False):raise RuntimeError('Sword attach failed')
 grip=json.loads((ROOT/'Feature/doc/evidence/sword-knuckle-20261110/grip-calibration.json').read_text(encoding='utf-8'))['_Human']['grip']
 tr=unreal.Transform();tr.translation=unreal.Vector(*grip['translation']);tr.rotation=unreal.Quat(*grip['quaternion']);sword.set_actor_relative_transform(tr,False,False)
 camera=unreal.Vector(330,-260,150);unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,unreal.Vector(0,0,85)))
 actors.clear_actor_selection_set()
 stages=[]
 times={'Idle':[('guard',0)],'AltSlash05Repair1':[('ready',.1),('windup',.3),('contact',.4833333),('recovery',1.1)],'Primary02':[('ready',.3),('contact',.6666667),('recovery',1.2)],'Primary03':[('ready',.6),('contact',.9333333),('recovery',1.75)],'Special':[('ready',0),('windup',.8),('contact',1.1333333),('recovery',1.85)],'Charged':[('ready',1.35),('windup',1.5),('contact',1.75),('recovery',3.3)]}
 for name,points in times.items():
  clip=unreal.load_asset(FOLDER+'/A_GreatSword_'+name+'_Human')
  if not clip:raise RuntimeError('Missing staged clip '+name)
  for label,t in points:stages.append((name,label,t,clip))
 STATE.update({'comp':comp,'sword':sword,'stages':stages,'index':0,'frame':0,'rows':[],'task':None,'capture_start':0})
 STATE['callback']=unreal.register_slate_post_tick_callback(step)
def step(dt):
 s=STATE
 try:
  if s['index']>=len(s['stages']):
   (OUT/'captures.json').write_text(json.dumps(s['rows'],indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(s['callback'])
   unreal.log('RESULT=WEAPON_PREVIEW_CAPTURED');unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
  name,label,t,clip=s['stages'][s['index']]
  if s['frame']==0:s['comp'].play_animation(clip,False);s['comp'].set_play_rate(0);s['comp'].set_position(t,False);unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'Rogue10m.RefreshSwordPreview');s['stage_started']=time.monotonic()
  if s['task'] is None and time.monotonic()-s['stage_started']>=2.0:
   opts=unreal.AnimPoseEvaluationOptions();opts.optional_skeletal_mesh=s['comp'].get_skeletal_mesh_asset();opts.extract_root_motion=False;opts.incorporate_root_motion_into_pose=True
   pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,t,opts)
   errors={}
   for bone in ['hand_r','hand_l','head']:
    expected=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
    actual=s['comp'].get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    errors[bone]=(actual-expected).length()
   if max(errors.values())>1.:raise RuntimeError('Rendered pose is stale '+str(errors))
   s['pose_errors']=errors
   rp=(s['comp'].get_socket_location('hand_r')+s['comp'].get_socket_location('middle_01_r'))*.5
   lp=(s['comp'].get_socket_location('hand_l')+s['comp'].get_socket_location('middle_01_l'))*.5
   finger=s['comp'].get_socket_location('middle_01_r')-s['comp'].get_socket_location('hand_r')
   z=rp-lp;rotation=unreal.MathLibrary.make_rot_from_zy(z,finger)
   s['sword'].set_actor_location_and_rotation(rp,rotation,False,False)
   filename=OUT/(name+'_'+label+'.png');s['filename']=filename;s['task']=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(filename));s['capture_start']=time.monotonic()
   if not s['task'] or not s['task'].is_valid_task():raise RuntimeError('Invalid screenshot task')
  s['frame']+=1
  if s['task']:
   if time.monotonic()-s['capture_start']>60:raise RuntimeError('Screenshot completion timeout')
   if s['task'].is_task_done():
    if not s['filename'].exists() or s['filename'].stat().st_size<1024:raise RuntimeError('Screenshot missing/empty '+str(s['filename']))
    s['rows'].append({'clip':clip.get_path_name(),'time':t,'image':s['filename'].name,'bytes':s['filename'].stat().st_size,'pose_error_cm':s['pose_errors'],'source':'isolated Unreal Editor pose preview, not live combat'})
    s['task']=None;s['frame']=0;s['index']+=1

 except Exception:
  unreal.unregister_slate_post_tick_callback(s['callback']);unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
try:begin()
except Exception:
 unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
