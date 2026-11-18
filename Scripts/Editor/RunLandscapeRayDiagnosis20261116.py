"""Entry point for the read-only finite-radius query diagnosis."""
import importlib.util
from pathlib import Path
import sys

path = Path("D:/Project/Rogue10m/Scripts/Editor/DiagnoseLandscapeRayMiss20261116.py")
spec = importlib.util.spec_from_file_location("crop_ray_diagnosis_20261116", path)
module = importlib.util.module_from_spec(spec)
previous = sys.dont_write_bytecode
sys.dont_write_bytecode = True
try:
    spec.loader.exec_module(module)
    result = module.diagnose()
finally:
    sys.dont_write_bytecode = previous
print("LANDSCAPE_RAY_DIAGNOSIS_20261116 " + result["status"])
