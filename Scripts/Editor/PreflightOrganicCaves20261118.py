"""Read only public import/mesh query bindings before any organic asset authoring."""
from pathlib import Path
import json
import unreal as u
OUT=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118/api-preflight.json')
assert not OUT.exists()
required={'AssetImportTask':(), 'FbxImportUI':(), 'FbxFactory':(), 'StaticMeshEditorSubsystem':('get_collision_complexity',),
 'GeometryScript_AssetUtils':('copy_mesh_from_static_mesh_v2',),
 'GeometryScript_MeshQueries':('get_all_vertex_positions','get_all_triangle_indices'),
 'GeometryScript_List':('convert_vector_list_to_array','convert_triangle_list_to_array'),
 'DynamicMesh':(), 'GeometryScriptCopyMeshFromAssetOptions':(), 'GeometryScriptMeshReadLOD':()}
bindings={}
for name,methods in required.items():
    obj=getattr(u,name,None)
    bindings[name]={'class_available':obj is not None,'methods':{m:callable(getattr(obj,m,None))for m in methods}}
report={'bindings':bindings,'assets_or_properties_changed':False,'all_available':all(r['class_available']and all(r['methods'].values())for r in bindings.values())}
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(report,f,indent=2)
print('ORGANIC_IMPORT_BINDINGS '+json.dumps(report))
