"""Read exact replacement binding and RenderData availability return shape."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('D:/Project/Rogue10m');BASE=ROOT/'Feature/doc/evidence/cave-organic20261118';p=BASE/'native-repair-api.json';assert not p.exists()
r={}
for n in dir(u.GeometryScript_AssetUtils):
    if n.replace('_','').lower()in['copymeshtostaticmesh','checkstaticmeshhasavailablelod']:
        r[n]=str(getattr(u.GeometryScript_AssetUtils,n).__doc__)
cls=u.GeometryScriptCopyMeshToAssetOptions;r['copy_options']=str(cls().__doc__)
r['write_lod']=str(u.GeometryScriptMeshWriteLOD().__doc__)
conf=json.loads((BASE/'validation-config.json').read_text());mesh=u.load_asset(conf['expected_mesh_paths']['Q'])
lod=u.GeometryScriptMeshReadLOD(lod_type=u.GeometryScriptLODType.RENDER_DATA,lod_index=0)
name=[n for n in dir(u.GeometryScript_AssetUtils)if n.replace('_','').lower()=='checkstaticmeshhasavailablelod'][0]
v=getattr(u.GeometryScript_AssetUtils,name)(mesh,lod);r['availability_actual']={'type':str(type(v)),'repr':str(v)}
with p.open('x',encoding='utf-8',newline='\n')as f:json.dump(r,f,indent=2)
print('ORGANIC_NATIVE_REPAIR_API_READ_ONLY')
