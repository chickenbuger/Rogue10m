"""Apply the exact diagnosed two-rock relief, then actual read-only turn sweep."""
from pathlib import Path
import importlib.util
import json
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118'
out=BASE/'author-live/camera-relief.json';assert not out.exists()
def module(name,file):
    s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
m=module('reviewed_two_rock_camera_relief','Scripts/Editor/RelieveCaveCameraJunction20261118.py')
try:
    result=m.relieve_cave_camera_junction(allow_editor_mutation=True,
        expected_spec_sha256='92e2464e246528952718203c7e82ed7455c20102fa7b8c5586835229f02fb92f',
        expected_composite_blake3='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc',
        expected_original_layer_blake3='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955',
        expected_dirty_packages=[])
except m.CaveCameraReliefError as error:
    with (BASE/'author-live/camera-relief-failure.json').open('x',encoding='utf-8') as f:json.dump(error.report,f,ensure_ascii=False,indent=2)
    raise
with out.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
assert result['success']
arc=module('actual_camera_arc_after_two_rock_relief','Scripts/Editor/AuditCaveCameraArc20261118.py').audit('after')
assert arc['passed'],arc['minimum']
print('CAVES TWO_ROCK_CAMERA_RELIEF_AND_1440_ACTUAL_ARC_PASS')
