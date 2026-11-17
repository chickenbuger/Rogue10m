"""Keep one coordinator-owned serial editor session alive; no asset mutations."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
if root != Path("D:/Project/Rogue10m").resolve():
    raise RuntimeError("Unexpected project for the granted magic particle editor slot")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
output = root / "Feature/doc/evidence/magic-particle-candidates-20261110/MagicMcp01/editor-ready.json"
output.write_text(json.dumps({"project": str(root), "status": "serial_editor_session_ready",
                              "asset_changes": False}, indent=2), encoding="utf-8")
unreal.log("RESULT=MAGIC_PARTICLE_EDITOR_SESSION_READY no_asset_changes=true")
