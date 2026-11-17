"""Resume the one inspected unsaved Q mesh, then create Z exactly once."""
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
api=load('organic_resume_api','Scripts/Editor/NaturalWorldDressing20261117.py')
m=load('organic_resume_meshes','Scripts/Editor/OrganicCaves20261118Mesh.py')
config=json.loads((BASE/'validation-config.json').read_text(encoding='utf-8'));spec=json.loads((BASE/'geometryspec-v2.json').read_text(encoding='utf-8'))
assert json.loads((OUT/'meshes-failure-2.json').read_text())['dirty_packages_after']==api._dirty(u)
r={'passed':False,'pid':os.getpid(),'assets_saved':False,'explicit_resume_of_inspected_Q':True,'sites':{}}
rock=u.load_asset(config['expected_material_path']);floor=u.load_asset(config['expected_floor_material_path'])
try:
    for site in spec['sites']:
        sid=site['site_id']
        if sid=='Q':
            mesh=u.load_asset(config['expected_mesh_paths'][sid]);assert mesh
            report=m.finish_organic_cave_mesh(u,mesh,site,rock,floor,allow_editor_mutation=True)
        else:mesh,report=m.create_organic_cave_mesh(u,site,config['expected_mesh_paths'][sid].split('.')[0],rock,floor,allow_editor_mutation=True)
        r['sites'][sid]=report
        with(OUT/('mesh-'+sid+'.json')).open('x',encoding='utf-8',newline='\n')as f:json.dump(report,f,indent=2)
    r['dirty_packages_after']=api._dirty(u);assert len(r['dirty_packages_after'])==4;r['passed']=True
    with(OUT/'meshes.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    print('ORGANIC_CAVE_TWO_NATIVE_MESHES_CREATED_UNSAVED')
except Exception:
    r['traceback']=traceback.format_exc();r['dirty_packages_after']=api._dirty(u)
    with(OUT/'meshes-resume-failure.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    raise
