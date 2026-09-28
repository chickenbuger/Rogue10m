# 던전 보상 보물상자 Actor 제작 결과

## 결과

- 에셋: `/Game/World/Rewards/BP_DungeonRewardChest`
- 파일: `Content/World/Rewards/BP_DungeonRewardChest.uasset`
- 부모 클래스: `AActor`
- 용도: 던전 클리어 보상 지점에 배치하거나 스폰하는 시각·상호작용 기반 Actor

## 구성

- 목재 상자 본체와 둥근 뚜껑을 11개의 `StaticMeshComponent`로 조립했다.
- 금속 테두리, 띠, 잠금장치를 금색 및 강철 재질로 구분했다.
- `LidPivot`을 두어 이후 열림 애니메이션이나 Timeline을 연결할 수 있게 했다.
- `RewardInteractionZone`은 플레이어 상호작용 감지를 위한 `OverlapAllDynamic` 박스다.
- `RewardGlow`는 보상 오브젝트의 가독성을 높이는 낮은 반경의 따뜻한 점광원이다.
- Actor 태그는 `DungeonRewardChest`, `RewardContainer`이며 Tick은 비활성화했다.

## 배치 방법

던전 보상 위치에 `BP_DungeonRewardChest`를 직접 배치하거나 던전 완료 처리에서 스폰한다. 실제 개봉 판정, 보상 테이블 조회, 중복 수령 방지, 저장 연동은 별도 게임플레이 시스템에서 `RewardInteractionZone`과 Actor 태그를 기준으로 연결한다.

## 검증

- Unreal Editor Python API로 Blueprint를 생성·컴파일·저장했다.
- 별도 `UnrealEditor-Cmd` 프로세스에서 에셋을 재로딩했다.
- 부모 클래스, 태그, Tick 비활성화, 11개 메시, 재질, 충돌, 오버랩 영역, 조명 설정을 자동 검증했다.
- 검증 결과: `RESULT=PASSED`, 오류 0건.
- 엔진 로그의 `AdvancedPortalsSystemVFX/SM_Plane` 물리 경고는 기존 외부 에셋에서 발생했으며 이번 보물상자와 무관하다.

## 현재 범위

이번 작업은 배치 가능한 보물상자 Actor 에셋과 확장 지점까지 포함한다. 실제 뚜껑 개방, 보상 지급, 이펙트·사운드 및 세이브 데이터 연동은 후속 작업 범위다.
