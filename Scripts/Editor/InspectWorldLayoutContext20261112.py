"""Read-only editor snapshot for the Landscape production-layout checkpoint."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
assert root == Path('D:/Project/Rogue10m'), root
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert not unreal.EditorLevelLibrary.get_game_world(), 'Stop PIE first.'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
records = []
for actor in actors:
    label = actor.get_actor_label()
    cls = actor.get_class().get_name()
    if any(s in (label + ' ' + cls).lower() for s in
           ('portal', 'guide', 'boss', 'playerstart', 'worldsettings', 'landscape', 'light', 'fog', 'atmosphere', 'postprocess', 'navigation')):
        p = actor.get_actor_location()
        rec = dict(label=label, actor_path=actor.get_path_name(), cls=cls,
                   location=[p.x, p.y, p.z], tags=[str(t) for t in actor.tags])
        if 'Portal' in cls:
            for key in ('target_level', 'target_level_name', 'interaction_text'):
                try:
                    rec[key] = str(actor.get_editor_property(key))
                except Exception:
                    pass
        if 'Boss' in label or 'Boss' in cls:
            rec['components'] = [{'cls': c.get_class().get_name(), 'path': c.get_path_name()}
                                 for c in actor.get_components_by_class(unreal.ActorComponent)]
        records.append(rec)
settings = world.get_world_settings()
gm = settings.get_editor_property('default_game_mode')
result = dict(map=world.get_path_name(), game_mode=gm.get_path_name() if gm else None,
              actor_count=len(actors), actors=records,
              dirty_maps=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
              dirty_content=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
out = root / 'Feature/doc/evidence/landscape-world-blockout-20261112' / ('context-' + world.get_name() + '.json')
out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
