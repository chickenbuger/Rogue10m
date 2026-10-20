"""Capture actual v3 Unreal viewports with measured editor frames and cleanup."""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Feature/doc/evidence/arin-reference-refinement-20261009"
VIEWS = {
    "village-front": {"location": {"x": 440, "y": -1950, "z": 125}, "rotation": {"pitch": -8, "yaw": 90, "roll": 0}},
    "village-face": {"location": {"x": 440, "y": -1730, "z": 160}, "rotation": {"pitch": -3, "yaw": 90, "roll": 0}},
    "village-oblique": {"location": {"x": 620, "y": -1850, "z": 135}, "rotation": {"pitch": -10, "yaw": 126.87, "roll": 0}},
}


def remote(expression):
    editor_scripts = (ROOT / "Scripts/Editor").as_posix()
    command = ("import sys,json,importlib; sys.path.insert(0," + repr(editor_scripts) + "); "
               "import ReviewReferenceArinLOD as review; review=importlib.reload(review); "
               "print(json.dumps(" + expression + "))")
    process = subprocess.run([sys.executable, str(ROOT / "Scripts/RunEditorRemote.py"),
                              "--command", command, "--timeout", "12"],
                             capture_output=True, text=True, encoding="utf-8")
    try:
        body = json.loads(process.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError("Editor remote response was not JSON: " + process.stdout[-1000:] + process.stderr[-1000:]) from exc
    if process.returncode or not body.get("success"):
        raise RuntimeError(str(body) + process.stderr[-1000:])
    return json.loads(body["output"][-1]["output"])


def official(out, stem, name, arguments):
    argument_file = out / (stem + "-args.json")
    response_file = out / (stem + "-response.json")
    argument_file.write_text(json.dumps({"toolset_name": "EditorToolset.EditorAppToolset",
        "tool_name": name, "arguments": arguments}), encoding="utf-8")
    process = subprocess.run([sys.executable, str(ROOT / "Scripts/InvokeUnrealMCP.py"),
        "--tool", "call_tool", "--arguments-file", str(argument_file),
        "--output", str(response_file), "--timeout", "30"],
        capture_output=True, text=True, encoding="utf-8")
    if process.returncode:
        raise RuntimeError(process.stdout[-1000:] + process.stderr[-1000:])
    response = json.loads(response_file.read_text(encoding="utf-8"))
    if response.get("error") or response["result"].get("isError"):
        raise RuntimeError("Official tool rejected: " + name)
    body = json.loads(next(item["text"] for item in response["result"]["content"] if item["type"] == "text"))
    return body.get("returnValue")


def settled_frames(token, phase, previous_ticks, lod):
    """Require real editor ticks after the camera change, not elapsed time alone."""
    started = time.monotonic()
    time.sleep(2)
    while True:
        state = remote("review.frame_probe_status(" + repr(token) + "," + repr(phase) + ")")
        component = state["component"]
        frames = state["ticks"] - previous_ticks
        correct_lod = lod is None or (component["forced_value"] == lod + 1 and component["predicted_lod_index"] == lod)
        if frames >= 2 and correct_lod:
            state["ticks_since_camera_change"] = frames
            state["minimum_ticks_required"] = 2
            state["wait_seconds"] = time.monotonic() - started
            return state
        if time.monotonic() - started >= 15:
            raise RuntimeError("Viewport did not settle in actual editor frames/LOD: " + str(state))
        time.sleep(.25)


def same_camera(expected, actual):
    for group, keys in (("location", ("x", "y", "z")),
                        ("rotation", ("pitch", "yaw", "roll")), ("scale", ("x", "y", "z"))):
        for key in keys:
            difference = float(actual[group][key]) - float(expected[group][key])
            if group == "rotation":
                difference = (difference + 180) % 360 - 180
            if not math.isfinite(difference) or abs(difference) > .001:
                return False
    return True


def capture_views(out, phase, lod, settled, probe_token):
    saved_camera = official(out, "saved-camera", "GetCameraTransform", {})
    if saved_camera is None:
        raise RuntimeError("Official viewport camera snapshot is unavailable")
    manifest = {"source": "actual Epic official Unreal MCP CaptureViewport, editor world",
        "phase": phase, "views": VIEWS, "captures": {}, "capture_metadata": {},
        "settled_camera": settled, "minimum_seconds_before_capture": 2 if settled else 0,
        "real_editor_ticks_required": 2 if settled else 0, "asset_mutation_requested": False}
    try:
        for name, view in VIEWS.items():
            frame_state = None
            if settled:
                official(out, name + "-camera", "SetCameraTransform",
                         {"transform": {**view, "scale": {"x": 1, "y": 1, "z": 1}}})
                # Snapshot after camera application so only following ticks count.
                before = remote("review.frame_probe_status(" + repr(probe_token) + "," + repr(phase) + ")")
                frame_state = settled_frames(probe_token, phase, before["ticks"], lod)
            else:
                state = remote("review.inspect(" + repr(phase) + ")")
                if lod is not None and (state["forced_value"] != lod + 1 or state["predicted_lod_index"] != lod):
                    raise RuntimeError("Unsettled forced LOD did not reach requested index; use --settled: " + str(state))
            body = official(out, name, "CaptureViewport", {
                "captureTransform": None if settled else {**view, "scale": {"x": 1, "y": 1, "z": 1}},
                "annotations": None, "bShowUI": False})
            image = body["image"]
            if image["mimeType"] != "image/png":
                raise RuntimeError("Official viewport returned a non-PNG image")
            path = out / (name + ".png")
            path.write_bytes(base64.b64decode(image["data"], validate=True))
            body["image"] = {"mimeType": "image/png", "saved": str(path),
                             "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                             "bytes": path.stat().st_size}
            # Replace the native response file so encoded pixels are not duplicated.
            (out / (name + "-response.json")).write_text(json.dumps(body, indent=2), encoding="utf-8")
            manifest["captures"][name] = str(path)
            manifest["capture_metadata"][name] = {"image": body["image"], "editor_frame_state": frame_state}
            print("Saved actual Unreal capture: " + str(path), flush=True)
        state = remote("review.inspect(" + repr(phase) + ")")
        manifest["lod_component"] = state
        if lod is not None and (state["forced_value"] != lod + 1 or state["predicted_lod_index"] != lod):
            raise RuntimeError("Forced LOD did not actually reach the reviewed index: " + str(state))
        manifest["status"] = "captured_requires_visual_review"
        (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    except Exception as exc:
        manifest.update({"status": "failed_preserve_evidence", "error": str(exc)})
        (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        raise
    finally:
        active_error = sys.exc_info()[1]
        try:
            official(out, "restored-camera", "SetCameraTransform", {"transform": saved_camera})
            restored = official(out, "verified-restored-camera", "GetCameraTransform", {})
            verified = same_camera(saved_camera, restored)
            (out / "camera-restoration.json").write_text(json.dumps({"previous": saved_camera,
                "restored": restored, "matched_within_tolerance": verified}, indent=2), encoding="utf-8")
            if not verified:
                raise RuntimeError("Viewport camera did not return to its snapshot")
        except Exception as exc:
            (out / "camera-restoration-error.json").write_text(json.dumps({"error": str(exc)}, indent=2), encoding="utf-8")
            if active_error is None:
                raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", choices=["baseline", "refined"], required=True)
    parser.add_argument("--lod", type=int, choices=[0, 1, 2])
    parser.add_argument("--settled", action="store_true", help="Hold each camera for actual editor ticks before capturing")
    parser.add_argument("--run-id", help="Optional new evidence subdirectory; preserve earlier captures")
    args = parser.parse_args()
    if args.lod is not None and args.phase != "refined":
        raise ValueError("Forced LOD review requires refined phase")
    if args.run_id is not None and not re.fullmatch(r"[A-Za-z0-9_-]+", args.run_id):
        raise ValueError("run-id accepts letters, digits, underscores, and hyphens only")
    out = EVIDENCE / "unreal-captures" / args.phase
    if args.lod is not None:
        out = out / ("lod-" + str(args.lod))
    if args.run_id is not None:
        out = out / args.run_id
    if out.exists() and any(out.iterdir()):
        raise RuntimeError("Preserve existing capture evidence; choose a new --run-id: " + str(out))
    out.mkdir(parents=True, exist_ok=True)
    previous = None
    probe = None
    try:
        snapshot = remote("review.inspect(" + repr(args.phase) + ")")
        previous = {
            "previous_forced_value": snapshot["forced_value"],
            "previous_update_animation_in_editor": snapshot["update_animation_in_editor"],
            "previous_background_throttle": snapshot["background_throttle"],
        }
        (out / "before-component.json").write_text(json.dumps(snapshot, indent=2), encoding="utf-8")
        # Baseline never writes forced LOD. Animation/throttle changes are temporary.
        forced = args.lod + 1 if args.lod is not None else None
        remote("review.request(" + repr(forced) + ",True,False," + repr(args.phase) + ")")
        if args.settled:
            # Keep the owned token even if the remote response is interrupted.
            probe = {"token": uuid.uuid4().hex}
            remote("review.start_frame_probe(" + repr(args.phase) + "," + repr(probe["token"]) + ")")
        capture_views(out, args.phase, args.lod, args.settled, probe["token"] if probe else None)
    finally:
        active_error = sys.exc_info()[1]
        cleanup = {"phase": args.phase, "errors": [], "asset_saved": False}
        if probe is not None:
            try:
                cleanup["frame_probe"] = remote("review.stop_frame_probe(" + repr(probe["token"]) + ")")
            except Exception as exc:
                cleanup["errors"].append("Frame probe cleanup: " + str(exc))
        if previous is not None:
            try:
                restore_forced = previous["previous_forced_value"] if args.lod is not None else None
                restore = remote("review.request(" + repr(restore_forced) + ","
                    + repr(previous["previous_update_animation_in_editor"]) + ","
                    + repr(previous["previous_background_throttle"]) + "," + repr(args.phase) + ")")
                if (restore["forced_value"] != previous["previous_forced_value"]
                    or restore["update_animation_in_editor"] != previous["previous_update_animation_in_editor"]
                    or restore["background_throttle"] != previous["previous_background_throttle"]):
                    raise RuntimeError("Component/background state did not return to its exact snapshot")
                cleanup["component"] = restore
            except Exception as exc:
                cleanup["errors"].append("Component/background cleanup: " + str(exc))
        (out / "restored-component.json").write_text(json.dumps(cleanup, indent=2), encoding="utf-8")
        if cleanup["errors"] and active_error is None:
            raise RuntimeError("Capture cleanup failed: " + str(cleanup["errors"]))


if __name__ == "__main__":
    main()
