"""Read only the five already-loaded source-material graphs and usage flags."""
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
OUT=ROOT/'Feature/doc/evidence/landscape-natural-boundary20261117/author-live'
assert os.getpid()==json.loads((OUT/'startup.json').read_text())['pid']
report=[]
for name in ('DarkTimber','WarmStone','Banner_Teal','Banner_Ochre','Banner_Blue'):
    package='/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_'+name
    asset=u.find_object(None,package+'.'+package.rsplit('/',1)[1])
    assert isinstance(asset,u.Material)
    nodes=list(u.MaterialEditingLibrary.get_material_expressions(asset))
    file=ROOT/'Content'/(package[6:]+'.uasset')
    report.append({'package':package,'used_with_instanced_static_meshes':asset.get_editor_property('used_with_instanced_static_meshes'),
        'expression_classes':dict(Counter(n.get_class().get_name() for n in nodes)),
        'expression_count':len(nodes),'source_file':file.relative_to(ROOT).as_posix(),
        'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
with (OUT/'source-material-inspection.json').open('x') as f:json.dump({'read_only':True,'materials':report},f,indent=2)
print('NATURAL_WORLD SOURCE_MATERIAL_GRAPH_READ_ONLY')
