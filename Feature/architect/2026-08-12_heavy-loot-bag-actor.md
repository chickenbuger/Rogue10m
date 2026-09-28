# 묵직한 전리품 주머니 Actor 설계

## 목표

인벤토리에서 버린 아이템과 사냥 전리품이 공통으로 사용할 수 있는 월드 드롭용 Actor Blueprint를 만든다. 에셋은 기존 `ARogue10mDroppedItem`을 상속해 아이템 스택과 이름 표시 계약을 유지하고, 개별 아이템 메시 대신 한눈에 알아볼 수 있는 묵직한 천 주머니 실루엣을 제공한다.

## 범위

- 생성 에셋: `/Game/World/Loot/BP_HeavyLootBag`
- 부모 클래스: `ARogue10mDroppedItem`
- 외형: 눌린 구형 본체, 묶인 주머니 목, 매듭, 좌우 끈
- 머티리얼: 프로젝트의 Stylized Village 천·목재 머티리얼 재사용
- 연결 지점: `URogue10mInventoryComponent::DroppedItemClass`와 향후 몬스터 전리품 스폰 클래스
- 제외 범위: 드롭 입력 UI, 몬스터 사망 보상 로직, 획득 상호작용 로직

## 구현 원칙

- `.uasset` 바이너리를 직접 수정하지 않고 Unreal Editor Python API로 생성한다.
- 기존 `ARogue10mDroppedItem`의 `ItemStack`과 `ItemNameText`를 재사용한다.
- 아이템 Data Asset의 `DroppedWorldMesh`가 주머니 외형을 덮어쓰지 않도록 상속된 `ItemMesh`는 숨기고 충돌을 비활성화한다.
- 주머니 본체만 `OverlapAllDynamic` 충돌을 사용하며 장식 컴포넌트는 충돌을 끈다.
- Tick과 동적 조명을 추가하지 않는다.

## 작업 패킷

### Packet 1 — Actor Blueprint 생성

- 수정 영역: `Content/World/Loot/`, `Scripts/Editor/BuildHeavyLootBagActor.py`
- 완료 조건: `BP_HeavyLootBag`가 `ARogue10mDroppedItem`을 상속하고 5개 주머니 시각 컴포넌트를 가진다.
- 검증 명령: UnrealEditor-Cmd에서 생성 스크립트 실행.
- 롤백 경계: 신규 Blueprint와 생성 스크립트만 제거.

### Packet 2 — 에셋 검증

- 수정 영역: `Scripts/Editor/ValidateHeavyLootBagActor.py`
- 완료 조건: 부모 클래스, 컴포넌트, 메시, 가시성, 충돌, 머티리얼, 컴파일 상태 검증 통과.
- 검증 명령: 새 UnrealEditor-Cmd 프로세스에서 Validator 실행.
- 롤백 경계: 검증 스크립트만 제거.

### Packet 3 — 문서화

- 수정 영역: `Feature/doc/`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`
- 완료 조건: 생성 경로, 연결 방법, 검증 결과와 Notion 요약 후보 기록.
- 롤백 경계: 이번 작업에서 추가한 문서 항목만 제거.

## 완료 기준

- 에셋이 Content Browser에서 Actor Blueprint로 로드된다.
- 부모가 `ARogue10mDroppedItem`이며 기존 `InitializeDroppedItem()` 호출을 받을 수 있다.
- 주머니 본체·목·매듭·끈 2개가 저장되고 재로드 후에도 설정이 유지된다.
- 상속된 개별 아이템 메시가 숨겨져 어떤 아이템을 담아도 공통 주머니 실루엣이 유지된다.
- 인벤토리 컴포넌트의 `DroppedItemClass` 또는 몬스터 전리품 스폰 클래스에 지정할 수 있다.
