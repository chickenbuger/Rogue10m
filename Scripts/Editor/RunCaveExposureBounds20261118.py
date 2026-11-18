"""Explicit minimal repair of the two cave exposure shapes; no asset save."""
from pathlib import Path
import importlib.util
import json
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118'
out=BASE/'author-live/exposure-bounds.json';assert not out.exists()
s=importlib.util.spec_from_file_location('reviewed_cave_bounds_repair',ROOT/'Scripts/Editor/RepairCaveExposureBounds20261118.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
result=m.repair_cave_exposure_bounds(allow_editor_mutation=True,
    expected_spec_sha256='fb2cb4bd5e9108507dee21d16af499c1ef19679ebbe029244705b2d55586fc70',
    expected_composite_blake3='67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc',
    expected_original_layer_blake3='7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955',
    expected_dirty_packages=json.loads((BASE/'author-live/exposure.json').read_text())['dirty_after'])
with out.open('x',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2)
assert result['success'],result.get('error')
print('CAVES LOCAL_PP_BOUNDS_QUERY_ONLY_ALL_IGNORE_EV3_REPAIRED')
