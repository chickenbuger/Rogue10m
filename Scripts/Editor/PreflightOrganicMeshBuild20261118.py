"""Read only native GeometryScript build bindings; no asset creation."""
from pathlib import Path
import json
import unreal as u
OUT=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118/mesh-build-api.json')
assert not OUT.exists()
targets={'GeometryScript_MeshEdits':['append_buffers_to_mesh'],
 'GeometryScript_NewAssetUtils':['create_new_static_mesh_asset_from_mesh'],
 'GeometryScript_Materials':['set_polygroup_material_id'],
 'GeometryScriptSimpleMeshBuffers':[], 'GeometryScriptCreateNewStaticMeshAssetOptions':[]}
r={}
for cls,names in targets.items():
    obj=getattr(u,cls,None);r[cls]={'available':obj is not None,'methods':{n:str(getattr(obj,n,None).__doc__)for n in names}}
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_MESH_NATIVE_BUILD_BINDINGS '+json.dumps(r))
