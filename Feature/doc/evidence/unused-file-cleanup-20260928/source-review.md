# 독립 후보·정리 도구 검토

작성: 2026-09-28 / 검증1. 파일 삭제는 수행하지 않았다.

## 최종 후보 판정

자동 후보 137개를 재검토했다. `reviewed_candidates.json`의 `files`가 최종 파일별 판단이다.
- 중복 삭제 가능: 50개, 6,642,409바이트. 검토 시 원본과 보존 사본의 SHA-256을 모두 다시 확인했다.
- 보존: 87개. 실행 코드·동적 입력·불확실한 분석 자료를 포함한다.
- 별도 referenced_duplicates 20개는 기존 감사에서 참조가 확인된 항목이므로 전부 보존한다.
- Source/Scripts/Config 249개 정적 감사에서 삭제 가능한 파일을 입증하지 못했으므로 보존한다. Content는 동적/바이너리 참조를 텍스트만으로 입증할 수 없어 보존한다.

## 자동 검색이 놓친 입력

1. appearance-head/finalize.py:27-28은 basename 목록과 디렉터리를 합쳐 build/runtime/assets/geometry 로그 등을 shutil.copy2 입력으로 읽는다.
2. third-person-inspection/finalize.py:32-33은 같은 방식으로 build/runtime/final/verified/path-check 로그를 읽는다.
3. head-framing/finalize-prep.py:4-5는 build/initial/generated/diff 로그 등을 읽는다.
4. boxing-reference/make-gameplay.py:9는 `{style}.log`로 boxing.log/rooted.log/longguard.log를 읽는다.
5. effect-animation-reference/contact.py:4는 frames.glob(frame_*.jpg)를 읽으므로 frame_09.jpg와 frame_16.jpg는 중복 사본이 있어도 입력으로 보존한다.
6. 여러 run.ps1은 `$Label + '.log'`를 읽는다. 과거 산출물의 필수성은 호출별로 다르지만 이번 정리는 보수적으로 해당 폴더의 후보 .log 전체를 보존했다. arms-only/boxing-combos 배열 Log 입력도 보존했다.

## 실행·소스 파일 보존 목록

동일 사본이 있어도 수동 실행 경로 호환을 위해 아래 14개를 보존한다.

- `tmp/boxing-reference/build.cmd`
- `tmp/head-framing/run.ps1`
- `tmp/relaxed-idle/run.ps1`
- `tmp/boxing-direct/run.ps1`
- `tmp/appearance-head/run.ps1`
- `tmp/third-person-inspection/run.ps1`
- `tmp/appearance-head/encode.py`
- `tmp/head-framing/encode.ps1`
- `tmp/third-person-inspection/encode.py`
- `tmp/boxing-direct/make-preview.py`
- `tmp/boxing-direct/make-lookdown.py`
- `tmp/boxing-direct/make-lookdown40.py`
- `tmp/boxing-reference/inspect.cpp`
- `tmp/relaxed-idle/make-preview.py`

## Tier B 계획 보완

계획 문서에 별도 Tier B를 추가했다. 현재 원본과 생성 코드로 전 프레임을 실제 재생성하고 기존 PNG와 픽셀·크기·모드가 같으며 최종 MP4도 검증한 labeled 폴더만 대상으로 한다. 원본/스크립트/로그/폰트/최종 영상은 보존한다. PNG 바이트 압축이나 메타데이터의 동일 복원과 픽셀 동일 재생성을 구분한다. 검증2 최종 통과 목록 밖 폴더는 삭제하지 않는다.
원본 Saved 프레임은 읽기만 하며 수정·삭제하지 않는다. Tier B 실제 삭제 목록은 아직 이 검토 문서에 포함하지 않았으며 root가 검증2 자료와 합친 manifest를 따로 검증한다.

## apply-cleanup.ps1 검토

확인: 프로젝트 절대 루트 고정, tmp 삭제 경계, 파일만 허용, ancestor reparse 거부, Git tracked 거부, backup/before/decoder/tool 경로 거부, 허용 kind 검사, 전수 원본 크기·SHA 확인, retained dependency 해시 확인, 삭제·보존 집합 중복 거부, Preview 기본값, 기존 receipt 거부, 삭제 직전 재해시, 단일 PowerShell Remove-Item -LiteralPath, 개별 삭제 receipt 및 삭제 후 보존 해시 확인.

보강 권고: duplicate의 keep 존재뿐 아니라 original entry.sha256과 해당 retained dependency.sha256이 같은지 도구에서도 비교하도록 root에 알렸다. 이번 최종 50개 후보의 쌍 해시는 독립 재확인에서 일치했다. 현재 스크립트 검사만으로 잘못 조립한 manifest의 서로 다른 파일 쌍까지 막는다고 주장하지 않는다.

실제 삭제 전 root가 canonical path·활성 프로세스 사용·최종 manifest·의존성 중복을 다시 검증하고 Preview를 실행한다. 실행 후 삭제 개수/바이트·보존 해시·복구 정보를 결과 문서에서 확정한다.

## 최종 매니페스트 독립 검사

2026-09-28, 실제 삭제 전 root 요청으로 `selected-cleanup.json`을 읽기 전용 검사했다. 결과 PASS, 오류 0.

- 삭제 대상 2,831개, canonical absolute path 기준 유일 대상 2,831개. 모두 tmp 경계 안이며 금지 영역 대상 0.
- 보존 의존성 2,858개와 삭제 집합 교집합 0.
- Tier A 50개는 reviewed_candidates.json의 delete_duplicate 승인 항목과 원본 경로·keep·SHA가 일치한다. keep은 보존 의존성에 등록되어 있고 원본 SHA와 같다.
- Tier B 2,781개는 검증2 재생성 자료의 전 프레임 pixel hash 일치 및 final video 존재 폴더에 정확히 대응한다. 파일별 source/generator/required_log/final_video가 모두 보존 의존성에 등록되어 있다.
- 총 바이트는 2,553,045,782로 매니페스트와 일치한다.
- apply-cleanup.ps1에 권고한 duplicate 쌍 SHA 비교가 추가된 것을 확인했다.
- root의 Preview 결과 `Verified 2831 files, 2,553,045,782 bytes, 2858 retained dependencies. Preview only. No files removed.`를 확인했다. 이번 독립 검토는 구조·집합·증거 대응 검사이며, 전체 바이트 재해시는 root Preview 결과를 사용했다.

검토 담당은 파일 삭제를 실행하지 않았다. 실제 삭제 완료 여부와 후검증은 root 실행 receipt 및 결과 문서에서 확정한다.