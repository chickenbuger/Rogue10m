"""Enable the upstream bridge in this project's isolated Blender configuration."""
import importlib
import json
import os
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[2]
MODULE = "codex_blender_mcp_bridge"
if MODULE not in bpy.context.preferences.addons:
    bpy.ops.preferences.addon_enable(module=MODULE)
bpy.ops.wm.save_userpref()
bridge = importlib.import_module(MODULE)
ok, message = bridge._server_start("127.0.0.1", 9877)
evidence = ROOT / "Feature/doc/evidence/arin-blender-20261007"
evidence.mkdir(parents=True, exist_ok=True)
(evidence / "bootstrap.json").write_text(json.dumps({
    "ok": ok, "message": message, "pid": os.getpid(),
    "version": bpy.app.version_string, "module": MODULE,
    "user_config": os.environ.get("BLENDER_USER_CONFIG"),
    "user_scripts": os.environ.get("BLENDER_USER_SCRIPTS"),
}, indent=2), encoding="utf-8")
if not ok:
    raise RuntimeError(message)
