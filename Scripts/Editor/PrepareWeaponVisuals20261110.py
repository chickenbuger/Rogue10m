"""Create original low-poly weapons through the Unreal OBJ importer; no vendor files changed."""
from pathlib import Path
import math,json,unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'Art/Weapons/Starter20261110'
DEST='/Game/Weapons/Starter20261110'
OUT=ROOT/'Feature/doc/evidence/sword-knuckle-20261110'
class Mesh:
 def __init__(self): self.v=[];self.faces=[]
 def face(self,points,material):
  base=len(self.v)+1;self.v+=points
  for i in range(1,len(points)-1): self.faces.append((material,[base,base+i,base+i+1]))
 def box(self,c,s,mat):
  x,y,z=c;a,b,d=[v/2 for v in s]
  p=[(x-a,y-b,z-d),(x+a,y-b,z-d),(x+a,y+b,z-d),(x-a,y+b,z-d),(x-a,y-b,z+d),(x+a,y-b,z+d),(x+a,y+b,z+d),(x-a,y+b,z+d)]
  for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:self.face([p[i] for i in f],mat)
 def rings(self,rings,mat):
  for a,b in zip(rings[:-1],rings[1:]):
   for i in range(len(a)):j=(i+1)%len(a);self.face([a[i],a[j],b[j],b[i]],mat)
  self.face(list(reversed(rings[0])),mat);self.face(rings[-1],mat)
 def cylinder(self,z0,z1,r,n,mat):
  self.rings([[(r*math.cos(i*2*math.pi/n),r*math.sin(i*2*math.pi/n),z) for i in range(n)] for z in [z0,z1]],mat)
 def torus(self,c,major,minor,mat):
  # Finger aperture axis X; guard spans Y and face projects along X.
  rows=[]
  for i in range(17):
   a=i*2*math.pi/16
   rows.append([(c[0]+minor*math.sin(j*2*math.pi/8),c[1]+(major+minor*math.cos(j*2*math.pi/8))*math.cos(a),c[2]+(major+minor*math.cos(j*2*math.pi/8))*math.sin(a)) for j in range(8)])
  for a,b in zip(rows[:-1],rows[1:]):
   for j in range(8):k=(j+1)%8;self.face([a[j],a[k],b[k],b[j]],mat)
 def write(self,name):
  lines=['# Original Rogue10m weapon geometry authored 2026-11-10. Units: centimeters.','mtllib weapons.mtl','o '+name]+['v %.6f %.6f %.6f'%p for p in self.v]
  previous=None
  for material,f in self.faces:
   if previous!=material: lines.append('usemtl '+material);previous=material
   lines.append('f '+' '.join(map(str,f)))
  path=ART/(name+'.obj');path.write_text('\n'.join(lines)+'\n',encoding='utf-8');return path

def material(name,color,metal,rough):
 path=DEST+'/M_'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+name,DEST,unreal.Material,unreal.MaterialFactoryNew())
 lib=unreal.MaterialEditingLibrary
 col=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-400,0);col.set_editor_property('constant',unreal.LinearColor(*color,1))
 lib.connect_material_property(col,'',unreal.MaterialProperty.MP_BASE_COLOR)
 for p,value,y in [(unreal.MaterialProperty.MP_METALLIC,metal,150),(unreal.MaterialProperty.MP_ROUGHNESS,rough,250)]:
  e=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-400,y);e.set_editor_property('r',value);lib.connect_material_property(e,'',p)
 lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat,False);return mat

def main():
 ART.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
 ART.joinpath('weapons.mtl').write_text('newmtl Steel\nKd 0.24 0.30 0.36\nnewmtl Brass\nKd 0.47 0.28 0.09\nnewmtl Leather\nKd 0.07 0.025 0.015\n',encoding='utf-8')
 sword=Mesh()
 # Four-facet diamond blade, tapered to a point with a broad readable silhouette.
 sword.rings([[(-w,0,z),(0,-t,z),(w,0,z),(0,t,z)] for z,w,t in [(18,7.5,1.6),(26,9,1.8),(112,6.5,1.5),(140,.12,.12)]],'Steel')
 sword.box((0,0,16),(34,5,4),'Brass');sword.box((-15,0,18),(5,6,7),'Brass');sword.box((15,0,18),(5,6,7),'Brass')
 sword.cylinder(-26,14,2.5,12,'Leather')
 for z in range(-24,14,3):sword.cylinder(z,z+.55,2.7,12,'Brass')
 sword.cylinder(-30,-26,4.1,8,'Brass');sword.cylinder(13,17,3.2,12,'Brass')
 knuckle=Mesh()
 for y in [-4.5,-1.5,1.5,4.5]:knuckle.torus((0,y,0),1.35,.42,'Brass')
 knuckle.box((.5,0,-1.45),(2,12.4,1.2),'Leather')
 knuckle.box((.5,0,1.35),(2.3,12.4,1.2),'Brass')
 for y in [-4.5,-1.5,1.5,4.5]:knuckle.box((1.5,y,1.5),(1,1.8,1.7),'Steel')
 specs=[('SM_StarterGreatSword',sword),('SM_StarterKnuckle',knuckle)]
 mats={'Steel':material('WeaponSteel',(.24,.30,.36),.85,.28),'Brass':material('WeaponBrass',(.47,.28,.09),.75,.35),'Leather':material('WeaponLeather',(.07,.025,.015),0,.82)}
 results=[]
 for name,geo in specs:
  src=geo.write(name);path=DEST+'/'+name
  if True:
   opts=unreal.FbxImportUI();opts.set_editor_property('import_mesh',True);opts.set_editor_property('import_as_skeletal',False);opts.set_editor_property('import_materials',False);opts.set_editor_property('import_textures',False);opts.set_editor_property('automated_import_should_detect_type',False);opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
   data=opts.get_editor_property('static_mesh_import_data');data.set_editor_property('combine_meshes',True);data.set_editor_property('auto_generate_collision',False);data.set_editor_property('convert_scene',False);data.set_editor_property('convert_scene_unit',False)
   task=unreal.AssetImportTask();task.filename=str(src);task.destination_path=DEST;task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=True;task.options=opts;task.factory=unreal.FbxFactory()
   unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  mesh=unreal.load_asset(path)
  if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Mesh import failed '+path)
  slots=mesh.get_editor_property('static_materials')
  for i,slot in enumerate(slots):
   key=str(slot.get_editor_property('material_slot_name'));mesh.set_material(i,mats.get(key,mats['Steel']))
  unreal.EditorAssetLibrary.set_metadata_tag(mesh,'Rogue10m.Source','Original procedural mesh, Scripts/Editor/PrepareWeaponVisuals20261110.py')
  unreal.EditorAssetLibrary.set_metadata_tag(mesh,'Rogue10m.Units','centimeters; grip center at origin')
  unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
  box=mesh.get_bounding_box();results.append({'path':mesh.get_path_name(),'source':str(src.relative_to(ROOT)),'triangles_authored':len(geo.faces),'vertices_authored':len(geo.v),'bounds_min':[box.min.x,box.min.y,box.min.z],'bounds_max':[box.max.x,box.max.y,box.max.z],'materials':[str(s.material_slot_name) for s in slots]})
 (OUT/'weapons-authored.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
 print('RESULT=WEAPON_VISUAL_ASSETS_CREATED')
if __name__=='__main__':main()