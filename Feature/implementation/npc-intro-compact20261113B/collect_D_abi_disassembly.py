"""Collect bounded local PE/PDB and MSVC disassembly evidence; no game execution.

This is a manual-review packet, not an ABI or startup acceptance gate.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import types

R = Path(__file__).resolve().parents[3]
HELPER = R / 'tmp/PackageAbiVerification20261113C/verify_package_abi.py'
PIN = '269A5ADA0B95750F985DEAE0210E1E7192ECBA8668922FFF95DF307CD687FB8F'
DUMPBIN = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--abi-report', required=True)
    parser.add_argument('--expected-abi-report-sha', required=True)
    parser.add_argument('--output-directory', required=True)
    args = parser.parse_args()
    raw = HELPER.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper() == PIN
    h = types.ModuleType('local_offline_symbols'); h.__file__ = str(HELPER)
    exec(compile(raw, str(HELPER), 'exec'), h.__dict__)
    report_path = h.physical(args.abi_report)
    report_pin = h.file_pin(report_path, args.expected_abi_report_sha)
    report = json.loads(report_path.read_text(encoding='utf-8'))
    h.file_pin(report_path, report_pin['sha256'])
    assert report['schema'] == 'R10-offline-package-abi-v1'
    assert report['status'] == 'ABI_48_PAIR_AND_PINNED_DEFINITIONS_VERIFIED'
    out = h.physical(args.output_directory)
    assert out.is_relative_to(R / 'tmp/PackageAbiVerification20261113C') and not out.exists()
    for name in ('exe', 'pdb', 'definitions'):
        h.file_pin(report[name]['path'], report[name]['sha256'])
    exe, pdb = Path(report['exe']['path']), Path(report['pdb']['path'])
    pe = h.PE(exe)
    symbols = h.Symbols(pdb, pe)
    try:
        assert pe.identity == h.pdb_identity(pdb) == report['pdb_identity']
        out.mkdir()
        table = report['producer']
        rows = []
        for row in table['rows']:
            record = pe.read(row['rva'], 48)
            info = row['info_rva']
            section = next((s for s in pe.sections if s[0] <= info < s[0] + max(s[1], s[3])), None)
            assert section is not None
            rows.append({
                **row, 'record_hex': record.hex(),
                'version_info_size': struct.unpack_from('<Q', record, 32)[0],
                'version_info_hash': struct.unpack_from('<I', record, 40)[0],
                'version_info_padding_hex': record[44:48].hex(),
                'info_symbol': symbols.at(info),
                'info_section': {'rva': section[0], 'virtual_size': section[1],
                                 'raw_size': section[3], 'characteristics': hex(section[4]),
                                 'writable': bool(section[4] & 0x80000000),
                                 'full_32_bytes_in_writable_section': bool(section[4] & 0x80000000)
                                     and info + 32 <= section[0] + max(section[1], section[3])},
                'registration_function': symbols.at(row['register_rva']),
                'ops_function': symbols.at(row['ops_rva']),
            })
        attack_symbols = symbols.matching('*Rogue10mAttackSkillData*')
        candidates = [s for s in attack_symbols if 'dynamic initializer' in s['name'] and 'CompiledInDeferFile' in s['name']]
        crt_begin = symbols.matching('__xc_a')
        crt_end = symbols.matching('__xc_z')
        crt = {'begin_symbols': crt_begin, 'end_symbols': crt_end,
               'status': 'UNVERIFIED', 'initializer_entries': []}
        if len(crt_begin) == len(crt_end) == 1:
            begin, end = crt_begin[0]['rva'], crt_end[0]['rva']
            if 0 <= begin < end and (end - begin) % 8 == 0 and end - begin <= 65536:
                entries = struct.unpack('<' + 'Q' * ((end - begin) // 8), pe.read(begin, end - begin))
                addresses = {pe.base + s['rva']: s for s in candidates}
                crt['initializer_entries'] = [
                    {'table_entry_rva': begin + index * 8, 'initializer': addresses[address]}
                    for index, address in enumerate(entries) if address in addresses
                ]
                crt['status'] = 'CRT_POINTER_TABLE_READ_REQUIRES_STARTUP_CALL_REVIEW'
        functions = [('consumer', report['consumer_candidates'][0]['symbol']),
                     ('registry', report['consumer_candidates'][0]['callee'])]
        functions.extend(('initializer-' + str(i), s) for i, s in enumerate(candidates))
        disassembly = []
        dumpbin_pin = h.file_pin(DUMPBIN, hashlib.sha256(DUMPBIN.read_bytes()).hexdigest())
        for label, symbol in functions:
            length = pe.function_size(symbol['rva'], symbol['size'])
            assert 0 < length <= 16384
            start = pe.base + symbol['rva']
            argv = [str(DUMPBIN), '/NOLOGO', '/NOPDB', '/DISASM:BYTES',
                    '/RANGE:' + hex(start) + ',' + hex(start + length - 1), str(exe)]
            p = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW,
                               timeout=30)
            target = out / (label + '.dumpbin.txt')
            target.write_bytes(p.stdout)
            error_target = out / (label + '.stderr.txt')
            error_target.write_bytes(p.stderr)
            assert p.returncode == 0 and p.stdout.strip() and not p.stderr.strip()
            instructions = re.findall(rb'^\s*([0-9A-Fa-f]{16}):\s+[0-9A-Fa-f]{2}\s', p.stdout, re.M)
            addresses = [int(value, 16) for value in instructions]
            assert addresses and min(addresses) == start and max(addresses) < start + length
            disassembly.append({'label': label, 'symbol': symbol, 'length': length,
                                'argv': argv, 'exit_code': p.returncode,
                                'stdout': h.file_pin(target, hashlib.sha256(p.stdout).hexdigest())})
        evidence = {
            'status': 'BOUNDED_DISASSEMBLY_COLLECTED_MANUAL_REVIEW_PENDING',
            'abi_report': report_pin,
            'exe': report['exe'], 'pdb': report['pdb'], 'definitions': report['definitions'],
            'pe_rsds': pe.identity, 'producer_rows': rows,
            'attack_data_symbols': attack_symbols, 'initializer_candidates': candidates,
            'crt_initializer_table': crt,
            'dumpbin': dumpbin_pin, 'disassembly': disassembly,
            'game_UE_process_executions': 0, 'processes_attached_or_opened': 0,
            'network_symbol_sources': False, 'startup_or_runtime_QA': 'NOT_PERFORMED',
            'remaining_review': [
                'Instruction boundaries and loop register/data/control flow.',
                'Initializer pointer/count2 must reference this producer table and the reviewed consumer.',
                'Info symbol/storage32 writable and both full VersionInfo fields must be appropriate.',
                'Caller-bound definitions must match actual final UBT forced include/action provenance.',
            ],
        }
        for name in ('exe', 'pdb', 'definitions'):
            h.file_pin(report[name]['path'], report[name]['sha256'])
        h.file_pin(report_path, report_pin['sha256'])
        (out / 'collection.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'status': evidence['status'], 'output': str(out),
                          'functions': len(disassembly), 'initializers': len(candidates)}))
    finally:
        symbols.close()
        pe.close()


if __name__ == '__main__':
    main()
