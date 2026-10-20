"""Render the authored Arin motions in a fresh process without saving the source.

Invoke through Blender with --input/--out. GIFs contain an entire sampled cycle
and omit the repeated terminal keyframe. GIF delays are quantized to 10 ms,
with cumulative rounding preserving the authored clip's total duration.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

try:
    import bpy
except ModuleNotFoundError:
    bpy = None
if bpy is not None:
    from mathutils import Vector
    import numpy as np
    sys.path.insert(0, str(Path(__file__).parent))
    import BuildArin as common


EXPECTED_SOURCE_SHA = "379e07e07388f8095cfce59093deba2de4534bf028c706440674bcf55bae5528"


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def points(mesh):
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data = evaluated.to_mesh()
    values = np.empty(len(data.vertices) * 3, dtype=np.float64)
    data.vertices.foreach_get("co", values)
    evaluated.to_mesh_clear()
    return values.reshape((-1, 3))


def assemble_preview(out):
    """Assemble PNGs with the existing workspace Python/Pillow runtime."""
    from PIL import Image, ImageDraw
    manifest = out / "render-manifest.json"
    report = json.loads(manifest.read_text(encoding="utf-8"))
    if report.get("source_preserved") is not True:
        raise RuntimeError("Render manifest did not prove source preservation")
    if sha(Path(report["source"])) != report["source_sha256_before"]:
        raise RuntimeError("Source changed since motion rendering")
    for key, clip in report["clips"].items():
        images = []
        for record in clip["renders"]:
            if not record["gif_sample"]:
                continue
            path = Path(record["file"])
            if sha(path) != record["sha256"]:
                raise RuntimeError("Rendered PNG bytes changed before GIF assembly")
            with Image.open(path) as image:
                images.append(image.convert("RGB"))
        duration_ms = clip["ideal_frame_delay_ms"]
        delays = [int(round((i + 1) * duration_ms / 10) * 10 - round(i * duration_ms / 10) * 10)
                  for i in range(len(images))]
        if sum(delays) != round(1000 * clip["authored_duration_seconds"]):
            raise RuntimeError("GIF timing does not preserve authored duration")
        gif = out / (key + ".gif")
        images[0].save(gif, save_all=True, append_images=images[1:], loop=0,
                       duration=delays, optimize=False, disposal=2)
        with Image.open(gif) as result:
            actual_delays = []
            for index in range(result.n_frames):
                result.seek(index)
                actual_delays.append(result.info["duration"])
            if result.n_frames != len(images) or actual_delays != delays:
                raise RuntimeError("Encoded GIF differs from the intended sampled timeline")
        selected = [0, len(images) // 6, len(images) // 3, len(images) // 2,
                    2 * len(images) // 3, len(images) - 1]
        board = Image.new("RGB", (400 * 3, (506 + 24) * 2), (25, 30, 32))
        draw = ImageDraw.Draw(board)
        for cell, index in enumerate(selected):
            x, y = (cell % 3) * 400, (cell // 3) * 530
            board.paste(images[index], (x, y + 24))
            draw.text((x + 8, y + 5), key + " frame " + str(clip["frames_in_gif"][index]), fill="white")
        sheet = out / (key + "-contact-sheet.png")
        board.save(sheet)
        clip.update(encoded_frame_delays_ms=actual_delays, encoded_duration_ms=sum(actual_delays),
                    gif={"file": str(gif), "sha256": sha(gif), "frames": len(images)},
                    contact_sheet={"file": str(sheet), "sha256": sha(sheet)})
    report["source_sha256_after_assembly"] = sha(Path(report["source"]))
    report["source_preserved"] = report["source_sha256_after_assembly"] == report["source_sha256_before"]
    report["status"] = "rendered_requires_visual_review" if report["source_preserved"] else "failed"
    manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["source_preserved"]:
        raise RuntimeError("Source bytes changed during GIF assembly")
    print("ARIN_MOTION_PREVIEW_GIFS_COMPLETE", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--assemble", action="store_true", help="Use workspace Python/Pillow after PNG rendering")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
    source = args.input.resolve()
    out = args.out.resolve()
    if args.assemble:
        assemble_preview(out)
        return
    if bpy is None:
        raise RuntimeError("Render mode requires Blender; --assemble runs with ordinary Python")
    source_before = sha(source)
    if source_before != EXPECTED_SOURCE_SHA:
        raise RuntimeError("Motion preview source differs from the frozen reviewed input")
    if (out / "render-manifest.json").exists():
        raise RuntimeError("Preserve the previous preview manifest; use a new output directory")
    bpy.ops.wm.open_mainfile(filepath=str(source))
    rig = bpy.data.objects.get("Arin_Rig")
    mesh = bpy.data.objects.get("Arin_Adventurer")
    if rig is None or mesh is None:
        raise RuntimeError("Expected the existing Arin rig and mesh")
    scene = bpy.context.scene
    fps = scene.render.fps / scene.render.fps_base
    if abs(fps - 30) > 1e-6:
        raise RuntimeError("Expected the authored 30 fps time base")
    for obj in list(bpy.data.objects):
        if obj.type in {"CAMERA", "LIGHT"} or obj.name == "Studio Ground":
            bpy.data.objects.remove(obj, do_unlink=True)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("Arin Motion Preview World")
    camera = common.studio()
    scene.cycles.samples = 8
    scene.cycles.use_denoising = True
    scene.render.use_persistent_data = True
    scene.render.resolution_x, scene.render.resolution_y = 400, 506
    scene.render.resolution_percentage = 100
    camera.location = (2.6, -4, 1.45)
    target = (0, 0, .85)
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.ortho_scale = 1.95
    out.mkdir(parents=True, exist_ok=True)
    report = {"source": str(source), "source_sha256_before": source_before,
              "blender_version": bpy.app.version_string, "fps": fps,
              "render_engine": scene.render.engine, "cycles_samples": scene.cycles.samples,
              "denoising": True, "resolution": [400, 506], "presentation": "BuildArin original AgX studio",
              "camera": {"position": list(camera.location), "target": list(target),
                         "orthographic_scale": camera.data.ortho_scale},
              "geometry_or_material_edit": False, "source_saved": False, "clips": {}}
    for key, action_name, period, step in [("walk", "Arin_Walk", 36, 2), ("talk", "Arin_Talk", 90, 3)]:
        action = bpy.data.actions.get(action_name)
        if action is None or tuple(action.frame_range) != (1.0, float(period + 1)):
            raise RuntimeError("Missing or unexpected authored action range: " + action_name)
        rig.animation_data_create()
        rig.animation_data.action = action
        if len(action.slots):
            rig.animation_data.action_slot = action.slots[0]
        if rig.animation_data.nla_tracks:
            for track in rig.animation_data.nla_tracks:
                track.mute = True
        frame_dir = out / key
        frame_dir.mkdir(parents=True, exist_ok=True)
        frames = list(range(1, period + 1, step))
        records = []
        for frame in frames + ([period] if period not in frames else []):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            path = frame_dir / ("frame-%03d.png" % frame)
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            sampled = frame in frames
            records.append({"frame": frame, "gif_sample": sampled, "file": str(path), "sha256": sha(path)})
            print("ARIN_MOTION_PREVIEW_FRAME " + key + " " + str(frame), flush=True)
        scene.frame_set(1)
        bpy.context.view_layer.update()
        start_points = points(mesh)
        scene.frame_set(period + 1)
        bpy.context.view_layer.update()
        terminal_points = points(mesh)
        loop_delta = float(np.linalg.norm(terminal_points - start_points, axis=1).max())
        duration_ms = 1000 * step / fps
        report["clips"][key] = {"action": action_name, "authored_frame_range": list(action.frame_range),
            "frames_in_gif": frames, "excluded_duplicate_endpoint_frame": period + 1,
            "final_pre_endpoint_frame_rendered": period, "sample_step_frames": step,
            "authored_duration_seconds": period / fps, "ideal_frame_delay_ms": duration_ms,
            "loop_endpoint_max_vertex_delta_m": loop_delta, "renders": records}
        (out / "render-manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    report["source_sha256_after"] = sha(source)
    report["source_preserved"] = report["source_sha256_after"] == source_before
    report["status"] = "rendered_requires_visual_review" if report["source_preserved"] else "failed"
    (out / "render-manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["source_preserved"]:
        raise RuntimeError("Source bytes changed during the read-only render")
    print("ARIN_MOTION_PREVIEW_COMPLETE", flush=True)


if __name__ == "__main__":
    main()
