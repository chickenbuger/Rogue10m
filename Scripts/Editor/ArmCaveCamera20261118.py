"""Arm the reviewed observer before a fresh normal Play-In-Viewport session."""
from pathlib import Path
import importlib.util
import json
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118'
assert json.loads((BASE/'author-live/final-save-result.json').read_text())['success']
assert json.loads((BASE/'author-live/exposure-save-result.json').read_text())['success']
assert json.loads((BASE/'author-live/camera-relief-save-result.json').read_text())['success']
assert json.loads((BASE/'qa-camera-final-live/result.json').read_text())['passed']
assert json.loads((BASE/'camera-arc-after.json').read_text())['passed']
spec=importlib.util.spec_from_file_location('cave_actual_camera_observer',ROOT/'Scripts/Editor/ObserveCaveTraversalCamera20261118.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
observer=m.arm_cave_traversal_camera(owner_confirms_play_in_viewport=True,allow_approved_3d_capture=True,
    expected_plan_sha256='39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74',
    output_directory_name='runtime-camera-verified')
