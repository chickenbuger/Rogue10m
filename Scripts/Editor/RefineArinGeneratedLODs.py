"""Protect garment/eye boundaries in only Arin v2's generated LOD1/2."""
import importlib
import sys
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0,str(project/'Scripts/Editor'))
import ApplyRefinedArin as application
application=importlib.reload(application)
data,blueprint,cdo,actor=application._guide_context()
guide_before=application._guide_state(data,blueprint,cdo,actor)
manifest,sources,hashes=application._surface_sources()
mesh=application._require(application.MESH_PATH,unreal.SkeletalMesh)
application._verify_provenance(mesh,hashes)
subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
before=application._lod_metrics(mesh)
application._json(application.EVIDENCE/'unreal-lod-first-reduction.json',before)
old_skeleton=mesh.get_editor_property('skeleton')
old_lod0_vertices=subsystem.get_num_verts(mesh,0)
mesh.modify()
application._configure_generated_lods(mesh)
if not subsystem.regenerate_lod(mesh,3,False,False):
    raise RuntimeError('Owned LOD regeneration failed')
after=application._lod_metrics(mesh)
if subsystem.get_num_verts(mesh,0)!=old_lod0_vertices or mesh.get_editor_property('skeleton')!=old_skeleton:
    raise RuntimeError('Imported LOD0 or shared skeleton changed')
if guide_before!=application._guide_state(data,blueprint,cdo,actor):
    raise RuntimeError('Guide properties changed during LOD refinement')
application._save(mesh)
application._json(application.EVIDENCE/'unreal-lod-refinement.json',{
    'status':'generated_requires_visual_review','source_hashes':hashes,
    'reason':'Actual initial .25/.125 reduction exposed ivory garment patches and lost eye detail',
    'before':before,'after':after,'lod0_vertices_unchanged':True,
    'shared_skeleton_unchanged':True,'guide_properties_unchanged':True,
    'modified_and_saved_only':application.MESH_PATH})
unreal.log('ARIN_BOUNDARY_PROTECTED_LODS_SAVED')
