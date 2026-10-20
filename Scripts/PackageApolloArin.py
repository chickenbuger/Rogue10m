"""Preserve the native Apollo export and package an explicitly static Arin draft."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'Feature/doc/evidence/arin-apollo-20261005'
PACK = EVIDENCE / 'hull-export-pack'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extract-only', action='store_true')
    args = parser.parse_args()
    archive = EVIDENCE / 'arin-apollo-native-export.zip'
    with zipfile.ZipFile(archive) as source:
        if source.testzip() is not None:
            raise RuntimeError('Native Apollo archive has a failed CRC')
        for member in source.infolist():
            destination = (PACK / member.filename).resolve()
            if not destination.is_relative_to(PACK.resolve()):
                raise RuntimeError('Archive path leaves the task-owned export directory')
        source.extractall(PACK)
    model = PACK / 'assets/model.fbx'
    glb = PACK / 'assets/model.glb'
    if not model.is_file() or model.stat().st_size == 0:
        raise RuntimeError('Actual Apollo FBX file is absent')
    if sha(glb) != sha(EVIDENCE / 'hull/arin-hull-normalized.glb'):
        raise RuntimeError('Apollo export changed the normalized hull GLB')
    manifest = read(PACK / 'manifest.json')
    for entry in manifest['files']:
        path = (PACK / entry['path']).resolve()
        if not path.is_relative_to(PACK.resolve()) or sha(path) != entry['sha256']:
            raise RuntimeError('Export manifest file or checksum mismatch')
    if args.extract_only:
        print(json.dumps({'status': 'extracted_actual_export', 'fbx': str(model),
                          'glb_original_sha256_preserved': True}))
        return
    inspection = read(EVIDENCE / 'hull-inspection/fbx-inspection.json')
    rig = read(EVIDENCE / 'rig/arin-rig-report.json')
    if inspection['status'] != 'passed_reimport' or inspection['armature_count'] != 0:
        raise RuntimeError('Static FBX re-import proof is missing or inconsistent')
    if abs(inspection['world_dimensions_blender_units'][2] - 1.7) > 0.002:
        raise RuntimeError('FBX roundtrip did not preserve the 1.7m height')
    if rig['status'] != 'failed':
        raise RuntimeError('Review the rig acceptance result before packaging this static-only draft')
    report = {
        'status': 'static_draft_exported_npc_acceptance_failed',
        'provider': 'unchanged ApolloHullAdapter.generate', 'provider_lane': 'local_draft',
        'vendor_commit': '8cadd481d0155589b39456a6266d88072d7111cf',
        'geometry_checks': read(EVIDENCE / 'hull/hull-report.json')['checks'],
        'triangles': inspection['total_triangles'], 'height_m': inspection['world_dimensions_blender_units'][2],
        'armatures': 0, 'animations': 0, 'appearance': 'geometry-only, no production textures',
        'npc_applied': False, 'existing_npc_preserved': True,
        'rig_failure': rig['error'],
        'interpretation': 'Apollo ready/upload score is pipeline readiness; it does not certify NPC appearance or animation.',
        'actual_fbx': str(model), 'fbx_sha256': sha(model), 'fbx_bytes': model.stat().st_size,
        'glb_sha256': sha(glb), 'native_archive_sha256': sha(archive),
    }
    readme = (
        '# 아린 Apollo 정적 3D 초안\n\n'
        'assets/model.fbx 또는 assets/model.glb가 실제 모델입니다. previews는 검사 이미지입니다.\n'
        '이 모델은 정면·측면 실루엣을 교차해 생성한 170cm 입체 초안입니다. 뼈·Idle·완성 텍스처가 없습니다.\n'
        '별도 리깅 시도는 팔을 내릴 때 망토 늘어짐과 얼굴 중복으로 실패했습니다. 게임 NPC 적용을 승인하지 않았습니다.\n'
        'Apollo ready와 업로드 점수 1.0은 파일 처리 상태이며 게임용 품질을 뜻하지 않습니다.\n'
        '원본 native manifest는 유지했습니다. CPU local_draft 출처는 provenance/hull-report.json을 확인하세요.\n'
        '기존 StartHub 안내 NPC 외형·대화·아이템 지급은 교체하지 않았습니다.\n'
    )
    (PACK / 'README-KO.md').write_text(readme, encoding='utf-8')
    (EVIDENCE / 'delivery-status.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    final = EVIDENCE / 'arin-apollo-static-draft.zip'
    with zipfile.ZipFile(final, 'w', compression=zipfile.ZIP_DEFLATED) as output:
        for path in sorted(PACK.rglob('*')):
            if path.is_file():
                output.write(path, path.relative_to(PACK).as_posix())
        for source, target in [
            (EVIDENCE / 'delivery-status.json', 'provenance/delivery-status.json'),
            (EVIDENCE / 'hull/hull-report.json', 'provenance/hull-report.json'),
            (EVIDENCE / 'multiview/reference-manifest.json', 'provenance/reference-manifest.json'),
            (EVIDENCE / 'hull-inspection/fbx-inspection.json', 'provenance/fbx-inspection.json'),
            (EVIDENCE / 'rig/arin-rig-report.json', 'provenance/failed-rig-report.json'),
            (EVIDENCE / 'hull-shape-inspection/fbx-inspection.json', 'provenance/shape-inspection.json'),
        ]:
            output.write(source, target)
        for view in ('front', 'side', 'oblique'):
            output.write(EVIDENCE / f'hull-shape-inspection/fbx-{view}.png',
                         f'inspection/clay-{view}.png')
    with zipfile.ZipFile(final) as output:
        if output.testzip() is not None:
            raise RuntimeError('Final draft archive failed CRC verification')
    print(json.dumps({'status': report['status'], 'archive': str(final),
                      'fbx': str(model), 'npc_applied': False}, ensure_ascii=False))


if __name__ == '__main__':
    main()
