import json
from pathlib import Path
import unreal

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
prefix = '/Game/__MapMakerTests/MapMakerFixture_08C900784684B3FBC4FF1EA97A950065'
for suffix in ('_HLODLayer_Merged', '_HLODLayer_Instanced'):
    path = prefix + suffix
    if assets.does_asset_exist(path):
        if not assets.delete_asset(path):
            raise RuntimeError('Could not clean owned test asset: ' + path)
    if assets.does_asset_exist(path):
        raise RuntimeError('Owned test asset remains: ' + path)
unreal.log('MAP_MAKER_OWNED_FIXTURE_CLEANUP_PASS')
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Rogue10m/Maps/OpenRunMap')
if not world:
    raise RuntimeError('Could not load the actual gameplay world for read-only verification')
facts = json.loads(unreal.Rogue10mOpenWorldAuthoring.inspect_open_world_terrain())
region = json.loads(unreal.Rogue10mOpenWorldAuthoring.load_open_world_editor_region())
if not region.get('editor_region_loaded'):
    raise RuntimeError('Could not load the bounded editor-only terrain region: ' + repr(region))
facts = json.loads(unreal.Rogue10mOpenWorldAuthoring.inspect_open_world_terrain())
if facts.get('success') is False or not facts.get('world_partition') or not facts.get('streaming_enabled'):
    raise RuntimeError('Actual gameplay world does not satisfy the existing World Partition contract: ' + repr(facts))
out = Path('D:/Project/Rogue10m/Feature/doc/evidence/world-map-maker-20261024/production-world-readback.json')
out.write_text(json.dumps(facts, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('MAP_MAKER_PRODUCTION_WORLD_READBACK_PASS')
