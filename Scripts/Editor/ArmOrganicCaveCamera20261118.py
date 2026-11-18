"""Arm unchanged real-player traversal against the new continuous shells."""
from pathlib import Path
import builtins
import importlib.util
import json
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118'
assert json.loads((BASE/'author-live/final-save-result.json').read_text())['success']
assert json.loads((BASE/'qa-final-live/result.json').read_text())['passed']
scene=json.loads((BASE/'author-live/scene.json').read_text())
s=importlib.util.spec_from_file_location('organic_real_camera_observer',ROOT/'Scripts/Editor/OrganicCaves20261118Camera.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
builtins._rogue_organic_cave_camera_20261118=m
observer=m.arm_organic_cave_traversal_camera(expected_editor_pid=49456,
    shell_component_paths={sid:r['shell']for sid,r in scene['new_component_paths'].items()},
    owner_confirms_play_in_viewport=True,allow_approved_3d_capture=True,
    output_directory_name='runtime-camera-verified')
