# 하단 전투 HUD 시안 적용 결과

- 작업: `Sprint#5-15-compact-combat-hud`
- 날짜: 2026-10-05
- 상태: 구현·UMG 저장·빌드·실제 게임 6단계 검증·화면 검토 완료. 922개 assertion, 실패 0.
- 기준 시안: `evidence/compact-hud-20261005/approved-concept.png`
- 설계: [하단 전투 HUD 시안 적용 계획](../architect/2026-10-05_compact-combat-hud.md)
- 사용 안내: [하단 전투 HUD 구성](../../Docs/CompactCombatHUDGuide.md)

## 적용한 화면

승인된 시안의 검정·청동 하단 패널을 UMG로 구성했다. 체력/직업 자원, 왼쪽 5개 스킬, 중앙 무기 문장, 오른쪽 4개 잠금 아이템, 경험치와 레벨을 하나의 디자인 공간 안에 배치했다. 이전 별도 경험치 줄을 패널 안으로 옮기고 하단 여백을 줄였다.

| 요소 | 적용 기준 |
| --- | --- |
| 전투 코어 | 1200×196, 가운데 정렬, 하단 여백 20, 양옆 최소 24 |
| 축소 규칙 | `ScaleToFit/DownOnly`, 기존 UMG DPI 상속 |
| 경험치 | `UI_CombatCanvas` 내부, 위치 `(128, 177)`, 크기 1032×10 |
| 슬롯 | 스킬 5개·아이템 4개, 간격 20, 퀵 슬롯 72×108 |
| 차징 입력 | 72×42 영역에 `HOLD`와 마우스 입력을 두 줄로 표시 |
| 중앙 문장 | 실제 장착 무기명, 아이덴티티 원형 게이지·퍼센트 |
| 획득 카드 | 실제 아이콘·이름·수량, 카드 320×44, 최대 3개 |

원형 문장과 둥근 패널은 네이티브 UMG 브러시로 표현했다. 획득 카드가 전투 패널 옆에 들어갈 공간이 부족하면 패널 위로 이동한다. 긴 아이템 이름은 말줄임하며 큰 수량은 별도 축소 영역에 표시한다.

## 데이터 연결과 호환 동작

체력·스태미나·마나·경험치·레벨·무기·스킬 상태·획득 목록은 기존 C++ View 데이터에서 갱신한다. 지팡이 계열의 마나 표시 규칙, 자원이 없는 무기의 `--` 표시, 잠긴 아이템 슬롯, 스킬 드래그 연결과 쿨다운 동작을 유지했다.

`UI_LockGlyph`, `UI_ReadyHighlight`, `UI_WeaponNameText`와 두 줄 입력 옵션은 선택 바인딩이다. Compact 전용 파트가 해당 표시를 사용하며 기존 파트는 기존 표시를 유지할 수 있다. Main HUD는 획득 카드 전용 클래스를 사용하고 설정이 없으면 기존 `LogLineWidgetClass`로 돌아간다. 전투 로그의 기본 설정과 `L` 키 토글도 유지했다.

카드 파트에는 `UI_MessageText`도 연결했다. 아이템 View를 일반 View로 재사용할 때 이전 아이콘·이름·수량을 숨기고 메시지를 표시하는 호환 경로이며 QA에서 직접 View를 바꿔 확인한다. 기존 `GetItemAcquisitionEntries()`는 수량 0 이하의 로그를 피드에서 제외한다. 이 작업으로 경험치 획득 메시지의 피드 노출 규칙을 바꾸지는 않았다.

## 변경 범위와 재현

C++ 변경은 `Rogue10mBottomHUDWidget`, `Rogue10mHudWidgetParts`, `Rogue10mMainHUDWidget`의 표현과 배치에 한정했다. 전투 데이터, 맵, 애니메이션은 이 작업의 변경 범위에 포함하지 않았다.

Editor 적용기는 아래 패키지 7개만 명시 저장한다.

- `/Game/Widget/WBP_Rogue10mMainHUD`
- `/Game/Widget/Parts/WBP_BottomHUD`
- `/Game/Widget/Parts/Compact/WBP_CompactVitalBar`
- `/Game/Widget/Parts/Compact/WBP_CompactExperienceLine`
- `/Game/Widget/Parts/Compact/WBP_CompactQuickSlot`
- `/Game/Widget/Parts/Compact/WBP_CompactIdentity`
- `/Game/Widget/Parts/Compact/WBP_CompactAcquisitionLine`

`Scripts/Editor/ApplyCompactCombatHUD.py`가 구성하고 `Scripts/Editor/ValidateCompactCombatHUD.py`가 바인딩·부모·내부 배치·원래 Main HUD 위젯 보존을 검사한다. 새 리플렉션 프로퍼티는 정식 빌드 후 Editor를 재시작해 로드한다. 변경 전 Main/Bottom과 HUD 소스는 `evidence/compact-hud-20261005/before/`에 보관했다.

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\VerifyCompactCombatHUD.ps1 -Map '/Game/Rogue10m/Maps/StartHub'
.\Scripts\CheckGeneratedChanges.ps1
```

적용 스크립트와 검증 스크립트는 별도 역할이다. `.uasset`의 재구성·저장은 Unreal Editor 경로를 사용하며 실제 게임 검사는 별도 `UnrealEditor-Cmd.exe -game` 프로세스에서 실행한다. 런타임 명령은 플레이어 저장 데이터를 변경하지 않고 검증용 상태와 해상도를 바꾼 뒤 종료한다.

## 검증 기록

저장된 위젯과 최종 코드로 실제 게임을 실행해 아래 결과를 확인했다.

| 검증 | 상태 | 증거 |
| --- | --- | --- |
| Editor 빌드 | UI 전체 빌드 28.42초 성공. 최종 QA 코드 포함 별도 모듈 빌드 25.40초 성공 | `build-fixed.log`, `build-validation-module.log` |
| Compact UMG 적용·저장 | 패키지 7개 검증 PASS, 공식 MCP dirty=false | `apply-layout-fix.log`, `setup-validation.json` |
| 실제 게임 6단계 QA | 922개 assertion PASS, 실패 0. 현재 PNG 6개 시각·크기 검사 PASS | `runtime.log`, `runtime-contract.txt`, `verify-runtime.log` |
| 실제 화면 검토·독립 Unreal 리뷰 | 1920×1080·800×600 화면에서 HOLD 두 줄, 하단 여백, 획득 카드 비중첩 확인. 독립 C++/UMG API 리뷰 완료 | 실제 PNG와 아래 시행착오 기록 |
| 생성 파일 변경 점검 | 생성 경로 변경 없음, Harness path check PASS. AST·PowerShell parse·HUD diff 검사 PASS | `generated-changes.log` |
| 기존 UI 보존 | 기존 112개 중 Main/Bottom만 변경. 나머지 110개 SHA256 동일, 신규 Compact 5개 | `preservation.json` |

런타임 검사는 1280×720 → 1920×1080 → 2560×1440 → 1920×800 → 800×600 → 1920×1080으로 창을 바꾼다. 800×600 단계에는 ApplicationScale 1.25배를 적용한다. 실제 HUD의 화면 내 배치, 경험치의 캔버스 소속, 두 줄 입력의 필요한 높이, 잠금 기호, 공격 쿨다운, 무기명과 마나·스태미나 교체를 검사한다. 콤보 공격의 공유 쿨다운은 콤보 창이 닫힌 뒤 시작하므로 실스킬의 콤보 시간과 현재 공격속도로 관찰 시점을 계산한다.

획득 테스트는 실제 초심자의 반지·가죽 장화·가죽 갑옷 Data Asset을 PlayerController의 획득 경로로 전달한다. 카드 개수·아이콘·이름·수량·패널 겹침·시간 만료를 확인하고 한 행을 일반 View로 재사용해 잔여 아이템 표시가 남지 않는지도 검사한다. 촬영한 PNG 6개의 생성 시각과 해상도는 PowerShell 검증기가 확인한다.

증거 폴더: `Feature/doc/evidence/compact-hud-20261005/`. 캡처 이름은 `CompactCombatHUD_00_1280x720.png`부터 `CompactCombatHUD_05_1920x1080.png`까지다. 자동 검증이 성공하더라도 글자 가독성·색감·시안 유사성의 최종 평가는 실제 이미지를 보고 기록한다.

## 수정 과정과 최종 판정

첫 실제 실행에서 HOLD 두 번째 줄이 잘렸다. 키 영역을 34에서 42로 늘리고 슬롯 높이를 100에서 108로 맞췄다. 레벨도 5만큼 올려 패널 안에 정리했다. 첫 쿨다운 실패는 콤보 창이 닫히기 전에 검사한 fixture 문제였다. 실제 공격 데이터와 전투 코드는 유지하고 실제 콤보 종료 직후를 관찰하도록 고쳤다. 첫 실행 로그는 `runtime-first.log`, `runtime-contract-first.txt`에 보존했다.

Editor Python API의 TextBlock justification과 Blueprint parent 조회는 UE 5.8에 맞는 프로퍼티 setter 및 공식 BlueprintTools로 수정했다. 최초 적용은 7개 저장 이후 validator에서 멈췄으며, 최종 validator와 실제 게임 재로드에서 모두 확인했다.

다른 채팅이 같은 프로젝트의 Editor를 사용하고 있어 고정 DLL 재링크가 한 차례 잠겼다(`build-final.log`). 실행 중인 Editor를 종료하지 않고 UBT `-ModuleWithSuffix=Rogue10m,10515`로 최종 검증 코드를 빌드했다. 최종 게임 프로세스는 이 모듈을 로드하여 6단계를 통과했다. 새 리플렉션 필드는 앞선 전체 빌드와 Editor 재시작 후 이미 로드되어 있었다. 모듈 매니페스트는 UBT가 생성했으며 직접 수정하지 않았다.

최종 결과는 적용 완료다. 패키징·온라인 동기화·장시간 성능 검사는 이번 HUD 적용 범위에 포함하지 않았다. 승인된 래스터 시안을 네이티브 UMG로 구현했으므로 장식의 질감·그라데이션은 원본 이미지와 다르다. 실제 데이터와 DPI 대응을 유지하면서 하단 위치와 정보 구성을 반영했다.

작업 전에 `Sprint#5-15-compact-combat-hud`를 만들고 전환했다. 공유 체크아웃에서 다른 채팅이 이후 `Sprint#5-14-open-world-monsters`로 전환했으며, 동시 작업을 방해하지 않도록 강제로 다시 전환하지 않았다. 기존 미커밋 작업을 보존했고 커밋·푸시는 하지 않았다.
