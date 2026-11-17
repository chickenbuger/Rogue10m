"""Read candidate GameMode defaults; no saves, input, selection or PIE."""
import json
from pathlib import Path
import unreal

root = Path("D:/Project/Rogue10m")
assert Path(unreal.Paths.project_dir()).resolve() == root.resolve()
out = root / "Feature/doc/evidence/map-resume-20261113/candidate-mode-defaults.json"
assert not out.exists()
world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A")
assert world.get_path_name().startswith("/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.")
mode = world.get_world_settings().get_editor_property("default_game_mode")
defaults = unreal.get_default_object(mode)
record = {"world": world.get_path_name(), "game_mode_class": mode.get_path_name(),
          "is_rogue10m_game_mode": isinstance(defaults, unreal.Rogue10mGameMode),
          "default_classes": {}, "gui_input_operations": 0, "assets_saved": False,
          "boundary": "Native inheritance/CDO inspection only; class names do not prove camera or play behavior"}
for prop in ("default_pawn_class", "game_state_class", "player_state_class", "player_controller_class", "hud_class"):
    value = defaults.get_editor_property(prop)
    record["default_classes"][prop] = value.get_path_name() if value else None
pawn_class = defaults.get_editor_property("default_pawn_class")
record["is_rogue10m_character"] = bool(pawn_class and isinstance(unreal.get_default_object(pawn_class), unreal.Rogue10mCharacter))
record["dirty_maps"] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
record["dirty_content"] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
out.write_text(json.dumps(record, indent=2), encoding="utf-8")
assert not record["dirty_maps"] and not record["dirty_content"]
