"""Start the existing project-local Blender bridge without overwriting v1 proof."""
import importlib
import json
import os
from pathlib import Path

import bpy

ROOT=Path(__file__).resolve().parents[2]
MODULE='codex_blender_mcp_bridge'
if MODULE not in bpy.context.preferences.addons:
    bpy.ops.preferences.addon_enable(module=MODULE)
bridge=importlib.import_module(MODULE)
ok,message=bridge._server_start('127.0.0.1',9877)
evidence=ROOT/'Feature/doc/evidence/arin-reference-refinement-20261009'
(evidence/'blender-live-bootstrap.json').write_text(json.dumps({'ok':ok,'message':message,
    'pid':os.getpid(),'version':bpy.app.version_string,'blend_file':bpy.data.filepath,
    'preferences_saved':False,'endpoint':'127.0.0.1:9877'},indent=2),encoding='utf-8')
if not ok:
    raise RuntimeError(message)
