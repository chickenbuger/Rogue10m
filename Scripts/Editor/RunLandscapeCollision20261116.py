"""Entry point for reviewed, read-only saved-candidate collision queries."""
import importlib.util
import json
from pathlib import Path
import sys

root = Path("D:/Project/Rogue10m")
evidence = root / "Feature/doc/evidence/landscape-production-20261116"
cold = json.loads((evidence / "cold-reload/comparison.json").read_text(encoding="utf-8"))
assert cold["passed"] and cold["componentCount"] == cold["collisionCount"] == 45
assert not cold["dirtyAfter"]["maps"] and not cold["dirtyAfter"]["content"]
spec = importlib.util.spec_from_file_location(
    "landscape_crop_collision_20261116", root / "Scripts/Editor/ValidateLandscapeCrop20261116.py")
module = importlib.util.module_from_spec(spec)
previous = sys.dont_write_bytecode
sys.dont_write_bytecode = True
try:
    spec.loader.exec_module(module)
    result = module.validate(reload_confirmed=True)
finally:
    sys.dont_write_bytecode = previous
print("LANDSCAPE_COLLISION_20261116 " + result["status"])
