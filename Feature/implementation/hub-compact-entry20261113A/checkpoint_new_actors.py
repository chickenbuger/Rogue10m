"""Physically retain the six newly owned packages before their scoped correction."""
from pathlib import Path
import hashlib,json,shutil
R=Path(__file__).resolve().parents[3]
E=R/'Feature/doc/evidence/hub-compact-entry20261113A'
B=R/'tmp/HubCompactEntry20261113A/after-author02-before-collision-fix'
assert not B.exists() and not (E/'new-actor-before-fix.json').exists()
saved=json.loads((E/'author02-result.json').read_text(encoding='utf-8'))
rows=[]
for package in saved['exact_new_packages']:
    relative='Content/'+package.removeprefix('/Game/')+'.uasset'
    source=R/relative;target=B/relative
    assert source.resolve().is_relative_to((R/'Content/__ExternalActors__/Rogue10m/Maps/StartHub').resolve())
    data=source.read_bytes();sha=hashlib.sha256(data).hexdigest().upper()
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    assert hashlib.sha256(target.read_bytes()).hexdigest().upper()==sha
    rows.append({'path':relative,'sha256':sha,'size':len(data),'backup':str(target)})
with (E/'new-actor-before-fix.json').open('x',encoding='utf-8') as f:
    json.dump({'status':'SIX_NEW_OWNED_PACKAGES_PHYSICALLY_BACKED_UP','files':rows},f,indent=2)
print('Six new packages backed up; no Source Content modified')
