"""Scoped headless preparation entry point. Leaves activation for measured visual review."""
import runpy,unreal
from pathlib import Path
p=Path(unreal.Paths.project_dir()).resolve()
try:
 if '-RogueSkipWeaponMeshes' not in unreal.SystemLibrary.get_command_line():
  runpy.run_path(str(p/'Scripts/Editor/PrepareWeaponVisuals20261110.py'),run_name='__main__')
 runpy.run_path(str(p/'Scripts/Editor/PrepareGreatSwordAnimations20261110.py'),run_name='__main__')
 unreal.log('RESULT=WEAPON_PREPARATION_PASSED')
finally:unreal.SystemLibrary.quit_editor()
