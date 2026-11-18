# 경로·시간 예산 검증 도구

**프로젝트에 통합된 독립 오프라인 도구입니다. 실제 맵에 경로를 구현하거나 플레이를 측정한 결과가 아닙니다.** Python 3.11 표준 라이브러리와 기존 PowerShell 정적 검증기를 사용합니다. 에디터·게임 빌드가 필요하지 않습니다.

최신 사용자 기준은 **기본 이동 직행으로 보스 입구60초 이내**, **성장하면서 보스룸 도착 약900초**입니다. 보스 처치 시간은 그 이후입니다. 모든 좌표·속도·활동 시간은 설계 가정으로 표시합니다.

## 실행

저장소 루트에서 실행합니다. 일반 Python이 있으면 `$routePython` 대신 `python`을 사용합니다. 아래 UE 번들 Python은 독립 인터프리터이며 에디터를 기동하지 않습니다.

```powershell
$routePython = 'D:\Program Files\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
& $routePython -B Tools/RouteBudget/verify.py
& $routePython -B Tools/RouteBudget/route_budget.py report
& $routePython -B Tools/RouteBudget/route_budget.py path --skip V1 V2 V3 V4 V5
& $routePython -B Tools/RouteBudget/route_budget.py scenario direct --speed 4
& $routePython -B Tools/RouteBudget/route_budget.py scenario revisit --select Q_V1_supply D_V3_quarry
& $routePython -B Tools/RouteBudget/route_budget.py scenario revisit --select
& $routePython -B Tools/RouteBudget/route_budget.py scenario revisit --extra-seconds 180
& $routePython -B Tools/RouteBudget/route_budget.py return-budget --remaining-seconds 600 --objective-seconds 180
& $routePython -B -m unittest discover -s Tools/RouteBudget -p test_route_budget.py -v
```

CLI 기본 입력은 기존 [경로 계획 JSON](../../Feature/doc/evidence/five-village-route-spec-20261110/route-plan.json)입니다. 경로를 현재 디렉터리가 아닌 도구 위치에서 계산하므로 다른 디렉터리에서도 절대 스크립트 경로로 실행할 수 있습니다. `--plan`으로 다른 입력을 명시할 수 있으며, 전역 `--plan`, `--output`은 하위 명령 앞에 둡니다. 기본 출력은 stdout입니다. `--output`은 새 파일만 생성하며 이미 있는 출력 파일은 보존하고 종료코드2로 거부합니다.

`verify.py`는 매 실행 `evidence/<UTC 실행 식별자>/`를 새로 만듭니다. 실제 입력과 정적 검증기를 그 폴더에 바이트 단위로 스냅샷하고 Python·PowerShell 양쪽이 같은 스냅샷을 읽습니다. 정적 검사199개를 매번 새로 실행하며 과거 결과를 재사용하지 않습니다. 원본 JSON·검증기·공유 보고서는 수정하지 않습니다. 로그는 새 파일 전용으로 생성합니다.

실행 폴더에는 `verification-summary.json`, `unittest.txt`, `model-report.json`, `input-manifest.json`, `existing-static-validation.json`, `static-verification.txt`와 두 입력 스냅샷이 남습니다. 실행 전후 원본 해시와 두 보고서의 바이트 재현 여부도 기록합니다. 생성 증거와 Python 캐시는 이 도구의 `.gitignore`로 분리합니다. 통합 시점 결과는 [VALIDATION.md](VALIDATION.md)에 남겼습니다.

## 실제 입력과 회귀 fixture

- CLI와 통합 검증은 프로젝트의 실제 설계 JSON을 기본으로 읽습니다.
- `inputs/route-plan.json`은 독립 도구 제작 당시의 고정 회귀 fixture입니다. 테스트의 메모리 변이와 CLI 테스트는 이 fixture를 명시적으로 사용해 다른 작업이 실제 설계를 수정해도 회귀 기준이 바뀌지 않습니다.
- `inputs/VerifyRoutePlan.ps1`, `inputs/architecture-snapshot.md`, `provenance/standalone-*`는 이전 독립 작업의 출처 기록입니다. 통합 검증에는 실제 기존 검증기의 새 스냅샷을 사용합니다. provenance의 ‘미통합’ 표기는 당시 상태이며 현재 통합 상태는 본 문서와 VALIDATION.md를 따릅니다.

기존 architect 문서는 **입구720초 + 보스180초 = 처치900초**로 작성돼 있습니다. 최신 JSON과 사용자 목표는 **입구900초**입니다. V4 활동165초·V5 던전270초도 기존 표와 다릅니다. 이 불일치는 기록으로 남겼으며 기존 문서를 덮어쓰거나 자동 수정하지 않았습니다.

## 계산 범위

| 기능 | 계산 계약 |
|---|---|
| 경로 | 기존10노드·19개 왕복 간선. 평면 간선 거리의 합과 최단경로 |
| 마을 건너뛰기 | 마을 미방문32개 조합에서 S→B 연결성 확인 |
| 재방문·우회 | 기존5개 예시 경로,90개 방향별 노드 쌍 도달 및 왕복 거리 |
| 선택 콘텐츠 | 활동 ID 선택과 추가 시간의 산술. 장소·의뢰 선후행·성장 효율은 검증하지 않음 |
| 여행 예산 | 기하 이동 + 기존 travel 여유. 속도/거리 변경 시 이동만 교체해 중복 합산 방지 |
| 왕복 판단 | 현재→의뢰 마을→현재→보스 이동 + 남은 목표 + 보고 + 보스 + 여유 |
| 시간 한도 | 가정 보스 완료가 잔여시간과 같으면0초 시간초과. 여유 보존 여부는 별도 표시 |
| 중복 방지 | 활동 ID와 선택적 `coverage_keys` 중복 거부. 원본에는 공유 키가 없어 의미상 중복은 미확정 |
| 일시정지 | 외부 정지 시간을 반사실 입력으로 넣으면 벽시계만 증가. 필드 마을 pause 구현을 입증하지 않음 |

`five_visits`, `skip_west`, `direct`는 `budget_coverage=geometry_only_lower_bound_no_content_budget`로 표시합니다. 특히 다섯 마을 방문의 이동 하한은 모든 콘텐츠 완료 시간이 아닙니다. 성장 경로의900초보다 짧은 계산 결과도 충분히 성장했다는 판정은 아닙니다.

CLI 정상 계산은 종료코드0, 잘못된 입력은2입니다. 시간 한도를 넘는 시나리오라도 계산 자체가 정상이면0을 반환하므로 JSON의 `entry_target_delta_seconds`, `can_finish_before_timeout`, `reserve_preserved`를 확인합니다. `verify.py`는 회귀·새 정적 검사·재현·입구900초 기준이 모두 맞아야0을 반환합니다.

## 적용 상태와 후속 작업

이번 적용 범위는 새 `Tools/RouteBudget/`뿐입니다. 기존 게임 소스·에셋·프로젝트 설정·공유 DevLog·스프린트 변경로그를 수정하지 않았고 브랜치 변경·커밋·푸시도 하지 않았습니다. 한국어 일지와 스프린트 기록 후보는 이 폴더 안에 별도로 제공합니다.

실제 지형·충돌·고저차·스트리밍·이동 속도·입력·퀘스트 재방문 상태·보상·안전 마을 메뉴 정지·보스 입구 및 이탈·풀런은 별도 게임 QA로 남습니다. 이 도구의 통과를 실제60초 직행/15분 도착 달성이나 게임 경로 구현 완료로 표현하지 않습니다.
