"""Apply the reviewed two wall/ceiling material bindings; no geometry change."""
from pathlib import Path
import importlib.util
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'wall-texture.json').exists() and not (OUT/'wall-texture-failure.json').exists()
visibility=json.loads((OUT/'visibility.json').read_text());assert visibility['success']
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert set(dirty)==set(visibility['dirty_after'])
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
s=importlib.util.spec_from_file_location('cave_wall_texture_author',ROOT/'Scripts/Editor/RefineCaveRockWalls20261118.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
try:
    r=m.refine_cave_rock_walls(allow_editor_mutation=True,
        expected_spec_sha256='a4127f8ca450c50c114001d60c92e05da98c380c9597c22715f382c3827b3cd8',
        expected_composite_blake3=n['composite_blake3'],expected_original_layer_blake3=n['original_layer_blake3'],
        expected_dirty_packages=dirty)
    assert r['success']
    with (OUT/'wall-texture.json').open('x',encoding='utf-8') as f:json.dump(r,f,ensure_ascii=False,indent=2)
    print('CAVES CONTINUOUS_ROCK_WALLS_AND_CEILINGS_APPLIED')
except Exception as e:
    if hasattr(e,'report'):
        with (OUT/'wall-texture-failure.json').open('x',encoding='utf-8') as f:json.dump(e.report,f,ensure_ascii=False,indent=2)
    u.log_error('CAVES ROCK_WALLS '+str(e))
    raise
