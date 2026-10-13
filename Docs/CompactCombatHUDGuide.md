# 하단 전투 HUD 구성

2026-10-14부터 실제 메인 HUD는 승인한 몬스터헌터 참고 시안에 따라 좌측 생존·성장 정보와 우측 스킬·아이덴티티로 분리한다. 현재 구성과 재현 명령은 [좌우 분리 HUD 가이드](SplitCombatHUDGuide.md)를 따른다. 아래는 2026-10-05 중앙 배치의 이전 구성과 복구용 도구 기록이다.

## 이전 중앙 배치

2026-10-05에 승인한 imagegen 시안을 게임의 UMG 위젯으로 옮긴 구성이다. 검정·청동 패널에 체력/스태미나 바, 중앙 무기명, 왼쪽 5개 스킬과 오른쪽 4개 아이템 슬롯, 경험치와 레벨을 배치했다. 원형 문장과 화면 하단 패널은 네이티브 UMG 브러시로 그린다. 기존 ApplyCompactCombatHUD.py는 분리 모드를 명시적으로 끄고 중앙 배치를 적용한다.

## 소유 경로

- Main: `/Game/Widget/WBP_Rogue10mMainHUD`
- Bottom: `/Game/Widget/Parts/WBP_BottomHUD`
- 전용 파트: `/Game/Widget/Parts/Compact/`
- C++ 표현: `Source/Rogue10m/UI/Widgets/Rogue10mBottomHUDWidget.*`, `Rogue10mHudWidgetParts.*`, `Rogue10mMainHUDWidget.*`

Widget Blueprint는 색상·프레임·폰트·내부 배치를 구성하고 C++는 실제 자원·경험치·스킬 상태·무기명·획득 데이터를 전달한다. 이전 ReferenceMetal/Gothic 공용 파트와 기존 게임 데이터는 보존한다.

## 배치 기준

디자인 코어는 1200×196, 아래 여백은 20, 양옆 최소 여백은 24다. 모두 UMG 논리 좌표이며 화면 픽셀 값은 DPI에 따라 달라진다. `UI_CombatCanvas` 안에서 체력·자원·아이콘·경험치가 같이 축소된다. `ScaleToFit/DownOnly`와 기본 DPI 상속을 유지한다.

경험치는 같은 캔버스의 `(128, 177)`, 크기 `1032×10`에 배치하고 레벨은 왼쪽 하단 `(48, 166)`에 둔다. 스킬과 아이템 슬롯의 간격은 20이다. 기본 마우스 입력은 `LMB`, `RMB`, 차징 입력은 두 줄의 `HOLD`/`LMB` 또는 `HOLD`/`RMB`, 회피는 `E`로 표시한다. 입력 글자의 영역은 72×42이며 퀵 슬롯 전체는 72×108이다.

새 잠금 기호가 연결되면 잠긴 슬롯의 아이콘과 반복 잠금 텍스트를 숨긴다. 첫 번째 기본 공격 슬롯의 준비 강조는 실제 해금·쿨다운 상태에서 결정한다. 이전 파트는 선택 바인딩과 기존 입력 표시를 그대로 사용할 수 있다.

중앙 무기명은 실제 장착 무기에서 가져온다. 자원이 있는 무기는 원형 게이지·퍼센트를 유지하며 자원이 없으면 `--`로 표시한다. 지팡이 계열에서 마나가 활성화되어 있으면 마나 바를, 그 외에는 스태미나 바를 표시한다. 아이템 4칸의 기존 잠금 상태와 스킬 연결·쿨다운 로직은 유지한다.

획득 카드는 실제 아이템 아이콘·이름·수량을 나누어 표시한다. 카드 크기는 320×44, 행 높이는 50, 최대 3개의 카드 영역은 320×144다. 긴 아이템 이름은 말줄임하고 수량은 별도 `DownOnly` 영역 안에 맞춘다. 오른쪽에 충분한 공간이 있으면 하단에서 64만큼 띄워 배치하며 공간이 부족하면 축소된 전투 패널 위로 이동해 겹침을 피한다.

카드 전용 클래스가 없으면 기존 로그 행 클래스를 사용한다. 파트의 `UI_MessageText`는 아이템 데이터가 없는 일반 View를 전달할 때 텍스트를 표시하는 호환 경로다. 아이템 획득 피드가 수량 0 이하의 로그를 건너뛰는 기존 `GetItemAcquisitionEntries()` 규칙은 유지되므로 경험치 알림을 새로 추가한 것은 아니다. 전투 로그는 기존 `L` 키 토글을 유지한다.

## 재현과 검증

C++를 정식 빌드한 후 Unreal Editor에서 `Scripts/Editor/ApplyCompactCombatHUD.py`를 실행한다. 새 리플렉션 프로퍼티를 추가한 경우 Editor를 재시작한다. 적용기는 Compact 파트 5개와 Main/Bottom, 총 7개 패키지를 명시 저장한다. `Scripts/Editor/ValidateCompactCombatHUD.py`는 저장된 구성, 위젯 부모, 네이티브 바인딩과 변경 범위를 확인한다.

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\VerifyCompactCombatHUD.ps1 -Map '/Game/Rogue10m/Maps/StartHub'
.\Scripts\CheckGeneratedChanges.ps1
```

런타임 검사는 `UnrealEditor-Cmd.exe -game`의 실제 렌더링으로 1280×720, 1920×1080, 2560×1440, 1920×800, 800×600에서 확인한 뒤 1920×1080으로 다시 늘린다. 800×600 단계는 ApplicationScale을 1.25배로 올려 축소 경로도 검사한다. 이 명령은 임시 게임 프로세스에서만 실행하며 검사 후 종료한다. 검증용 상태 변경을 플레이어 저장 데이터에 기록하지 않는다.

실제 게임 QA 결과와 캡처는 `Feature/doc/evidence/compact-hud-20261005/`, 최종 적용 결과와 한계는 `Feature/doc/2026-10-05_compact-combat-hud.md`에 기록한다. 원래 Main/Bottom 패키지와 HUD 소스의 체크포인트는 증거 폴더의 `before/`에 있다. 바이너리 위젯은 Unreal Editor를 통해 수정한다.
