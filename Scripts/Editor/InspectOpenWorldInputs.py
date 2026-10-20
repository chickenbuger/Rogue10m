"""Read-only inventory and content checkpoint before OpenRunMap authoring."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path('D:/Project/Rogue10m').resolve():
    raise RuntimeError('Unexpected project')
evidence = root / 'Feature/doc/evidence/open-world-20261005'
baseline = evidence / 'baseline-content.json'
if not baseline.exists():
    files = {}
    for path in (root / 'Content').rglob('*'):
        if path.is_file():
            stat = path.stat()
            files[str(path.relative_to(root)).replace('\\', '/')] = {
                'size': stat.st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    baseline.write_text(json.dumps(files, indent=2), encoding='utf-8')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
report = {'world': world.get_path_name(), 'assets': [], 'partition': {}}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
names = {'MI_Terrain', 'AutoLayer_LayerInfo', 'SM_Flagstone_FullXFull',
         'SM_FlagStone_Pillar_Full', 'SM_FlagStone_Steps_01', 'SM_FlagStone_Trim_Arch',
         'BP_WindMill', 'BP_WaterMill', 'SM_WaterMesh', 'SM_SM_WaterMesh', 'MI_Water',
         'SM_Wood_Floor_FullxFull_LP', 'SM_Boulder_Large_01', 'SM_Boulder_Medium_01'}
for data in registry.get_assets_by_path('/Game/Stylized_Village', recursive=True):
    if str(data.asset_name) not in names:
        continue
    asset = data.get_asset()
    row = {'path': asset.get_path_name(), 'class': asset.get_class().get_name()}
    if isinstance(asset, unreal.StaticMesh):
        bounds = asset.get_bounds()
        row['origin'] = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
        row['size'] = [2*bounds.box_extent.x, 2*bounds.box_extent.y, 2*bounds.box_extent.z]
    if str(data.asset_name) == 'AutoLayer_LayerInfo':
        row['layer_name'] = str(asset.get_editor_property('layer_name'))
    report['assets'].append(row)
try:
    partition = world.get_world_settings().get_editor_property('world_partition')
    runtime = partition.get_editor_property('runtime_hash')
    report['partition'] = {'class': partition.get_class().get_name(), 'runtime_hash': runtime.get_class().get_name()}
    for prop in ('runtime_partitions', 'grids'):
        try:
            report['partition'][prop] = str(runtime.get_editor_property(prop))
        except Exception:
            pass
except Exception as error:
    report['partition']['error'] = str(error)
for package in ['/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode',
                '/Game/Monster/BP_BaseMonster', '/Game/Monster/BP_MonsterSpawner']:
    asset = unreal.load_asset(package)
    report.setdefault('gameplay', []).append({'path': package, 'exists': bool(asset)})
(evidence/'inputs.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('[OpenWorldInputs] '+json.dumps(report, ensure_ascii=False))
