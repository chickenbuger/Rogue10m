# Widget Blueprint HUD 작업 가이드

## 현재 구조

Rogue10m의 화면 UI는 Canvas HUD를 사용하지 않고 UMG Widget Blueprint로만 구성한다.

- 생성 담당: ARogue10mPlayerController::InitializeRunHUD
- 메인 C++ 부모: URogue10mMainHUDWidget
- View 데이터 부모: URogue10mRunHUD
- 기본 Widget Blueprint: Content/Widget/WBP_Rogue10mMainHUD
- 파트 Widget Blueprint: Content/Widget/Parts
- 패널/설정/로그 상태 소유자: ARogue10mPlayerController
- 체력과 자원 데이터: ARogue10mPlayerState가 소유한 URogue10mAttributeSet

화면 UI는 플레이어 컨트롤러가 생성하는 `WBP_Rogue10mMainHUD`와 하위 위젯이 관리한다. 게임 모드는 엔진의 기본 `AHUD`를 사용한다.

## 메인 Widget Blueprint

WBP_Rogue10mMainHUD의 부모는 Rogue10mMainHUDWidget이어야 한다.

자동 바인딩이 필요한 파트 이름:

- HealthBarWidget
- StaminaBarWidget
- ManaBarWidget
- ProgressionWidget
- IdentityWidget
- MonsterInfoWidget

동적 자식이 들어갈 컨테이너 이름:

- SkillSlotContainer
- ItemSlotContainer
- SystemLogContainer
- ItemAcquisitionContainer
- MinimapMarkerContainer

BindWidgetOptional을 사용하므로 일부 항목이 없어도 C++ 로드는 가능하다. 자동 연결이 필요하면 이름과 부모 C++ 클래스가 정확해야 한다.

## 파트 부모 클래스

| Widget Blueprint | C++ 부모 | 역할 |
| --- | --- | --- |
| WBP_VitalBar | Rogue10mVitalBarWidget | 체력, 스테미나, 선택 자원 |
| WBP_Progression | Rogue10mProgressionWidget | 레벨과 경험치 |
| WBP_Identity | Rogue10mIdentityWidget | 무기 숙련도와 아이덴티티 |
| WBP_MonsterInfo | Rogue10mMonsterInfoWidget | 조준 중인 몬스터 정보 |
| WBP_QuickSlot | Rogue10mQuickSlotWidget | 스킬/아이템 슬롯 |
| WBP_LogLine | Rogue10mLogLineWidget | 시스템 로그 한 줄 |
| WBP_MinimapMarker | Rogue10mMinimapMarkerWidget | 미니맵 마커 |

각 파트는 Set...View 호출 뒤 발생하는 BP_On...Changed 이벤트에서 Text, ProgressBar, Image를 갱신한다.

## 수치 바

- 체력: GetHealthView, 빨간색
- 스테미나: GetStaminaView, 파란색
- 마나: GetManaView, bVisible이 true일 때만 표시
- 아이덴티티: GetIdentityView, 회색 기본 테두리

모든 바는 Current, Max, Normalized, ValueText, PercentText를 제공한다.

## 동적 스킬 슬롯

GetSkillQuickSlotViews는 현재 무기에 바인딩되고 해금된 공격 Data Asset만 반환한다.

- 기본 최소 슬롯은 1개다.
- 무기 변경 또는 스킬 해금으로 배열 수가 바뀌면 SkillSlotContainer의 자식을 다시 생성한다.
- QuickSlotWidgetClass에는 WBP_QuickSlot을 지정한다.
- 콤보는 공유 쿨타임을 사용하며 다음 콤보 대기 중에는 표시 아이콘이 다음 공격으로 바뀐다.

## 패널 상태

I, B, K, O/Escape/F10 입력은 PlayerController의 패널 상태를 변경한다.

GetPanelStateView에서 다음 값을 읽을 수 있다.

- bEquipmentVisible
- bItemWindowVisible
- bSkillTreeVisible
- bSettingsVisible

패널이 하나라도 열리면 마우스 커서와 GameAndUI 입력 모드가 활성화되고 캐릭터 이동과 공격이 차단된다. 실제 패널 디자인과 표시 애니메이션은 Widget Blueprint에서 구현한다.

## 로그와 획득 알림

- 시스템 로그: GetSystemLogEntries
- 획득 알림: GetItemAcquisitionEntries
- 각 항목: Message, Color, RemainingSeconds
- 최신 항목이 배열 앞에 위치한다.

LogLineWidgetClass에 로그 파트 Blueprint를 지정하면 컨테이너 자식이 자동 관리된다.

## 몬스터 정보와 피해 피드백

- GetLookedAtMonsterInfoView: 카메라 중앙 선상 몬스터의 이름, 레벨, 속성, 상태, 체력
- GetDamageFeedbackView: 플레이어 피격 오버레이의 강도와 투명도
- GetAimCrossLineView: 패널이 닫혀 있고 캐릭터가 살아 있을 때 조준선 표시

## 점검 순서

1. WBP_Rogue10mMainHUD 부모가 Rogue10mMainHUDWidget인지 확인한다.
2. 자동 바인딩 이름과 파트 부모 클래스를 확인한다.
3. QuickSlotWidgetClass, LogLineWidgetClass, MinimapMarkerWidgetClass를 지정한다.
4. Compile과 Save를 실행한다.
5. PIE에서 체력/스테미나, 공격 슬롯, 몬스터 정보, 패널 입력 모드를 확인한다.
