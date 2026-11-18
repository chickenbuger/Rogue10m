"""Save only the diagnosed WIP checkpoint so native height repair can run clean.

This is not a final acceptance or a passing QA result. The original 2016-query
report and its one failed wall-isolation case stay unchanged.
"""
from pathlib import Path
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/caves20261118';OUT=BASE/'author-live'
assert not (OUT/'collar-pre-save-result.json').exists()
qa=json.loads((BASE/'qa-final-live/validation.json').read_text())
assert qa['passed'] is False and qa['traceback'] is None and qa['capsule_query_count']==2016
assert {k for k,v in qa['checks'].items() if not v}=={
    'all20_wall_crossings_hit_exact_structure','all_planned_capsule_queries_completed_without_failures'}
bad=[r for r in qa['capsule_results'] if not r['passed']]
assert len(bad)==1 and bad[0]['case']['id']=='Z_wall_09'
assert 'LandscapeStreamingProxy' in bad[0]['query']['hit']['actor'] and not bad[0]['query']['hit']['start_penetrating']
latest=json.loads((OUT/'triplanar.json').read_text());assert latest['success']
target='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
n=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
assert n['success'] and n['composite_blake3']=='b9f28e82d7661e83555d5f72942ea999ca6aea9554969753a889ea50f87e1970'
dirty=sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+
              list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
assert set(dirty)==set(latest['dirty_after']) and len(dirty)==24
plan={'schema':'Rogue10m.NaturalBoundaryPackageSave.20261117.v1','map':target,'confirm_save':True,
      'expected_composite_blake3':n['composite_blake3'],'expected_original_layer_blake3':n['original_layer_blake3'],'packages':dirty}
with (OUT/'collar-pre-save-plan.json').open('x',encoding='utf-8') as f:json.dump(plan,f,indent=2)
r=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.save_candidate_packages(json.dumps(plan)))
with (OUT/'collar-pre-save-result.json').open('x',encoding='utf-8') as f:json.dump(r,f,indent=2)
assert r['success'],r.get('error')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
with (OUT/'collar-repair-context.json').open('x',encoding='utf-8') as f:
    json.dump({'checkpoint_saved':True,'packages':24,'qa_passed':False,'failed_case_preserved':'Z_wall_09',
               'reason':'Repair terrain interpolation below the existing wall; keep the original exact-wall hit test.',
               'acceptance_complete':False,'automatic_retry':False},f,indent=2)
print('CAVES KNOWN_WIP_EXACT24_SAVED_CLEAN_FOR_TERRAIN_COLLAR_REPAIR')
