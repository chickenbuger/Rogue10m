"""Read-only same-session clean checkpoint proof before rendered revision."""
import json
import os
from pathlib import Path
import unreal as u

ROOT=Path('D:/Project/Rogue10m');OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117'
assert not u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert os.getpid()==json.loads((OUT/'author-live/startup.json').read_text())['pid']
native=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate())
before=json.loads((OUT/'author-live/native-before.json').read_text())
applied=json.loads((OUT/'author-live/apply-height.json').read_text())
assert native['success'] and native['landscape_reports_up_to_date'] and not native['dirty_packages']
assert native['composite_blake3']==applied['target_composite_blake3'] and native['original_layer_blake3']==before['original_layer_blake3']
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
assert len(actors)==1400
with (OUT/'refinement-preflight.json').open('x') as f:json.dump({'pid':os.getpid(),'native':native,'actor_count':len(actors),'new_layer_guid':applied['new_layer_guid'],'read_only':True},f,indent=2)
print('NATURAL_WORLD REFINEMENT_CLEAN_CHECKPOINT_CONFIRMED')
