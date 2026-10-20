"""Actual assembled form preview before spending effort on texture baking."""
import json
from pathlib import Path
import sys
import bpy

sys.path.insert(0,str(Path(__file__).parent))
import BuildRefinedArin as refined
import BuildArin as common

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.world=bpy.data.worlds.new("Arin Studio World")
collection=bpy.data.collections.new("Arin Form Candidate")
bpy.context.scene.collection.children.link(collection)
materials=common.materials()
objects=refined.ArinBodyRefined.build_body(materials,collection)+refined.ArinHeadRefined.build_head(materials,collection)
rig=common.create_rig()
refined.bind_refined(objects,rig)
common.idle(rig)
out=refined.EVIDENCE/"form-preview-final"
out.mkdir(parents=True,exist_ok=True)
common.EVIDENCE=out
camera=common.studio()
if "--back-only" in sys.argv:
    common.render(camera,"back",(0,4,.95))
    bpy.ops.wm.save_as_mainfile(filepath=str(out/"form-candidate.blend"))
    print("ARIN_SCOPED_BACK_REVIEW_COMPLETE",flush=True)
    raise SystemExit(0)
common.render(camera,"face",(.35,-2,1.6),(0,0,1.555),.38)
common.render(camera,"front",(0,-4,.95))
common.render(camera,"profile",(2,0,1.6),(0,0,1.555),.38)
common.render(camera,"body-oblique",(2.6,-4,1.45))
common.render(camera,"back",(0,4,.95))
common.render(camera,"hands",(.6,-2,.8),(.29,-.02,.70),.26)
bpy.ops.wm.save_as_mainfile(filepath=str(out/"form-candidate.blend"))
rig.animation_data.action=None
for bone in rig.pose.bones:
    bone.rotation_mode="QUATERNION"
    bone.rotation_quaternion=(1,0,0,0)
for name,angle in (("upperarm_l",68),("upperarm_r",-68)):
    common.world_rotation(rig,name,(0,1,0),angle)
common.world_rotation(rig,"thigh_l",(1,0,0),-25)
common.world_rotation(rig,"calf_l",(1,0,0),70)
bpy.context.view_layer.update()
common.render(camera,"knee-bend-oblique",(2.6,-4,1.45))
common.render(camera,"knee-bend-right",(4,0,.95))
report={"parts":len(objects),"triangles":sum(sum(len(p.vertices)-2 for p in obj.data.polygons) for obj in objects),
        "bones":len(rig.data.bones),"actual_blender":bpy.app.version_string,"requires_visual_review":True}
(out/"form-report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print(json.dumps(report),flush=True)
