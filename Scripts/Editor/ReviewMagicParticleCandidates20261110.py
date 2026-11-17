"""Transient editor-only Niagara simulation controls for actual MCP viewport review.

No assets/maps/settings are saved. Native MCP lacks age/advance/pause operations,
so the existing local editor Python bridge controls only these owned test actors.
"""
from pathlib import Path
import math
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/'Scripts'))
from MagicParticleCandidateSpec20261110 import build_spec
SPEC=build_spec('MagicMcp01')
STATE={}
STEP=1/240


def begin():
    if STATE:raise RuntimeError('Preview already exists; inspect/clean it first')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name()!='/Engine/Maps/Entry.Entry':raise RuntimeError('Only the owned scratch Entry world is allowed')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():raise RuntimeError('A map was dirty before preview')
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    STATE['before']={a.get_path_name() for a in sub.get_all_level_actors()}
    STATE['actors']=[];STATE['components']={};STATE['poses']={};STATE['world']=world.get_path_name()
    for index,effect in enumerate(SPEC['effects']):
        system=unreal.load_asset(effect['system_path'])
        if not isinstance(system,unreal.NiagaraSystem):raise RuntimeError('Missing compiled candidate '+effect['system_path'])
        actor=sub.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,index*120,100),unreal.Rotator(),True)
        if not actor:raise RuntimeError('Transient preview spawn failed')
        STATE['actors'].append(actor)
        actor.set_actor_label('MagicCandidatePreview_'+effect['id'],False)
        component=actor.get_component_by_class(unreal.NiagaraComponent)
        component.set_auto_activate(False)
        component.set_asset(system,True)
        component.set_force_solo(True)
        component.set_custom_time_dilation(1.)
        component.set_relative_scale3d(unreal.Vector(1,1,1))
        component.set_age_update_mode(unreal.NiagaraAgeUpdateMode.TICK_DELTA_TIME)
        component.deactivate()
        STATE['components'][effect['id']]=component
    return inspect()


def pose(normalized_age):
    if not STATE:raise RuntimeError('Begin first')
    if not math.isfinite(normalized_age) or not 0<=normalized_age<=3:raise RuntimeError('Invalid review age')
    for effect in SPEC['effects']:
        component=STATE['components'][effect['id']]
        component.set_paused(False)
        component.activate(True)
        ticks=math.floor(effect['particle_lifetime_world_seconds']*normalized_age/STEP)
        component.advance_simulation(ticks,STEP)
        component.set_paused(True)
        STATE['poses'][effect['id']]={'requested_normalized_age':normalized_age,'manual_ticks':ticks,
                                    'manual_step_seconds':STEP,'manual_elapsed_seconds':ticks*STEP}
    return inspect()


def inspect():
    if not STATE:return {'active_preview':False}
    rows=[]
    for effect in SPEC['effects']:
        component=STATE['components'].get(effect['id'])
        if not component:continue
        scale=component.get_relative_transform().scale3d
        rows.append({'role':effect['id'],'component':component.get_path_name(),
                     'asset':component.get_editor_property('asset').get_path_name(),
                     'active':component.is_active(),'paused':component.is_paused(),
                     'pose':STATE['poses'].get(effect['id']),
                     'component_scale':[scale.x,scale.y,scale.z]})
    return {'active_preview':True,'world':STATE['world'],'saved_assets':False,'gameplay_input':False,'components':rows}


def clean_view():
    if not STATE:raise RuntimeError('Begin first')
    sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if 'game_view_before' not in STATE:STATE['game_view_before']=sub.editor_get_game_view()
    sub.editor_set_game_view(True)
    return {'game_view':sub.editor_get_game_view(),'game_view_before':STATE['game_view_before']}


def end():
    if not STATE:return {'already_clean':True}
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    destroyed=[]
    for actor in STATE['actors']:
        if not actor.get_actor_label().startswith('MagicCandidatePreview_'):raise RuntimeError('Ownership label changed; refuse to destroy')
        path=actor.get_path_name()
        if not sub.destroy_actor(actor):raise RuntimeError('Could not destroy owned preview actor '+path)
        destroyed.append(path)
    after={a.get_path_name() for a in sub.get_all_level_actors()}
    restored=after==STATE['before']
    if 'game_view_before' in STATE:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(STATE['game_view_before'])
    result={'actors_destroyed':destroyed,'actor_set_restored':restored,
            'dirty_maps':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
            'dirty_content':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
            'assets_or_maps_saved':False}
    if not restored:raise RuntimeError('Preview actor baseline not restored')
    STATE.clear()
    return result
