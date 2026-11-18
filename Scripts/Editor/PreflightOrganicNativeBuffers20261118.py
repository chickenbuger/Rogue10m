"""Transient DynamicMesh API probe; creates no assets and changes no scene."""
from pathlib import Path
import json
import unreal as u
OUT=Path('D:/Project/Rogue10m/Feature/doc/evidence/cave-organic20261118/native-buffer-api.json')
assert not OUT.exists()
r={}
for name in ['GeometryScriptSimpleMeshBuffers','GeometryScriptCreateNewStaticMeshAssetOptions','GeometryScriptGroupLayer','GeometryScriptMeshSelection']:
    obj=getattr(u,name)();r[name]={'doc':str(obj.__doc__),'members':[n for n in dir(obj) if not n.startswith('_')]}
for name in ['GeometryScript_UVs','GeometryScript_Materials']:
    cls=getattr(u,name);r[name]={n:str(getattr(cls,n).__doc__) for n in dir(cls) if 'projection' in n or 'polygroup_material' in n}
r['build_settings']={n:str(getattr(u.StaticMeshEditorSubsystem,n).__doc__) for n in ['get_lod_build_settings','set_lod_build_settings']}
b=u.GeometryScriptSimpleMeshBuffers()
b.vertices=[u.Vector(0,0,0),u.Vector(100,0,0),u.Vector(0,100,0),u.Vector(0,0,100)]
b.triangles=[u.IntVector(0,2,1),u.IntVector(0,1,3),u.IntVector(0,3,2),u.IntVector(1,2,3)]
groups=[n for n in dir(b) if n.replace('_','').lower()=='trigroupids'];assert len(groups)==1
setattr(b,groups[0],[0,0,0,1])
d=u.DynamicMesh();a=u.GeometryScript_MeshEdits.append_buffers_to_mesh(d,b,0)
m=u.GeometryScript_Materials.set_polygroup_material_id(d,u.GeometryScriptGroupLayer(),1,1)
r['transient_tetra']={'group_property':groups[0],'triangles':d.get_triangle_count(),'append_tuple':str(type(a)), 'material_result':str(m),'dirty':[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
with OUT.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_TRANSIENT_NATIVE_BUFFER_API_PASS')
