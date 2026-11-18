"""Explicit bounded local exposure candidate after the actual dark PIE image."""
from pathlib import Path
import importlib.util
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118'
out=BASE/'author-live/exposure.json'
assert not out.exists()
spec=importlib.util.spec_from_file_location('reviewed_cave_exposure_candidate',ROOT/'Scripts/Editor/TuneCaveExposure20261118.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
result=m.tune_cave_exposure(allow_editor_mutation=True,
    expected_spec_sha256='bc58e611db30ae46e604be7b3c72a3af02a94a57a6eef288faf23eb5feea8627',
    expected_composite_blake3='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc',
    expected_original_layer_blake3='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955',
    expected_dirty_packages=[])
with out.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
assert result['success'],result.get('error')
print('CAVE_LOCAL_EV0_CANDIDATE_APPLIED_NOT_VISUAL_ACCEPTANCE')
