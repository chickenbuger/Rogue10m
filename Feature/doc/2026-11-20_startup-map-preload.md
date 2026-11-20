# 시작맵과 오픈 월드 사전 로딩

게임의 기본 시작 화면은 StartHub에서 열린다. 첫 게임 틱부터 OpenRunMap 패키지를 낮은 우선순위로 읽는다. 캐릭터 선택을 마치고 같은 StartHub를 게임 플레이 모드로 다시 열 때도 준비한 OpenRunMap을 유지한다. 필드에 진입하면 복귀할 StartHub 패키지를 비동기로 읽는다.

게임 인스턴스에서 다음 목적지의 초기화 전 월드를 하나까지 보관한다. Unreal이 레벨 이동 중 메모리를 정리할 때 해당 월드를 유지하고, 엔진이 그 월드를 사용한 것을 확인한 뒤 참조를 해제한다. 포털의 입장 저장, 탐험 타이머, 필드 정산과 복귀는 기존 경로를 사용한다.

World Partition 셀, 플레이어와 액터 초기화는 이동 과정에서 처리한다. 사전 로딩이 진행 중일 때도 기존 이동을 요청할 수 있다. 에디터 PIE와 서버는 기존 로드 경로를 사용한다. [Unreal Engine World Partition 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)

## 검증 방법

- 별도 작업 사본에서 UE 5.8 Development Editor를 빌드한다.
- 레벨 인자 없이 실행하고 StartHub의 메뉴 모드와 `MAP_PRELOAD queued` 및 `ready` 순서를 확인한다.
- `Scripts/VerifyCharacterLobbyStyle.ps1`의 렌더링 검사로 시작 메뉴에서 캐릭터를 선택해 게임 플레이 StartHub에 진입한다. 메뉴 재로드와 게임 진입 중 필드 패키지 요청이 중복되는지 확인한다.
- `Scripts/VerifyLegacyPortalTravel.ps1`의 Legacy3와 `Scripts/ValidateRunCompletion.ps1`의 Victory 경로로 실제 포털과 결과 복귀를 검사한다. `MAP_PRELOAD travel`의 `prewarmed=1`은 이동 전에 준비를 완료했다는 뜻이다. `MAP_PRELOAD loaded`의 `reused=1`은 엔진이 준비한 월드를 실제로 사용했다는 뜻이다.
- `-RogueNoMapPreload`를 추가한 별도 프로세스로 같은 경로를 비교한다. 각 실행의 `-AbsLog`를 `Scripts/CheckUnrealOutputLog.py`로 검사한다.

## 검증 결과

UE 5.8 Development Editor 빌드와 Win64 Shipping 대상의 서브시스템 소스 컴파일을 통과했다. 맵 인자를 생략한 렌더링 실행에서 StartHub가 메뉴 모드로 열리고, 첫 게임 틱에 OpenRunMap 준비를 시작한 뒤 12ms 후 완료했다. 포털 검증의 네 프로세스는 각각 17개 검사를 통과했다.

기본 시작 화면에서 실제 캐릭터 선택 UI로 게임 플레이 StartHub에 진입했다. 렌더링 검사 341개를 통과했고, 메뉴 재로드와 게임 입장을 거치는 동안 OpenRunMap 사전 로드 요청은 한 번만 발생했다.

| 사전 로딩 | 포털 작동부터 엔진 LoadMap 완료까지 | 이동 전 준비 / 재사용 |
| --- | --- | --- |
| 켜짐 | 3.286초, 2.662초 | 2/2, 2/2 |
| 꺼짐 | 3.339초, 4.033초 | 0/2, 0/2 |

포털 비교는 같은 작업 사본을 사용한 NullRHI 실행이며 각 프로세스는 별도 저장 슬롯을 썼다. 2회씩의 표본은 이 환경에서 관측한 결과다. 월드 파티션 셀 전체 준비와 첫 화면 표시까지의 시간은 별도로 측정하지 않았다.

렌더링 승리·정산·복귀는 77개 검사를, 새 프로세스 저장 복원은 17개 검사를 통과했다. 복귀 전에 StartHub 준비가 끝났고 복귀에서 그 월드를 실제로 재사용했다. 각 실행의 Unreal 출력 오류는 0개였다. [검증 수치](evidence/startup-map-preload-20261120/verification.json)
