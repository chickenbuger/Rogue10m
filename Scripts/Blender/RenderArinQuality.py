"""Read-only fixed-camera and critical-pose evidence, never save the asset.

Run in a fresh background Blender process. Exact rotations are recorded in a
manifest rather than mistaking normalized weights for anatomical pose quality.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

sys.path.insert(0,str(Path(__file__).parent))
import BuildArin as common


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--input",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--baseline-only",action="store_true")
    parser.add_argument("--motion-only",action="store_true")
    parser.add_argument("--pose-only",action="store_true")
    args=parser.parse_args(sys.argv[sys.argv.index("--")+1:])
    args.input=args.input.resolve()
    args.out=args.out.resolve()
    if args.input.suffix.lower()==".blend":
        bpy.ops.wm.open_mainfile(filepath=str(args.input.resolve()))
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        if args.input.suffix.lower()==".fbx":
            bpy.ops.import_scene.fbx(filepath=str(args.input.resolve()),use_anim=True)
        elif args.input.suffix.lower()==".glb":
            bpy.ops.import_scene.gltf(filepath=str(args.input.resolve()))
        else:
            raise ValueError("Expected BLEND/FBX/GLB")
    for obj in list(bpy.data.objects):
        if obj.type in {"LIGHT","CAMERA"} or obj.name=="Studio Ground":
            bpy.data.objects.remove(obj,do_unlink=True)
    rig=next(obj for obj in bpy.data.objects if obj.type=="ARMATURE")
    scene=bpy.context.scene
    if scene.world is None:
        scene.world=bpy.data.worlds.new("Arin Studio World")
    scene.frame_set(1)
    camera=common.studio()
    args.out.mkdir(parents=True,exist_ok=True)
    common.EVIDENCE=args.out
    views=[("front",(0,-4,.95)),("right",(4,0,.95)),("left",(-4,0,.95)),
           ("oblique",(2.6,-4,1.45)),("back",(0,4,.95))]
    report={"input":str(args.input.resolve()),"sha256":hashlib.sha256(args.input.read_bytes()).hexdigest(),
            "blender_version":bpy.app.version_string,"presentation":"BuildArin.studio Cycles32 AgX fixed meter cameras",
            "evidence":[],"geometry_repaired":False,"source_saved":False}
    def capture(name,position,target=(0,0,.87),scale=1.95):
        common.render(camera,name,position,target,scale)
        path=args.out/(name+".png")
        report["evidence"].append({"view":name,"position":list(position),"target":list(target),"scale":scale,
             "file":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
    if args.motion_only:
        scene.cycles.samples=16
        scene.render.resolution_x,scene.render.resolution_y=600,760
        action=rig.animation_data.action if rig.animation_data else None
        if action is None:
            raise RuntimeError('Motion preview requires an actual imported action')
        start,end=map(float,action.frame_range)
        frames=[start+(end-start)*index/12 for index in range(13)]
        for index,frame in enumerate(frames):
            scene.frame_set(math.floor(frame),subframe=frame-math.floor(frame))
            bpy.context.view_layer.update()
            capture("idle-frame-%03d"%index,(2.6,-4,1.45))
        report["frames"]=frames
        report["fps"]=scene.render.fps
        report['action_frame_range']=[start,end]
        report['clip_duration_seconds']=(end-start)/(scene.render.fps/scene.render.fps_base)
        (args.out/"motion-manifest.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
        return
    if not args.pose_only:
        for name,position in views:
            capture("idle-"+name,position)
        capture("idle-top",(0,0,5),(0,0,.85),1.9)
        capture("idle-face",(.35,-2,1.6),(0,0,1.555),.38)
        capture("idle-face-profile",(2,0,1.6),(0,0,1.555),.38)
        capture("idle-hands",(.6,-2,.8),(.29,-.02,.70),.26)
        capture("idle-boots",(.6,-2,.4),(0,-.015,.17),.46)
    if args.baseline_only:
        (args.out/"render-manifest.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
        return
    # QA has no keyframe insertions and no saved action/asset writes.
    original_action=rig.animation_data.action if rig.animation_data else None
    rig.animation_data.action=None
    def reset():
        for bone in rig.pose.bones:
            bone.rotation_mode="QUATERNION"
            bone.rotation_quaternion=(1,0,0,0)
            bone.location=(0,0,0)
            bone.scale=(1,1,1)
    reset()
    bpy.context.view_layer.update()
    for name,position in views[:2]:
        capture("neutral-"+name,position)
    for bone,angle in [("upperarm_l",68),("upperarm_r",-68)]:
        common.world_rotation(rig,bone,(0,1,0),angle)
    cases={
       "arm-bend":{"upperarm_l":((0,1,0),-52),"lowerarm_l":((0,1,0),60)},
       "reach":{"upperarm_l":((0,0,1),-90),"lowerarm_l":((0,1,0),15)},
       "torso-twist":{"spine_02":((0,0,1),15),"spine_03":((0,0,1),10)},
       "knee-bend":{"thigh_l":((1,0,0),-25),"calf_l":((1,0,0),70)},
    }
    report["qa_poses"]={}
    for name,rotations in cases.items():
        reset()
        for bone,angle in [("upperarm_l",68),("upperarm_r",-68)]:
            common.world_rotation(rig,bone,(0,1,0),angle)
        for bone,(axis,angle) in rotations.items():
            common.world_rotation(rig,bone,axis,angle)
        bpy.context.view_layer.update()
        report["qa_poses"][name]=rotations
        capture(name+"-oblique",(2.6,-4,1.45))
        capture(name+"-right",(4,0,.95))
    rig.animation_data.action=original_action
    scene.frame_set(1)
    bpy.context.view_layer.update()
    # Surface form evidence in neutral clay is additional to textured evidence.
    clay=bpy.data.materials.new("QA Neutral Clay")
    clay.use_nodes=True
    bsdf=clay.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value=(.38,.38,.38,1)
    bsdf.inputs["Roughness"].default_value=.8
    scene.view_layers[0].material_override=clay
    capture("clay-face",(.35,-2,1.6),(0,0,1.555),.38)
    capture("clay-body",(2.6,-4,1.45))
    scene.view_layers[0].material_override=None
    (args.out/"render-manifest.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print("ARIN_EVIDENCE_COMPLETE "+str(len(report["evidence"])),flush=True)


if __name__=="__main__":
    main()
