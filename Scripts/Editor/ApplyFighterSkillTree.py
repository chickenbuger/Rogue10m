"""Apply and validate only the fighter profiles and skill-tree widgets in Unreal Editor."""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ConfigureFighterSkillTree
import BuildMartialArtistSkillTreeWidgets
import ValidateMartialArtistSkillTree

ConfigureFighterSkillTree.main()
BuildMartialArtistSkillTreeWidgets.main()
ValidateMartialArtistSkillTree.main()
unreal.log("RESULT=FIGHTER_SKILL_TREE_EDITOR_PASSED")
