"""Apply the reviewed cave-only lighting/material correction exactly once."""
from pathlib import Path
import importlib.util
import json
import unreal as u

ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'visibility.json').exists() and not (OUT/'visibility-failure.json').exists()
spec=importlib.util.spec_from_file_location('cave_visibility_author',ROOT/'Scripts/Editor/RefineCaveVisibility20261118.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
expected=json.loads((OUT/'apply-height-confirmed.json').read_text())
assert n['success'] and n['composite_blake3']==expected['target_composite_blake3']
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
try:
    r=m.refine_cave_visibility(allow_editor_mutation=True,
        expected_spec_sha256='5509ec2f171704b4b963cb82726c9660f4e2c5b05ac883394ef63e72de644d81',
        expected_composite_blake3=n['composite_blake3'],expected_original_layer_blake3=n['original_layer_blake3'],
        expected_dirty_packages=dirty)
    assert r['success']
    with (OUT/'visibility.json').open('x',encoding='utf-8') as f:json.dump(r,f,ensure_ascii=False,indent=2)
    print('CAVES VISIBILITY_LOCAL_EXPOSURE_SIDE_MATERIAL_AND_COLOR_SUCCESS')
except Exception as e:
    if hasattr(e,'report'):
        with (OUT/'visibility-failure.json').open('x',encoding='utf-8') as f:json.dump(e.report,f,ensure_ascii=False,indent=2)
    u.log_error('CAVES VISIBILITY '+str(e))
    raise
