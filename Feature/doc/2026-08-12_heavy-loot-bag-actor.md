# 묵직한 전리품 주머니 Actor 제작 결과

## 결과

인벤토리 월드 드롭과 향후 몬스터 전리품이 공통으로 사용할 수 있는 `/Game/World/Loot/BP_HeavyLootBag` Actor Blueprint를 제작했다. 기존 `ARogue10mDroppedItem`을 상속하므로 `InitializeDroppedItem()`으로 아이템 스택과 표시 이름을 전달받을 수 있다.

## 에셋 구성

- 에셋 파일: `Content/World/Loot/BP_HeavyLootBag.uasset`
- 부모 클래스: `ARogue10mDroppedItem`
- `LootBagBody`: 눌린 구형 실루엣과 `OverlapAllDynamic` 충돌
- `LootBagNeck`: 주머니 입구를 모은 짧은 원통형 천 부분
- `LootBagKnot`: 밝은 갈색 매듭
- `LootBagTieLeft`, `LootBagTieRight`: 좌우로 처진 끈
- 이름 표시: 상속된 `ItemNameText`, 금빛 색상, World Size 15
- 성능: Tick·동적 조명 없음, 장식 컴포넌트 무충돌

## 외형 의도

넓고 낮게 눌린 본체와 위쪽으로 모인 목, 매듭과 두 개의 처진 끈으로 작은 아이템 하나가 아니라 여러 전리품이 담긴 묵직한 주머니처럼 읽히도록 구성했다. 프로젝트에 이미 포함된 Stylized Village의 어두운 천 머티리얼과 따뜻한 목재 계열 머티리얼을 재사용해 현재 스타일과 이질감이 적다.

## 사용 방법

- 인벤토리 드롭: 캐릭터 또는 인벤토리 소유 Blueprint의 `InventoryComponent > DroppedItemClass`에 `BP_HeavyLootBag`를 지정한다.
- 몬스터 전리품: 사망 보상 스폰 로직에서 `BP_HeavyLootBag` 클래스를 Spawn한 뒤 `InitializeDroppedItem()`을 호출한다.
- 개별 아이템 Data Asset의 `DroppedWorldMesh` 유무와 관계없이 공통 주머니 외형이 유지되도록 상속된 `ItemMesh`는 숨겼다.

## 검증

- UnrealEditor-Cmd 생성 스크립트: 성공, Blueprint 컴파일·저장 완료
- 새 UnrealEditor-Cmd 프로세스 재로드 Validator: `RESULT=PASSED`
- 검증 항목: 부모 클래스, 5개 컴포넌트, 메시, 머티리얼, 충돌 프로필, 상속 메시 가시성, 이름 표시 크기
- 프로젝트에 이미 존재하던 Advanced Portals `SM_Plane` 충돌 경고 외 신규 에셋 관련 오류 없음

## 제한 사항

이번 작업은 요청한 Actor `.uasset` 제작에 한정했다. 실제 인벤토리 Blueprint의 `DroppedItemClass` 지정과 아직 없는 몬스터 사망 보상 로직 연결은 별도 통합 작업이다.
