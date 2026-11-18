"""Editor-only isolated 30-candidate QA. Prepared script, never auto-executed on import.
Run through the official MCP Python tool only after an exclusive editor lease:
main(['--manifest', '<authoring manifest>', '--output', '<fresh directory>',
      '--lease-granted', '--allow-transient-map'])
No asset/DA/map save, no combat activation. One CPU sprite burst is a single flash,
not a ribbon. Screenshots require parent visual review, not automatic art approval.
API provenance: UE5.8 Actor.h/ActorConstruction.cpp, PyGenUtil.cpp/PyWrapperObject.cpp,
NiagaraComponent.h/.cpp, NiagaraSimCache.h, NiagaraSimCacheFunctionLibrary.h.
AddComponentByClass has ScriptNoExport; FinishAddComponent has InternalUseOnly.
Use named component construction and Object.call_method reflected FinishAddComponent.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, time
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
EVIDENCE=ROOT/'Feature/doc/evidence/sword-knuckle-20261110'
ANIM='/Game/Rogue10m/Animation/Combat/GreatSword20261110'
HUMAN='/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody'
MANNY='/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
RETARGET={'GreatSword':ANIM+'/ABP_Human_GreatSword',
          'Knuckle':'/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M'}
BONES=('hand_r','hand_l','middle_01_r','middle_01_l','head')
STATE={}

def load(path):
    obj=unreal.load_asset(path)
    if not obj: raise RuntimeError('Missing asset '+path)
    return obj

def vec(v): return [v.x,v.y,v.z]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def package_file(asset): return ROOT/'Content'/(asset.split('.')[0][6:]+'.uasset')
def cpose(comp):
    return {b:comp.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).translation for b in BONES}
def refresh():
    unreal.SystemLibrary.execute_console_command(STATE['world'],'Rogue10m.RefreshSwordPreview')
def write_report():
    s=STATE
    (s['out']/'captures.json').write_text(json.dumps(s['report'],indent=2),encoding='utf-8')

def stop(error=None):
    s=STATE
    if s.get('callback'):
        unreal.unregister_slate_post_tick_callback(s['callback']); s['callback']=None
    # Do NOT destroy or replace anything while screenshot task completion is uncertain.
    s['report']['status']='failed' if error else 'captured_pending_visual_review'
    s['report']['error']=str(error) if error else None
    s['report']['completed_count']=len(s['report']['captures'])
    s['report']['pending_screenshot_task']=bool(s.get('task') and not s['task'].is_task_done())
    if not s['report']['pending_screenshot_task']:
        try: cleanup_particle_actor()
        except Exception as exc: s['report']['particle_actor_cleanup_error']=str(exc)
    after={p:sha(Path(p)) for p in s['protected']}
    s['report']['protected_sha256_after']=after
    s['report']['protected_assets_unchanged']=after==s['protected']
    if after!=s['protected']: s['report']['status']='failed_protected_asset_changed'
    write_report()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    unreal.log(('RESULT=PARTICLE_PREVIEW_FAILED ' + str(error)) if error else 'RESULT=PARTICLE_PREVIEW_CAPTURED visual_review_pending=true')

def animation_specs():
    sword=json.loads((EVIDENCE/'greatsword-activation-plan.json').read_text(encoding='utf-8-sig'))['skills']
    result={}
    for skill,spec in sword.items():
        result[('GreatSword',skill)]={'asset':ANIM+'/A_GreatSword_'+spec['source'],
            'contact':spec['contact'],'cast':spec['start'],'trajectory':max(spec['start'],spec['contact']-.04),
            'evidence':'greatsword-activation-plan.json; source clip times, not gameplay delay'}
    with (EVIDENCE/'SwordAuthoredA/frames.csv').open(encoding='utf-8-sig',newline='') as file:
        for row in csv.DictReader(file):
            if not row['skill'].startswith('DA_Attack_Knuckle_') or float(row['damage'])<=0: continue
            skill=row['skill'].removeprefix('DA_Attack_Knuckle_'); key=('Knuckle',skill)
            if key in result: continue
            contact=float(row['montage_source_s']); asset=row['montage_source_path']
            if contact<0 or not asset.startswith('/Game/'): raise RuntimeError('Invalid observed contact source')
            result[key]={'asset':asset,'contact':contact,'cast':max(0,contact-.10),
                'trajectory':max(0,contact-.03),'evidence':'SwordAuthoredA/frames.csv first damage>0 row',
                'observed_frame':int(row['frame'])}
    if len(result)!=10: raise RuntimeError('Missing existing ten-skill animation evidence')
    return result

def setup_mesh(comp,mesh):
    comp.set_skeletal_mesh_asset(load(mesh)); comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_receives_decals(False); comp.set_enable_animation(True)
    comp.set_editor_property('pause_anims',False); comp.set_update_animation_in_editor(True)
    comp.set_component_tick_enabled(True)
    comp.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)

def spawn(cls,location=None,rotation=None):
    obj=STATE['actors'].spawn_actor_from_class(cls,location or unreal.Vector(),rotation or unreal.Rotator(),transient=True)
    if not obj: raise RuntimeError('Transient actor spawn failed')
    STATE['staged'].append(obj); return obj

def mesh_actor(asset):
    actor=spawn(unreal.StaticMeshActor); comp=actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE); comp.set_static_mesh(load(asset))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); comp.set_receives_decals(False)
    return actor

def stage_scene():
    s=STATE
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    s['world']=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not s['world']: raise RuntimeError('Blank editor world missing')
    s['actors']=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # UE5.8 LevelEditorSubsystem.cpp: True REMOVES a named realtime override;
    # it does not enable base realtime and ensures when the override is absent.
    # Preserve user/base/other subsystem state; request redraws during our slate ticks.
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
    s['report']['viewport_refresh']={'method':'EditorInvalidateViewports on slate ticks',
        'realtime_override_changed':False,'user_settings_saved':False}
    floor=mesh_actor('/Engine/BasicShapes/Cube'); floor.set_actor_location(unreal.Vector(0,0,-7),False,False)
    floor.set_actor_scale3d(unreal.Vector(8,8,.1))
    light=spawn(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(pitch=-40,yaw=-35,roll=0))
    light.light_component.set_editor_property('intensity',2.5)
    fill=spawn(unreal.PointLight,unreal.Vector(200,-220,220)); fill.light_component.set_editor_property('intensity',3500.)
    fill.light_component.set_editor_property('attenuation_radius',1000.)
    # Local transient post-process volume, never user settings/config or saved world.
    pp=spawn(unreal.PostProcessVolume); pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    for key,value in {'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,
        'auto_exposure_min_brightness':1.,'auto_exposure_max_brightness':1.,
        'override_auto_exposure_bias':True,'auto_exposure_bias':-1.,
        'override_bloom_intensity':True,'bloom_intensity':.15}.items(): settings.set_editor_property(key,value)
    pp.set_editor_property('settings',settings)
    source_actor=spawn(unreal.SkeletalMeshActor); source_actor.set_actor_label('QA_SwordPreview_20261110')
    source=source_actor.skeletal_mesh_component; setup_mesh(source,MANNY)
    source.set_visibility(False,False) # Do not propagate hidden state to retarget body.
    body=unreal.SkeletalMeshComponent(outer=source_actor,name='QAWeaponPreviewBody')
    setup_mesh(body,HUMAN)
    # Official generic reflection API exposes this non-Python-glue UFUNCTION.
    source_actor.call_method('FinishAddComponent',args=(body,False,unreal.Transform()))
    if body.get_owner()!=source_actor or body.get_attach_parent()!=source or str(body.get_name())!='QAWeaponPreviewBody': raise RuntimeError('Preview body ownership/attachment/name gate failed')
    body.set_visibility(True,False); body.add_tick_prerequisite_component(source)
    s.update(source_actor=source_actor,source=source,body=body)
    s['sword']=mesh_actor('/Game/Weapons/Starter20261110/SM_StarterGreatSword')
    s['knuckles']=[mesh_actor('/Game/Weapons/Starter20261110/SM_StarterKnuckle') for _ in range(2)]
    # Visible target bounds proxy, NOT a real target/damage actor or collision query.
    target=mesh_actor('/Engine/BasicShapes/Cylinder'); target.set_actor_scale3d(unreal.Vector(.68,.68,1.76))
    target.set_actor_location(unreal.Vector(145,0,88),False,False); s['target']=target
    s['target_bounds']={'center':[145,0,88],'radius_cm':34,'half_height_cm':88,'shape':'cylinder bounds proxy for capsule; noncombat'}
    target_hidden=s['report']['review_original_thirty'] or s['diagnostic_subset'] in ('primary01-six','readability-prototype-one')
    if target_hidden:
        target.set_is_temporarily_hidden_in_editor(True); target.set_actor_hidden_in_game(True)
    s['target_bounds']['hidden_in_preview']=target_hidden
    s['report']['target_hidden']=target_hidden
    camera=unreal.Vector(-300,280,200) if target_hidden else unreal.Vector(355,-325,195)
    look=unreal.Vector(40,0,90) if target_hidden else unreal.Vector(52,0,90)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,look))
    s['report']['camera']={'position':vec(camera),'look_at':vec(look)}
    s['actors'].clear_actor_selection_set()

def stage_pose(candidate):
    s=STATE; spec=s['animations'][(candidate['weapon'],candidate['skill'])]
    phase=candidate['phase']; seconds=spec[{'Cast':'cast','Trajectory':'trajectory','Impact':'contact'}[phase]]
    clip=load(spec['asset']); source=s['source']; body=s['body']
    source.play_animation(clip,False); source.set_play_rate(0.); source.set_position(seconds,False)
    # Reference baseline on actual Human mesh; no asset changes.
    body.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE); refresh(); s['body_reference']=cpose(body)
    bp=load(RETARGET[candidate['weapon']]); body.set_anim_instance_class(bp.generated_class())
    refresh(); refresh()
    s['pose']={'source_asset':clip.get_path_name(),'source_time_s':seconds,'clip':clip,'evidence':spec['evidence'],
               'retarget_class':RETARGET[candidate['weapon']]}
    s['stage_started']=time.monotonic(); s['mode']='pose_settle'

def verify_pose_and_weapons(candidate):
    s=STATE; source=s['source']; body=s['body']; refresh()
    opts=unreal.AnimPoseEvaluationOptions(); opts.optional_skeletal_mesh=source.get_skeletal_mesh_asset()
    opts.extract_root_motion=False; opts.incorporate_root_motion_into_pose=True
    pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(s['pose']['clip'],s['pose']['source_time_s'],opts)
    source_errors={b:(source.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).translation-unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD).translation).length() for b in BONES}
    if max(source_errors.values())>1.: raise RuntimeError('Source pose is stale/refpose: '+str(source_errors))
    current=cpose(body); deltas={b:(current[b]-s['body_reference'][b]).length() for b in BONES}
    if max(deltas[b] for b in ('hand_r','hand_l'))<1.: raise RuntimeError('Human retarget output remains reference/stale pose')
    if not body.get_anim_instance(): raise RuntimeError('Human retarget instance missing')
    s['pose']['source_pose_error_cm']=source_errors; s['pose']['body_reference_delta_cm']=deltas
    palms={side:(body.get_socket_location('hand_'+side)+body.get_socket_location('middle_01_'+side))*.5 for side in ('r','l')}
    sword_mode=candidate['weapon']=='GreatSword'; s['sword'].set_actor_hidden_in_game(not sword_mode)
    s['sword'].set_is_temporarily_hidden_in_editor(not sword_mode)
    for item in s['knuckles']: item.set_is_temporarily_hidden_in_editor(sword_mode)
    if sword_mode:
        axis=palms['r']-palms['l']; finger=body.get_socket_location('middle_01_r')-body.get_socket_location('hand_r')
        if axis.length()<1 or finger.length()<.1: raise RuntimeError('Degenerate sword grip basis')
        rotation=unreal.MathLibrary.make_rot_from_zy(axis,finger)
        s['sword'].set_actor_location_and_rotation(palms['r'],rotation,False,False)
        blade_mid=palms['r']+axis/axis.length()*70.
        locations={'Cast':palms['r'],'Trajectory':blade_mid}
        s['grip']={'method':'two palms + right middle finger Y roll','right_palm':vec(palms['r']),'left_palm':vec(palms['l']),'blade_mid_proxy':vec(blade_mid),'art_approved':False}
    else:
        for side,item in zip(('r','l'),s['knuckles']):
            hand=body.get_socket_transform('hand_'+side,unreal.RelativeTransformSpace.RTS_WORLD)
            rotation=hand.rotation
            if side=='l': rotation=rotation*unreal.Quat(1.,0.,0.,0.)
            item.set_actor_location_and_rotation(body.get_socket_location('middle_01_'+side),rotation.rotator(),False,False)
        locations={'Cast':palms['r'],'Trajectory':body.get_socket_location('middle_01_r')}
        s['grip']={'method':'hand orientation and animated middle_01 position; left local X roll 180','art_approved':False}
    # Bounds feedback near forward-facing target surface, not precise collision contact.
    locations['Impact']=unreal.Vector(109,0,100)
    return locations[candidate['phase']]

def simulation_schedule(candidate):
    lifetime=float(candidate['lifetime_sim_seconds'])
    if not 0.<lifetime<=.5: raise RuntimeError('Invalid candidate lifetime')
    # Eight whole steps, with delta derived from lifetime: no rounding to fixed 10ms.
    ticks=8; fraction=.25; target=lifetime*fraction; delta=target/ticks
    return {'lifetime_sim_seconds':lifetime,'target_relative_age':fraction,
            'target_age_s':target,'tick_count':ticks,'tick_delta_s':delta,
            'commanded_advance_s':ticks*delta,'age_tolerance_s':delta+1.e-4,
            'tolerance_reason':'At most one simulation step for birth-tick age plus 0.1ms float tolerance'}

def runtime_da_conditions(candidate):
    # Read-only snapshot. Do not apply these values to the isolated candidate preview.
    da=load(candidate['existing_skill_asset'])
    fields=('cast_effect_scale','impact_effect_scale','attack_effect_time_dilation',
            'cast_effect_emission_duration','impact_effect_emission_duration',
            'use_first_person_effect_overrides','first_person_cast_scale_multiplier',
            'first_person_impact_scale_multiplier','first_person_emission_duration_multiplier')
    return {'asset':da.get_path_name(),'read_only':True,
            'values':{key:da.get_editor_property(key) for key in fields},
            'trajectory_runtime_binding':'none; candidate only'}

def niagara_evidence(comp):
    schedule=STATE['simulation_schedule']
    if not unreal.SystemLibrary.is_valid(comp) or not comp.is_active(): raise RuntimeError('Niagara invalid/inactive')
    was_paused=comp.is_paused()
    diagnostic={'candidate_index':STATE.get('index'),'mode':STATE.get('mode'),
        'paused_before':was_paused,'capture_advance_simulation':False,
        'probe_evidence':'Feature/doc/evidence/sword-knuckle-20261110/particle-candidates/probe-a.json',
        'binding_contract':'Direct NiagaraSimCache or None; dataset key Age, not Particles.Age',
        'capture_contract':'Synchronous unpaused capture; finally restore original pause state; no world tick/yield'}
    STATE['report'].setdefault('simcache_binding_observations',[]).append(diagnostic)
    try:
        # ProbeA proves Paused -> WriteFrame false; unpause restores Running without
        # advancing age. UE5.8 NiagaraSimCache.cpp WriteFrame requires Running.
        if was_paused: comp.set_paused(False)
        if comp.is_paused(): raise RuntimeError('Could not enter running capture state')
        cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(STATE['world'])
        params=unreal.NiagaraSimCacheCreateParameters()
        params.set_editor_property('attribute_capture_mode',unreal.NiagaraSimCacheAttributeCaptureMode.ALL)
        captured=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,params,comp,False,schedule['tick_delta_s'])
        diagnostic.update(return_type=type(captured).__module__+'.'+type(captured).__name__,return_repr=repr(captured)[:2000])
        if captured is None: raise RuntimeError('Immediate SimCache returned None while unpaused; no retry')
        if not isinstance(captured,unreal.NiagaraSimCache): raise RuntimeError('Unexpected SimCache Python return type')
        cache=captured
        if cache.get_num_frames()!=1 or cache.get_num_emitters()!=1: raise RuntimeError('Expected one valid cached CPU emitter frame')
        emitter=cache.get_emitter_name(0)
        ages=list(cache.read_float_attribute('Age',emitter,0))
        positions=list(cache.read_position_attribute('Position',emitter,True,0))
        # UE5.8 NiagaraSimCache.h:540/547/568; optional post-update dataset reads, not material output.
        appearance={}
        for attr,method,fields in (('Color','read_color_attribute',('r','g','b','a')),
                                   ('SpriteSize','read_vector2_attribute',('x','y')),
                                   ('NormalizedAge','read_float_attribute',())):
            try:
                raw=list(getattr(cache,method)(attr,emitter,0))
                values=[[float(getattr(v,k)) for k in fields] if fields else float(v) for v in raw]
                appearance[attr]={'status':'available' if values else 'unavailable','values':values,
                                  'instance_count':len(values),'reason':None if values else 'Empty cache attribute; absent/trimmed or type mismatch, not inferred as zero'}
            except Exception as exc:
                appearance[attr]={'status':'unavailable','reason':type(exc).__name__+': '+str(exc)}
        diagnostic['post_update_appearance']=appearance
        system_age=cache.get_start_seconds(); target=schedule['target_age_s']; tolerance=schedule['age_tolerance_s']
        diagnostic.update(actual_system_age_s=system_age,particle_ages_s=ages,position_instance_count=len(positions),simulation_schedule=schedule)
        if len(ages)!=1 or len(positions)!=1 or not 0.<ages[0]<schedule['lifetime_sim_seconds']: raise RuntimeError('Expected exactly one living Age/Position particle')
        if not abs(system_age-target)<=tolerance or not abs(ages[0]-target)<=tolerance:
            raise RuntimeError('Actual SimCache ages outside lifetime25% target/tolerance: '+str(diagnostic))
        evidence={'active':True,'valid':True,'actual_dataset_verified':True,
            'post_update_appearance':appearance,
            'actual_system_age_s':system_age,'particle_ages_s':ages,
            'particle_relative_ages':[v/schedule['lifetime_sim_seconds'] for v in ages],
            'system_age_error_s':system_age-target,'particle_age_error_s':ages[0]-target,
            'particle_positions_world':[vec(v) for v in positions],'position_instance_count':len(positions),
            'living_particle_count':len(ages),'simulation_schedule':schedule,'cpu_emitters':1}
    finally:
        if unreal.SystemLibrary.is_valid(comp):
            comp.set_paused(was_paused)
            diagnostic['paused_after']=comp.is_paused()
            if comp.is_paused()!=was_paused: raise RuntimeError('Could not restore original Niagara pause state')
        else: diagnostic['component_invalid_during_capture']=True
    evidence['paused']=comp.is_paused()
    return evidence


def render_diagnostics(comp,candidate):
    owner=comp.get_owner()
    result={'component':comp.get_path_name(),'component_class':comp.get_class().get_path_name(),
        'owner_name':owner.get_name() if owner else None,'owner_class':owner.get_class().get_path_name() if owner else None,
        'owner_path':owner.get_path_name() if owner else None,'owner_label':owner.get_actor_label() if owner else None,
        'owner_hidden_in_editor':owner.is_hidden_ed() if owner else None,
        'component_is_visible':comp.is_visible(),'active':comp.is_active(),'paused':comp.is_paused(),
        'actual_scene_proxy_verified':False,'rendered_visibility_approval':'pending image review',
        'owner_route':'Transient NiagaraActor, not SpawnSystemAtLocation WorldSettings owner'}
    for prefix,obj,names in [('component',comp,('rendering_enabled','visible','hidden_in_game','render_in_main_pass','render_custom_depth','owner_no_see','only_owner_see','cast_shadow','visible_in_scene_capture_only','hidden_in_scene_capture')),
                             ('owner',owner,('hidden','hidden_ed','hidden_ed_temporary'))]:
        for name in names:
            try: result[prefix+'_'+name]=obj.get_editor_property(name)
            except Exception as exc: result[prefix+'_'+name]={'readable':False,'reason':str(exc)[:250]}
    # AInfo starts hidden, but WorldSettings.cpp:139 explicitly SetHidden(false).
    # Owner type alone cannot explain a missing effect. Record live flags instead.
    result['owner_route_isolation']='Matches successful magic preview, not proof that WorldSettings was hidden'
    result['world_settings_comparison']=[]
    for actor in STATE['actors'].get_all_level_actors():
        if isinstance(actor,unreal.WorldSettings):
            row={'path':actor.get_path_name(),'class':actor.get_class().get_path_name(),
                 'hidden_in_editor':actor.is_hidden_ed()}
            try:row['hidden_in_game']=actor.get_editor_property('hidden')
            except Exception as exc:row['hidden_in_game_read_error']=str(exc)[:250]
            result['world_settings_comparison'].append(row)
    try:
        override=unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.Particles')
        result['show_particles_console_override']=override
        result['show_particles_interpretation']='0 forces off, 1 forces on, other values do not establish active viewport default'
    except Exception as exc:result['show_particles_query_error']=str(exc)[:250]
    result['active_viewport_default_show_particles_verified']=False
    try:
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(comp)
        result['component_render_bounds']={'origin':vec(origin),'extent':vec(extent),'sphere_radius':radius}
    except Exception as exc:result['component_bounds_query_error']=str(exc)[:250]
    for label,query in [('system_fixed_bounds',lambda:comp.get_system_fixed_bounds()),
                        ('emitter_fixed_bounds',lambda:comp.get_emitter_fixed_bounds('DirectionalBurst'))]:
        try:result[label]=str(query())
        except Exception as exc:result[label]={'readable':False,'reason':str(exc)[:250]}
    result['bounds_limitation']='Component bounds and optional fixed bounds; not per-renderer RHI bounds. Invalid fixed box may mean dynamic bounds.'
    authored=candidate.get('readback',{})
    result['renderer_authoring_evidence']={'source':'existing authoring manifest, not live renderer proxy inspection',
        'renderer':authored.get('renderer'),'topology_renderers':authored.get('topology',{}).get('renderers'),
        'summary':authored.get('summary')}
    result['live_renderer_enabled_and_rhi_bounds_verified']=False
    return result

def cleanup_particle_actor():
    s=STATE; actor=s.get('particle_actor'); comp=s.get('particle')
    if not actor:return
    if not unreal.SystemLibrary.is_valid(actor): raise RuntimeError('Owned particle actor invalid before cleanup')
    if actor.get_path_name()!=s.get('particle_actor_path') or not isinstance(actor,unreal.NiagaraActor) or actor.get_actor_label()!=s.get('particle_actor_label'):
        raise RuntimeError('Particle actor ownership changed; refusing cleanup')
    if comp and unreal.SystemLibrary.is_valid(comp) and comp.get_owner()!=actor: raise RuntimeError('Particle component owner changed')
    path=actor.get_path_name()
    if not s['actors'].destroy_actor(actor): raise RuntimeError('Owned NiagaraActor destruction failed')
    s['report'].setdefault('destroyed_particle_actors',[]).append(path)
    s['particle']=None;s['particle_actor']=None

def stage_particle(candidate):
    s=STATE; location=verify_pose_and_weapons(candidate); system=load(candidate['asset'])
    # Match the successful magic preview: visible transient actor owns the component.
    # SpawnSystemAtLocation uses WorldSettings; its constructor overrides AInfo
    # to SetHidden(false). Owner hidden is NOT established: this is route isolation
    # matching the successful magic preview, with live flags recorded below.
    actor=spawn(unreal.NiagaraActor,location)
    label='QA_WeaponParticlePreview_'+str(s['index'])
    actor.set_actor_label(label,False); actor.set_actor_hidden_in_game(False)
    actor.set_is_temporarily_hidden_in_editor(False)
    s.update(particle_actor=actor,particle_actor_path=actor.get_path_name(),particle_actor_label=label)
    comp=actor.get_component_by_class(unreal.NiagaraComponent)
    if not comp or comp.get_owner()!=actor: raise RuntimeError('NiagaraActor component ownership failed')
    s['particle']=comp
    comp.set_auto_activate(False); comp.set_asset(system,True); comp.set_auto_destroy(False)
    comp.set_relative_scale3d(unreal.Vector(1,1,1)); comp.set_visibility(True,False)
    comp.set_hidden_in_game(False,False)
    schedule=simulation_schedule(candidate); s['simulation_schedule']=schedule
    comp.set_force_solo(True)
    comp.set_age_update_mode(unreal.NiagaraAgeUpdateMode.TICK_DELTA_TIME)
    comp.set_custom_time_dilation(1.)
    comp.set_paused(False); comp.activate(True)
    if comp.is_paused(): raise RuntimeError('Manual advance must not run paused')
    comp.advance_simulation(schedule['tick_count'],schedule['tick_delta_s']); comp.set_paused(True)
    evidence=niagara_evidence(comp)
    s['current']={'weapon':candidate['weapon'],'skill':candidate['skill'],'phase':candidate['phase'],
        'source_asset':s['pose']['source_asset'],'source_time_s':s['pose']['source_time_s'],
        'retarget_class':s['pose']['retarget_class'],'source_pose_error_cm':s['pose']['source_pose_error_cm'],
        'body_reference_delta_cm':s['pose']['body_reference_delta_cm'],'timing_evidence':s['pose']['evidence'],
        'niagara_asset':candidate['asset'],'niagara':evidence,'location':vec(location),'grip':s['grip'],
        'render_diagnostics':render_diagnostics(comp,candidate),
        'target_bounds':s['target_bounds'],'target_hidden':s['report']['target_hidden'],
        'camera':s['report']['camera'],'full_batch':s['report']['full_batch'],
        'review_original_thirty':s['report']['review_original_thirty'],
        'diagnostic_subset':s['diagnostic_subset'],'readability_prototype':s['report']['readability_prototype'],'activated_in_gameplay':False,'actual_combat':False,
        'preview_conditions':{'uniform_scale':1.,'custom_time_dilation':1.,'sample_relative_lifetime':.25,
            'force_solo':True,'age_update_mode':'TICK_DELTA_TIME',
            'runtime_equivalent':False,'limitation':'Isolated scale1/dilation1 candidate preview; current runtime DA scale, dilation, emission timers and first-person overrides are NOT applied. Not runtime visibility approval.'},
        'runtime_da_conditions_read_only':runtime_da_conditions(candidate),
        'trajectory':'single burst card/flash, not continuous ribbon or blade tracking',
        'attachment':'frozen pose placement only; no gameplay binding','visual_review':'pending'}
    s['stage_started']=time.monotonic(); s['mode']='particle_settle'

def step(dt):
    s=STATE
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
        if s['index']==len(s['candidates']): stop(); return
        candidate=s['candidates'][s['index']]
        if s['mode']=='next': stage_pose(candidate)
        elif s['mode']=='pose_settle' and time.monotonic()-s['stage_started']>=2.: stage_particle(candidate)
        elif s['mode']=='particle_settle' and time.monotonic()-s['stage_started']>=2.:
            s['current']['niagara_at_capture']=niagara_evidence(s['particle'])
            s['current']['render_diagnostics_at_capture']=render_diagnostics(s['particle'],candidate)
            if not s['particle'].is_paused(): raise RuntimeError('Screenshot requires paused Niagara after measurement')
            s['filename']=s['out']/(f"{s['index']+1:02d}_{candidate['weapon']}_{candidate['skill']}_{candidate['phase']}.png")
            s['task']=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(s['filename']))
            if not s['task'] or not s['task'].is_valid_task(): raise RuntimeError('Invalid ScreenshotTask')
            s['capture_started']=time.monotonic(); s['mode']='capture'
        elif s['mode']=='capture':
            if time.monotonic()-s['capture_started']>90: raise RuntimeError('Screenshot completion timeout; scene retained')
            if s['task'].is_task_done():
                if not s['filename'].is_file() or s['filename'].stat().st_size<=1024: raise RuntimeError('Missing/empty PNG')
                s['current'].update(image=s['filename'].name,bytes=s['filename'].stat().st_size,image_sha256=sha(s['filename']),screenshot_task_completed=True)
                s['report']['captures'].append(s['current']); write_report()
                # Only after completion: destroy the prior transient component and advance.
                cleanup_particle_actor()
                s['task']=None; s['index']+=1; s['mode']='next'
    except Exception as exc: stop(exc)


def select_capture_candidates(candidates, diagnostic_subset):
    # Called only AFTER full 30-manifest/compile/source validation and protection hashes.
    if diagnostic_subset is None: return list(candidates)
    if diagnostic_subset!='primary01-six': raise RuntimeError('Unknown diagnostic subset')
    selected=[c for c in candidates if c['weapon'] in ('GreatSword','Knuckle') and c['skill']=='Primary01']
    expected={(w,'Primary01',phase) for w in ('GreatSword','Knuckle') for phase in ('Cast','Trajectory','Impact')}
    if len(selected)!=6 or {(c['weapon'],c['skill'],c['phase']) for c in selected}!=expected: raise RuntimeError('Diagnostic subset must contain exact representative six')
    order={(w,phase):i for i,(w,phase) in enumerate((w,phase) for w in ('GreatSword','Knuckle') for phase in ('Cast','Trajectory','Impact'))}
    return sorted(selected,key=lambda c:order[(c['weapon'],c['phase'])])

def select_readability_prototype(path, manifest, protected):
    """Validate saved one-prototype provenance; never edit original30 or the prototype."""
    prototype=json.loads(path.read_text(encoding='utf-8-sig'))
    digest=hashlib.sha256(json.dumps(manifest,sort_keys=True,ensure_ascii=False).encode()).hexdigest()
    if prototype.get('source_manifest_sha256')!=digest: raise RuntimeError('Prototype original30 manifest digest mismatch')
    source=next(c for c in manifest['candidates'] if (c['weapon'],c['skill'],c['phase'])==('GreatSword','Primary01','Cast'))
    if prototype.get('source_candidate')!=source or prototype.get('source_asset')!=source['asset']: raise RuntimeError('Prototype source provenance mismatch')
    if prototype.get('status')!='one_prototype_saved_compile_checked_visual_preview_pending' or not prototype.get('saved') or prototype.get('activated') is not False or not prototype.get('protected_assets_unchanged'): raise RuntimeError('Prototype authoring not successful/unactivated')
    state=prototype['compile']; issues=prototype['issues']
    if state.get('bHasErrors',True) or state.get('bIsCompiling',True) or state.get('bIsStale',True) or issues.get('numErrors',1): raise RuntimeError('Prototype compile gate failed')
    asset=prototype['asset']; material=prototype['material_asset']
    base='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/ReadabilityCandidates/'
    if not asset.startswith(base) or not material.startswith(base) or asset.rsplit('/',1)[0]!=material.rsplit('/',1)[0]: raise RuntimeError('Prototype destination outside isolated scope')
    if set(prototype['created_paths'])!={asset,material} or set(prototype['created_hashes'])!={asset,material}: raise RuntimeError('Expected exactly two prototype assets')
    if prototype['rgb_gain']!=8 or not prototype['alpha_preserved']: raise RuntimeError('Expected RGB gain8 only')
    for k,v in prototype['inherited_cue_tint'].items():
        if abs(prototype['actual_cue_tint'][k]-v*(1 if k=='a' else 8))>1e-5: raise RuntimeError('Prototype tint provenance mismatch')
    if json.loads(prototype['readback']['renderer']['propertyValues'])['Material']!={'refPath':material+'.'+material.rsplit('/',1)[1]}: raise RuntimeError('Prototype renderer material mismatch')
    for key in ('all_inputs','lifecycle','emitter'):
        if prototype['readback'][key]!=prototype['source_readback'][key]: raise RuntimeError('Unexpected prototype input change')
    expected=prototype['protected_hashes_before']
    if len(expected)!=56 or expected!=prototype['protected_hashes_after']: raise RuntimeError('Prototype protection evidence failed')
    for package,value in {**expected,**prototype['created_hashes']}.items():
        file=package_file(package)
        if sha(file)!=value: raise RuntimeError('Prototype/protected package changed: '+package)
        protected[str(file)]=value
    selected=dict(source,asset=asset,readback=prototype['readback'],compile=state,issues=issues)
    evidence={'manifest':str(path),'manifest_sha256':sha(path),'source_asset':source['asset'],
        'asset':asset,'material_asset':material,'rgb_gain':8,'alpha_preserved':True,
        'inherited_cue_tint':prototype['inherited_cue_tint'],'actual_cue_tint':prototype['actual_cue_tint'],
        'scope':'One isolated prototype only; original30 unchanged; not runtime visibility approval'}
    return [selected],evidence



def validate_original_thirty_review(manifest, protected):
    # Pin the original manifest and the successful B preservation evidence, not its prototype assets.
    original_digest='51bf88116f2165f2b7ac90e439ddb761ea37349a99b65482153837daf78a5670'
    actual=hashlib.sha256(json.dumps(manifest,sort_keys=True,ensure_ascii=False).encode()).hexdigest()
    if actual!=original_digest: raise RuntimeError('Original thirty manifest digest mismatch; no overrides allowed')
    baseline=EVIDENCE/'particle-candidates/ReadabilityGain8B/manifest.json'
    if sha(baseline)!='01828feeb6dabcdc526b99e79abc5e155eceeabfc3946bd830a3b9678fae797b': raise RuntimeError('Protected56 baseline evidence changed')
    evidence=json.loads(baseline.read_text(encoding='utf-8-sig'))
    expected=evidence['protected_hashes_after']
    if len(expected)!=56 or expected!=evidence['protected_hashes_before'] or not evidence['protected_assets_unchanged']: raise RuntimeError('Invalid protected56 baseline')
    prefix='/Game/Rogue10m/VFX/Character/Combat/WeaponFantasy20261110/SkillCandidates/SkillParticlesA/'
    if len({c['asset'] for c in manifest['candidates']})!=30 or any(c['asset']!=prefix+c['name'] for c in manifest['candidates']): raise RuntimeError('Review requires exact original30 asset paths')
    for package,value in expected.items():
        file=package_file(package)
        if sha(file)!=value: raise RuntimeError('Protected56 asset changed: '+package)
        protected[str(file)]=value
    return {'original_manifest_digest':actual,'protected_package_count':56,
        'baseline_evidence':str(baseline),'brightness_override':False,'prototype_selected':False,
        'visual_approval':'pending parent review of each actual image; numerical gates are not visibility approval'}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--manifest',required=True); p.add_argument('--output',required=True)
    p.add_argument('--lease-granted',action='store_true'); p.add_argument('--allow-transient-map',action='store_true')
    p.add_argument('--diagnostic-subset',choices=['primary01-six','readability-prototype-one'],default=None,
        help='Capture only GreatSword/Knuckle Primary01 three phases; reverse camera, hide target proxy; NOT full batch')
    p.add_argument('--prototype-manifest',help='Required saved authoring manifest for readability-prototype-one')
    p.add_argument('--review-original-thirty',action='store_true',help='Explicit base30 review only: original assets, reverse camera, hidden target; no prototype or brightness override')
    a=p.parse_args(argv)
    if not a.lease_granted or not a.allow_transient_map: raise RuntimeError('Exclusive lease and transient-map authorization required')
    if a.review_original_thirty and (a.diagnostic_subset or a.prototype_manifest): raise RuntimeError('Original30 review cannot combine with diagnostic/prototype overrides')
    if not a.review_original_thirty and a.diagnostic_subset not in ('primary01-six','readability-prototype-one'): raise RuntimeError('Select explicit diagnostic or --review-original-thirty')
    if bool(a.prototype_manifest)!=(a.diagnostic_subset=='readability-prototype-one'): raise RuntimeError('Prototype manifest required only in explicit one-prototype mode')
    if STATE: raise RuntimeError('One preview per Python session; inspect existing state first')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(): raise RuntimeError('Dirty editor packages: do not discard/save them automatically')
    manifest_path=Path(a.manifest).resolve(); manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    candidates=manifest['candidates']
    if len(candidates)!=30 or len({(c['weapon'],c['skill'],c['phase']) for c in candidates})!=30: raise RuntimeError('Explicit manifest must contain 30 distinct candidates')
    if any(not c.get('saved') or c.get('activated') is not False for c in candidates): raise RuntimeError('Require saved unactivated candidates')
    expected={(w,k,p) for w in ('GreatSword','Knuckle') for k in ('Primary01','Primary02','Primary03','Special','Charged') for p in ('Cast','Trajectory','Impact')}
    if {(c['weapon'],c['skill'],c['phase']) for c in candidates}!=expected: raise RuntimeError('Manifest does not cover exact ten skills x three phases')
    for c in candidates:
        state=c.get('compile',{}); issues=c.get('issues',{})
        if state.get('bHasErrors',True) or state.get('bIsCompiling',True) or state.get('bIsStale',True) or issues.get('numErrors',1)!=0: raise RuntimeError('Candidate compile/issue evidence missing or failed')
    for asset,expected_hash in manifest['source_hashes_before'].items():
        if sha(package_file(asset))!=expected_hash: raise RuntimeError('Source changed since authoring; review before preview')
    out=Path(a.output).resolve()
    if out.exists(): raise RuntimeError('Fresh evidence output directory required')
    animations=animation_specs()
    paths=set(manifest['source_hashes_before'])|{c['asset'] for c in candidates}|{c['existing_skill_asset'] for c in candidates}
    protected={str(package_file(path)):sha(package_file(path)) for path in paths}
    old_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_map=old_world.get_path_name().split('.')[0] if old_world else ''
    if old_map.startswith('/Game/'):
        old_file=ROOT/'Content'/(old_map[6:]+'.umap')
        if old_file.is_file(): protected[str(old_file)]=sha(old_file)
    original_review=validate_original_thirty_review(manifest,protected) if a.review_original_thirty else None
    prototype_evidence=None
    if a.diagnostic_subset=='readability-prototype-one':
        selected,prototype_evidence=select_readability_prototype(Path(a.prototype_manifest).resolve(),manifest,protected)
    else: selected=select_capture_candidates(candidates,a.diagnostic_subset)
    out.mkdir(parents=True,exist_ok=False)
    STATE.update(out=out,candidates=selected,diagnostic_subset=a.diagnostic_subset,animations=animations,protected=protected,staged=[],index=0,mode='next',task=None,
        report={'status':'started','manifest':str(manifest_path),'manifest_sha256':sha(manifest_path),'captures':[],
            'expected_count':len(selected),'manifest_candidate_count':len(candidates),
            'full_batch':a.review_original_thirty,'review_original_thirty':a.review_original_thirty,
            'original_thirty_review':original_review,'diagnostic_subset':a.diagnostic_subset,
            'selected_assets':[c['asset'] for c in selected],'readability_prototype':prototype_evidence,
            'actual_combat':False,'activated_in_gameplay':False,'visual_review':'pending',
            'diagnostic_limitation':'Isolated frozen-pose model preview, not combat. Diagnostic camera and hidden target remove occlusion only; preview performs no material/brightness/asset writes; optional explicitly selected gain8 prototype is reported separately. Numeric dataset success is not visual approval.',
            'protected_sha256_before':protected,'saved_assets_or_maps':False})
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    try:
        stage_scene(); STATE['callback']=unreal.register_slate_post_tick_callback(step); write_report()
    except Exception as exc: stop(exc); raise

if __name__=='__main__': main()
