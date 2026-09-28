"""Apply only the XP, target, Bottom and Main widgets changed by first-person framing."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import unreal
from ApplyReferenceMetalHUD import build_experience, build_monster, build_bottom, apply_main
from ValidateReferenceMetalHUD import main as validate
def main():
    build_experience()
    build_monster()
    build_bottom()
    apply_main()
    validate()
    unreal.log("RESULT=FIRST_PERSON_HUD_FRAMING_PASSED assets=4")
if __name__ == "__main__":
    main()
