"""Temporary actual engine animation poses; restore all component settings."""
import unreal
import ReviewReferenceArinLOD as viewport

STATE='_rogue10m_arin_motion_review'
ROOT='/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/'

def begin():
    if getattr(unreal,STATE,None):
        raise RuntimeError('An owned animation review already exists')
    mesh=viewport.component()
    data=mesh.get_editor_property('animation_data')
    previous={'mode':mesh.get_editor_property('animation_mode'),
        'animation':data.get_editor_property('anim_to_play'),
        'looping':data.get_editor_property('saved_looping'),
        'playing':data.get_editor_property('saved_playing'),
        'rate':data.get_editor_property('saved_play_rate'),
        'position':data.get_editor_property('saved_position')}
    settings=viewport.request(1,True,False)
    setattr(unreal,STATE,{'previous':previous,'settings':settings})
    return {'previous_animation':previous['animation'].get_path_name(),'settings':settings}

def pose(key,frame):
    if not getattr(unreal,STATE,None) or key not in ('Walk','Talk'):
        raise RuntimeError('Missing review or unknown clip')
    mesh=viewport.component()
    clip=unreal.load_asset(ROOT+'A_Arin_'+key)
    mesh.play_animation(clip,True)
    mesh.set_play_rate(0.)
    mesh.set_position((frame-1)/30.,False)
    return inspect()

def inspect():
    mesh=viewport.component()
    instance=mesh.get_anim_instance()
    return {'animation':instance.get_animation_asset().get_path_name(),
        'position':mesh.get_position(),'play_rate':mesh.get_play_rate(),
        'root_world':list(mesh.get_socket_location('root').to_tuple()),
        'head_world':list(mesh.get_socket_location('head').to_tuple()),
        'feet_world':{s:list(mesh.get_socket_location('foot_'+s).to_tuple()) for s in ('l','r')},
        'arms_world':{s:list(mesh.get_socket_location('hand_'+s).to_tuple()) for s in ('l','r')},
        'lod':mesh.get_predicted_lod_level(),'asset_mutation_requested':False}

def end():
    state=getattr(unreal,STATE,None)
    if not state:
        raise RuntimeError('No review to restore')
    mesh=viewport.component()
    previous=state['previous']
    mesh.set_animation_mode(previous['mode'])
    mesh.play_animation(previous['animation'],previous['looping'])
    mesh.set_play_rate(previous['rate'])
    mesh.set_position(previous['position'],False)
    if not previous['playing']:
        mesh.stop()
    settings=state['settings']
    viewport.request(settings['previous_forced_value'],settings['previous_update_animation_in_editor'],
                     settings['previous_background_throttle'])
    setattr(unreal,STATE,None)
    data=mesh.get_editor_property('animation_data')
    verified=(data.get_editor_property('anim_to_play')==previous['animation']
        and data.get_editor_property('saved_play_rate')==previous['rate']
        and data.get_editor_property('saved_looping')==previous['looping']
        and data.get_editor_property('saved_playing')==previous['playing']
        and mesh.get_forced_lod()==settings['previous_forced_value']
        and bool(mesh.get_editor_property('update_animation_in_editor'))==settings['previous_update_animation_in_editor'])
    if not verified:
        raise RuntimeError('Review component restoration mismatch')
    return {'restored':verified,'state':inspect()}
