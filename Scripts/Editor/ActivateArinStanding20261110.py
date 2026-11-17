"""Activate only the visually reviewed guide idle reference; protect other tutorial fields."""
import unreal,json
from pathlib import Path
DATA='/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide'
OLD='/Game/Rogue10m/Characters/Arin/RefinedV3/ArinIdle'
NEW='/Game/Rogue10m/Characters/Arin/Standing20261110/A_Arin_BreathingIdle'
OUT=Path(unreal.Paths.project_dir())/'DevLog/20261110_HumanoidBossNpc/NPC/activation.json'
data=unreal.load_asset(DATA);target=unreal.load_asset(NEW)
fields=['guide_skeletal_mesh','guide_walk_animation','guide_talk_animation','guide_walk_reference_speed','guide_minimum_walk_speed','guide_name','guide_role','welcome_text','reward_received_text','reward_already_received_text','reward_id','starting_item','starting_item_quantity']
def snapshot():return {f:str(data.get_editor_property(f)) for f in fields}
before=snapshot();old=data.get_editor_property('guide_idle_animation')
if old.get_path_name().split('.')[0]!=OLD:raise RuntimeError('Guide idle changed since reviewed baseline')
if target.get_editor_property('skeleton')!=data.get_editor_property('guide_skeletal_mesh').get_editor_property('skeleton'):raise RuntimeError('Skeleton mismatch')
data.set_editor_property('guide_idle_animation',target)
if snapshot()!=before:raise RuntimeError('Protected field changed')
if not unreal.EditorAssetLibrary.save_loaded_asset(data,False):raise RuntimeError('Guide data save failed')
OUT.write_text(json.dumps({'status':'activated','asset':DATA,'previous':OLD,'new':NEW,'protected_fields_unchanged':before,'map_saved':False},ensure_ascii=False,indent=2),encoding='utf-8')
