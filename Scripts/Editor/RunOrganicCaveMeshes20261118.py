"""Explicit native creation of two source-pinned cave mesh assets; no save."""
from pathlib import Path
import importlib.util
import json
import os
import traceback
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'author-live'
assert os.getpid()==49456 and not (OUT/'meshes.json').exists()
def load(name,file):
    s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
api=load('organic_mesh_scene_api','Scripts/Editor/NaturalWorldDressing20261117.py')
m=load('explicit_organic_meshes','Scripts/Editor/OrganicCaves20261118Mesh.py')
config=json.loads((BASE/'validation-config.json').read_text(encoding='utf-8'))
spec=json.loads((BASE/'geometryspec-v2.json').read_text(encoding='utf-8'))
materials=json.loads((OUT/'materials.json').read_text(encoding='utf-8'));assert materials['success']
assert set(api._dirty(u))==set(materials['dirty_after'])
r={'passed':False,'pid':os.getpid(),'assets_saved':False,'sites':{}}
try:
    for site in spec['sites']:
        sid=site['site_id']
        mesh,report=m.create_organic_cave_mesh(u,site,config['expected_mesh_paths'][sid].split('.')[0],
            u.load_asset(config['expected_material_path']),u.load_asset(config['expected_floor_material_path']),allow_editor_mutation=True)
        r['sites'][sid]=report
        with (OUT/('mesh-'+sid+'.json')).open('x',encoding='utf-8',newline='\n')as f:json.dump(report,f,indent=2)
    r['passed']=True
    r['dirty_packages_after']=api._dirty(u)
    assert len(r['dirty_packages_after'])==4
    with(OUT/'meshes.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    print('ORGANIC_CAVE_TWO_NATIVE_MESHES_CREATED_UNSAVED')
except Exception:
    r['traceback']=traceback.format_exc();r['dirty_packages_after']=api._dirty(u)
    failure=OUT/('meshes-failure-'+str(1+len(list(OUT.glob('meshes-failure*.json'))))+'.json')
    with failure.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    raise
