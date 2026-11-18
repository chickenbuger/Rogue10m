"""Apply only the approved character-lobby UI and run its focused validators.

Run using UnrealEditor-Cmd -run=pythonscript, not a live editor with unsaved work.
The underlying author saves only WBP_CharacterLobby. No maps or SaveGames are saved.
Use forward slashes in the commandlet's -Script argument, including --output-dir;
the Unreal Python argument parser interprets backslash escapes such as \\t.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import traceback

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent.parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=ROOT / "tmp/CharacterLobbyStyle20261119/EditorA")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if not output.is_relative_to(ROOT.resolve()):
        raise RuntimeError("The result directory must be inside this project")
    output.mkdir(parents=True, exist_ok=True)
    report_file = output / "asset-validation.json"
    if report_file.exists():
        raise RuntimeError("Use a fresh result directory; preserve prior validation results")
    protected = (
        "Content/Texture/UI/CharacterLobby/T_CharacterLobby_Background.uasset",
        "Content/DataAsset/Character/Customization/DA_CharacterCustomizationCatalog.uasset",
        "Content/Material/UI/M_CharacterPreviewTransparent.uasset",
    )
    before = {name: digest(ROOT / name) for name in protected}
    report = {"success": False, "scope": "character lobby only", "checks": [],
              "protected_before": before, "saved_assets": []}
    try:
        import ConfigureCharacterSelectionStage as author
        import ValidateCharacterSelectionStage as stage
        import ValidateCharacterCustomizationAssets as customization
        import ValidateInheritedCharacterAssets as inherited
        import ValidateWidgetFlow as flow

        author.main()
        report["saved_assets"] = [author.WIDGET_PATH]
        for label, validator in (("selection_stage", stage),
                                 ("customization", customization),
                                 ("inherited_appearance", inherited),
                                 ("widget_flow", flow)):
            validator.main()
            report["checks"].append({"name": label, "passed": True})
        after = {name: digest(ROOT / name) for name in protected}
        report["protected_after"] = after
        if before != after:
            raise RuntimeError("A protected background, catalog or material changed")
        report["widget_sha256"] = digest(ROOT / "Content/Widget/Lobby/WBP_CharacterLobby.uasset")
        report["success"] = True
        unreal.log("ROGUE_CHARACTER_LOBBY_ASSET_VALIDATION: PASSED")
    except Exception:
        report["error"] = traceback.format_exc()
        raise
    finally:
        report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
