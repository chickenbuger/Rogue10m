# 월드맵·공유 통합 재개 첫 체크포인트

2026-11-13 UTC. **기준 Editor 빌드와 독립 WP 사본 저장 완료. 실제 외곽 축소·에디터 재로드·플레이는 아직 하지 않았다.**

## 현재 사용권과 보존

상위 담당자가 공유 Source/Config/Content·UE/MCP/통합 빌드 사용권을 이 Work에만 배정했다. 병렬 Work와 이 Work의 검토자는 격리/읽기 전용으로 제한했다. 외부 Codex의 변경은 별도 보호 대상이다. 현재 작업 브랜치 `Sprint#9-32-landscape-world-blockout`을 유지했고 checkout/reset/사용자 프로세스 종료를 하지 않았다.

최초 tracked dirty68개를 확인했다. Source/Config, 원본 WorldLayout 맵/외부 패키지/프로젝트 자산, StartHub/외부 패키지를 포함한 **2478파일**을 새 물리 백업에 복사하고 전후 SHA를 확인했다. 더 늦은 원본 관련 자동저장9개도 따로 보존했으며 원본으로 복원하거나 묵시적으로 병합하지 않았다. 프로젝트/Content 루트는 reparse가 아니며 프로젝트 `.agents/skills`는 없었다.

근거: [기준점·백업 목록](evidence/map-resume-20261113/BaselineA/checkpoint.json), [자동저장 보존](evidence/map-resume-20261113/BaselineA/autosave-recovery.json), [사용권 기록](evidence/map-resume-20261113/ownership.json).

## 실제 실행 결과

| 실행 | 실제 결과 | 미검증 |
| --- | --- | --- |
| `BuildEditor.ps1 -NoHotReload` | 정규 UHT/Editor 빌드 성공, exit0,134.95초. Source/Config339개 기존 파일 해시 변화0 | 종료 UI/전투 등 개별 기능 회귀 |
| 정식 `WorldPartitionRenameDuplicateBuilder` | `-Rename` 없이 독립 신규 맵 저장, exit0. 외부Actor1360/Object58 패키지 생성 | 에디터 재로드·층/높이/페인트 의미적 동등성 |
| 원본 보존 | 복제 전후 보호2478파일 SHA 변화0 | 더 늦은 autosave가 표현하는 변경의 복구 판단 |
| 병렬 QA 입력 | map QA20개 입력 SHA 모두 현재와 일치. Quit ARTIFACTS manifest SHA도 전달값과 일치 | 외부75/29·종료254/31 검사를 이번 Work가 재실행한 것은 아님 |

기준 DLL SHA256: `52273453B3E10F1BF63E758A828CFF7CBF9E44AADE24ADFEC1EF2AEB103213FC`.

신규 독립 사본: **`/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`**. 이름에 Compact가 들어가지만 현재는 축소 전 복제본이다. 기존 원본·허브 포털·기본맵·카탈로그·게임플레이 Source/Config를 이번에 변경하지 않았다. 후보를 기본 플레이맵/완성 콘텐츠로 전환하지 않았다.

근거: [실제 빌드](evidence/map-resume-20261113/BaselineA/baseline-build.json), [실제 복제·PID/종료/원본 해시](evidence/map-resume-20261113/DuplicateA/result.json), [병렬 입력 재확인](evidence/map-resume-20261113/BaselineA/parallel-packet-input-check.json).

## 축소안과 다음 작은 작업

독립 QA의900×562.5m는 중심(180,495.5)m에서 알려진 평면121범위를 포함하고 최소41.75m 여유가 있다. 현재20개 입력 해시도 일치한다. 이는 원래 Landscape 컴포넌트 격자와 맞지 않아 단순 삭제로 만들 수 없으며, 실제 충돌/추적·사용자 편집 보존을 인증하지 않는다. 같은 중심900×450m는 최종 보스장 pad가 북쪽으로14.5m 넘는다.

이번 보존 우선 경로는 **원래 컴포넌트 경계의9×5(1012.5×562.5m)**도 비교한다. 원본 좌표·표본·층을 재배치하지 않고 X sample315..882/Y630..945를 남기는 안이다. 유지45/외부211이며 남은 bounds는 X[-33750,67500],Y[22500,78750]cm다. 원래 ALandscape actor 원점(-90000,-90000,0)은 그대로 둔다. 좌표상 보스장 북측 여유52.5m를 포함하지만 actual actor/camera/leash 검증은 남아 있다. 크기 최종 채택은 아직 하지 않았다.

설치된 UE의 공개 LandscapeEditorUtils에는 컴포넌트 삭제 API가 없었다. private `FEdModeLandscape`를 게임 모듈에서 직접 호출하거나 `DestroyComponent`로 레이어/foliage 정리를 흉내 내지 않는다. 다음 단계용 helper2개를 **격리만** 했다.

- [기하/API 감사](../implementation/map-resume-20261113/geometry-audit/geometry-audit.md), [정확한 crop 입력](../implementation/map-resume-20261113/geometry-audit/crop-proposals.json).
- `Feature/implementation/map-resume-20261113/compact-helper/Source/Rogue10m/Editor/Rogue10mCompactWorldAuthoring.h/.cpp`.
- Source/Candidate 각각 정확한 map/project·Editor/noPIE·소유권 조건을 검사한다. 현재256/256collision·원점/scale·16proxy·정확 lattice를 확인한 뒤 false는45/211목록만 보고하며 true만 공개 `UpdateSelectedComponents`로211개를 선택한다. 삭제/저장/지형 쓰기 호출은 없다.
- 정적 API·Build 의존성·UHT 선언·수명/선택 경계의 독립 검토에서 추가 Critical/Medium 발견 없음. **실제 Source 설치/UHT/컴파일/라이브 호출은 미실행**이다.

다음 순서는 helper 신규2개만 추가→정규 빌드→단독 에디터에서 원본/후보 실시간 지형·dirty/층·현재 actors 확인→원본 변경 없이 후보 로드/선택 preview→후보의 native Landscape Manage/Delete→저장/재로드·높이/페인트/층·45컴포넌트/충돌 확인이다. UI 도구가 선택을 초기화하는지 먼저 확인한다. 기존 실패/자동저장을 무시하거나 API 없는 상태를 우회하지 않는다.

## 병렬 Work에 필요한 입력과 통합 순서

후보 경로는 위 Compact20261113A이며 아직 축소/플레이 기준으로 동결되지 않았다. 각 packet은 지원 map/package, 파일별 사전 SHA·변경 파일/패치·실제 검증과 미검증을 전달해야 한다. 과거 FiveVillage 전용 Quest guard를 새 맵 지원으로 추정하지 않는다.

확인한 현황은 공유 종료 UI7파일은 이미 설치됐고 이번 기준 빌드에서 컴파일됐으나 기능 미검증, Quest7개는 미설치, 근접 SkillV2는 이미 공유 통합돼 전체 재적용 대상이 아니다. 외부 B2의 custody7파일과 새 종료 supplement는 현재 공유와 다른 준비본이며 원본 위 전체 덮어쓰기를 하지 않는다. ArchiveE는 이전 완성 패키지이고 이번 상태의 패키지로 재사용하지 않는다.

최신 인계 위치:

- 맵 QA: `C:/Users/PC/Documents/Codex/2026-11-13/task/map-qa/REPORT.ko.md`와 `runs/hash-pinned-03/`.
- 종료/Windows: `C:/Users/PC/Documents/Codex/2026-11-12/task/QuitRegression20261113A/OWNER-RUNBOOK.md`.
- VFX: `C:/Users/PC/Documents/Codex/2026-11-11/task-2/vfx-refresh-20261113-delivery-v2/README.md`(보고받았고 공유 적용 전).

**후보/입력 동결→Supplement·필요 packet 선별 통합→최종 Editor 빌드→동일 DLL 회귀→전체 입력 seal→Game/cook/package→실제 패키지 종료/저장 재시작** 순서를 유지한다. B 전용 런처 경로만 B2로 바꾸거나 오래된 DLL로 pin 검사를 우회하지 않는다. 현재 기본4맵 cook으로 새 후보가 포함됐다고 보고하지 않는다.

3인칭·피격 취소·면역 보류·모든 콘텐츠의 사용자 편집 가능성을 유지한다. 이번에는 커밋/푸시하지 않았으며 큰 기존 WIP를 완료로 인증하지 않는다. 빌드와 복제 commandlet는 정상 종료했고 shared live editor는 아직 시작하지 않았다. 자세한 작업 패킷은 [재개 설계](../architect/2026-11-13_compact-world-resume.md)에 기록했다.
