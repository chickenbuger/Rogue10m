"""Build an honest CPU visual-hull prototype using Apollo's unchanged adapter.

The source PNGs are read only. Native-size silhouette masks are letterboxed
before inference to avoid Apollo's square-resize aspect distortion. This is
geometry preprocessing, not generation of alternate reference illustrations.
No API server, DB, network provider, procedural texture, or rig is started here.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import sys
import time
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Feature/doc/evidence/arin-apollo-20261005"


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apollo-root", type=Path, default=ROOT / "tmp/apollo-source")
    parser.add_argument("--front", type=Path, default=EVIDENCE / "multiview/front.png")
    parser.add_argument("--side", type=Path, default=EVIDENCE / "multiview/side.png")
    parser.add_argument("--back", type=Path, default=EVIDENCE / "multiview/back.png")
    parser.add_argument("--output-dir", type=Path, default=EVIDENCE / "hull")
    parser.add_argument("--height-m", type=float, default=1.70)
    parser.add_argument("--target-polycount", type=int, default=30000)
    parser.add_argument("--resolution", type=int, default=64)
    parser.add_argument("--side-facing", choices=("left", "right"), default="left")
    return parser.parse_args()


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def prepare_references(paths: dict, procgen, np, Image, output_dir: Path) -> tuple[dict, dict]:
    """Extract masks with Apollo's native-size helper; preserve source aspect."""
    entries = {}
    for view, path in paths.items():
        data = path.read_bytes()
        rgba = Image.open(io.BytesIO(data)).convert("RGBA")
        mask = procgen._foreground_mask_rgba(rgba)
        box = procgen._mask_bbox(mask, pad=0)
        if box is None or float(mask.mean()) < 0.005:
            raise ValueError(f"{view}: no usable foreground silhouette in {path}")
        x0, y0, x1, y1 = box
        entries[view] = {"data": data, "mask": mask[y0:y1, x0:x1], "metadata": {
            "path": str(path.resolve()), "sha256": digest(data),
            "size_px": [rgba.width, rgba.height], "foreground_bbox_px": list(box),
            "foreground_aspect": (x1 - x0) / (y1 - y0),
        }}

    # All silhouettes have the same height. A common square avoids stretching
    # a wide T-pose front or narrow profile in mask_from_image().
    canvas_size = 1024
    available = canvas_size - 96
    widest = max(entry["metadata"]["foreground_aspect"] for entry in entries.values())
    subject_height = min(available, int(available / max(1.0, widest)))
    prepared, metadata = {}, {}
    for view, entry in entries.items():
        width = max(1, int(round(subject_height * entry["metadata"]["foreground_aspect"])))
        silhouette = Image.fromarray((entry["mask"].astype(np.uint8)) * 255)
        silhouette = silhouette.resize((width, subject_height), Image.Resampling.NEAREST)
        canvas = Image.new("L", (canvas_size, canvas_size), color=0)
        origin = ((canvas_size - width) // 2, (canvas_size - subject_height) // 2)
        canvas.paste(silhouette, origin)
        # Opaque black subject on white background is what the actual Apollo
        # adapter accepts. Pixel colors carry no texture at this stage.
        opaque_mask = Image.fromarray(255 - np.asarray(canvas), mode="L").convert("RGB")
        buffer = io.BytesIO()
        opaque_mask.save(buffer, format="PNG")
        prepared[view] = buffer.getvalue()
        mask_path = output_dir / f"{view}-geometry-mask.png"
        mask_path.write_bytes(prepared[view])
        metadata[view] = {**entry["metadata"], "geometry_mask": str(mask_path.resolve()),
            "normalized_mask_canvas_px": [canvas_size, canvas_size],
            "normalized_subject_size_px": [width, subject_height],
            "normalized_subject_origin_px": list(origin),
        }
    return prepared, metadata


def main() -> int:
    args = arguments()
    if not (0.5 <= args.height_m <= 3.0):
        raise ValueError("height-m must be between 0.5 and 3.0")
    if not (5000 <= args.target_polycount <= 200000):
        raise ValueError("target-polycount must be between 5000 and 200000")
    if not (32 <= args.resolution <= 160):
        raise ValueError("resolution must be between 32 and 160")
    paths = {name: getattr(args, name).resolve() for name in ("front", "side", "back")}
    pending_back = None
    if not paths["back"].is_file():
        pending_back = str(paths.pop("back"))
    for path in paths.values():
        if not path.is_file():
            raise FileNotFoundError(f"required reference PNG is missing: {path}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    api_path = args.apollo_root.resolve() / "apps/api"
    if not (api_path / "apollo/providers/threed.py").is_file():
        raise FileNotFoundError(f"actual Apollo source is missing under {api_path}")
    sys.path.insert(0, str(api_path))
    import numpy as np
    from PIL import Image
    import trimesh
    from apollo import procgen
    from apollo.config import Settings
    from apollo.providers.threed import ApolloHullAdapter
    from apollo.qa.mesh_qa import mesh_qa
    from apollo.qa.multiview_qa import multiview_qa

    started = time.time()
    report = {"status": "running", "provider_id": "apollo_hull", "lane": "local_draft",
        "model": "apollo-hull-v1", "execution": "direct unchanged ApolloHullAdapter.generate",
        "mcp_or_api_job": False, "spec": {"height_m": args.height_m,
        "target_polycount": args.target_polycount, "source_identity": "Arin tutorial guide"},
        "used_for_geometry": ["front", "side"], "back_use": "reference QA and downstream texture only",
        "pending_optional_back_reference": pending_back,
        "rigged": False, "animated": False, "texture_baked": False,
        "quality_claim": "silhouette-derived 3D humanoid prototype; not production reconstruction",
        "known_limits": ["Front/side silhouette intersection cannot reconstruct concavities or hidden detail.",
            "Clothing, body, and carried items may be joined; neutral-pose deformation requires visual review.",
            "Apollo keeps the largest mesh component; disconnected small features may be discarded.",
            "AI-generated reference views are design hints, not scanned ground truth."],
        "coordinates": {"up": "+Y", "front_image_right": "+X", "side_image_right": "+Z",
            "reference_profile_facing": args.side_facing,
            "subject_forward": "-Z" if args.side_facing == "left" else "+Z",
            "back_texture_x": "mirror back image U to align with front geometry X",
            "units": "meters"}, "attempts": [],
        "vendor_sha256": {name: digest((api_path / name).read_bytes()) for name in (
            "apollo/providers/threed.py", "apollo/procgen.py")}}
    report_path = args.output_dir / "hull-report.json"
    try:
        images, metadata = prepare_references(paths, procgen, np, Image, args.output_dir)
        report["references"] = metadata
        report["source_multiview_qa"] = multiview_qa({name: paths[name].read_bytes() for name in paths})
        report["normalized_silhouette_multiview_qa"] = multiview_qa(images)
        # The hull adapter needs no DB or queue. Bind genuine Apollo Settings
        # for its base contract without touching the active Apollo API store.
        binding = SimpleNamespace(settings=Settings(data_dir=args.output_dir / "adapter-runtime"))
        adapter = ApolloHullAdapter(binding)
        report["provider_info"] = adapter.info()
        report["provider_health"] = adapter.health().to_dict()
        resolution = args.resolution
        chosen = None
        for attempt_index in range(3):
            result = adapter.generate(images=[images["front"], images["side"]],
                views=["front", "side"], prompt="Arin tutorial guide T-pose silhouette reconstruction",
                seed=0, params={"height_m": args.height_m, "resolution": resolution})
            path = args.output_dir / f"arin-hull-resolution-{resolution}.glb"
            path.write_bytes(result.data)
            mesh = trimesh.load(io.BytesIO(result.data), file_type="glb", force="mesh", process=True)
            qa = mesh_qa(result.data, target_polycount=args.target_polycount, print_target=True)
            record = {"resolution": resolution, "raw_glb": str(path.resolve()),
                "sha256": digest(result.data), "provider_params": result.params,
                "vertices": int(len(mesh.vertices)), "faces": int(len(mesh.faces)), "qa": qa}
            report["attempts"].append(record)
            save_json(report_path, report)
            if len(mesh.faces) <= args.target_polycount:
                chosen = (result, mesh, record)
                break
            # Uniformly regenerate near the face budget; avoid a simplifier
            # silently losing small reference-driven silhouette features.
            lower = int(resolution * (args.target_polycount / len(mesh.faces)) ** 0.5 * 0.95)
            lower = max(32, min(resolution - 4, lower))
            if lower >= resolution:
                break
            resolution = lower
        if chosen is None:
            raise RuntimeError("actual Apollo hull did not fit target face budget within three bounded attempts")
        result, mesh, selected = chosen
        raw_path = args.output_dir / "arin-hull-raw.glb"
        raw_path.write_bytes(result.data)
        raw_bounds = np.asarray(mesh.bounds).copy()
        raw_height = float(mesh.extents[1])
        if raw_height <= 0:
            raise ValueError("generated hull has no vertical extent")
        scale = args.height_m / raw_height
        center_x = float((raw_bounds[0, 0] + raw_bounds[1, 0]) * 0.5)
        center_z = float((raw_bounds[0, 2] + raw_bounds[1, 2]) * 0.5)
        translation = [-center_x, -float(raw_bounds[0, 1]), -center_z]
        mesh.apply_translation(translation)
        mesh.apply_scale(scale)
        normalized = trimesh.exchange.gltf.export_glb(mesh)
        final_path = args.output_dir / "arin-hull-normalized.glb"
        final_path.write_bytes(normalized)
        pieces = mesh.split(only_watertight=False)
        checks = {
            "geometry_present": len(mesh.faces) > 0,
            "closed_surface": bool(mesh.is_watertight),
            "consistent_winding": bool(mesh.is_winding_consistent),
            "positive_enclosed_volume": bool(mesh.is_volume and mesh.volume > 0),
            "single_connected_component": len(pieces) == 1,
            "finite_vertices": bool(np.isfinite(mesh.vertices).all()),
            "finite_normals": bool(np.isfinite(mesh.vertex_normals).all()),
            "face_budget": len(mesh.faces) <= args.target_polycount,
            "height_170cm": abs(float(mesh.extents[1]) - args.height_m) < 1e-5,
            "feet_at_zero": abs(float(mesh.bounds[0, 1])) < 1e-5,
            "nonflat_humanoid_depth": 0.12 <= float(mesh.extents[2]) <= args.height_m * 0.65,
        }
        report["selected_resolution"] = selected["resolution"]
        report["normalization_transform"] = {"order": "translation then uniform scale",
            "translation_m": translation, "scale": scale,
            "raw_bounds_m": raw_bounds.tolist(), "raw_height_m": raw_height}
        report["outputs"] = {"raw_glb": str(raw_path.resolve()),
            "normalized_glb": str(final_path.resolve()), "normalized_sha256": digest(normalized)}
        report["geometry"] = {"vertices": int(len(mesh.vertices)), "faces": int(len(mesh.faces)),
            "bounds_m": np.asarray(mesh.bounds).tolist(), "extents_m": np.asarray(mesh.extents).tolist(),
            "volume_m3": float(mesh.volume), "component_count": len(pieces)}
        report["checks"] = checks
        report["final_mesh_qa"] = mesh_qa(normalized, target_polycount=args.target_polycount, print_target=True)
        # Texture/UV warnings are expected: this stage intentionally exports
        # geometry only, leaving reference-faithful appearance to Blender.
        report["status"] = "passed" if all(checks.values()) else "failed"
        report["elapsed_seconds"] = round(time.time() - started, 3)
        save_json(report_path, report)
        print(json.dumps({"status": report["status"], "report": str(report_path.resolve()),
            "outputs": report["outputs"], "geometry": report["geometry"], "checks": checks}, ensure_ascii=False))
        return 0 if report["status"] == "passed" else 1
    except Exception as exc:
        report.update(status="failed", error={"type": type(exc).__name__, "message": str(exc)},
            elapsed_seconds=round(time.time() - started, 3))
        save_json(report_path, report)
        raise


if __name__ == "__main__":
    raise SystemExit(main())
