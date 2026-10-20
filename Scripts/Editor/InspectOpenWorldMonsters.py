"""Read-only six-species OpenRunMap inventory; root owns editor invocation.

Writes a checkpoint JSON below Feature/doc/evidence, never saves or changes
Unreal objects. Source animation candidates are restricted to existing owned
monster files and their skeletons are compared with the canonical mesh.
"""
from pathlib import Path
import json
import sys

import unreal


ROOT = Path("D:/Project/Rogue10m")
SCRIPT_DIR = ROOT / "Scripts/Editor"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))
from MonsterRosterDefinitions import MONSTER_ROSTER  # noqa: E402

MAP = "/Game/Rogue10m/Maps/OpenRunMap"
BLUEPRINT_ROOT = "/Game/Rogue10m/World/OpenWorld/Encounters"
REPORT = ROOT / "Feature/doc/evidence/open-world-monsters-20261005/inputs.json"
SPECIES = {
    "VerdantSlime": ("BP_OW_VerdantSlime", "Polyart", "Slime"),
    "Sporeling": ("BP_OW_Sporeling", "Polyart", "Mushroom"),
    "CrimsonSlime": ("BP_OW_CrimsonSlime", "PBR", "Slime"),
    "IronShell": ("BP_OW_IronShell", "PBR", "TurtleShell"),
    "ThornWarden": ("BP_OW_ThornWarden", "PBR", "Cactus"),
    "AbyssOverlordAzathor": ("BP_OW_FinalBoss", "PBR", "Beholder"),
}
DATA_FIELDS = (
    "monster_id", "display_name", "level", "monster_rank", "is_boss",
    "experience_reward", "max_health", "max_stamina", "max_mana",
    "health_regeneration_per_second", "skeletal_mesh", "animation_blueprint_class",
    "mesh_relative_location", "mesh_relative_rotation", "mesh_relative_scale",
    "walk_speed", "detection_range", "stop_distance", "behavior_tree_asset",
    "lose_sight_range", "forget_target_seconds", "patrol_radius",
    "patrol_wait_seconds", "max_chase_distance", "attack_skill", "attack_range",
    "attack_damage", "attack_interval", "destroy_on_death",
)
CDO_FIELDS = (
    "monster_data", "max_health", "attack_skill_data", "attack_range",
    "attack_damage", "attack_interval", "behavior_tree_asset",
    "ai_controller_class", "auto_possess_ai", "destroy_on_death",
)
SPAWNER_FIELDS = (
    "monster_class", "max_alive_monsters", "spawn_interval_seconds",
    "fill_to_maximum_on_begin_play", "maintain_population",
    "project_spawn_location_to_ground", "ground_trace_distance",
)


def path_of(value):
    return value.get_path_name() if value else None


def serialized(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (unreal.Vector, unreal.Rotator)):
        return list(value.to_tuple())
    return str(value)


def fields(obj, names):
    return {name: serialized(obj.get_editor_property(name)) for name in names}


def mesh_components(actor):
    mesh = actor.get_editor_property("mesh")
    capsule = actor.get_editor_property("capsule_component")
    return {
        "mesh": path_of(mesh.get_skeletal_mesh_asset()),
        "animation_mode": str(mesh.get_editor_property("animation_mode")),
        "anim_class": path_of(mesh.get_editor_property("anim_class")),
        "materials": [path_of(mesh.get_material(i)) for i in range(mesh.get_num_materials())],
        "relative_location": serialized(mesh.get_editor_property("relative_location")),
        "relative_rotation": serialized(mesh.get_editor_property("relative_rotation")),
        "relative_scale": serialized(mesh.get_editor_property("relative_scale3d")),
        "capsule_radius_cm": capsule.get_unscaled_capsule_radius(),
        "capsule_half_height_cm": capsule.get_unscaled_capsule_half_height(),
    }


def source_animations(style, family, skeleton):
    folder = ROOT / "Content/MonsterForSurvivalGame/Animation" / style / family
    if family == "Mushroom":
        folder = folder / "SmileVersion"
    paths = []
    for file in sorted(folder.glob("*.uasset")):
        if any(token in file.stem.lower() for token in ("idlenormal", "walkfwd", "walk_", "run", "attack", "gethit", "die")):
            paths.append("/Game/" + file.relative_to(ROOT / "Content").with_suffix("").as_posix())
    rows = []
    for path in paths:
        sequence = unreal.load_asset(path)
        if not isinstance(sequence, unreal.AnimSequence):
            rows.append({"path": path, "loaded": bool(sequence), "valid_anim_sequence": False})
            continue
        animation_skeleton = sequence.get_editor_property("skeleton")
        rows.append({"path": path, "loaded": True, "valid_anim_sequence": True,
                     "duration_seconds": sequence.get_play_length(),
                     "skeleton": path_of(animation_skeleton),
                     "matches_mesh_skeleton": animation_skeleton == skeleton})
    return rows


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if world.get_path_name().split(".")[0] != MAP or levels.is_in_play_in_editor():
        raise RuntimeError("Requires idle OpenRunMap editor; no map switch performed by inspector.")
    if REPORT.exists():
        raise RuntimeError("Existing monster inputs checkpoint must be preserved.")
    report = {"world": world.get_path_name(), "species": {}, "encounters": [], "bosses": []}
    canonical = {row["monster_id"]: row for row in MONSTER_ROSTER}
    for monster_id, (blueprint_name, style, family) in SPECIES.items():
        definition = canonical[monster_id]
        data = unreal.load_asset(definition["asset_path"])
        blueprint = unreal.load_asset(BLUEPRINT_ROOT + "/" + blueprint_name)
        if not data or not blueprint:
            raise RuntimeError("Missing authored species " + monster_id)
        cdo = unreal.get_default_object(blueprint.generated_class())
        mesh = unreal.load_asset(definition["mesh"])
        if not isinstance(mesh, unreal.SkeletalMesh):
            raise RuntimeError("Missing owned skeletal mesh " + definition["mesh"])
        skeleton = mesh.get_editor_property("skeleton")
        report["species"][monster_id] = {
            "canonical_source": definition,
            "canonical_data": fields(data, DATA_FIELDS),
            "blueprint": blueprint.get_path_name(),
            "cdo": fields(cdo, CDO_FIELDS),
            "cdo_visual": mesh_components(cdo),
            "mesh": {"path": mesh.get_path_name(), "skeleton": path_of(skeleton),
                     "material_slots": [{"slot": str(slot.get_editor_property("material_slot_name")),
                                         "material": path_of(slot.get_editor_property("material_interface"))}
                                        for slot in mesh.get_editor_property("materials")]},
            "available_animations": source_animations(style, family, skeleton),
        }
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report["loaded_editor_actor_count"] = len(actors)
    for actor in actors:
        if isinstance(actor, unreal.Rogue10mMonsterSpawner):
            area = actor.get_editor_property("spawn_area")
            report["encounters"].append({"label": actor.get_actor_label(),
                                         "location": serialized(actor.get_actor_location()),
                                         "spatial": bool(actor.get_editor_property("is_spatially_loaded")),
                                         "spawn_box_extent": serialized(area.get_unscaled_box_extent()),
                                         **fields(actor, SPAWNER_FIELDS)})
        elif isinstance(actor, unreal.Rogue10mBasicMonster):
            report["bosses"].append({"label": actor.get_actor_label(),
                                     "location": serialized(actor.get_actor_location()),
                                     "spatial": bool(actor.get_editor_property("is_spatially_loaded")),
                                     "monster_data": path_of(actor.get_editor_property("monster_data")),
                                     "components": mesh_components(actor)})
    settings = world.get_world_settings()
    game_mode_class = settings.get_editor_property("default_game_mode")
    report["game_mode"] = {"class": path_of(game_mode_class)}
    if game_mode_class:
        report["game_mode"].update(fields(unreal.get_default_object(game_mode_class),
                                          ("start_run_on_start_play", "use_prototype_run_duration", "prototype_run_duration_seconds")))
    character = unreal.load_asset("/Game/DataAsset/Character/DA_Character_Default")
    report["starter_character"] = fields(character, ("max_health", "attack_power", "defense", "walk_speed", "sprint_speed"))
    report["starter_attacks"] = {}
    for name in ("LeftJab", "RightJab", "RightStraight", "RightHook"):
        skill = unreal.load_asset("/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_" + name)
        if not skill:
            raise RuntimeError("Missing active starter attack " + name)
        report["starter_attacks"][name] = fields(skill, ("damage", "attack_cooldown", "attack_range", "charge_seconds", "hit_start_delay_seconds"))
    report["dirty_maps"] = [path_of(value) for value in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    report["dirty_content"] = [path_of(value) for value in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("[OpenWorldMonsterInputs] " + json.dumps({"report": str(REPORT), "species": len(report["species"]),
                                                        "encounters": len(report["encounters"]), "bosses": len(report["bosses"]),
                                                        "starter_attacks": report["starter_attacks"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
