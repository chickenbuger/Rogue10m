# 다섯 마을 실제 여행 fixture·실행기 구현 결과

2026-11-11 / **격리 구현·오프라인 검사 완료, Source 설치·UE 컴파일·실제 여행 미실행**.

정상 프로필과 실제 포털 overlap을 통한 여행,1200초·3인칭·실제 도보, 직행/재방문, 포기 정산·저장·Hub 귀환을 확인할 C++ fixture와 전용 실행기를 구현했다. 공유 Editor 빌드 중 Source 동결 지시를 받아 fixture는 격리 사본으로만 작성했다. 현재 Windows 패키징 준비로 Source·에셋·설정이 다시 동결됐으며, 새 Source 파일과 다음 빌드의 준비 상태를 먼저 전달하라는 지시에 맞춰 설치 전 인계한다. 에디터/UE/UBT는 실행·접속하지 않았다.

## 변경 파일

- `Feature/implementation/five-village-travel-20261111/Rogue10mFiveVillageTravelRuntimeTest.cpp`: 신규 Source 설치 대상1개. CoreTicker가 월드 여행을 넘어 유지되며 프로필·Pawn·PlayerState·GameState를 다시 바인딩한다.
- `Scripts/VerifyFiveVillageTravel.ps1`: 기본 계획 모드, 명시적 사용권/설치본/새 DLL 검사 후에만 실제 실행. 서로 다른 두 QA namespace·소유 프로세스 timeout·원본 저장 보존·결과 종합 판정.
- `Scripts/FiveVillageTravelContract.ps1`: 결과 schema·milestone·GUID·시간·방문·재화·저장 변경 판정.
- `Scripts/Tests/TestFiveVillageTravelContract20261111.ps1`, `TestFiveVillageTravelPlan20261111.ps1`: 임시 파일과 계획 모드만 사용한 오프라인 검증.

실제 설치 대상은 `Source/Rogue10m/Tests/Rogue10mFiveVillageTravelRuntimeTest.cpp`다. 이번 패킷에서 이 Source 파일을 추가하지 않았다. 기존 Source·맵·에셋·세이브·카메라·빌드 설정도 수정하지 않았다.

## 검증된 것

- 합성 JSON·임시/숨김 저장·namespace 계약 **83검사·실패0**.
- 실제 실행기 기본 모드·사용권 guard·별도 case/namespace·명령행·무실행 **27검사·실패0, 프로세스 시도0**. 패키징된 게임 EXE는 실행 전에 거부한다.
- PowerShell AST와 독립 Source API/수명·여행/정산 계약 검토. 빈 원본 저장 목록 직렬화와 singleton 배열 자동 해제 문제를 수정하고 음성 대조군을 추가했다.
- fixture에는 S 시작 위치, 위치 점프·속도 변조 거부, Direct 필드29500cm 이상 보행과 실제 입구선 통과를 구현했다. 이 코드는 아직 엔진에서 실행하지 않았으므로 실제60초 통과 결과는 아니다.

정산은 AbandonRun의 실제 저장·receipt·baseline·ReturnToTown 경로를 대상으로 한다. 보스 승리, 비영 재화 획득/지출 rollback,15분 콘텐츠, 물리 입력·전체 런은 인증하지 않는다. 첫 월드 여행에서는 인벤토리 instance identity를 유지하지만 귀환 Hub의 새 starter GUID까지 같도록 요구하지 않는다.

## 담당자 통합

통합 담당자의 [입구 활성화 기록](evidence/five-village-runtime-20261111/entry-activation-20261111T111907248692Z.json)에서 런 연결 클래스 빌드와 새 Hub 포털 저장·재로드를 확인했다. 이 기록의 정상 여행은 `not_tested`이며, 이번 fixture는 해당 DLL에 아직 포함되지 않았다. 추가 Source 파일1개를 검토/설치하고 새 DLL을 빌드한 뒤 단독 UE 사용권에서 실행한다. 정상 진입·시계·방문/재방문·정산·저장·귀환의 실제 결과가 있어야 통과다. [실행/설치 절차](evidence/five-village-travel-20261111/README.md), [계약 검사](evidence/five-village-travel-20261111/offline-contract-tests.json), [계획 검사](evidence/five-village-travel-20261111/offline-plan-tests.json), [준비 상태와 파일 해시](evidence/five-village-travel-20261111/preparation-validation.json)를 참고한다.

현재 남은 의존성은 Source 설치·통합 빌드와 공유 UE 사용권이다. 승인 거부나 결과 불명인 엔진 동작은 없다. Source·UE 미실행은 상위 담당자의 충돌 방지 지시에 따른 것이며 커밋·푸시하지 않았다.
