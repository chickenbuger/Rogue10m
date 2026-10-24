"""Read-only live editor evidence; QA_STEP is supplied by the remote caller."""
import json
from pathlib import Path
import re
import unreal

step = globals().get('QA_STEP', 'snapshot')
if not re.fullmatch(r'[a-z0-9_-]{1,60}', step):
    raise RuntimeError('Invalid QA step')
root = Path(unreal.Paths.project_dir()).resolve()
if root.as_posix().casefold() != 'd:/project/rogue10m':
    raise RuntimeError('Wrong editor project')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actors.get_all_level_actors()

def facts(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = mesh_component.static_mesh if mesh_component else None
    return {'name': actor.get_name(), 'label': actor.get_actor_label(), 'path': actor.get_path_name(),
            'class': actor.get_class().get_path_name(), 'folder': str(actor.get_folder_path()),
            'location': {'x': location.x, 'y': location.y, 'z': location.z},
            'rotation': {'pitch': rotation.pitch, 'yaw': rotation.yaw, 'roll': rotation.roll},
            'scale': {'x': scale.x, 'y': scale.y, 'z': scale.z},
            'mesh': mesh.get_path_name() if mesh else None}

data = {'step': step, 'world': world.get_path_name(), 'actor_count': len(all_actors),
        'maker_actors': [facts(a) for a in all_actors if a.actor_has_tag('Rogue10m.MapMaker')],
        'selected': [facts(a) for a in actors.get_selected_level_actors()][:10],
        'dirty_maps': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
        'dirty_content': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
output = root / 'Feature/doc/evidence/map-maker-live-qa-20261025' / ('live-' + step + '.json')
output.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('MAP_MAKER_LIVE_EVIDENCE ' + step + ' ' + json.dumps(data, ensure_ascii=True))
