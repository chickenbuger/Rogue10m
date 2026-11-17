"""Read-only render data / UE primitive winding / collision diagnostics."""
from pathlib import Path
import importlib.util
import json
import traceback
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'native-mesh-diagnostic.json';assert not OUT.exists()
def mod(n,p):
    s=importlib.util.spec_from_file_location(n,ROOT/p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
qa=mod('organic_diagnostic_qa','Scripts/Editor/OrganicCaves20261118QA.py')
rqa=mod('organic_diagnostic_render','Scripts/Editor/OrganicCaves20261118RenderQA.py')
config=json.loads((BASE/'validation-config.json').read_text());r={}
for sid,p in config['expected_mesh_paths'].items():
    m=u.load_asset(p)
    r[sid]={'render_triangle_count':m.get_num_triangles(0),'body_setup':str(m.get_editor_property('body_setup'))}
cube=u.load_asset('/Engine/BasicShapes/Cube');v,t,i=qa.extract_static_mesh_geometry(u,cube)
with(ROOT/'tmp/OrganicCaves20261118/EngineCube-source.json').open('x',encoding='utf-8')as f:json.dump({'vertices_cm':v,'triangles':t,'triangle_material_ids':i},f)
try:r['render_validation']=rqa.validate_render_geometry(config,expected_qa_sha256=__import__('hashlib').sha256((ROOT/'Scripts/Editor/OrganicCaves20261118QA.py').read_bytes()).hexdigest())
except Exception:r['render_validation_error']=traceback.format_exc()
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_MESH_READ_ONLY_RENDER_PHYSICS_DIAGNOSTIC')
