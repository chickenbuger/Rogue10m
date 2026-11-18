"""Pin nested actual compiler/linker response files without changing generated files."""
from pathlib import Path
import hashlib
import json
import re
import types
R=Path(__file__).resolve().parents[3];E=R/'Feature/doc/evidence/hub-entry-windows20261114A'
P=R/'Feature/implementation/native51-internal-smoke-cook-fix-f/strict_primitives.py'
raw=P.read_bytes();assert hashlib.sha256(raw).hexdigest().upper()=='0CDDDF8F2C5F8C8894DD1D5B8992546E0993E392F479DAA8B551CC7D891D6E0C'
c=types.ModuleType('response_pin');c.__file__=str(P);exec(compile(raw,str(P),'exec'),c.__dict__)
proof=c.load(c.stable_bytes(E/'compile-provenance.json'))
for pin in proof['records'].values():c.read_pin(pin)
pending=[proof['records'][key]['path'] for key in ('unity_rsp','pch_rsp','link_rsp')]
seen={};edges=[]
while pending:
    path=c.physical(pending.pop(),True)
    assert c.child(path,R/'Intermediate'),path
    if str(path) in seen:continue
    pin=c.file_pin(path);text=c.read_pin(pin).decode('utf-8-sig')
    seen[str(path)]=pin
    assert not re.search(r'WITH_(?:LIVE_CODING|HOT_RELOAD|RELOAD)\b',text),path
    for quoted,unquoted in re.findall(r'(?m)^\s*@(?:"([^"\r\n]+)"|([^\s"\r\n]+))\s*$',text):
        child=c.physical(quoted or unquoted,True)
        edges.append({'from':str(path),'to':str(child)})
        pending.append(str(child))
assert any(Path(pin['path']).name=='Rogue10m.Shared.rsp' for pin in seen.values())
result={'status':'ACTUAL_NESTED_RESPONSE_CHAIN_PINNED_NO_RELOAD_MACRO_OVERRIDES_IN_RECORDED_RSP_FILES',
    'parent_proof':c.file_pin(E/'compile-provenance.json'),'response_files':list(seen.values()),'nested_edges':edges,
    'scope':'Current generated unity/PCH/link response graph, forced includes in parent12; not every translation unit or global ABI certification',
    'generated_files_modified':0,'Game_or_UE_executions':0}
target=E/'compile-response-supplement.json';assert not target.exists()
with target.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
print(json.dumps({'status':result['status'],'response_files':len(seen),'nested_edges':len(edges)},indent=2))
