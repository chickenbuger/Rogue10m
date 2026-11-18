"""Create an isolated, exact-production-map terrain observer/selection facade."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/native-landscape-crop20261114A'
checkpoint=json.loads((E/'physical-checkpoint.json').read_text(encoding='utf-8-sig'))
assert checkpoint['status']=='LATEST_COMPACT_NPC_DA_HUB_SOURCE_CONFIG_PHYSICAL_BACKUP_VERIFIED_NO_ASSETS_CHANGED'
files={row['path']:row for row in checkpoint['backed_up_files']}
before=[];created=[]
for suffix in ('h','cpp'):
    relative='Source/Rogue10m/Editor/Rogue10mCompactWorldAuthoring.'+suffix
    source=R/relative;raw=source.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()==files[relative]['sha256']
    text=raw.decode('utf-8-sig')
    changed=text.replace('Rogue10mCompact','Rogue10mCrop20261114').replace('WorldLayoutCompact20261113A','WorldLayoutCompactCrop20261114A')
    assert changed.replace('WorldLayoutCompactCrop20261114A','WorldLayoutCompact20261113A').replace('Rogue10mCrop20261114','Rogue10mCompact')==text
    target=R/('Source/Rogue10m/Editor/Rogue10mCrop20261114WorldAuthoring.'+suffix)
    assert not target.exists()
    data=changed.encode('utf-8')
    with target.open('xb') as f:f.write(data)
    before.append({'path':relative,'sha256':hashlib.sha256(raw).hexdigest().upper()})
    created.append({'path':target.relative_to(R).as_posix(),'sha256':hashlib.sha256(data).hexdigest().upper(),'size':len(data)})
report={'status':'NEW_EXACT_PRODUCTION_MAP_HELPER_DERIVED_EXISTING_HELPER_UNCHANGED_NOT_BUILT',
    'recorded_utc':datetime.now(timezone.utc).isoformat(),'existing_sources':before,'new_sources':created,
    'new_class':'URogue10mCrop20261114WorldAuthoring','exact_candidate_map':'/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A',
    'scope':'Existing read-only terrain/raw/semantic/dependency code plus public candidate-only component selection; namespace/class/map substitutions only; no terrain deletion/import/save function',
    'old_helper_guards_unchanged':True,'original_source_files_modified':0,'new_source_files':2,
    'new_Editor_DLL_or_reflection_verified':False,'metadata_clone_QA_or_denied_approval_actions':0}
target=E/'helper-derivation.json';assert not target.exists()
target.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
