"""Apply the reviewed world-aligned stone projection after the UV rejection."""
from pathlib import Path
import importlib.util
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'triplanar.json').exists() and not (OUT/'triplanar-failure.json').exists()
prior=json.loads((OUT/'wall-texture.json').read_text());assert prior['success']
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert set(dirty)==set(prior['dirty_after'])
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
s=importlib.util.spec_from_file_location('cave_triplanar_author',ROOT/'Scripts/Editor/RefineCaveTriplanarStone20261118.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
try:
    r=m.refine_cave_triplanar_stone(allow_editor_mutation=True,
        expected_spec_sha256='0d24c669a60f9c998401afcc7f7c75e74f6fe1f72c499496fda3139e5c47353f',
        expected_composite_blake3=n['composite_blake3'],expected_original_layer_blake3=n['original_layer_blake3'],
        expected_dirty_packages=dirty)
    assert r['success']
    with (OUT/'triplanar.json').open('x',encoding='utf-8') as f:json.dump(r,f,ensure_ascii=False,indent=2)
    print('CAVES WORLD_ALIGNED_STONE_WALL_CEILING_ROOF_SIDE_SUCCESS')
except Exception as e:
    if hasattr(e,'report'):
        with (OUT/'triplanar-failure.json').open('x',encoding='utf-8') as f:json.dump(e.report,f,ensure_ascii=False,indent=2)
    u.log_error('CAVES TRIPLANAR '+str(e))
    raise
