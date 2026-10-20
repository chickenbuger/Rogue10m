"""Place one simple dialogue/supplies NPC in the saved StartHub via Editor."""
import json
from pathlib import Path
import sys
import unreal

ROOT = Path('D:/Project/Rogue10m')
sys.path.insert(0, str(ROOT / 'Scripts/Editor'))
from ApplyOpenWorldBasicMonsters import data_asset, require
from InspectVillageGround import actor_state

EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-monsters-20261005'
OWN = '/Game/Rogue10m/World/StartVillage/Tutorial'
MAP = '/Game/Rogue10m/Maps/StartHub'
LABEL = 'SV_TutorialGuide_Arin'


def main():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Requires idle StartHub')
    checkpoint = EVIDENCE / 'npc-before-state.json'
    if not checkpoint.exists():
        checkpoint.write_text(json.dumps({'actors': [actor_state(a) for a in actors.get_all_level_actors()]},
                                        ensure_ascii=False, indent=2), encoding='utf-8')
    data_path = OWN + '/DA_StartVillageGuide'
    data = data_asset(data_path, unreal.Rogue10mTutorialDataAsset)
    mesh = require('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
    idle = require('/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle')
    if mesh.get_editor_property('skeleton') != idle.get_editor_property('skeleton'):
        raise RuntimeError('Guide animation skeleton mismatch')
    item = require('/Game/DataAsset/Item/Starter/DA_Item_HealthPotion')
    if int(item.get_editor_property('max_stack_size')) < 2:
        raise RuntimeError('Supply quantity exceeds actual item stack size')
    data.modify()
    for key, value in {'guide_skeletal_mesh': mesh, 'guide_idle_animation': idle,
                       'starting_item': item, 'starting_item_quantity': 2,
                       'reward_id': 'StartVillageSupplies'}.items():
        data.set_editor_property(key, value)
    bp_path = OWN + '/BP_StartVillageGuide'
    bp = unreal.load_asset(bp_path)
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.Rogue10mTutorialNPC)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_StartVillageGuide', OWN, unreal.Blueprint, factory)
    if not bp:
        raise RuntimeError('Guide Blueprint creation failed')
    bp.modify()
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('tutorial_data', data)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.apply_guide_appearance()
    for value in (data, bp):
        if not unreal.EditorAssetLibrary.save_loaded_asset(value, False):
            raise RuntimeError('Guide asset save failed ' + value.get_path_name())
    found = [a for a in actors.get_all_level_actors() if a.get_actor_label() == LABEL]
    if len(found) > 1:
        raise RuntimeError('Duplicate guide actors')
    if found:
        actor = found[0]
    else:
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(440,-1610,1000),
                unreal.Vector(440,-1610,-500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                False, [], unreal.DrawDebugTrace.NONE, True)
        if not hit or not hit.to_tuple()[0]:
            raise RuntimeError('Guide placement has no walkable ground')
        ground_z = float(hit.to_tuple()[5].z)
        actor = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(440,-1610,ground_z+2),
                                              unreal.Rotator(pitch=0,yaw=-90,roll=0))
        actor.set_actor_label(LABEL)
    actor.modify()
    actor.set_editor_property('tutorial_data', data)
    actor.set_editor_property('is_spatially_loaded', False)
    actor.set_editor_property('tags', ['Rogue10m.StartVillage', 'TutorialGuide'])
    actor.set_folder_path('StartVillage/Gameplay')
    actor.apply_guide_appearance()
    if not levels.save_current_level():
        raise RuntimeError('StartHub guide save failed')
    report = {'map': MAP, 'label': LABEL, 'location': list(actor.get_actor_location().to_tuple()),
              'rotation': list(actor.get_actor_rotation().to_tuple()),
              'data': data_path, 'blueprint': bp_path, 'mesh': mesh.get_path_name(),
              'idle': idle.get_path_name(), 'item': item.get_path_name(), 'quantity': 2,
              'saved_assets': [data_path, bp_path], 'actor_packages': [actor.get_package().get_name()]}
    (EVIDENCE / 'npc-application.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('[StartHubGuide] Saved Arin and two health-potion supply configuration')


if __name__ == '__main__':
    main()
