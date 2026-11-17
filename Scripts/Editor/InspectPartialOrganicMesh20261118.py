"""Read-only actual native Q source geometry after comparator rejection."""
from pathlib import Path
import importlib.util
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');OUT=ROOT/'tmp/OrganicCaves20261118/Q-native-before-settings.json';assert not OUT.exists()
s=importlib.util.spec_from_file_location('organic_partial_mesh_qa',ROOT/'Scripts/Editor/OrganicCaves20261118QA.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
config=json.loads((ROOT/'Feature/doc/evidence/cave-organic20261118/validation-config.json').read_text())
mesh=u.load_asset(config['expected_mesh_paths']['Q']);assert mesh
vs,ts,ids=m.extract_static_mesh_geometry(u,mesh)
settings=u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_lod_build_settings(mesh,0)
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump({'vertices_cm':vs,'triangles':ts,'triangle_material_ids':ids,'build_settings':str(settings)},f,separators=(',',':'))
print('ORGANIC_PARTIAL_Q_READ_ONLY_GEOMETRY_EXTRACTED')
