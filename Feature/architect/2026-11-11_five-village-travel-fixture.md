# 다섯 마을 실제 여행 QA fixture·실행기

상태: **격리 fixture·실행기 구현 및 오프라인83+27검사 완료, Source 설치·UE 빌드·실행 전**.2026-11-11. 구현 중 공유 Source 동결 요청을 받아 격리 경로만 작성했다. Editor 빌드 후 잠시 해제됐으나 현재 Windows 패키징 준비로 Source·에셋·설정이 다시 동결됐다. 새 Source 파일/다음 빌드의 준비 정보를 먼저 전달하라는 지시에 맞춰 설치 대상과 완성본을 인계한다. UE 사용권은 통합 담당자가 계속 보유한다.

## 범위

정상 StartHub 프로필과 실제 새 포털 overlap을 거쳐 후보 GameMode/1200초/3인칭으로 들어가고, 실제 CharacterMovement로 직행 또는 재방문 경로를 이동한다. 기존 AbandonRun 정산·저장·ReturnToTown으로 Hub 복귀까지 관측한다. 직접 OpenLevel, overlap 함수 호출, 위치 순간이동, FinishRun(true), 테스트 보스 사망으로 통과를 만들지 않는다. 보스 승리·15분 성장·물리 입력은 이 fixture의 인증 범위가 아니다.

## 변경 격리

- C++ fixture: `Feature/implementation/five-village-travel-20261111/Rogue10mFiveVillageTravelRuntimeTest.cpp`. 공유 빌드가 끝나기 전에는 Source에 설치하지 않는다.
- 실행기: `Scripts/VerifyFiveVillageTravel.ps1`. 기본은 계획 출력만 하며 명시적 실행/사용권 인계가 없으면 UE를 시작하지 않는다.
- 결과 계약/검증 helper와 오프라인 테스트는 이번 전용 Scripts 경로에만 둔다. 기존 실행기·게임플레이 소스·맵·저장은 수정하지 않는다.
- 증거: `Feature/doc/evidence/five-village-travel-20261111/`.

## 실제 실행 계약

Direct/Revisit을 각각 새 프로세스·새 QA 저장 namespace로 실행한다. Direct는 모든 마을 생략과 실제 보스 입구선 통과, Revisit은 V1→V3→V4→V1→V4→B와 V2/V5 생략을 검증한다. 전체 시간은 직접 관측한 wall/런 시계/입구 통과를 분리하며, Runtime Component의300cm 접근 반경을 입구선으로 대체하지 않는다.

실제 마을/입구/Hub 바닥에서 이동하고, 현재 Pawn·PlayerState·GameState·RunId·프로필·초기/정산/귀환 재화를 새 월드마다 다시 확인한다. 결과는 실제 milestone8개와 checks/failures를 갖는 JSON/이벤트 기록이다. 정산 시나리오는 Abandoned이며 정상 보스 승리를 검증한 것으로 표시하지 않는다.

## Ultrawork Packets

| Packet | 목표·변경 위치 | 완료·검증 | 롤백 |
| --- | --- | --- | --- |
| T0 계약 조사 | 기존 profile/portal/settlement/QA 읽기 | 공개API·fresh namespace·여행 후 재바인딩 확인 | 변경 없음 |
| T1 격리 fixture | Feature/implementation 전용 C++ | 코드/API·watchdog·정상 포털/물리 이동·정산/귀환 milestone 검토. Source/UE 미실행 | 새 격리 파일만 |
| T2 실행기 | 신규 PowerShell/helper/tests | 계획 모드·결과 판정·원본/숨김 저장 보존·소유 프로세스 timeout 격리 테스트 | 신규 코드만 |
| T3 담당자 통합 | Source에 fixture 설치→UE 빌드→단독 실행 | 정상 포털이 실제 존재하고 새 DLL에 fixture가 포함된 상태에서 Direct/Revisit 각각 종료0·milestone·저장 보존·로그 오류0 | fixture 신규 Source 파일·전용 QA 저장/증거만. 원본 저장 삭제 금지 |

## 금지와 한계

기존 실제 저장 삭제/이행·카메라 교체·1인칭 개발·새 포털의 테스트 중 임의 스폰·공유 UE/UBT 동시 실행을 하지 않는다. 실행기는 실행 직전 다른 UE/UBT 후보 프로세스가 있으면 중단하고 이 작업이 시작한 프로세스만 종료할 수 있다. PASS 문자열만으로 통과하지 않고 JSON·scenario·필수 milestone·동일 프로필·원본/예상 밖 신규 저장·엔진 오류를 함께 확인한다.

공유 Source 동결 때문에 fixture 실제 설치와 엔진 컴파일/실행은 담당자 통합 단계다. 독립 helper 테스트 결과와 실제 여행 검증을 명확히 구분한다.
