"""Verify a clean FBX reimport and save a 2-view Blender skeleton trace."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix


def main():
    folder, helpers = [Path(arg).resolve() for arg in sys.argv[sys.argv.index("--") + 1:]]
    sys.path.insert(0, str(helpers))
    from AuthorWhirlwind import evaluated_matrices, import_rig

    bpy.ops.wm.read_factory_settings(use_empty=True)
    rig = import_rig(folder / "MannyBreakingKick.fbx")
    scene = bpy.context.scene
    original = json.loads((folder / "blender-poses.json").read_text())
    rows = original["frames"]
    scene_to_cm = Matrix.Diagonal((100.0, 100.0, 100.0, 1.0))
    rebuilt = []
    for index, row in enumerate(rows):
        pose = evaluated_matrices(rig, index / (len(rows) - 1), scene, scene_to_cm)
        rebuilt.append({name: list(pose[name].translation) for name in row})
    max_error = max(math.dist(row[name], other[name]) for row, other in zip(rows, rebuilt) for name in row)
    root = max(math.dist(row["root"], rebuilt[0]["root"]) for row in rebuilt)
    support_slide = max(math.dist(row["ball_l"], rebuilt[0]["ball_l"]) for row in rebuilt)
    lead_travel = max(math.dist(row["ball_r"], rebuilt[0]["ball_r"]) for row in rebuilt)
    action = rig.animation_data.action
    measured = {
        "fbx_sha256": hashlib.sha256((folder / "MannyBreakingKick.fbx").read_bytes()).hexdigest(),
        "bone_count": len(rig.pose.bones), "local_action_range": list(action.frame_range),
        "component_landmark_reimport_error_cm": max_error, "root_travel_cm": root,
        "support_toe_travel_cm": support_slide, "lead_toe_travel_cm": lead_travel,
        "lead_toe_at_montage_13_of_60": rebuilt[13]["ball_r"],
        "lead_toe_min_height_cm": min(row["ball_r"][2] for row in rebuilt),
    }
    (folder / "fresh-reimport.json").write_text(json.dumps(measured, indent=2) + "\n", encoding="utf-8")
    if len(rig.pose.bones) != 89 or max_error > .15 or root > .01 or support_slide > .01 or lead_travel < 90:
        raise RuntimeError(measured)

    # Fixed side and oblique projection of evaluated Blender landmarks, four phases.
    image = bpy.data.images.new("Breaking Kick pose trace", width=880, height=440, alpha=False)
    px = list((.055, .071, .088, 1) * (880 * 440))
    edges = (("head", "pelvis"), ("pelvis", "hand_l"), ("pelvis", "hand_r"),
             ("pelvis", "thigh_l"), ("thigh_l", "calf_l"), ("calf_l", "foot_l"), ("foot_l", "ball_l"),
             ("pelvis", "thigh_r"), ("thigh_r", "calf_r"), ("calf_r", "foot_r"), ("foot_r", "ball_r"))

    def line(a, b, color):
        n = max(abs(b[0] - a[0]), abs(b[1] - a[1]), 1)
        for i in range(n + 1):
            x = round(a[0] + (b[0] - a[0]) * i / n)
            y = round(a[1] + (b[1] - a[1]) * i / n)
            for dx, dy in ((0, 0), (1, 0), (0, 1), (-1, 0), (0, -1)):
                if 0 <= x + dx < 880 and 0 <= y + dy < 440:
                    at = ((y + dy) * 880 + x + dx) * 4
                    px[at:at + 4] = color

    for i, frame in enumerate((0, 8, 13, 35)):
        for angle, level in ((0.0, 245), (.67, 28)):
            def project(raw):
                return (round(56 + i * 220 + (-raw[1] * math.cos(angle) + raw[0] * math.sin(angle)) * 1.32),
                        round(level + raw[2] * 1.18))
            line((i * 220 + 10, level), (i * 220 + 210, level), [.27, .4, .45, 1])
            pose = rebuilt[frame]
            for start, end in edges:
                color = [1., .56, .28, 1] if end in ("calf_r", "foot_r", "ball_r") else [.68, .82, .91, 1]
                line(project(pose[start]), project(pose[end]), color)
    image.pixels.foreach_set(px)
    image.filepath_raw = str(folder / "blender-kick-pose-trace.png")
    image.file_format = "PNG"
    image.save()


if __name__ == "__main__":
    main()
