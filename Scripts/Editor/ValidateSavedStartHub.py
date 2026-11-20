"""Read-only cold validation of the authored StartHub level.

Run with Scripts/VerifyStartHubDetail.ps1. The optional layout JSON augments
the fixed gameplay contract with exact labels and public destinations.
"""

import json
import math
from pathlib import Path
import re
import sys
import traceback

import unreal

sys.path.insert(0, str(Path(__file__).with_name("StartHubDetail")))
import Author


HUB = "/Game/Rogue10m/Maps/StartHub"
FIELD = "/Game/Rogue10m/Maps/OpenRunMap"
EXTERNAL = "/Game/__ExternalActors__/Rogue10m/Maps/StartHub/"
SPAWN = (0, -2040, 112)
ARIN = (440, -1510, 2)
PORTAL = (0, 2135, 200)
ACTOR_LABELS = {
    "start": "SV_PlayerStart_SouthApproach",
    "guide": "SV_TutorialGuide_Arin",
    "portal": "SV_RunPortal_ToOpenWorld",
    "ground": "SV_WalkableGrass_120m",
}
ROUTES = {
    "east_bypass_native_portal_replay": [(0, -2040), (650, -1100), (650, 950), (0, 1800), (0, 2080)],
    "west_bypass": [(0, -2040), (-650, -1000), (-650, 950), (0, 1800), (0, 2080)],
    "arin_interaction_approach": [(0, -2040), (155, -1510)],
    "equipment_approach": [(-650, -820), (-925, -820)],
    "forge_approach": [(650, -825), (1090, -825)],
    "expedition_approach": [(-650, 850), (-1020, 850)],
    "archive_approach": [(650, 900), (1070, 900)],
    # Pass between the expedition stall and the original west-side lantern.
    "west_training_approach": [(-650, 950), (-650, 1080), (-1280, 1145), (-1280, 1360)],
}
DRESSING_TAG = "Rogue10m.StartHubDetail"
DRESSING_GROUPS = ("Arrival", "Plaza", "Departure", "Equipment", "Forge", "Expedition", "Archive", "Training", "Perimeter")
RADIUS_CM = 42.0
HALF_HEIGHT_CM = 96.0
CAPSULE_Z_CM = 132.0


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def files_dirty():
    utils = unreal.EditorLoadingAndSavingUtils
    return sorted(package.get_name() for package in [*utils.get_dirty_map_packages(), *utils.get_dirty_content_packages()])


def new_world():
    world = unreal.EditorLoadingAndSavingUtils.load_map(HUB)
    if not world or world.get_path_name() != HUB + ".StartHub":
        raise RuntimeError(f"Cold load did not open {HUB}: {world}")
    partition = unreal.WorldPartitionBlueprintLibrary
    descriptors = partition.get_actor_descs()
    partition.load_actors([descriptor.get_editor_property("guid") for descriptor in descriptors])
    unreal.SystemLibrary.execute_console_command(world, "Editor.AsyncStaticMeshCompilationFinishAll")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    actor_paths = {actor.get_path_name() for actor in actors}
    missing = sorted(str(descriptor.get_editor_property("actor_path")) for descriptor in descriptors
                     if str(descriptor.get_editor_property("actor_path")) not in actor_paths)
    return world, actors, len(descriptors), missing


def blocking_hit(query):
    results = query if isinstance(query, tuple) else (query,)
    hits = [result for result in results if isinstance(result, unreal.HitResult)]
    if not hits:
        return None
    fields = hits[0].to_tuple()
    if not fields[0]:
        return None
    actor = fields[9]
    return {"actor": actor.get_actor_label() if actor else None, "impact_cm": xyz(fields[5]),
            "normal": xyz(fields[7]), "initial_overlap": bool(fields[1])}


def sweep(world, start, end, ignore=()):
    return blocking_hit(unreal.SystemLibrary.capsule_trace_single_by_profile(
        world, unreal.Vector(*start), unreal.Vector(*end), RADIUS_CM, HALF_HEIGHT_CM,
        "Pawn", False, list(ignore), unreal.DrawDebugTrace.NONE, True))


def floor_hit(world, x, y, ignore=()):
    return blocking_hit(unreal.SystemLibrary.line_trace_single_by_profile(
        world, unreal.Vector(x, y, 84.0), unreal.Vector(x, y, -100.0),
        "Pawn", False, list(ignore), unreal.DrawDebugTrace.NONE, True))


def segment_samples(a, b):
    intervals = max(1, math.ceil(math.dist(a, b) / 125.0))
    for index in range(intervals + 1):
        fraction = index / intervals
        yield [round(initial + (final - initial) * fraction, 4) for initial, final in zip(a, b)]


def check_scene(report, layout):
    checks = report["checks"]

    def record(name, passed, **observed):
        checks.append({"name": name, "passed": bool(passed), **observed})

    record("no_dirty_packages_before_cold_reload", not (dirty := files_dirty()), packages=dirty)
    world, actors, descriptors, missing = new_world()
    groups = {}
    for actor in actors:
        groups.setdefault(actor.get_actor_label(), []).append(actor)
    by_label = {label: matches[0] for label, matches in groups.items() if len(matches) == 1}
    record("all_world_partition_descriptors_loaded", not missing, descriptor_count=descriptors, missing=missing)
    record("exactly_one_player_start_and_guide_and_departure_portal",
           all(sum(isinstance(actor, kind) for actor in actors) == 1 for kind in (
               unreal.PlayerStart, unreal.Rogue10mTutorialNPC, unreal.Rogue10mRunPortal)))

    for key, expected in (("start", SPAWN), ("guide", ARIN), ("portal", PORTAL)):
        actor = by_label.get(ACTOR_LABELS[key])
        actual = xyz(actor.get_actor_location()) if actor else None
        record(f"{key}_original_anchor", actual is not None and math.dist(actual, expected) <= 1.0,
               expected_cm=expected, actual_cm=actual)

    portal = by_label.get(ACTOR_LABELS["portal"])
    if portal:
        target = str(portal.get_editor_property("target_level_name"))
        starts = bool(portal.get_editor_property("start_run_timer_on_travel"))
        options = str(portal.get_editor_property("additional_travel_options"))
        box = portal.get_editor_property("portal_collision")
        gate_axis = xyz(portal.get_actor_right_vector())
        trigger_extent = xyz(box.get_scaled_box_extent())
        overlap_enabled = (box.is_query_collision_enabled() and box.get_editor_property("generate_overlap_events")
                           and box.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
                           == unreal.CollisionResponseType.ECR_OVERLAP)
        record("existing_portal_contract_targets_field_and_starts_run",
               target == FIELD and starts and not options and overlap_enabled
               and math.dist(gate_axis, (0, 1, 0)) < 0.01 and math.dist(trigger_extent, (120, 80, 180)) < 0.1,
               destination=target, starts_run=starts, extra_options=options, pawn_overlap=bool(overlap_enabled),
               gate_right_vector=gate_axis, trigger_half_extent_cm=trigger_extent)
    mode_class = world.get_world_settings().get_editor_property("default_game_mode")
    mode = unreal.get_default_object(mode_class) if mode_class else None
    record("editor_default_hub_game_mode_waits_for_departure",
           isinstance(mode, unreal.Rogue10mGameMode) and not mode.get_editor_property("start_run_on_start_play"))

    ground = by_label.get(ACTOR_LABELS["ground"])
    if ground:
        center, extent = ground.get_actor_bounds(True)
        record("retains_60m_ground_and_height", math.dist(xyz(extent), (3000, 3000, 60)) < 1.0
               and abs(center.z + extent.z) < 0.1, center_cm=xyz(center), half_extent_cm=xyz(extent))
        ignore_for_base = [actor for actor in actors if actor != ground]
        bad_base = []
        samples = list(range(-2700, 2701, 540))
        for x in samples:
            for y in samples:
                hit = floor_hit(world, x, y, ignore_for_base)
                if not hit or hit["actor"] != ACTOR_LABELS["ground"] or abs(hit["impact_cm"][2]) > 0.1:
                    bad_base.append({"xy_cm": [x, y], "hit": hit})
        for x, y in ((3050, 0), (-3050, 0), (0, 3050), (0, -3050)):
            if hit := floor_hit(world, x, y, ignore_for_base):
                bad_base.append({"xy_cm": [x, y], "hit": hit, "expect_no_ground": True})
        record("base_collision_covers_60m_square_only", not bad_base,
               floor_samples=len(samples) ** 2, outside_samples=4, failures=bad_base)
    else:
        record("retains_60m_ground_and_height", False, actor=None)

    houses = sorted(label for label in by_label if re.fullmatch(r"SV_House_\d\d_Foundation", label))
    record("ten_existing_house_foundations_retained", len(houses) == 10, labels=houses)
    hit = sweep(world, (0, -1940, 300), (0, -1940, 60))
    record("pawn_capsule_query_detects_support_positive_control",
           hit is not None and not hit["initial_overlap"] and hit["normal"][2] > 0.7, hit=hit)

    for name, (x, y) in {"North": (0, 1), "South": (0, -1), "West": (-1, 0), "East": (1, 0)}.items():
        failures = []
        wall_label = "SV_BoundaryBlocking_" + name
        wall = by_label.get(wall_label)
        ignore_for_wall = [actor for actor in actors if actor != wall]
        offsets = (-2750, -2200, -1100, 0, 1100, 2200, 2750)
        for offset in offsets:
            a = (x * 2800 + y * offset, y * 2800 + x * offset, 115.0)
            b = (x * 3040 + y * offset, y * 3040 + x * offset, 115.0)
            hit = sweep(world, a, b, ignore_for_wall)
            if not hit or hit["actor"] != wall_label:
                failures.append({"offset_cm": offset, "hit": hit})
        record(f"pawn_boundary_has_no_exit_{name}", not failures, samples=len(offsets), failures=failures)

    tag = (layout or {}).get("tag", DRESSING_TAG)
    tags_by_actor = {actor.get_path_name(): {str(value) for value in actor.tags} for actor in actors}
    tagged = [actor for actor in actors if tag in tags_by_actor[actor.get_path_name()]]
    bad_packages = sorted(actor.get_package().get_name() for actor in tagged
                          if not actor.get_package().get_name().startswith(EXTERNAL))
    record("authored_dressing_all_stored_as_start_hub_external_actors",
           bool(tagged) and not bad_packages, tag=tag, actor_count=len(tagged), other_packages=bad_packages)
    plan = Author.DetailPlan()
    for section in (Author.Services, Author.Landmarks, Author.Perimeter, Author.Finishes):
        section.author(plan)
    expected_labels = {Author.label(row) for row in plan.meshes + plan.lights}
    tagged_labels = {actor.get_actor_label() for actor in tagged}
    record("exact_dressing_recipe_is_saved_without_duplicates",
           tagged_labels == expected_labels and len(tagged) == len(expected_labels),
           count=len(tagged), expected_count=len(expected_labels),
           missing=sorted(expected_labels - tagged_labels), extra=sorted(tagged_labels - expected_labels))
    for group in DRESSING_GROUPS:
        expected = (layout or {}).get("expected_labels", {}).get(group, [])
        group_actors = [actor for actor in tagged if tag + "." + group in tags_by_actor[actor.get_path_name()]]
        group_labels = {actor.get_actor_label() for actor in group_actors}
        missing = sorted(set(expected) - group_labels)
        bad_labels = sorted(label for label in group_labels
                            if label not in by_label or not label.startswith("StartHubDetail_"))
        record(f"visual_group_saved_{group}", bool(group_actors) and not missing and not bad_labels,
               actor_count=len(group_actors), expected_labels=expected, missing=missing, bad_labels=bad_labels)

    finish = Author.DetailPlan()
    Author.Finishes.author(finish)
    finish_collision_failures = []
    for row in finish.meshes:
        actor = by_label.get(Author.label(row))
        component = actor.get_component_by_class(unreal.StaticMeshComponent) if actor else None
        expected = "BlockAll" if row["blocking"] else "NoCollision"
        if not component or str(component.get_collision_profile_name()) != expected:
            finish_collision_failures.append(Author.label(row))
    record("finish_collision_matches_visible_walkable_decks", not finish_collision_failures,
           count=len(finish.meshes), walkable_decks=sum(row["blocking"] for row in finish.meshes),
           failures=finish_collision_failures)

    pad_failures = []
    for name in sorted(Author.VISUAL_PADS):
        actor = by_label.get(name)
        component = actor.get_component_by_class(unreal.StaticMeshComponent) if actor else None
        if (not component or component.is_visible() or not component.get_editor_property("hidden_in_game")
                or component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION):
            pad_failures.append(name)
    record("old_pads_are_hidden_and_do_not_have_invisible_collision", not pad_failures,
           count=len(Author.VISUAL_PADS), failures=pad_failures)

    deck_hits = []
    deck_failures = []
    for row in (row for row in finish.meshes if row["blocking"]):
        name = Author.label(row)
        actor = by_label.get(name)
        if not actor:
            deck_failures.append(name)
            continue
        x, y, z = row["center"]
        dx, dy, dz = (value / 2 for value in row["dimensions"])
        # Sample the surface near each edge in isolation. The same mesh must
        # provide the collision a Pawn sees, including outside the old pad.
        ignore_others = [other for other in actors if other != actor]
        for offset_x, offset_y in ((0, 0), (-dx + 25, -dy + 25), (dx - 25, -dy + 25),
                                   (-dx + 25, dy - 25), (dx - 25, dy - 25)):
            start = (x + offset_x, y + offset_y, 240)
            hit = sweep(world, start, (*start[:2], 50), ignore_others)
            if not hit or hit["initial_overlap"] or abs(hit["impact_cm"][2] - (z + dz)) > 8:
                deck_failures.append({"actor": name, "point_cm": list(start[:2]), "hit": hit})
            deck_hits.append(hit)
    record("pawn_can_stand_on_visible_deck_center_and_four_corners", not deck_failures,
           samples=len(deck_hits), failures=deck_failures)

    bad_lights = []
    for row in plan.lights:
        actor = by_label.get(Author.label(row))
        component = actor.get_component_by_class(unreal.PointLightComponent) if actor else None
        if not component:
            bad_lights.append(Author.label(row))
            continue
        color = component.get_editor_property("light_color")
        if ((color.r, color.g, color.b) != row["color"]
                or abs(component.get_editor_property("intensity") - row["intensity"] * 0.24) > 0.01):
            bad_lights.append(Author.label(row))
    record("all_local_lights_save_correct_warm_color_and_intensity", not bad_lights,
           count=len(plan.lights), failures=bad_lights)

    routes = {**ROUTES, **(layout or {}).get("routes", {})}
    for name, points in routes.items():
        failures, checked_floors, checked_sweeps = [], 0, 0
        for a, b in zip(points, points[1:]):
            for direction_index, (start, end) in enumerate(((a, b), (b, a))):
                checked_sweeps += 1
                if hit := sweep(world, (*start, CAPSULE_Z_CM), (*end, CAPSULE_Z_CM)):
                    failures.append({"direction": direction_index, "from_cm": start, "to_cm": end, "hit": hit})
            for x, y in segment_samples(a, b):
                checked_floors += 1
                hit = floor_hit(world, x, y)
                if not hit or not -1.0 <= hit["impact_cm"][2] <= 36.0 or hit["normal"][2] < 0.7:
                    failures.append({"floor_xy_cm": [x, y], "hit": hit})
        record(f"walk_clearance_{name}", len(points) >= 2 and not failures, route_xy_cm=points,
               capsule_sweeps=checked_sweeps, ground_samples=checked_floors, failures=failures)

    record("no_dirty_packages_after_read_only_checks", not (dirty := files_dirty()), packages=dirty)


def main():
    command_line = unreal.SystemLibrary.get_command_line()
    output = unreal.SystemLibrary.parse_param_value(command_line, "RogueStartHubQaOutput=")
    if not output or not Path(output).is_dir():
        raise RuntimeError("Pass -RogueStartHubQaOutput=<existing run directory>.")
    destination = Path(output) / "cold-validation.json"
    if destination.exists():
        raise FileExistsError(destination)
    config_path = unreal.SystemLibrary.parse_param_value(command_line, "RogueStartHubQaLayout=")
    report = {"map": HUB, "method": "independent read-only editor commandlet cold load and Pawn-profile collision probes",
              "capsule_radius_cm": RADIUS_CM, "capsule_half_height_cm": HALF_HEIGHT_CM,
              "checks": [], "feature_layout_supplied": bool(config_path)}
    try:
        layout = json.loads(Path(config_path).read_text(encoding="utf-8-sig")) if config_path else None
        check_scene(report, layout)
    except Exception:
        report["error"] = traceback.format_exc()
    report["passed"] = bool(report["checks"]) and "error" not in report and all(row["passed"] for row in report["checks"])
    with destination.open("x", encoding="utf-8", newline="\n") as file:
        json.dump(report, file, ensure_ascii=False, indent=2)
        file.write("\n")
    if not report["passed"]:
        failed = [row["name"] for row in report["checks"] if not row["passed"]]
        raise RuntimeError(f"StartHub cold QA failed: {failed}; {report.get('error', '')}; report {destination}")
    unreal.log(f"START_HUB_DETAIL_COLD_QA_PASSED checks={len(report['checks'])} report={destination}")


if __name__ == "__main__":
    main()
