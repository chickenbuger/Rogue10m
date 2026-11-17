"""Pin a targeted native LLVM PDB query for the completed D archive only."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import subprocess
import types

R = Path(__file__).resolve().parents[3]
BASE = R / 'tmp/PackageAbiVerification20261113C'
H = BASE / 'verify_package_abi.py'
ABI = BASE / 'D-actual-archive-01.json'
ABI_SHA = '9E24D20E8551B00B40BF37D981C24422A3395A99E1805C1D3A6C7AD29B2459E8'
TOOL = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/llvm-pdbutil.exe')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-name', required=True)
    parser.add_argument('--global-name')
    parser.add_argument('--type-index')
    args = parser.parse_args()
    assert args.output_name.replace('-', '').replace('_', '').isalnum()
    assert not (args.global_name and args.type_index)
    raw = H.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == '269A5ADA0B95750F985DEAE0210E1E7192ECBA8668922FFF95DF307CD687FB8F'
    h = types.ModuleType('pinned_offline_symbols'); h.__file__ = str(H)
    exec(compile(raw, str(H), 'exec'), h.__dict__)
    h.file_pin(ABI, ABI_SHA)
    report = json.loads(ABI.read_text(encoding='utf-8'))
    assert report['status'] == 'ABI_48_PAIR_AND_PINNED_DEFINITIONS_VERIFIED'
    h.file_pin(report['pdb']['path'], report['pdb']['sha256'])
    output = BASE / 'D-native-types-01'
    output.mkdir(exist_ok=True)
    target = h.physical(output / (args.output_name + '.txt'))
    receipt = output / (args.output_name + '.process.json')
    assert not target.exists() and not receipt.exists()
    argv = [str(TOOL), 'dump']
    if args.global_name:
        argv += ['--globals', '--global-name=' + args.global_name]
    elif args.type_index:
        assert 0x1000 <= int(args.type_index, 16) <= 0xffffffff
        argv += ['--types', '--type-index=' + args.type_index]
    else:
        argv += ['--summary']
    argv += [report['pdb']['path']]
    tool_pin = h.file_pin(TOOL, hashlib.sha256(TOOL.read_bytes()).hexdigest())
    start = datetime.now(timezone.utc).isoformat()
    p = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       creationflags=subprocess.CREATE_NO_WINDOW, timeout=45)
    with target.open('xb') as stream:
        stream.write(p.stdout)
    error = output / (args.output_name + '.stderr.txt')
    with error.open('xb') as stream:
        stream.write(p.stderr)
    result = {'status': 'ACTUAL_NATIVE_LLVM_QUERY_RETURNED_REQUIRES_TYPE_LINK_REVIEW',
              'args': argv, 'exit_code': p.returncode, 'started_utc': start,
              'ended_utc': datetime.now(timezone.utc).isoformat(), 'tool': tool_pin,
              'abi_report_sha256': ABI_SHA, 'pdb': report['pdb'],
              'stdout': h.file_pin(target, hashlib.sha256(p.stdout).hexdigest()),
              'stderr': h.file_pin(error, hashlib.sha256(p.stderr).hexdigest()),
              'UE_Game_executions': 0, 'process_attachment': False}
    h.file_pin(report['pdb']['path'], report['pdb']['sha256'])
    h.file_pin(ABI, ABI_SHA)
    with receipt.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(p.stdout.decode('utf-8', 'replace'))
    assert p.returncode == 0 and not p.stderr.strip() and p.stdout.strip()


if __name__ == '__main__':
    main()
