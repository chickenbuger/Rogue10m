"""Inert owner-started organic-shell camera regression.

Reuse the pinned, previously reviewed analog input/PCM/capture observer without
editing it. Replace its obsolete hidden-source OBB test with actual complex
Camera14 sweeps. Arming needs the inspected editor PID and exact shell paths.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
_runner = None


def _qa():
    spec = importlib.util.spec_from_file_location("organic_camera_qa", ROOT / "Scripts/Editor/OrganicCaves20261118QA.py")
    value = importlib.util.module_from_spec(spec)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(value)
    finally:
        sys.dont_write_bytecode = old
    return value


def arm_organic_cave_traversal_camera(*, expected_editor_pid, shell_component_paths,
                                    owner_confirms_play_in_viewport=False,
                                    allow_approved_3d_capture=False,
                                    output_directory_name="runtime-camera-organic"):
    """Arm only; the owner starts the normal game viewport and reviews six PNGs."""
    global _runner
    qa = _qa()
    require = qa.require
    require(_runner is None and owner_confirms_play_in_viewport is True and allow_approved_3d_capture is True,
            "One owner-started PlayInViewport observation with approved3D capture required")
    require(isinstance(expected_editor_pid, int) and os.getpid() == expected_editor_pid,
            "Exact newly inspected editor PID required")
    require(set(shell_component_paths) == {"Q", "Z"} and len(set(shell_component_paths.values())) == 2,
            "Two exact Q/Z shell component paths required")
    require(isinstance(output_directory_name, str) and output_directory_name.startswith("runtime-camera-")
            and len(output_directory_name) <= 80
            and all(c.isascii() and (c.isalnum() or c in "-_") for c in output_directory_name),
            "Bounded runtime-camera-* evidence name required")
    output = BASE / output_directory_name
    require(not output.exists(), "Preserve prior evidence; select a fresh output directory")
    frozen = qa.frozen_contract()
    predecessor = qa.pinned_module("organic_reviewed_camera_observer",
        "Scripts/Editor/ObserveCaveTraversalCamera20261118.py", qa.OBSERVER_SHA256)
    import unreal as u
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    require(Path(u.Paths.project_dir()).resolve() == ROOT and str(u.SystemLibrary.get_engine_version()).startswith("5.8.")
            and editor.get_editor_world().get_path_name().split(".")[0] == TARGET and not editor.get_game_world(),
            "Exact idle UE5.8 project required")
    actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
    components = {c.get_path_name(): c for a in actors for c in a.get_components_by_class(u.StaticMeshComponent)}
    require(set(shell_component_paths.values()) <= set(components), "Actual loaded shell components missing")
    for path in shell_component_paths.values():
        component = components[path]
        mesh = component.get_editor_property("static_mesh")
        require(mesh is not None and not isinstance(component, u.InstancedStaticMeshComponent)
                and component.get_editor_property("visible") and not component.get_editor_property("hidden_in_game")
                and component.get_collision_response_to_channel(u.CollisionChannel.ECC_CAMERA) == u.CollisionResponseType.ECR_BLOCK
                and component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN) == u.CollisionResponseType.ECR_BLOCK
                and component.get_collision_enabled() == u.CollisionEnabled.QUERY_AND_PHYSICS
                and u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_collision_complexity(mesh)
                == u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
                "Visible continuous shell with default Pawn/Camera complex-as-simple collision required: " + path)
    target_values, height_guard = predecessor.reviewed_height_contract(frozen["plan"])
    native = json.loads(u.Rogue10mNaturalBoundary20261117Authoring.inspect_candidate(True))
    heights = native.get("composite_heights_uint16", [])
    require(native.get("success") and native.get("read_only") and native.get("dirty_package_names_unchanged")
            and native.get("landscape_reports_up_to_date") and native.get("layer_count") == 2
            and native.get("component_count") == native.get("collision_count") == 45
            and native.get("original_layer_blake3") == predecessor.ORIGINAL_LAYER
            and native.get("composite_blake3") == predecessor.COLLAR_NATIVE_BLAKE3
            and heights == target_values and len(heights) == 179488
            and hashlib.sha256(struct.pack("<"+"H"*len(heights), *heights)).hexdigest() == predecessor.COLLAR_GRID_SHA256,
            "Bit-exact pre-existing terrain/collar/source layer required")
    require(frozen["route_length_cm"] / 620 < 700, "Default run budget architecture review required")
    output.mkdir(parents=True, exist_ok=False)
    (output / "captures").mkdir(exist_ok=False)
    predecessor.OUT = output
    original_write = predecessor.write_new
    geometry_policy = {"schema": "Rogue10m.OrganicCaveActualCameraGeometry.20261118.v1",
        "actual_complex_Camera_sphere_radius_cm": 14.0, "actual_sweep_pivot_to_current_camera": True,
        "surface_contact_tolerance_cm": .5, "obsolete_hidden_source_OBB_test_used": False,
        "minimum_camera_pivot_distance_cm": 140.0, "maximum_sustained_pull_in_cm": 180.0,
        "sustained_pull_in_max_sim_seconds": .5, "movement_capture_observer_sha256": qa.OBSERVER_SHA256,
        "route_sha256": qa.ROUTE_SHA256, "shell_editor_component_paths": shell_component_paths}

    def write(name, value):
        if name in {"armed", "result"}:
            value = dict(value, organic_geometry_policy=geometry_policy)
        original_write(name, value)

    predecessor.write_new = write

    class OrganicObserver(predecessor._Observer):
        def __init__(self, *args):
            super().__init__(*args)
            # Old boxes describe the intentionally hidden, collision-disabled
            # predecessor. They are not geometry in this validation run.
            self.boxes = []

        def telemetry(self, sim, movement):
            sample = super().telemetry(sim, movement)
            sample.pop("planned_core_box_embeddings", None)
            trace = qa.public_camera_sphere(self.u, self.world, sample["pivot_cm"], sample["camera_cm"],
                                           actors_to_ignore=(self.pawn,), trace_complex=True)
            sample["actual_complex_Camera14_pivot_to_camera"] = trace
            distance = sample["camera_to_pivot_cm"]
            require(not trace.get("start_penetrating", False)
                    and (not trace["blocking_hit"] or trace["distance_cm"] >= distance-.5),
                    "Actual Camera14 sphere is obstructed before the observed camera position")
            require(abs(sample["camera_FOV"]-90) <= .01, "Normal90deg camera FOV changed")
            return sample

    helpers = predecessor.module("organic_runtime_public_profile_helpers", "Scripts/Editor/PlaytestLandscapeCrop20261116.py")
    runner = OrganicObserver(u, helpers, frozen["plan"], frozen["routes"])
    runner.height_guard = height_guard
    _runner = runner
    runner.handle = u.register_slate_post_tick_callback(runner.tick)
    write("armed", {"pid": os.getpid(), "route_waypoints": 276, "chains": 12, "captures": 6,
        "input_policy": predecessor.INPUT_POLICY, "height_guard": height_guard,
        "owner_starts_PIE": "PlayInViewport; no separate game window", "normal_walk_cap_cm_s": 620,
        "normal_run_duration_seconds": 1200, "normal_spring_arm_cm": 320, "normal_probe_radius_cm": 14,
        "normal_camera_pitch_degrees": -12, "normal_FOV_degrees": 90,
        "physical_input": False, "teleports": 0, "speed_overrides": 0, "run_timer_overrides": 0,
        "camera_component_changes": 0})
    return {"armed": True, "owner_starts_PIE": True, "output": str(output)}
