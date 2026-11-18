"""Pin the actual D compile/link chain; read generated files, never edit them."""
from pathlib import Path
import hashlib
import json
import re

R = Path(__file__).resolve().parents[3]
E = R / 'Feature/doc/evidence/hub-entry-windows20261114A'
G = R / 'Intermediate/Build/Win64/x64/Rogue10m/Development'
M = R / 'Intermediate/Build/Win64/x64/UnrealGame/Development/Rogue10m'
ENGINE = Path('D:/Program Files/UE_5.8/Engine')


def pin(p):
    before = p.stat()
    raw = p.read_bytes()
    after = p.stat()
    assert (before.st_size, before.st_mtime_ns, before.st_ino) == (after.st_size, after.st_mtime_ns, after.st_ino)
    return {'path': str(p), 'size': len(raw), 'sha256': hashlib.sha256(raw).hexdigest().upper(),
            'mtime_ns': after.st_mtime_ns}


def main():
    target = E / 'compile-provenance.json'
    assert not target.exists()
    names = {
        'shared_definitions': G / 'Engine/SharedDefinitions.Engine.Project.ValApi.ValExpApi.Cpp20.h',
        'pch_header': G / 'Engine/SharedPCH.Engine.Project.ValApi.ValExpApi.Cpp20.h',
        'pch_source': G / 'Engine/SharedPCH.Engine.Project.ValApi.ValExpApi.Cpp20.cpp',
        'pch_rsp': G / 'Engine/SharedPCH.Engine.Project.ValApi.ValExpApi.Cpp20.h.obj.rsp',
        'module_definitions': M / 'Definitions.Rogue10m.h',
        'unity_source': M / 'Module.Rogue10m.1.cpp',
        'unity_rsp': M / 'Module.Rogue10m.1.cpp.obj.rsp',
        'generated_source': R / 'Intermediate/Build/Win64/UnrealGame/Inc/Rogue10m/UHT/Rogue10mAttackSkillData.gen.cpp',
        'link_rsp': G / 'Rogue10m.exe.rsp',
        'build_h': ENGINE / 'Source/Runtime/Core/Public/Misc/Build.h',
        'uobject_base_h': ENGINE / 'Source/Runtime/CoreUObject/Public/UObject/UObjectBase.h',
        'deferred_registry_h': ENGINE / 'Source/Runtime/CoreUObject/Private/UObject/DeferredRegistry.h',
    }
    records = {key: pin(value) for key, value in names.items()}
    text = {key: value.read_text(encoding='utf-8-sig') for key, value in names.items()}
    assert '#include "SharedDefinitions.Engine.Project.ValApi.ValExpApi.Cpp20.h"' in text['pch_header']
    assert '#include "SharedDefinitions.Engine.Project.ValApi.ValExpApi.Cpp20.h"' in text['module_definitions']
    assert '/FI"' + names['pch_header'].as_posix() + '"' in text['unity_rsp']
    assert '/FI"' + names['module_definitions'].as_posix() + '"' in text['unity_rsp']
    assert names['generated_source'].as_posix() in text['unity_source']
    assert (M / 'Module.Rogue10m.1.cpp.obj').as_posix() in text['link_rsp']
    assert (G / 'Engine/SharedPCH.Engine.Project.ValApi.ValExpApi.Cpp20.h.obj').as_posix() in text['link_rsp']
    macros = ('WITH_LIVE_CODING', 'WITH_RELOAD', 'WITH_HOT_RELOAD', 'IS_MONOLITHIC', 'WITH_EDITOR', 'UE_WITH_CONSTINIT_UOBJECT')
    pattern = r'^\s*#\s*(?:define|undef)\s+(?:' + '|'.join(macros) + r')\b.*$'
    definitions = {key: re.findall(pattern, value, re.M) for key, value in text.items()}
    assert '#define WITH_LIVE_CODING 1' in definitions['shared_definitions']
    assert not definitions['module_definitions']
    assert not definitions['unity_source'] and not definitions['generated_source']
    assert 'WITH_LIVE_CODING' not in text['unity_rsp'] and 'WITH_RELOAD' not in text['unity_rsp']
    assert 'WITH_LIVE_CODING' not in text['pch_rsp'] and 'WITH_RELOAD' not in text['pch_rsp']
    owner = json.loads((E / 'owner-result.json').read_text(encoding='utf-8-sig'))
    assert owner['exit_code'] == 0
    post = json.loads((Path(owner['output_root']) / 'evidence/postflight.json').read_text(encoding='utf-8-sig'))
    result = {'status': 'ACTUAL_NEW_COMPILE_LINK_CHAIN_PINNED_REQUIRES_FINAL_BINARY_REVIEW_NO_RUNTIME',
              'records': records, 'observed_macro_directives': definitions,
              'final_postflight': pin(Path(owner['output_root']) / 'evidence/postflight.json'),
              'postflight_status': post['status'],
              'note': 'Actual generated TU -> module/PCH response -> link response provenance. No preprocessing or generated file mutation. Final ABI and startup remain separate checks.',
              'generated_files_modified': 0, 'UE_Game_executions': 0}
    for key, value in names.items():
        assert pin(value) == records[key]
    with target.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, indent=2); stream.write('\n')
    print(json.dumps({'status': result['status'], 'output': str(target), 'files': len(records)}))


if __name__ == '__main__':
    main()
