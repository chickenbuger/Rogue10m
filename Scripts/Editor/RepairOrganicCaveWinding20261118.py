"""Correct the two exact unsaved new mesh assets to UE clockwise winding."""
from pathlib import Path
import importlib.util
import json
import os
import traceback
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';OUT=BASE/'author-live'
assert os.getpid()==49456 and not(OUT/'winding-repair.json').exists()
s=importlib.util.spec_from_file_location('organic_explicit_winding_repair',ROOT/'Scripts/Editor/OrganicCaves20261118Mesh.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
config=json.loads((BASE/'validation-config.json').read_text());spec=json.loads((BASE/'geometryspec-v2.json').read_text())
r={'passed':False,'sites':{},'assets_saved':False,'artist_source_modified':False}
try:
    for site in spec['sites']:
        sid=site['site_id'];mesh=u.load_asset(config['expected_mesh_paths'][sid]);assert mesh
        r['sites'][sid]=m.repair_organic_cave_mesh(u,mesh,site,u.load_asset(config['expected_material_path']),u.load_asset(config['expected_floor_material_path']),allow_editor_mutation=True)
    r['passed']=True
    with(OUT/'winding-repair.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    print('ORGANIC_TWO_UNSAVED_MESH_WINDING_REPAIRED')
except Exception:
    r['traceback']=traceback.format_exc()
    with(OUT/'winding-repair-failure.json').open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
    raise
