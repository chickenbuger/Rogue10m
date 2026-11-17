"""Plan or create one NEW, editable introductory village-quest Data Asset.

Importing or running this file only describes the plan; it does not import Unreal.
After integration/build and an explicit shared-editor handoff, the owner may call:

    create_intro_definition(
        destination="/Game/Rogue10m/Data/Quests/DA_IntroSupply_MyDraft",
        supported_world="/Game/Rogue10m/Maps/MyChosenMap",
        execute=True, owner_handoff="owner confirmed idle editor", build_verified=True)

The selected World is loaded as a reference, never opened or edited. Only the
fresh definition package is saved. Failures never delete assets or save maps.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
DEFINITION_CLASS = "/Script/Rogue10m.Rogue10mVillageQuestDataAsset"
_PACKAGE = re.compile(r"/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\Z")
_VENDOR_SEGMENTS = {"vendor", "vendors", "thirdparty", "third_party", "marketplace",
                    "megascans", "quixel", "fab", "startercontent", "plugins"}
_METHODS = ("reset_to_intro_supply_example", "validate_configuration", "get_enabled_quest_count")


class AuthoringError(RuntimeError):
    """A failed guard or authoring operation; no automatic cleanup is attempted."""


def _asset_paths(value, *, destination=False):
    if not isinstance(value, str) or not value or value != value.strip():
        raise AuthoringError("An explicit canonical asset path is required.")
    parts = value.split(".")
    package = parts[0]
    name = package.rsplit("/", 1)[-1]
    if len(parts) > 2 or (len(parts) == 2 and parts[1] != name) or not _PACKAGE.fullmatch(package):
        raise AuthoringError("Use a /Game package path or its matching Package.Asset object path.")
    if destination:
        if not package.startswith("/Game/Rogue10m/") or not name.startswith("DA_"):
            raise AuthoringError("Choose a NEW project-owned /Game/Rogue10m/.../DA_* destination.")
        if any(part.casefold() in _VENDOR_SEGMENTS for part in package.split("/")):
            raise AuthoringError("Vendor destinations are not project-owned authoring targets.")
    return package, package + "." + name


def plan(*, destination=None, supported_world=None):
    """Pure plan: no Unreal import, directory creation, asset load or save."""
    target = _asset_paths(destination, destination=True) if destination is not None else (None, None)
    world = _asset_paths(supported_world) if supported_world is not None else (None, None)
    if target[0] and target[0] == world[0]:
        raise AuthoringError("Definition destination must differ from the selected World package.")
    return {"status": "plan_only_not_executed", "destination": target[0],
            "supported_world": world[0], "definition_class": DEFINITION_CLASS,
            "requires": ["explicit execute=True", "shared owner_handoff", "build_verified=True",
                         "fresh project-owned destination", "owner-selected supported World",
                         "compiled reset/validation/count API"],
            "expected": {"stable_slots": 5, "enabled_quests": 1, "npc": "서린",
                         "local_pickups": 2, "reward": "gold_only", "optional_story": False},
            "saves": "only the newly created definition package",
            "map_modified": False, "runtime_verified": False}


def _read(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as exc:
        raise AuthoringError("Compiled editor property unavailable: " + name) from exc


def _coordinates(value, names):
    try:
        result = tuple(float(getattr(value, name)) for name in names)
    except (AttributeError, TypeError, ValueError) as exc:
        raise AuthoringError("Expected a finite vector or rotator.") from exc
    if not all(math.isfinite(component) for component in result):
        raise AuthoringError("Expected finite layout and pickup coordinates.")
    return result


def _verify_intro(asset, world_object_path):
    # The native bool is authoritative; localized result text is diagnostic only.
    if _read(asset, "last_validation_passed") is not True:
        raise AuthoringError("Native configuration validation failed; inspect LastValidationResult.")
    count = asset.get_enabled_quest_count()
    rows = list(_read(asset, "quests"))
    if type(count) is not int or count != 1 or len(rows) != 5:
        raise AuthoringError("Intro definition requires five stable slots and exactly one enabled quest.")
    enabled = [_read(row, "enabled") for row in rows]
    if any(type(value) is not bool for value in enabled) or enabled != [True, False, False, False, False]:
        raise AuthoringError("Only the first intro quest row may be enabled.")
    for row in rows:
        if (_read(row, "supply_clue_objective_index") != -1
                or _read(row, "offers_clue_analysis") is not False
                or _read(row, "accepts_clue_report") is not False):
            raise AuthoringError("All optional-story roles must be disabled, including draft slots.")
        if _read(row, "combat_objective_index") != -1 or _read(row, "combat_monster_data") is not None:
            raise AuthoringError("The intro template must not contain a monster objective or monster asset.")
        if _read(row, "reward_item") is not None:
            raise AuthoringError("The intro template must not contain item or book rewards.")
    first = rows[0]
    gold = _read(first, "reward_gold")
    if type(gold) is not int or gold <= 0:
        raise AuthoringError("The enabled intro quest requires a positive gold-only reward.")
    if str(_read(first, "quest_id")) != "Q_Intro_supply" or not str(_read(first, "npc_name")).strip():
        raise AuthoringError("The compiled intro template requires its stable quest ID and a named NPC.")
    labels = list(_read(first, "objective_labels"))
    locations = list(_read(first, "objective_locations"))
    if len(labels) != 2 or any(not str(label).strip() for label in labels) or len(locations) != 2:
        raise AuthoringError("The intro template requires two named collection pickups.")
    points = [_coordinates(point, ("x", "y", "z")) for point in locations]
    npc = _coordinates(_read(first, "npc_location"), ("x", "y", "z"))
    if points[0] == points[1] or any(math.dist(point, (0, 0, 0)) > 5000 for point in points + [npc]):
        raise AuthoringError("Use two distinct local example pickups within 50m of the authoring anchor.")
    anchor = _coordinates(_read(asset, "layout_origin_cm"), ("x", "y", "z"))
    rotation = _coordinates(_read(asset, "layout_rotation"), ("pitch", "yaw", "roll"))
    if any(abs(component) > 1e-6 for component in anchor + rotation):
        raise AuthoringError("The intro template must retain a zero layout anchor and rotation.")
    selected_world = _read(asset, "supported_world")
    if selected_world is None or selected_world.get_path_name() != world_object_path:
        raise AuthoringError("The definition did not retain the explicitly selected supported World.")


def _assert_fresh(unreal, package, object_path, project):
    if (unreal.EditorAssetLibrary.does_asset_exist(package)
            or unreal.find_object(None, object_path, follow_redirectors=False) is not None
            or unreal.find_package(package) is not None):
        raise AuthoringError("Destination already exists or is loaded; choose a NEW package: " + package)
    content = (project.parent / "Content").resolve()
    owned_root = (content / "Rogue10m").resolve()
    physical = (content / package.removeprefix("/Game/")).resolve()
    try:
        content.relative_to(project.parent.resolve())
        if owned_root != content / "Rogue10m":
            raise ValueError("Project-owned Content root is redirected.")
        physical.relative_to(owned_root)
    except ValueError as exc:
        raise AuthoringError("Destination resolves outside project-owned Content/Rogue10m.") from exc
    if any(physical.with_suffix(suffix).exists() for suffix in (".uasset", ".umap", ".uexp", ".ubulk")):
        raise AuthoringError("Destination already has package files; choose a NEW package: " + package)


def create_intro_definition(*, destination=None, supported_world=None, execute=False,
                            owner_handoff="", build_verified=False):
    """Return a plan by default. Explicit execution creates/saves only a new definition."""
    report = plan(destination=destination, supported_world=supported_world)
    if type(execute) is not bool:
        raise AuthoringError("execute must be an explicit boolean.")
    if not execute:
        return report
    if not isinstance(owner_handoff, str) or not owner_handoff.strip() or build_verified is not True:
        raise AuthoringError("The shared editor owner must confirm owner_handoff and build_verified=True.")
    if destination is None or supported_world is None:
        raise AuthoringError("Execution requires an explicit NEW destination and owner-selected supported_world.")
    package, object_path = _asset_paths(destination, destination=True)
    _, world_object_path = _asset_paths(supported_world)

    import unreal

    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != (ROOT / "Rogue10m.uproject").resolve() or not project.is_file():
        raise AuthoringError("Wrong project; integrate this reviewed script into the target Rogue10m project first.")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels or levels.is_in_play_in_editor():
        raise AuthoringError("An idle editor is required; finish PIE before creating the definition.")
    definition_class = unreal.load_class(None, DEFINITION_CLASS)
    if definition_class is None:
        raise AuthoringError("Compiled quest definition class unavailable; build and restart first.")
    default = unreal.get_default_object(definition_class)
    if default is None or any(not callable(getattr(default, method, None)) for method in _METHODS):
        raise AuthoringError("Compiled Python reset/validation/count methods unavailable; build and restart first.")
    if type(_read(default, "last_validation_passed")) is not bool:
        raise AuthoringError("Compiled last_validation_passed boolean unavailable.")
    _assert_fresh(unreal, package, object_path, project)
    world = unreal.load_object(None, world_object_path, type=unreal.World, follow_redirectors=False)
    if not isinstance(world, unreal.World) or world.get_path_name() != world_object_path:
        raise AuthoringError("Owner-selected supported_world must resolve to that exact World asset.")
    # Loading a World can load dependencies; recheck the destination before creation.
    _assert_fresh(unreal, package, object_path, project)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", definition_class)
    _assert_fresh(unreal, package, object_path, project)
    try:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            package.rsplit("/", 1)[1], package.rsplit("/", 1)[0], definition_class, factory,
            overwrite_existing=False)
    except Exception as exc:
        raise AuthoringError("Asset creation raised an error. This tool attempted no save; inspect any new draft. "
                             "No cleanup was performed. Target: " + object_path) from exc
    if asset is None:
        raise AuthoringError("New definition creation failed; no save was attempted: " + package)
    save_attempted = False
    try:
        if asset.get_path_name() != object_path or asset.get_class() != definition_class or asset is default:
            raise AuthoringError("Factory did not return a new definition of the exact requested path/class.")
        asset.reset_to_intro_supply_example()
        asset.set_editor_property("supported_world", world)
        asset.validate_configuration()
        _verify_intro(asset, world_object_path)
        save_attempted = True
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise AuthoringError("save_loaded_asset did not report success.")
    except Exception as exc:
        state = ("Save was attempted; its outcome may be uncertain. Inspect the new package and disk."
                 if save_attempted else "No save was attempted. Inspect any new draft; no cleanup was performed.")
        raise AuthoringError(str(exc) + " " + state + " Target: " + object_path) from exc
    return {**report, "status": "definition_saved", "destination": package,
            "owner_handoff": owner_handoff, "enabled_quests": 1, "native_validation_passed": True,
            "saved_assets": [object_path], "runtime_verified": False}


if __name__ == "__main__":
    print(json.dumps(plan(), ensure_ascii=False, indent=2))
