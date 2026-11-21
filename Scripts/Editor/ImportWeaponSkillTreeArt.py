"""Import the weapon skill medallions with the shared Slate texture settings."""
from pathlib import Path
import runpy

import unreal


if __name__ == "__main__":
    common_importer = runpy.run_path(str(Path(__file__).with_name("ImportCommonSkillTreeArt.py")))
    common_importer["import_texture"]("T_WeaponSkillMedallions", (1024, 1024))
    unreal.log("RESULT=WEAPON_SKILL_TREE_ART_IMPORTED")
