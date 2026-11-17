"""Install one explicit quest director in the existing five-village candidate.

Importing this module performs no Unreal or filesystem mutation. Run only after
the shared editor/build owner integrates and builds the new runtime classes.
"""
from __future__ import annotations

import datetime
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CANDIDATE = "/Game/Rogue10m/Maps/Prototypes/FiveVillageGraybox20261110"
DIRECTOR_CLASS = "/Script/Rogue10m.Rogue10mVillageQuestDirector"
TAG = "Rogue10m.VillageQuests20261111"


def install(*, editor_handoff: str, build_verified: bool = False):
    if not editor_handoff.strip() or not build_verified:
        raise RuntimeError("The shared editor/build owner must confirm handoff and the new-class build.")
    import unreal

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != (ROOT / "Rogue10m.uproject").resolve():
        raise RuntimeError("Wrong project; copy the reviewed script into the integration project first.")
    world = worlds.get_editor_world()
    if not world or world.get_path_name().split(".")[0] != CANDIDATE or levels.is_in_play_in_editor():
        raise RuntimeError("Open the idle five-village candidate. No map is switched automatically.")
    director_class = unreal.load_class(None, DIRECTOR_CLASS)
    if not director_class:
        raise RuntimeError("Quest director class unavailable. Build and restart the editor first.")
    existing = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.Rogue10mVillageQuestDirector)]
    if len(existing) > 1:
        raise RuntimeError("Multiple quest directors exist. Inspect them before changing this map.")
    if existing:
        return {"status": "already_present", "director": existing[0].get_path_name(), "runtime_verified": False}
    saving = unreal.EditorLoadingAndSavingUtils
    dirty = list(saving.get_dirty_map_packages()) + list(saving.get_dirty_content_packages())
    if dirty:
        raise RuntimeError("Clean editor packages are required; preserve unrelated edits before installation.")
    director = actors.spawn_actor_from_class(director_class, unreal.Vector(0, 0, 0))
    if not director:
        raise RuntimeError("Quest director spawn failed.")
    director.set_actor_label("FV_OptionalVillageQuestDirector")
    director.set_folder_path("FiveVillageRuntime20261111/Quests")
    director.set_editor_property("tags", [unreal.Name(TAG)])
    if not levels.save_current_level():
        raise RuntimeError("Map save failed; preserve the placed director and inspect the editor.")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    report = {"status": "director_saved", "director": director.get_path_name(),
              "map": CANDIDATE, "editor_handoff": editor_handoff,
              "runtime_verified": False, "physical_input_verified": False,
              "default_rewards": "run_gold_only", "npc_count_expected_while_running": 5,
              "exploration_markers_expected": 9, "combat_targets_expected": 1}
    destination = ROOT / "Feature/doc/evidence/village-quests-20261111"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / ("director-install-" + stamp + ".json")).write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    print("Import and call install after the shared editor/build owner confirms handoff.")
