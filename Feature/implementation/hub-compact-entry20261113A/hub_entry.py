"""Normal source-project Hub inspection/authoring through public UE APIs only."""
from pathlib import Path
from datetime import datetime, timezone
import json
import os
import traceback
import unreal

R=Path('D:/Project/Rogue10m')
E=R/'Feature/doc/evidence/hub-compact-entry20261113A'
HUB='/Game/Rogue10m/Maps/StartHub'
MAP='/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A'
DA='/Game/Rogue10m/World/Production/WorldLayoutCompact20261113A/Quests/DA_IntroSupply_Compact20261113B'
TAG='Rogue10m.Compact.HubEntry20261113A'
PHASE=os.environ['R10_HUB_PACKET_PHASE']
assert PHASE in ('inspect01','inspect02','inspect03','author01','author02','cold01','diagnose_cold02','collision01','cold03')
assert Path(unreal.Paths.project_dir()).resolve()==R


def vec(v):return [v.x,v.y,v.z]
def rot(v):return [v.pitch,v.yaw,v.roll]
def path(v):return v.get_path_name() if v else None
def dirty():
    return list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
def clean():assert not dirty(),[p.get_name() for p in dirty()]
def write(name,data):
    with (E/name).open('x',encoding='utf-8') as stream:json.dump(data,stream,ensure_ascii=False,indent=2)
def load_map(name):
    clean()
    w=unreal.EditorLoadingAndSavingUtils.load_map(name)
    assert w and w.get_path_name()==name+'.'+name.rsplit('/',1)[-1]
    clean()
    return w
def actor_record(a):
    o,b=a.get_actor_bounds(True)
    return {'path':a.get_path_name(),'class':a.get_class().get_path_name(),'label':a.get_actor_label(),
            'location_cm':vec(a.get_actor_location()),'rotation':rot(a.get_actor_rotation()),
            'scale':vec(a.get_actor_scale3d()),'collision_bounds_origin':vec(o),'collision_bounds_extent':vec(b),
            'spatially_loaded':a.get_editor_property('is_spatially_loaded'),
            'editor_only':a.get_editor_property('is_editor_only_actor'),
            'tags':[str(t) for t in a.tags]}
def portal_record(a):
    return {**actor_record(a),'target_level_name':str(a.get_editor_property('target_level_name')),
            'start_run_timer_on_travel':a.get_editor_property('start_run_timer_on_travel'),
            'additional_travel_options':a.get_editor_property('additional_travel_options')}
def all_actors():return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def portals():return sorted([portal_record(a) for a in all_actors() if isinstance(a,unreal.Rogue10mRunPortal)],key=lambda a:a['label'])
def load_descs(all_hub=False):
    descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    selected=descs if all_hub else [d for d in descs if path(d.get_editor_property('native_class')) in ('/Script/Engine.PlayerStart','/Script/Rogue10m.Rogue10mVillageQuestDirector')]
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.get_editor_property('guid') for d in selected])
    if PHASE!='inspect01':
        # Installed StaticMeshCompiler.cpp/AsyncCompilationHelpers.cpp: official
        # finish barrier invokes PostStaticMeshCompilation/RecreatePhysicsState.
        # This targets this independent source commandlet, never live D input.
        w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(w,'Editor.AsyncStaticMeshCompilationFinishAll')
    return descs
def mode_record(w):
    mode=w.get_world_settings().get_editor_property('default_game_mode')
    assert mode
    default=unreal.get_default_object(mode)
    assert isinstance(default,unreal.Rogue10mGameMode)
    gs=default.get_editor_property('game_state_class')
    state=unreal.get_default_object(gs)
    return {'class':path(mode),'game_state_class':path(gs),
            'pawn_class':path(default.get_editor_property('default_pawn_class')),
            'start_run_on_start_play':default.get_editor_property('start_run_on_start_play'),
            'use_prototype_run_duration':default.get_editor_property('use_prototype_run_duration'),
            'prototype_run_duration_seconds':default.get_editor_property('prototype_run_duration_seconds'),
            'run_duration_seconds':state.get_editor_property('run_duration_seconds')}
def trace(w,start,end,radius=None):
    common=[w,unreal.Vector(*start),unreal.Vector(*end)]
    if radius is None:
        raw=unreal.SystemLibrary.line_trace_single(*common,unreal.TraceTypeQuery.ECC_VISIBILITY,False,[],unreal.DrawDebugTrace.NONE,True)
    else:
        raw=unreal.SystemLibrary.capsule_trace_single(*common,radius,96.,unreal.TraceTypeQuery.ECC_VISIBILITY,False,[],unreal.DrawDebugTrace.NONE,True)
    if raw is None:
        return {'start':start,'end':end,'blocking_hit':False,'impact_point':None,'impact_normal':None,
                'radius_cm':radius,'half_height_cm':96. if radius is not None else None}
    values=raw if isinstance(raw,tuple) else (raw,)
    hits=[v for v in values if isinstance(v,unreal.HitResult)]
    assert len(hits)==1,(type(raw).__name__,str(raw))
    hit=hits[0]
    # HitResult has HasNativeBreak=GameplayStatics.BreakHitResult. Its public
    # Python native-break tuple has the 18 outputs declared by UE5.8's header.
    fields=hit.to_tuple()
    assert len(fields)==18 and isinstance(fields[0],bool) and isinstance(fields[1],bool)
    assert isinstance(fields[5],unreal.Vector) and isinstance(fields[7],unreal.Vector)
    return {'start':start,'end':end,'blocking_hit':fields[0],'initial_overlap':fields[1],
            'impact_point':vec(fields[5]),'impact_normal':vec(fields[7]),
            'hit_actor':path(fields[9]),'trace_channel':'Visibility',
            'radius_cm':radius,'half_height_cm':96. if radius is not None else None,'trace_channel':'Visibility'}


def inspect():
    candidate=load_map(MAP)
    ds=load_descs()
    starts=[actor_record(a) for a in all_actors() if isinstance(a,unreal.PlayerStart)]
    directors=[a for a in all_actors() if isinstance(a,unreal.Rogue10mVillageQuestDirector)]
    assert len(starts)==len(directors)==1
    definition=unreal.load_asset(DA)
    assert definition and directors[0].get_editor_property('quest_definition')==definition
    supported=path(definition.get_editor_property('supported_world'))
    assert supported==MAP+'.'+MAP.rsplit('/',1)[-1]
    mode=mode_record(candidate)
    assert mode['run_duration_seconds']==1200 and not mode['use_prototype_run_duration']
    assert not mode['start_run_on_start_play']
    assert not starts[0]['spatially_loaded'] and not starts[0]['editor_only']
    director=actor_record(directors[0])
    director_desc=[d for d in ds if str(d.get_editor_property('actor_path'))==directors[0].get_path_name()]
    assert len(director_desc)==1
    director_layers=[str(v) for v in director_desc[0].get_editor_property('data_layer_assets')]
    assert not director['spatially_loaded'] and not director['editor_only'] and not director_layers
    candidate_record={'world':candidate.get_path_name(),'mode':mode,'player_starts':starts,
                      'director':director,'director_data_layers':director_layers,'definition':path(definition),'supported_world':supported,
                      'actor_desc_count':len(ds)}
    hub=load_map(HUB); descs=load_descs(True)
    hub_mode=mode_record(hub)
    assert hub_mode['run_duration_seconds']==1200 and not hub_mode['use_prototype_run_duration']
    assert not hub_mode['start_run_on_start_play']
    old=portals()
    described={str(d.get_editor_property('actor_path')) for d in descs}
    loaded={a.get_path_name() for a in all_actors()}
    missing=sorted(described-loaded)
    contract={'candidate':candidate_record,'hub':hub.get_path_name(),'hub_mode':hub_mode,'old_portals':old,
              'hub_actor_desc_count':len(descs),'loaded_actor_count':len(loaded),'missing_described_actor_paths':missing,
              'static_mesh_finish_barrier':PHASE!='inspect01','owner_pid':os.getpid()}
    write(PHASE+'-contract-before-traces.json',contract)
    assert not missing,missing
    expected=json.loads((R/'Feature/doc/evidence/npc-intro-compact20261113B/hub-portals-live.json').read_text(encoding='utf-8'))['portals']
    assert len(old)==3 and not any(p['target_level_name']==MAP for p in old)
    fields=('path','label','class','target_level_name','start_run_timer_on_travel','additional_travel_options','location_cm')
    assert {p['label']:{k:p[k] for k in fields} for p in old}=={p['label']:{k:p[k] for k in fields} for p in expected}
    assert not any(TAG in [str(t) for t in a.tags] for a in all_actors())
    grounds=[trace(hub,[x,y,3000.],[x,y,-1000.]) for x,y in [(-1440.,4270.),(-1440.,3970.),(-1440.,3670.)]]
    assert all(g['blocking_hit'] for g in grounds),grounds
    assert all(g['impact_normal'][2]>=0.8 for g in grounds),grounds
    assert max(g['impact_point'][2] for g in grounds)-min(g['impact_point'][2] for g in grounds)<2.,grounds
    ground=grounds[0]['impact_point'][2]
    approach=trace(hub,[-1440.,3670.,ground+120.],[-1440.,4340.,ground+120.],42.)
    assert not approach['blocking_hit'],approach
    z=ground+180.
    for p in old:
        center=p['collision_bounds_origin'];extent=p['collision_bounds_extent']
        assert any(abs(a-b)>ea+eb+20. for a,b,ea,eb in zip([-1440.,4270.,z],center,[120.,80.,180.],extent)),p
    clean()
    return {'status':'CURRENT_NORMAL_HUB_AND_COMPACT_CONTRACT_READ_ONLY','owner_pid':os.getpid(),
            'candidate':candidate_record,'hub':hub.get_path_name(),'hub_mode':hub_mode,
            'old_portals':old,'hub_actor_desc_count':len(descs),'loaded_actor_count':len(loaded),
            'missing_described_actor_paths':missing,'static_mesh_finish_barrier':PHASE!='inspect01',
            'ground_queries':grounds,'approach_capsule_query':approach,
            'proposed_portal_location_cm':[-1440.,4270.,z],'dirty_packages':[],'packages_saved':[],
            'physics_queries_are_not_player_or_portal_travel':True,'denied_metadata_clone_QA':False}


def author():
    before=json.loads((E/'inspect03-result.json').read_text(encoding='utf-8'))
    assert before['status']=='CURRENT_NORMAL_HUB_AND_COMPACT_CONTRACT_READ_ONLY'
    w=load_map(HUB);load_descs(True)
    assert portals()==before['old_portals']
    described={str(d.get_editor_property('actor_path')) for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()}
    assert described.issubset({a.get_path_name() for a in all_actors()})
    ground=trace(w,[-1440.,4270.,3000.],[-1440.,4270.,-1000.])
    assert ground['blocking_hit'] and not ground['initial_overlap'] and ground['impact_point'][2]==0.
    approach=trace(w,[-1440.,3670.,120.],[-1440.,4340.,120.],42.)
    assert not approach['blocking_hit']
    assert not any(TAG in [str(t) for t in a.tags] for a in all_actors())
    original_paths={r['path'] for r in json.loads((E/'inputs-before.json').read_text(encoding='utf-8'))['files']}
    x,y,z=before['proposed_portal_location_cm']
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    made=[]
    def owned(cls,label,pos,rotation=unreal.Rotator()):
        a=actors.spawn_actor_from_class(cls,unreal.Vector(*pos),rotation)
        assert a and a.get_world()==w
        a.set_actor_label(label)
        a.set_editor_property('tags',[unreal.Name(TAG)])
        a.set_editor_property('is_spatially_loaded',False)
        a.set_editor_property('is_editor_only_actor',False)
        made.append(a);return a
    portal=owned(unreal.Rogue10mRunPortal,'CompactCandidate20261113A_RunPortal',(x,y,z))
    portal.set_editor_property('target_level_name',unreal.Name(MAP))
    portal.set_editor_property('start_run_timer_on_travel',True)
    portal.set_editor_property('additional_travel_options','')
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    stone=unreal.load_asset('/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_WarmStone')
    glow=unreal.load_asset('/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_PortalGlow')
    assert cube and stone and glow
    ground=z-180.
    for label,pos,size,mat in [
        ('PillarLeft',(x-175.,y,ground+220.),(65.,90.,440.),stone),
        ('PillarRight',(x+175.,y,ground+220.),(65.,90.,440.),stone),
        ('Lintel',(x,y,ground+465.),(410.,100.,75.),stone),
        ('Glow',(x,y,ground+250.),(250.,8.,390.),glow),
    ]:
        a=owned(unreal.StaticMeshActor,'CompactCandidate20261113A_'+label,pos)
        c=a.get_component_by_class(unreal.StaticMeshComponent)
        assert c.set_static_mesh(cube)
        c.set_material(0,mat);c.set_collision_profile_name('NoCollision')
        a.set_actor_scale3d(unreal.Vector(*(v/100. for v in size)))
    sign=owned(unreal.TextRenderActor,'CompactCandidate20261113A_Sign',(x,y-90.,ground+560.),unreal.Rotator(pitch=0,yaw=-90,roll=0))
    text=sign.get_component_by_class(unreal.TextRenderComponent)
    text.set_text('COMPACT CANDIDATE');text.set_world_size(32.)
    text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    text.set_text_render_color(unreal.Color(r=120,g=235,b=245,a=255))
    owned_paths={a.get_path_name() for a in made}
    assert len(made)==len(owned_paths)==6 and len({a.get_actor_label() for a in made})==6
    owners=[];ownership=[]
    for a in made:
        assert a.get_world()==w and a.get_path_name().startswith(HUB+'.StartHub:PersistentLevel.')
        p=a.get_package()
        ownership.append({'actor':a.get_path_name(),'package':p.get_name() if p else None,
                          'external':a.is_package_external(),
                          'data_layers':[str(v) for v in a.get_editor_property('data_layer_assets')],
                          'external_data_layer_asset':path(a.get_editor_property('external_data_layer_asset'))})
        assert isinstance(p,unreal.Package) and a.is_package_external()
        assert not a.get_editor_property('data_layer_assets') and a.get_editor_property('external_data_layer_asset') is None
        owners.append(p)
    packages={p.get_name() for p in owners}
    write(PHASE+'-package-ownership.json',{'actors':ownership,'dirty_packages':[p.get_name() for p in dirty()]})
    assert len(set(owners))==len(packages)==6
    for package in packages:
        assert package.startswith('/Game/__ExternalActors__/Rogue10m/Maps/StartHub/')
        relative='Content/'+package.removeprefix('/Game/')+'.uasset'
        assert relative not in original_paths
        physical=(R/relative).resolve()
        assert physical.is_relative_to((R/'Content/__ExternalActors__/Rogue10m/Maps/StartHub').resolve())
        assert not physical.exists()
    changed=dirty()
    assert set(changed)==set(owners) and {p.get_name() for p in changed}==packages,[p.get_name() for p in changed]
    assert [p for p in portals() if p['target_level_name']!=MAP]==before['old_portals']
    intention={'owner_pid':os.getpid(),'new_actor_paths':sorted(owned_paths),'exact_new_packages':sorted(packages),
               'existing_map_or_external_packages_in_save':False,'source_map':HUB,'destination':MAP}
    write(PHASE+'-save-intent.json',intention)
    assert unreal.EditorLoadingAndSavingUtils.save_packages(owners,True)
    clean()
    descs=unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    ours=[d for d in descs if str(d.get_editor_property('actor_path')) in owned_paths]
    assert len(ours)==6 and {str(d.get_editor_property('actor_package')) for d in ours}==packages
    after=portals()
    assert [p for p in after if p['target_level_name']!=MAP]==before['old_portals']
    return {'status':'ADDITIVE_HUB_COMPACT_ENTRY_SIX_NEW_ACTORS_SAVED_RUNTIME_UNVERIFIED',
            **intention,'portals_after':after,'new_actors':[actor_record(a) for a in made],
            'dirty_packages_after':[],'ordinary_portal_travel_or_live_game_input':False,
            'D_package_contains_this_change':False,'landscape_cropped':False,'denied_metadata_clone_QA':False}


def cold():
    saved=json.loads((E/'author02-result.json').read_text(encoding='utf-8'))
    w=load_map(HUB);load_descs(True)
    assert portals()==saved['portals_after']
    made=sorted([actor_record(a) for a in all_actors() if TAG in [str(t) for t in a.tags]],key=lambda x:x['path'])
    assert made==sorted(saved['new_actors'],key=lambda x:x['path'])
    clean()
    return {'status':'SAVED_HUB_ENTRY_IDENTICAL_IN_NEW_COMMANDLET_PROCESS','owner_pid':os.getpid(),
            'world':w.get_path_name(),'portals':portals(),'new_actors':made,'dirty_packages':[],
            'ordinary_portal_travel_or_user_input':False,'D_package_contains_this_change':False,
            'denied_metadata_clone_QA':False}


def diagnose_cold():
    saved=json.loads((E/'author02-result.json').read_text(encoding='utf-8'))
    w=load_map(HUB);load_descs(True)
    actual=sorted([actor_record(a) for a in all_actors() if TAG in [str(t) for t in a.tags]],key=lambda x:x['path'])
    expected=sorted(saved['new_actors'],key=lambda x:x['path'])
    comparisons=[{'path':a['path'],'field':k,'saved_session':b.get(k),'cold_session':a.get(k)}
                 for a,b in zip(actual,expected) for k in a if a.get(k)!=b.get(k)]
    details=[]
    for a in sorted([a for a in all_actors() if TAG in [str(t) for t in a.tags]],key=lambda a:a.get_actor_label()):
        record={'actor':actor_record(a),'package':a.get_package().get_name(),'external':a.is_package_external(),
                'data_layers':[str(v) for v in a.get_editor_property('data_layer_assets')],
                'external_data_layer_asset':path(a.get_editor_property('external_data_layer_asset'))}
        if isinstance(a,unreal.StaticMeshActor):
            component=a.get_component_by_class(unreal.StaticMeshComponent)
            record['mesh']=path(component.get_editor_property('static_mesh'))
            record['material0']=path(component.get_material(0))
            record['collision_enabled']=str(component.get_collision_enabled())
            record['collision_disabled']=component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            record['collision_profile_name']=str(component.get_collision_profile_name())
            record['use_default_collision']=component.get_editor_property('use_default_collision')
        if isinstance(a,unreal.TextRenderActor):
            component=a.get_component_by_class(unreal.TextRenderComponent)
            color=component.get_editor_property('text_render_color')
            record['text']=str(component.get_editor_property('text'))
            record['font']=path(component.get_editor_property('font'))
            record['world_size']=component.get_editor_property('world_size')
            record['color']=[color.r,color.g,color.b,color.a]
            record['horizontal_alignment']=str(component.get_editor_property('horizontal_alignment'))
        details.append(record)
    clean()
    return {'status':'READ_ONLY_COLD_DIFFERENCE_CAPTURED_NO_ACCEPTANCE','owner_pid':os.getpid(),
            'portals':portals(),'portal_records_equal':portals()==saved['portals_after'],
            'actual_new_actors':actual,'expected_new_actors':expected,'differences':comparisons,
            'deep_readback':details,'dirty_packages':[],'assets_saved':False,
            'ordinary_portal_travel_or_user_input':False,'denied_metadata_clone_QA':False}


def correct_collision():
    saved=json.loads((E/'author02-result.json').read_text(encoding='utf-8'))
    backup=json.loads((E/'new-actor-before-fix.json').read_text(encoding='utf-8'))
    assert backup['status']=='SIX_NEW_OWNED_PACKAGES_PHYSICALLY_BACKED_UP'
    w=load_map(HUB);load_descs(True)
    assert portals()==saved['portals_after']
    made=[a for a in all_actors() if TAG in [str(v) for v in a.tags]]
    assert len(made)==6
    assert {a.get_path_name() for a in made}==set(saved['new_actor_paths'])
    assert {a.get_package().get_name() for a in made}==set(saved['exact_new_packages'])
    before=trace(w,[-1440.,3670.,120.],[-1440.,4340.,120.],42.)
    assert before['blocking_hit'],'Retain unexpected changed state instead of altering it.'
    owners=[]
    for a in made:
        if isinstance(a,unreal.StaticMeshActor):
            a.modify();component=a.get_component_by_class(unreal.StaticMeshComponent);component.modify()
            # Virtual StaticMesh override disables bUseDefaultCollision as well.
            component.set_collision_profile_name('NoCollision')
            assert not component.get_editor_property('use_default_collision')
            assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            owners.append(a.get_package())
        elif isinstance(a,unreal.TextRenderActor):
            a.modify();component=a.get_component_by_class(unreal.TextRenderComponent);component.modify()
            component.set_text_render_color(unreal.Color(r=120,g=235,b=245,a=255))
            owners.append(a.get_package())
    assert len(set(owners))==5
    assert set(owners)==set(dirty())
    assert {p.get_name() for p in owners}.issubset(saved['exact_new_packages'])
    after=trace(w,[-1440.,3670.,120.],[-1440.,4340.,120.],42.)
    assert not after['blocking_hit']
    write(PHASE+'-save-intent.json',{'purpose':'Correct only five newly owned packages; all earlier project files remain protected',
          'packages':[p.get_name() for p in owners],'before_capsule':before,'after_capsule':after})
    assert unreal.EditorLoadingAndSavingUtils.save_packages(owners,True)
    clean()
    assert portals()==saved['portals_after']
    return {'status':'OWNED_NEW_DECORATIONS_NO_COLLISION_PROFILE_AND_EXPLICIT_RGB_SAVED',
            'owner_pid':os.getpid(),'saved_packages':[p.get_name() for p in owners],
            'before_capsule':before,'after_capsule':after,'old_and_new_portals_unchanged':True,
            'actors_created':0,'old_project_files_saved':False,'dirty_packages':[],
            'live_D_input_or_denied_metadata_clone_QA':False}


def cold_deep():
    base=cold()
    probe=diagnose_cold()
    assert probe['portal_records_equal'] and not probe['differences']
    saved=json.loads((E/'author02-result.json').read_text(encoding='utf-8'))
    assert {r['package'] for r in probe['deep_readback']}==set(saved['exact_new_packages'])
    meshes=[r for r in probe['deep_readback'] if 'mesh' in r]
    signs=[r for r in probe['deep_readback'] if 'text' in r]
    assert len(meshes)==4 and len(signs)==1
    for r in probe['deep_readback']:
        assert r['external'] and not r['data_layers'] and r['external_data_layer_asset'] is None
    for r in meshes:
        assert r['mesh']=='/Engine/BasicShapes/Cube.Cube'
        name='M_StartVillage_PortalGlow' if r['actor']['label'].endswith('_Glow') else 'M_StartVillage_WarmStone'
        assert r['material0']=='/Game/Rogue10m/World/StartVillage/Materials/'+name+'.'+name
        assert r['collision_disabled'] and not r['use_default_collision'] and r['collision_profile_name']=='NoCollision'
    sign=signs[0]
    assert sign['text']=='COMPACT CANDIDATE' and sign['world_size']==32.
    assert sign['font']=='/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField'
    assert sign['color']==[120,235,245,255]
    assert sign['horizontal_alignment']==str(unreal.HorizTextAligment.EHTA_CENTER)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    capsule=trace(world,[-1440.,3670.,120.],[-1440.,4340.,120.],42.)
    assert not capsule['blocking_hit']
    clean()
    base.update({'status':'SAVED_HUB_ENTRY_AND_COMPONENT_DATA_IDENTICAL_AFTER_COLD_RELOAD',
                 'deep_readback':probe['deep_readback'],'cold_capsule_query':capsule,
                 'actor_desc_count':len(unreal.WorldPartitionBlueprintLibrary.get_actor_descs()),
                 'rendered_visual_review':'UNVERIFIED: null RHI reads only'})
    return base


try:
    result={'inspect01':inspect,'inspect02':inspect,'inspect03':inspect,'author01':author,'author02':author,'cold01':cold,'diagnose_cold02':diagnose_cold,'collision01':correct_collision,'cold03':cold_deep}[PHASE]()
    result['recorded_utc']=datetime.now(timezone.utc).isoformat()
    write(PHASE+'-result.json',result)
    print(json.dumps({'phase':PHASE,'status':result['status'],'pid':os.getpid()}))
except Exception:
    write(PHASE+'-error.json',{'phase':PHASE,'pid':os.getpid(),'traceback':traceback.format_exc(),
          'source_purpose':'normal Hub development; no denied metadata/clone QA or live game input'})
    raise
