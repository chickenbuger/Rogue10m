"""Task-owned serialized official Unreal MCP helper for monster authoring."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/open-world-monsters-20261005'
EVIDENCE.mkdir(parents=True, exist_ok=True)


def call(tool, arguments, name, timeout=60):
    args = EVIDENCE / (name + '-args.json')
    output = EVIDENCE / (name + '.json')
    args.write_text(json.dumps(arguments, ensure_ascii=False, indent=2), encoding='utf-8')
    result = subprocess.run([sys.executable, str(ROOT / 'Scripts/InvokeUnrealMCP.py'),
                             '--tool', tool, '--arguments-file', str(args), '--output', str(output),
                             '--timeout', str(timeout)], capture_output=True, text=True, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(name + ': ' + result.stdout[-2000:] + result.stderr[-1000:])
    response = json.loads(output.read_text(encoding='utf-8'))
    if response.get('error') or response['result'].get('isError'):
        raise RuntimeError('Unreal MCP rejected ' + name)
    bodies = [json.loads(c['text']) for c in response['result'].get('content', []) if c['type'] == 'text']
    if len(bodies) != 1:
        raise RuntimeError('Unexpected MCP response ' + name)
    print('MCP', name, 'completed', flush=True)
    return bodies[0].get('returnValue', bodies[0])
