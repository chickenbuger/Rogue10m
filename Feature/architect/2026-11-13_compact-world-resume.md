# 월드맵·공유 통합 재개 패킷

2026-11-13 UTC. 사용자 개발 재개로 중단 지시는 해제됐다. 상위 담당자가 이 Work에만 공유 Source/Config/Content·UE/MCP/통합 빌드 사용권을 배정했다. 다른 외부 Codex의 변경은 계속 보호한다. 두 검토자를 명시적 GPT-6.1-Sol/ultra로 실행했으며 Fast 인자는 제공되지 않아 적용을 주장하지 않는다.

## 현재 Scope Gate

- 작업 브랜치 `Sprint#9-32-landscape-world-blockout` 유지. 파괴적 checkout/reset/프로세스 종료를 하지 않는다.
- 최초 관측 tracked dirty68개. 기존 WIP는 이번 완료/커밋에 묶지 않는다. 프로젝트/Content root는 reparse가 아닌 물리 디렉터리다.
- 최초 관측 UE/UBT/dotnet 프로세스0. 이것만으로 외부 Codex의 향후 사용을 막을 수 없으므로 변경 직전 재검사와 해시를 사용한다.
- 프로젝트 `.agents/skills`, `.agents`, `.codex`는 현재 없다. 설치된 Unreal MCP skill과 local project harness를 읽었다. 현재 도구 카탈로그에는 Unreal MCP 도구가 없어 기존 `Scripts/InvokeUnrealMCP.py`와 설정된 loopback8001 연결을 우선 확인한다.
- 이전1.8km 저장본/보행 증거와900×562.5m 산술 후보를 구분한다. 현재 DLL은2026-11-11 17:32Z이며 종료 UI Source가 그 이후 설치돼 통합 정규 빌드가 필요하다.

## 유지하는 계약

최신 WorldMapProductionGuide/두 카탈로그/콘셉트 PNG/마비노기 비교 기획을 기준으로3큰거점+5전문초소 안의 H/M/Q/T/F 다섯 기능·29양방향길·300m 중앙 직행·선택/재방문을 유지한다.15일반/4중간/1최종을 구분한다.3인칭·피격 취소·면역 보류·책 탐험 발견·무기별 입력과 Ctrl→첫Q기한→QQQ 자동발사·런/영구 성장 정책은 변경하지 않는다. 지형/마을/무기·기술·모션/VFX는 사용자 편집 가능성을 보존한다.

## Ultrawork Packets

| Packet | 목표/수정 범위 | 완료 조건/검증 | 되돌릴 경계 |
| --- | --- | --- | --- |
| R0 소유·기준점 | 현재 변경/설정/Source/원본 맵과 외부 패키지 해시·백업, 단독 사용권 기록 | 새 전용 evidence manifest, 원본 지문, 다른 프로세스 미조작 | 신규 evidence/backup만 |
| R1 통합 기준 빌드 | 현재 공유 WIP의 정규 Editor 빌드 | 실행/종료와 actual compile result, 전후 Source 해시 확인 | 생성된 빌드 결과, 기존 WIP 임의수정 금지 |
| R2 실시간 원본 확인 | 현재 saved source map·Landscape/layers/proxies/actors/dirty/PIE 읽기 | MCP live project/world identity, 현재 컴포넌트·재질/레이어·다중 container 상태 기록 | 원본 수정/저장 없음 |
| R3 독립 사본 | 정식 WP duplicate builder 또는 공개 Editor API로 새 후보 | 새 대상/외부폴더 부재 확인, 원본 불변, independent external packages 저장·재로드 | 새 후보 패키지만 |
| R4 범위 축소 | 독립 후보의 외곽만 축소. 사용자 현재 높이/페인트/레이어/액터를 보존할 공개 API 선택 | 중앙/거점/도로/전투장/경계 여유·충돌·bounds 확인 | 새 후보 지형/환경만 |
| R5 작은 플레이 QA | 후보 직행·마을/초소 한 묶음과 편집/Undo/저장 | 실제 보행/시야 캡처·접지/시계·사용자 편집/재로드 | 후보 전용 변경 |
| R6 공유 통합 | 격리 NPC/퀘스트·G08·VFX·Quit/Windows packets 중 준비된 것만 검토 | 사전해시/백업·정규 빌드·기능별 실검증 후 동일체크포인트 패키징 | 파일별 scoped rollback, 다른WIP 보존 |

R0/R1 또는 live 원본 확인까지의 작은 결과를 먼저 보고한다. 긴 전체 제작을 완료했다고 표현하지 않는다. 같은 실패2회면 API/원인 재설계로 돌아간다.

## 축소 크기와 API 결정

900×562.5m는 검토안이다. 원본 컴포넌트 경계의8×5 crop는 서쪽 중간보스 pad 여유가 약7m로 좁다는 감사 결과가 있다.9×5(1012.5×562.5m) 사본이면 기존 샘플/좌표를 유지하면서 원본 X sample315..882, Y630..945 경계로 더 넓은 여유를 확보한다. 이는 기하 제안이며 실제 current actor/충돌/추적 범위로 확정한다.

`WorldPartitionRenameDuplicateBuilder`는 기본 duplicate이며 `-Rename`은 사용하지 않는다. 기존 대상이면 삭제 경로가 있으므로 대상 map/external package 부재를 먼저 하드가드한다. 다중 actor container/LevelInstance/PCG가 있으면 누락 여부를 먼저 조사한다.

설치된 LandscapeEditorUtils에는 공개 DeleteComponents가 없다. private FEdModeLandscape API를 공개 기능으로 가장하거나 원본에서 컴포넌트를 임의 파괴하지 않는다. 공식 MCP/공개 Editor 경로를 조사하고, 필요 시 current composite height·paint export→새 editable Landscape import를 별도 검증한다. 과거 R16만으로 현재 사용자 편집이 보존됐다고 주장하지 않는다. 레이어 보존을 확인하지 못하면 축소를 멈추고 원본 보존 사본/차단 원인을 보고한다.

## 병렬 작업 경계

상위 담당자의 NPC/퀘스트, G08, VFX, Quit/Windows Work 및 이 Work의 기하/통합 검토자에게는 공유 수정/UE/UBT/MCP/Cook/게임 실행·종료 권한이 없다. 이 Work가 빌드·에디터를 직렬로 운용한다. 병렬 입력은 원본 경로+SHA, 지원 맵·클래스/API, 패치/실행기·실제 검증과 미검증, 충돌 파일 목록이다. 루트는 먼저 파일을 다시 비교하고 필요한 변경만 통합한다.


## 2026-11-13 체크포인트 C 범위 조정

사용자 지시에 따라 Computer Use와 동일한 GUI입력 API 우회를 중단했다. R4 native Manage/Delete는 보류하며 public Import도 layerGUID/brush/rawBA/외부참조 보존 증거 전 적용하지 않는다. R2 데이터확인과 R3 후보 보존은 실제 NullRHI commandlet로 확인했다. R6의 G08+VFX29/NPC30 소스를 사전해시·백업 후 통합하고 UHT 배열 Units2곳만 호환 수정하여 빌드/43native/30오프라인을 통과했다. 실제 NPC배치·에셋 활성화·물리플레이·Windows gate는 미완료다. 후속 비GUI packet은 raw B/A와 brush/PCG/spline/foliage/참조 인벤토리 및 명시적 매핑 설계이며, rollback 경계는 새 helper/새 후보뿐이고 사용자 원본과 기존 봉인은 유지한다. 자세한 실제/미완 범위는 Feature/doc/2026-11-13_map-integration-checkpoint-c.md.
