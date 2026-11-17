"""Restore the requested session language after English-only engine startup tests.

Only the checked launcher supplies these flags. This never saves user settings.
"""
import json
from pathlib import Path
import re
import unreal


def main():
    root = Path(__file__).resolve().parents[2]
    if Path(unreal.Paths.project_dir()).resolve() != root:
        raise RuntimeError("Wrong project for session culture restoration")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Culture restoration is editor startup only; PIE must be stopped")
    command = unreal.SystemLibrary.get_command_line()
    requested = {}
    for key in ("Language", "Locale"):
        matches = re.findall(r"(?:^|\s)-RogueEditor" + key + r"=([A-Za-z0-9-]+)(?=\s|$)", command)
        if len(matches) != 1:
            raise RuntimeError("Checked launcher must supply exactly one -RogueEditor" + key)
        requested[key.lower()] = matches[0]
    library = unreal.InternationalizationLibrary
    before = {"language": library.get_current_language(), "locale": library.get_current_locale()}
    if before != {"language": "en", "locale": "en"}:
        raise RuntimeError("English startup culture was not established: " + str(before))
    results = {"language": library.set_current_language(requested["language"], False),
               "locale": library.set_current_locale(requested["locale"], False)}
    after = {"language": library.get_current_language(), "locale": library.get_current_locale()}
    record = {"before": before, "requested": requested, "setter_results": results, "after": after,
              "save_to_config": False, "ui_visual_verified": False}
    print("ROGUE_EDITOR_CULTURE " + json.dumps(record, ensure_ascii=False, sort_keys=True))
    if not all(results.values()) or after != requested:
        raise RuntimeError("Session language/locale restoration failed")


if __name__ == "__main__":
    main()
