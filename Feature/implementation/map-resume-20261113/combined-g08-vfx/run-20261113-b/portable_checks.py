from pathlib import Path
import ast
import json
import os
import subprocess
import sys
from datetime import datetime, timezone

run = Path(__file__).resolve().parent
candidate = run / 'G08ThenVFX'
delivery = run / 'Delivery'
env = dict(os.environ, ROGUE_VFX_TEST_TEMP_ROOT='C:/Users/PC/AppData/Local/Temp', PYTHONDONTWRITEBYTECODE='1')
args = [sys.executable, '-B', '-m', 'unittest', 'discover', '-s', 'Scripts/Tests', '-p', 'test_weapon_skill_vfx_*.py', '-v']
result = subprocess.run(args, cwd=candidate, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
(delivery / 'portable-vfx-unittest.stdout.log').write_bytes(result.stdout)
(delivery / 'portable-vfx-unittest.stderr.log').write_bytes(result.stderr)
manifest = json.loads((delivery / 'combined-manifest.json').read_text())
ast_files = []
for entry in manifest['entries']:
    if entry['path'].endswith('.py'):
        ast.parse((candidate / entry['path']).read_text(encoding='utf-8-sig'), filename=entry['path'])
        ast_files.append(entry['path'])
report = {'created_utc': datetime.now(timezone.utc).isoformat(),
          'g08_standalone': {'command': 'Scripts/Tests/TestAuthoredContactTimeline.ps1', 'exit_code': 0, 'scenarios': 24, 'checks': 230, 'compiler': 'MSVC C++17 /W4 /WX', 'ue_animation_physics_run': False, 'observed_stdout': 'authored_contact_timeline: 24 scenarios, 230 checks passed; Unreal/animation/physics not run'},
          'vfx_unittest': {'command': args, 'exit_code': result.returncode, 'temp_root': env['ROGUE_VFX_TEST_TEMP_ROOT'], 'stdout_log': 'portable-vfx-unittest.stdout.log', 'stderr_log': 'portable-vfx-unittest.stderr.log'},
          'python_ast': {'passed': True, 'count': len(ast_files), 'paths': ast_files},
          'prior_attempts_preserved': [{'attempt': 'pytest', 'exit_code': 1, 'reason': 'No module named pytest; no installation attempted'}, {'attempt': 'unittest with environment TEMP root', 'exit_code': 1, 'tests': 53, 'setup_errors': 40, 'reason': 'per-command TEMP path exceeded fixture limit; no test/code edits; returned output preserved'}],
          'not_run': ['Staff fake-MCP suite requires legacy test fixture outside the requested union; prior packet evidence is separate', 'UE build/automation/render/live binding/physical input/cook']}
(delivery / 'portable-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(result.stderr.decode(errors='replace')[-1800:])
print(json.dumps({'vfx_exit_code': result.returncode, 'python_ast_files': len(ast_files)}))
sys.exit(result.returncode)
