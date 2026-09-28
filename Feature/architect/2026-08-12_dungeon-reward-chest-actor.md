# 던전 보상 보물상자 Actor 설계

## 목표

던전 클리어 보상 위치에 배치하거나 런타임으로 생성할 수 있는 보물상자 Actor Blueprint를 만든다. 이번 범위는 시각적 상자 에셋과 충돌·상호작용 준비 구조까지이며, 실제 열기 입력과 보상 지급 로직은 후속 기능에서 연결한다.

## 범위

- 생성 에셋: `/Game/World/Rewards/BP_DungeonRewardChest`
- 부모 클래스: `AActor`
- 외형: 어두운 목재 하부 상자, 둥근 뚜껑, 금속 테두리·밴드, 잠금장치
- 상호작용 준비: `RewardInteractionZone` 오버랩 컴포넌트
- 연출 준비: 뚜껑 파츠를 묶는 `LidPivot`, 따뜻한 색상의 작은 `RewardGlow`
- 제외 범위: 열기 애니메이션, 플레이어 입력, 보상 테이블, 중복 획득 방지, 저장/복제 로직

## 기반 선택

현재 프로젝트의 `ARogue10mBreakableActor`는 공격을 받아 파괴되는 벽·상자 계약을 갖는다. 던전 보상 상자는 파괴 대상이 아니므로 이를 상속하지 않고 독립 `AActor` Blueprint로 제작한다. 향후 전용 C++ 보상 상자 클래스가 추가되면 같은 SCS 컴포넌트 구성을 유지한 채 Reparent할 수 있다.

## 구성 원칙

- `.uasset`을 직접 수정하지 않고 Unreal Editor Python API로 생성한다.
- 기본 도형 메시와 프로젝트 내 목재·금속 머티리얼을 조합해 외부 신규 원본 없이 에셋을 만든다.
- 본체와 뚜껑만 월드 차단 충돌을 사용하고 장식 파츠는 충돌을 끈다.
- 상호작용 영역은 `OverlapAllDynamic`이며 메시 충돌과 분리한다.
- Tick은 비활성화하고 보상 광원은 그림자를 끈다.
- 뚜껑 관련 파츠는 `LidPivot` 아래에 배치해 후속 열기 회전의 단일 제어점을 제공한다.

## 작업 패킷

### Packet 1 — 보물상자 Blueprint 생성

- 수정 영역: `Content/World/Rewards/`, `Scripts/Editor/BuildDungeonRewardChestActor.py`
- 완료 조건: 목재 본체·둥근 뚜껑·금속 장식·잠금장치가 저장된 Actor Blueprint 생성.
- 검증 명령: UnrealEditor-Cmd 생성 스크립트.
- 롤백 경계: 신규 Blueprint와 생성 스크립트.

### Packet 2 — 상호작용·연출 준비 구조

- 수정 영역: 동일 Blueprint의 `LidPivot`, `RewardInteractionZone`, `RewardGlow`.
- 완료 조건: 뚜껑 파츠 계층, 오버랩 충돌, 그림자 없는 광원, Actor 태그 저장.
- 검증 명령: 새 UnrealEditor-Cmd 프로세스에서 에셋 재로드.
- 롤백 경계: 준비 컴포넌트만 제거.

### Packet 3 — 검증 및 문서화

- 수정 영역: `Scripts/Editor/ValidateDungeonRewardChestActor.py`, `Feature/doc/`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`.
- 완료 조건: 부모·컴포넌트·메시·머티리얼·충돌·태그·Tick·광원 검증 및 기록.
- 롤백 경계: 검증 스크립트와 이번 문서 항목.

## 완료 기준

- `/Game/World/Rewards/BP_DungeonRewardChest`가 Actor Blueprint로 로드된다.
- 목재·금속 외형 컴포넌트와 잠금장치가 재로드 후 유지된다.
- `LidPivot` 아래의 뚜껑 파츠를 회전시킬 수 있다.
- `RewardInteractionZone`이 상호작용 후보 탐색용 오버랩 영역으로 존재한다.
- Actor에 `DungeonRewardChest`, `RewardContainer` 태그가 저장된다.
- Tick 비활성화와 그림자 없는 제한 반경 광원이 유지된다.
