"""Fresh UE editor-process persisted-asset verification; no saves/imports."""
import json
import os
from pathlib import Path
import sys
import traceback

import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(ROOT/"Scripts/Editor"))
import ApplyReferenceArin as application

report={"fresh_process_id":os.getpid(),"runtime":"UnrealEditor-Cmd fresh process, NullRHI persistence only",
        "rendering_test":False,"asset_mutation_requested":False}
try:
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(application.MAP_PATH):
        raise RuntimeError("Could not load actual StartHub")
    result=application.main(validate_only=True)
    report.update({"status":"passed","core":result["core"],"texture_count":result["texture_count"],
        "material_count":result["material_count"],"lods":result["lods"],"guide_preserved":True,
        "dependencies":result["package_files_verified"]})
except Exception:
    report.update({"status":"failed","exception":traceback.format_exc()})
(ROOT/"Feature/doc/evidence/arin-reference-refinement-20261009/unreal-fresh-process.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
if report["status"]!="passed":
    raise RuntimeError(report["exception"])
unreal.log("ARIN_REFERENCE_FRESH_PROCESS_PASSED pid="+str(os.getpid()))
