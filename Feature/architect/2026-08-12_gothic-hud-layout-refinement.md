# 고딕 전투 HUD 하단 배치 보정 설계

## 목표

- 경험치 바를 화면 최하단 전체 폭에 밀착하고 진행 색상을 초록색으로 변경한다.
- 체력과 스태미나를 각각 적색/황금색으로 명확히 구분한다.
- 기존 스킬 5칸과 아이템 5칸의 기능 바인딩을 유지하면서, 스킬 왼쪽과 아이템 오른쪽의 빈 공간을 장식 패널로 채운다.
- 중앙 메달리온을 화면 중앙에 맞추고 좌우 HUD를 대칭으로 정돈한다.

## Ultrawork Packets

### 1. 하단 날개 텍스처 제작

- 대상: `Content/UI/HUD/Gothic`
- 입력: 기존 고딕 HUD 프레임 및 슬롯 프레임
- 완료 조건: 좌측 스킬 패널과 우측 아이템 패널용 투명 PNG 2종 생성
- 검증: 알파 채널, 투명 모서리, 5개 슬롯 개구부 확인
- 롤백 경계: 신규 PNG 및 Texture2D만 제거

### 2. UMG 배치 및 색상 보정

- 대상: `WBP_Rogue10mMainHUD`, `WBP_SkillSlotPanel`, `WBP_ItemSlotPanel`, `WBP_LevelExperiencePanel`
- 완료 조건: 경험치 바 하단 밀착/초록색, 체력 적색, 스태미나 황금색, 좌우 패널 대칭 배치
- 검증: 필수 BindWidget 이름 보존 및 Widget Blueprint 컴파일
- 롤백 경계: 위 4개 Widget Blueprint와 신규 Texture2D

### 3. 시각 및 자동 검증

- 대상: Unreal Editor Designer, `Saved/GothicIntegratedUIValidation.txt`
- 완료 조건: 자동 검증 통과 및 1920x1080 Editor 캡처 생성
- 검증: 검증 스크립트와 실제 Editor 화면 캡처
- 롤백 경계: 검증 결과/캡처 파일만 제거

## 설계 결정

- 슬롯의 기능 위젯 수와 이름은 변경하지 않고 배경 날개만 확장한다.
- 체력/스태미나는 런타임 데이터 색상과 동일하게 각각 적색/황금색 프리뷰를 제공한다.
- 경험치 프레임은 1920x52의 얇은 전용 텍스처로 재구성해 위젯 자체가 뷰포트 최하단 경계에 닿도록 한다.
- 좌측 스킬 날개와 우측 아이템 날개는 각각 480x104 논리 영역을 사용하며 중앙 메달리온 아래에서 대칭으로 연결한다.

## 검증 명령

```powershell
& "D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Project\Rogue10m\Rogue10m.uproject" -run=pythonscript -script="D:\Project\Rogue10m\Scripts\Editor\ApplyGothicIntegratedUI.py" -unattended -nop4 -nosplash -nullrhi
& "D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Project\Rogue10m\Rogue10m.uproject" -run=pythonscript -script="D:\Project\Rogue10m\Scripts\Editor\ValidateGothicIntegratedUI.py" -unattended -nop4 -nosplash -nullrhi
```
