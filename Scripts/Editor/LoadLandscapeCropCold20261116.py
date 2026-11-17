"""Load the saved candidate's bounded editor region in a fresh editor process."""
import json
from pathlib import Path
import unreal

ROOT = Path("D:/Project/Rogue10m")
MAP = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OUT = ROOT / "Feature/doc/evidence/landscape-production-20261116/cold-region"
assert Path(unreal.Paths.project_dir()).resolve() == ROOT
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world()
assert editor.get_editor_world().get_path_name().split(".")[0] == MAP
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
OUT.mkdir(exist_ok=False)
result = json.loads(unreal.Rogue10mCrop20261114WorldAuthoring.load_candidate_region())
with (OUT / "load-result.json").open("x", encoding="utf-8") as stream:
    json.dump(result, stream, ensure_ascii=False, indent=2)
assert result.get("success") and result.get("dirty_package_names_unchanged"), result.get("error")
print("LANDSCAPE_COLD_REGION_20261116 LOADED; verify after editor updates")
