"""Read-only diagnosis of the first two unsaved derivatives; no retry."""
from pathlib import Path
import importlib.util
import json
import unreal as u

ROOT=Path('D:/Project/Rogue10m')
BASE=ROOT/'Feature/doc/evidence/caves20261118'
spec=importlib.util.spec_from_file_location('partial_cave_materials',ROOT/'Scripts/Editor/CaveMaterials20261118.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
reader=m.module('partial_cave_graph','Scripts/Editor/NaturalRoadMaterial20261117.py')
reader._guard(u)
before=reader._dirty(u)
assert set(before)=={m.OWN+'/M_CaveSurface',m.OWN+'/MI_CaveGrass'}
lib=u.MaterialEditingLibrary
report={'dirty_before':before,'materials':{},'editor_asset_mutations':False,'assets_saved':False}
for name in ['M_CaveSurface','MI_CaveGrass']:
 obj=u.load_asset(m.OWN+'/'+name)
 assert obj is not None
 base=obj.get_editor_property('parent') if isinstance(obj,u.MaterialInstance) else obj
 report['materials'][name]={'class':obj.get_class().get_path_name(),'parent':base.get_path_name(),
   'texture_parameter_names':list(map(str,lib.get_texture_parameter_names(base))),
   'scalar_parameter_names':list(map(str,lib.get_scalar_parameter_names(base))),
   'graph':reader._fingerprint(u,lib,base),
   'texture_expressions':[{'class':n.get_class().get_name(),'parameter_name':str(n.get_editor_property('parameter_name')),
      'texture':n.get_editor_property('texture').get_path_name() if n.get_editor_property('texture') else None}
     for n in lib.get_material_expressions(base) if isinstance(n,u.MaterialExpressionTextureSampleParameter2D)]}
 if isinstance(obj,u.MaterialInstance):
  report['materials'][name]['actual_texture_values']={str(k):(lib.get_material_instance_texture_parameter_value(obj,k).get_path_name()
    if lib.get_material_instance_texture_parameter_value(obj,k) else None) for k in lib.get_texture_parameter_names(base)}
inspection=m.module('partial_cave_inspection','Scripts/Editor/InspectCaveSites20261118.py')
baseline=json.loads((BASE/'baseline-live/baseline-materials.json').read_text())
states={p:inspection.material_state(u,u.load_asset(p),reader) for p in [m.GROUND,m.GRASS,m.ROCK,m.LAMP]}
report['source_materials_unchanged']=all(value==(baseline['materials'].get(p) or baseline['materials'].get(p+'.'+p.rsplit('/',1)[1])) for p,value in states.items())
report['native']=json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(False))
report['actor_count']=len(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
assert report['source_materials_unchanged'] and report['actor_count']==1404 and before==reader._dirty(u)
with (BASE/'partial-material-inspection.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
print('CAVES PARTIAL_MATERIAL_INSPECTION_READ_ONLY_COMPLETE')
