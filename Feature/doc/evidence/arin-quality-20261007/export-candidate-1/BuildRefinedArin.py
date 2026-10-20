"""Build Arin v2 in a dedicated Blender process; preserve v1 artifacts."""
import hashlib
import importlib
import json
from pathlib import Path
import sys

import bpy

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import BuildArin as common
import ArinBodyRefined
import ArinHeadRefined
import ArinSurface

OUT = ROOT/"Art/NPC/Arin/Refined"
EVIDENCE = ROOT/"Feature/doc/evidence/arin-quality-20261007"


def bind_refined(objects, rig):
    bilateral = [obj for obj in objects if obj.get("arin_bilateral_legs")]
    common.bind(objects, rig)
    for obj in objects:
        for modifier in obj.modifiers:
            if modifier.type=="ARMATURE":
                # Match FBX/glTF/Unreal linear blend skinning in authored QA.
                modifier.use_deform_preserve_volume=False
    for obj in bilateral:
        for vertex in obj.data.vertices:
            p = obj.matrix_world @ vertex.co
            side = "l" if p.x>=0 else "r"
            if abs(p.x)<.045 and p.z>.89:
                weights = {"pelvis":1}
            else:
                weights = common.blend_axis(p.z,[(.13,"foot_"+side),(.23,"calf_"+side),
                    (.51,"calf_"+side),(.60,"thigh_"+side),(.87,"thigh_"+side),(1.00,"pelvis")])
            for group in obj.vertex_groups:
                group.remove([vertex.index])
            for bone, weight in weights.items():
                if weight>1e-6:
                    obj.vertex_groups[bone].add([vertex.index],weight,"REPLACE")


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.world=bpy.data.worlds.new("Arin Studio World")
    scene.unit_settings.system="METRIC"
    scene.unit_settings.scale_length=1
    collection = bpy.data.collections.new("Arin Refined Editable Parts")
    scene.collection.children.link(collection)
    materials = common.materials()
    # Warmer, less glossy skin and hair; all actual palette/variation is baked.
    materials["hair"].node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value=.64
    materials["hair_light"].node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value=.69
    objects = ArinBodyRefined.build_body(materials,collection)+ArinHeadRefined.build_head(materials,collection)
    triangles = sum(sum(len(p.vertices)-2 for p in obj.data.polygons) for obj in objects)
    if triangles>55000:
        raise RuntimeError("Candidate exceeds55k triangle budget: "+str(triangles))
    rig = common.create_rig()
    bind_refined(objects,rig)
    common.idle(rig)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"Arin_EditableParts.blend"))
    authored_parts = len(objects)
    objects, surface = ArinSurface.build_atlases(objects,rig,materials,OUT/"Textures")
    common.OUT = OUT
    common.export(objects,rig)
    # Portable final scene: no unused pre-bake procedural materials/images.
    for material in list(bpy.data.materials):
        if material.users==0:
            bpy.data.materials.remove(material)
    camera = common.studio()
    scene.render.engine="CYCLES"
    scene.cycles.samples=32
    scene.frame_set(1)
    common.EVIDENCE = EVIDENCE/"candidate"
    common.EVIDENCE.mkdir(exist_ok=True)
    for name,pos in [("arin-front",(0,-4,.95)),("arin-right",(4,0,.95)),
            ("arin-left",(-4,0,.95)),("arin-oblique",(2.6,-4,1.45)),("arin-back",(0,4,.95))]:
        common.render(camera,name,pos)
    common.render(camera,"arin-top",(0,0,5),(0,0,.85),1.9)
    common.render(camera,"arin-face",(.35,-2,1.60),(0,0,1.555),.38)
    common.render(camera,"arin-face-profile",(2,0,1.60),(0,0,1.555),.38)
    common.render(camera,"arin-hand",(.7,-2,.75),(.31,-.025,.70),.28)
    common.render(camera,"arin-boots",(.6,-2,.40),(0,-.015,.17),.46)
    scene.frame_set(1)
    for image in list(bpy.data.images):
        if image.source=="FILE" and image.users>0:
            image.filepath="//Textures/"+Path(image.filepath).name
            image.pack()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/"Arin.blend"))
    report={"status":"candidate_exported_requires_freshprocess_and_visual_review",
        "blender_version":bpy.app.version_string,"authored_parts":authored_parts,
        "runtime_meshes":len(objects),"triangles":triangles,"bones":len(rig.data.bones),
        "materials":4,"units":"meters Zup front-Y","idle_seconds":2,
        "surface_manifest":str(OUT/"surface-manifest.json"),"files":{}}
    for path in OUT.iterdir():
        if path.suffix in {".fbx",".glb",".blend"}:
            report["files"][path.name]={"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
    (EVIDENCE/"model-build.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps(report),flush=True)
    return report


if __name__=="__main__":
    result=main()
