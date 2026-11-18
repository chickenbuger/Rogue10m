"""Import-inert live read-only proof of the exact cold metadata function.

Run only after the current PIE session ends. No bootstrap, callbacks, map load,
save, property write, PIE control, editor quit or automatic retry is performed.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import traceback

ROOT=Path("D:/Project/Rogue10m")
OUT=ROOT/"Feature/doc/evidence/caves20261118/cold-metadata-preflight.json"
TARGET="/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"


def run_preflight():
    report={"schema":"Rogue10m.CaveColdMetadataPreflight.20261118.v1","passed":False,
            "assets_saved":False,"actor_or_property_mutations":False,"callbacks_registered":False,
            "editor_quit_called":False,"automatic_retry":False}
    try:
        import unreal as u
        editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
        if (Path(u.Paths.project_dir()).resolve()!=ROOT or editor.get_game_world()
                or u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
                or editor.get_editor_world().get_path_name().split(".")[0]!=TARGET):
            raise RuntimeError("Exact candidate editor world with PIE stopped required; nothing was changed")
        def dirty():
            return sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
                           +list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
        report["dirty_before"]=dirty()
        file=ROOT/"Scripts/Editor/ColdCaves20261118.py"
        spec=importlib.util.spec_from_file_location("cave_metadata_preflight_cold",file)
        cold=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cold)
        cold.u=u
        report["cold_script_sha256"]=hashlib.sha256(file.read_bytes()).hexdigest()
        report["inventory"]=cold.terrain_actor_inventory()
        report["dirty_after"]=dirty()
        if report["dirty_before"]!=report["dirty_after"]:
            raise RuntimeError("Read-only metadata preflight changed dirty package names")
        report["passed"]=True
    except Exception:
        report["traceback"]=traceback.format_exc()
    return report


def main(output_name="cold-metadata-preflight.json"):
    if output_name not in ("cold-metadata-preflight.json","cold-locale-metadata-preflight.json"):
        raise ValueError("Only the two explicit fresh metadata preflight filenames are allowed")
    result=run_preflight()
    with OUT.with_name(output_name).open("x",encoding="utf-8") as stream:
        json.dump(result,stream,ensure_ascii=False,indent=2,allow_nan=False)
        stream.write("\n")
    return result


if __name__=="__main__":
    main()
