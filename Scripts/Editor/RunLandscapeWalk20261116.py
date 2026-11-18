"""Owner-invoked, phased entry point; retains one observer module in UE Python."""
import argparse
import importlib.util
from pathlib import Path
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--phase", required=True,
                    choices=("menu", "profile", "resumeprofile", "confirmhub", "inspect", "field", "round1", "round2", "round3", "summary"))
phase = parser.parse_args().phase
key = "landscape_walk_qa_20261116"
if phase in ("resumeprofile", "confirmhub") and key in sys.modules:
    assert sys.modules[key]._runner is None, "Never replace a live movement observer"
    del sys.modules[key]
if key not in sys.modules:
    path = Path("D:/Project/Rogue10m/Scripts/Editor/PlaytestLandscapeCrop20261116.py")
    spec = importlib.util.spec_from_file_location(key, path)
    module = importlib.util.module_from_spec(spec)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    sys.modules[key] = module
module = sys.modules[key]
if phase == "menu":
    result = module.prepare_menu(allow_developer_pie_travel=True)
elif phase == "profile":
    result = module.create_profile_from_menu()
elif phase == "resumeprofile":
    result = module.resume_existing_created_profile_from_menu()
elif phase == "confirmhub":
    result = module.confirm_existing_profile_in_hub()
elif phase == "field":
    result = module.enter_candidate_from_hub(allow_developer_pie_travel=True)
elif phase.startswith("round"):
    number = int(phase[-1])
    result = module.begin_round(number, fresh_pie_confirmed=True,
                                allow_default_run_start=number > 1)
elif phase == "inspect":
    u, world, pc = module._context({module.HUB, module.TARGET})
    pawn = u.GameplayStatics.get_player_character(world, 0)
    instance = u.GameplayStatics.get_game_instance(world)
    profiles = [obj for obj in u.ObjectIterator(u.Rogue10mCharacterProfileSubsystem)
                if obj.get_outer() == instance]
    result = {"world": world.get_path_name(), "pawn": pawn.get_class().get_path_name(),
              "display_name": str(pawn.get_character_display_name()),
              "profile_counts": [obj.get_profile_count() for obj in profiles],
              "max_walk_speed": pawn.get_movement_component().max_walk_speed,
              "stat_move_speed": pawn.get_character_stat_snapshot().get_editor_property("MoveSpeed")}
    module._write_new("runtime-state-inspection", result)
else:
    result = module.summarize()
print("LANDSCAPE_WALK_PHASE_20261116 " + phase + " " + str(result))
