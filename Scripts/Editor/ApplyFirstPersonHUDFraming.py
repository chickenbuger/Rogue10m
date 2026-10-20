"""Apply only the XP, target, Bottom and Main widgets changed by first-person framing."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import unreal
from ApplyReferenceMetalHUD import build_experience, build_monster, build_bottom, apply_main
from ValidateReferenceMetalHUD import main as validate
def main():
    from ApplyClassThemedBottomHUD import BOTTOM_HUD_PATH, load_widget, widget_infos
    if any(info.widget.get_name() == "UI_CompactPanel" for info in widget_infos(load_widget(BOTTOM_HUD_PATH))):
        raise RuntimeError("Compact HUD is installed. Use ApplyCompactCombatHUD.py; the older first-person framing would overwrite it.")
    build_experience()
    build_monster()
    build_bottom()
    apply_main()
    validate()
    unreal.log("RESULT=FIRST_PERSON_HUD_FRAMING_PASSED assets=4")
if __name__ == "__main__":
    main()
