# 고딕 UI 통합 적용 설계

## 목표

기존 HUD와 메뉴의 배치 및 C++ `BindWidget` 이름은 유지하면서 준비된 고딕 텍스처를 인벤토리, 전체 폭 경험치/레벨 바, 몬스터 정보 패널에 실제 적용한다.

## Ultrawork Packets

### 1. 인벤토리 프레임 자산

- 대상: `Content/UI/HUD/Gothic`, `Scripts/PrepareGothicIntegratedUIAssets.py`
- 완료 조건: 560x610 위젯용 2배 해상도 투명 PNG 생성
- 검증: 1120x1220 RGBA, 외곽 투명 픽셀 존재
- 롤백 경계: 신규 PNG와 Texture2D만 제거

### 2. UMG 적용

- 대상: `WBP_InventoryWindow`, `WBP_InventoryCell`, `WBP_LevelExperiencePanel`, `WBP_MonsterInfo`, `WBP_Rogue10mMainHUD`
- 완료 조건: 기존 필수 바인딩을 보존하고 신규 텍스처 연결
- 검증: Widget Blueprint 컴파일, 필수 위젯명 및 슬롯 배치 자동 검사
- 롤백 경계: 위 Widget Blueprint 자산만 이전 버전으로 복원

### 3. 편집기 시각 검증

- 대상: Unreal Editor Widget Designer 또는 PIE
- 완료 조건: 인벤토리, 경험치/레벨, 몬스터 정보 프레임이 고딕 HUD와 한 톤으로 표시
- 검증: 실제 Editor 화면 캡처
- 롤백 경계: 프로젝트 상태 변경 없이 캡처만 폐기

## 설계 결정

- 경험치 원본은 1920x353의 장식 비율을 그대로 축소하면 레벨 원형 장식이 찌그러진다. 원본에서 원형 배지와 가로 트랙을 분리한 뒤 1920x112의 실사용 컴팩트 프레임으로 재조합한다.
- 인벤토리 외곽 프레임은 560x610 논리 크기의 2배 해상도인 1120x1220으로 준비한다.
- 몬스터 프레임은 840x136 텍스처를 420x68 위젯에 2:1로 연결한다.
- 기존 런타임 로직이 참조하는 `UI_InventoryGrid`, `UI_InventoryItemCanvas`, `UI_InventoryMoneyText`, `UI_InventoryWeightText`, `UI_LevelText`, `UI_ExperienceBar`, `UI_ExperienceText`, `UI_MonsterNameText`, `UI_MonsterHealthBar`, `UI_MonsterHealthText` 이름은 변경하지 않는다.

## 검증 명령

```powershell
& "D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Project\Rogue10m\Rogue10m.uproject" -run=pythonscript -script="D:\Project\Rogue10m\Scripts\Editor\ApplyGothicIntegratedUI.py" -unattended -nop4 -nosplash -nullrhi
& "D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Project\Rogue10m\Rogue10m.uproject" -run=pythonscript -script="D:\Project\Rogue10m\Scripts\Editor\ValidateGothicIntegratedUI.py" -unattended -nop4 -nosplash -nullrhi
```
