"""Run the existing guard/FootIK audit into this work item's evidence folder."""
import sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Scripts/Editor'))
import PrepareFighterCombatStance as stance

stance.EVIDENCE = ROOT / 'Feature/doc/evidence/moving-upper-body-attack-20261009/stance-audit'
try:
    stance.main()
finally:
    unreal.SystemLibrary.execute_console_command(
        unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'QUIT_EDITOR')
