from pathlib import Path
import json
r=Path('D:/Project/Rogue10m');e=r/'Feature/doc/evidence/unused-file-cleanup-20260928';m=json.loads((e/'selected-cleanup.json').read_text(encoding='utf-8'));receipt=[json.loads(x) for x in (e/'deleted-files.jsonl').read_text(encoding='utf-8-sig').splitlines() if x.strip()]
assert len(receipt)==len(m['files'])==2831
assert {x['original'] for x in receipt}=={x['original'] for x in m['files']}
assert all(not (r/x['original']).exists() for x in m['files'])
assert 'CLEANUP_PASSED' in (e/'apply.log').read_text(encoding='utf-8-sig',errors='replace')
summary={'status':'passed','removed_files':len(receipt),'removed_bytes':sum(x['bytes'] for x in receipt),'duplicate_files':50,'regeneratable_frames':2781,'frame_folders':10,'retained_dependencies':len(m['retained_dependencies']),'all_selected_absent':True,'all_retained_dependency_hashes_verified_before_and_after':True,'source_content_config_files_removed':0,'unreal_build':'not required; gameplay source/assets unchanged'}
(e/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
text='''# 사용하지 않는 임시 파일 정리

작성: 2026-09-28 / 브랜치: Sprint#4-41-unused-file-cleanup
상태: 정리 및 무결성 검증 완료. 커밋·푸시 없음.

## 정리 결과

사용자 요청에 따라 프로젝트 참조와 임시 산출물을 조사하고, 불필요함을 확인한 파일 2,831개(2,553,045,782바이트, 약2.55GB)를 삭제했다.

| 구분 | 삭제 수 | 확인 근거 |
| --- | ---: | --- |
| 자막 영상 제작용 임시 PNG | 2,781 | 10폴더 모든 이미지가 보존 원본·기존 생성 스크립트로 RGB 픽셀까지 동일하게 재생성됨. 최종 영상의 스트림·프레임 수 확인 |
| 중복 로그·검토 이미지 등 | 50 | 삭제 집합 밖에 바이트가 동일한 SHA-256 사본이 존재하고, 동적 경로까지 검토하여 필요한 입력을 제외 |

삭제는 `tmp` 아래의 확정된 개별 파일에만 적용했다. 폴더 전체 삭제나 Git clean은 사용하지 않았다. 빈 출력 폴더는 다음 재생성에 사용할 수 있도록 그대로 뒀다. 검증된 10개 폴더의 frame_*.png만 .gitignore에 추가하여 다시 생성해도 Git 변경 목록에 쌓이지 않게 했다.

## 보존한 파일

Source/Scripts/Config 249개를 조사했으나 바로 삭제해도 된다고 입증한 소스 파일은 없었다. 기존 FirstPersonPresentation은 초기 시선과 갱신에 쓰이며 FistAnimInstance는 대체 모드 참조가 남아 있다. Editor 도구와 Tests 역시 등록된 콘솔 진입점이 있어 보존했다. Blueprint에서 동적으로 참조할 수 있는 Content 애셋은 이름이나 텍스트 검색 결과만으로 삭제하지 않았다.

원본 촬영본, 최종 MP4, 참조 영상·원본 자료, 백업, 생성 스크립트, 필요한 로그, ffmpeg 등의 도구를 보존했다. 자동 중복 후보137개 중87개는 실행 코드·수동 명령 경로·basename 목록·동적 로그·glob 입력 등의 이유로 남겼다. 사전 감사에서 참조가 확인된 중복20개도 별도로 보존했다.

`tmp/boxing-direct/labeled-lookdown`의278프레임은 재생성 픽셀이 일치했지만 대응하는25도 최종 영상이 확인되지 않아 보존했다. 엔진의 Saved/Intermediate/Binaries/DerivedDataCache는 읽기 확인 외 수정하거나 정리하지 않았다.

## 검증

- 기획1 / 개발2 / 검증2 역할로 참조 감사, 후보 생성, 독립 검토, 전 프레임 재생성 검사를 분담했다.
- 재생성 감사11폴더3,059프레임 모두 RGB 픽셀 일치. 이 중 최종 영상도 확인된10폴더만 정리했다.
- 삭제 직전 전체 경로가 프로젝트tmp 내부인지, reparse 경로·Git tracked 파일·백업/도구가 아닌지 확인했다.
- 삭제 대상과 보존 의존성의 교집합0, 원본/보존 파일의 해시 재검사 통과.
- 실제 삭제 영수증2,831개와 확정 목록 일치, 삭제 대상 전부 부재 확인.
- 보존 의존성2,858개는 삭제 전후 SHA-256이 동일하다.
- 게임플레이 C++·애셋은 변경하지 않아 Unreal 빌드는 다시 실행하지 않았다. 정리 스크립트 preview/apply와 저장 경로·diff 검사를 실행했다.

## 삭제 목록과 복구

[확정 목록](evidence/unused-file-cleanup-20260928/selected-cleanup.json), [실제 삭제 기록](evidence/unused-file-cleanup-20260928/deleted-files.jsonl), [복구 안내](evidence/unused-file-cleanup-20260928/RESTORE.md).

중복 파일은 keep에서 원래 위치로 복사하면 바이트까지 동일하게 복구할 수 있다. 자막 PNG는 프로젝트 루트에서 보존한 generator 스크립트를 실행해 다시 만든다. 프레임 복구는 RGB 픽셀 동일성 기준이며 PNG 압축 바이트와 메타데이터가 동일하다는 의미는 아니다. 재인코딩 전에는 먼저 프레임을 생성한다.

읽기 전용 감사 도구: `Scripts/AuditUnusedWorkFiles.py`. 이 도구의 후보 출력만으로 삭제하지 않고, 이번처럼 실행 참조·재생성 입력과 함께 검토해야 한다.

증거: `recreatable-frames.json`, `reviewed_candidates.json`, `runtime-reference-audit.md`, `source-review.md`, `preview.log`, `apply.log`, `summary.json`.
'''
(r/'Feature/doc/2026-09-28_unused-file-cleanup.md').write_text(text,encoding='utf-8')
with (r/'DevLog/20260928.txt').open('a',encoding='utf-8') as f:f.write('''

## 2026-09-28 — 미사용 임시 파일 정리

### 정리 목적
현재 프로젝트의 사용하지 않는 파일을 정리했다. Sprint#4-41-unused-file-cleanup 브랜치에서 기존 변경 사항을 보존하며, 참조 조사와 재생성 검증을 먼저 진행했다.

### 정리 내용
기획 1 / 개발 2 / 검증 2 역할로 조사했다. 최종 영상이 남아 있고 현재 원본과 스크립트로 픽셀까지 동일하게 재생성한 임시 자막 PNG 2,781개와 바이트가 동일한 보존 사본이 있는 중복본 50개를 삭제했다. 총 2,831개, 2,553,045,782바이트(약 2.55GB)다. 재생성 이미지가 Git 변경 목록에 다시 쌓이지 않도록 검증한 10개 출력 폴더만 제외 규칙을 추가했다.

### 보존 판단
249개 소스·스크립트·설정 파일 조사에서 삭제 가능한 미사용 소스는 입증하지 못했다. 이전 1인칭 코드와 콘솔 도구에는 참조가 남아 있어 유지했다. 원본 촬영본·최종 영상·백업·생성 코드·입력 로그·도구·애셋도 보존했다. 최종 25도 영상이 확인되지 않은 이미지 278개는 재생성 가능하더라도 남겼다. 자동 중복 후보 중 동적 경로와 수동 실행 호환성이 필요한 87개도 유지했다.

### 검증 결과
정리 전후 보존 의존성 2,858개의 SHA-256이 일치했다. 삭제 영수증 2,831개가 확정 목록과 일치하고 대상 파일 부재를 확인했다. 재생성 감사는 모든 프레임의 RGB 픽셀을 대조했으며, 실제 삭제는 검증된 tmp 파일에만 개별 적용했다. 게임 코드·애셋 변경이 없어 UE 재빌드는 생략했다. 엔진 생성 폴더는 정리하지 않았다. 커밋·푸시는 없다.

### 관련 문서
Feature/architect/2026-09-28_unused-file-cleanup.md
Feature/doc/2026-09-28_unused-file-cleanup.md
Feature/doc/evidence/unused-file-cleanup-20260928/RESTORE.md

### Notion 요약 후보
제목: 미사용 임시 파일 2.55GB 정리
요약: 원본과 최종 결과물을 보존하면서 재생성 검증을 통과한 자막 프레임과 중복본을 정리했다. 소스·애셋은 참조를 확인하지 못한 상태에서 삭제하지 않았으며, 삭제 목록과 복구 방법을 기록했다.
''')
with (r/'Docs/SprintChangeLog.md').open('a',encoding='utf-8') as f:f.write('''

## Sprint#4-41 - 미사용 임시 파일 정리 (2026-09-28)

- 브랜치: `Sprint#4-41-unused-file-cleanup`
- 목표: 현재 참조와 복구 가능성을 확인하여 불필요한 파일을 정리한다.
- 주요 변경: 재생성 PNG2,781개와 중복본50개, 총2,831개/약2.55GB 삭제. 검증된 출력 폴더10개의 PNG Git 제외 규칙, 읽기 전용 감사 도구와 삭제·복구 매니페스트 추가.
- 검증: 원본·스크립트로 전 프레임 RGB 재생성 일치, 최종 영상 스트림·프레임 수 확인. 보존 의존성2,858개 전후SHA256일치, 삭제 영수증과 대상 부재 확인. 경로/diff검사. 게임 코드·애셋 변경 없어 UE 재빌드 생략.
- 상태: 정리 완료. 참조가 남은 기존FP코드·콘솔도구·원본·애셋·백업과 대응 최종 영상이 없는278프레임 보존. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_unused-file-cleanup.md`, `Feature/doc/2026-09-28_unused-file-cleanup.md`, `DevLog/20260928.txt`.
''')
with (r/'Feature/architect/2026-09-28_unused-file-cleanup.md').open('a',encoding='utf-8') as f:f.write('\n\n## 종료 기록\n\n2026-09-28: 확정 대상2,831개/2,553,045,782바이트 개별 삭제 완료. 중복50개와 재생성프레임2,781개, 보존 의존성2,858개 전후해시 일치. source/Content 삭제0. 원본과 최종 영상·백업·동적 참조 파일을 보존하고 삭제/복구 매니페스트를 작성했다. 결과 문서 참조.\n')
print(json.dumps(summary,ensure_ascii=False))
