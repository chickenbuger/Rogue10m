# 현재 위치 몬스터 테스트 UI

2026-10-11 · Sprint#5-26-monster-test-ui

넓은 맵에서 몬스터를 찾아갈 필요 없이 현재 캐릭터 근처에서 실제 몬스터를 시험하는 개발 UI를 추가했다. 기존 Encounter Blueprint·Monster Data Asset·공격 프로필을 사용하며 별도 스튜디오가 필요하지 않다.

## 사용법

1. Unreal Editor에서 OpenRunMap을 열고 Play한다. 게임 화면을 클릭한 뒤 **F6**을 누른다. 우측 위에도 테스트 창 안내 버튼이 표시된다.
2. 몬스터를 선택하고 **선택한 몬스터를 내 앞에 소환**을 누른다. 아자토르가 기본 선택이며 일반 몬스터 5종도 제공한다. 최대 6마리, 약 6m 앞의 안전한 지면에 생성한다.
3. 아자토르의 **기본 공격 / 정면 파동 / 심연 영역**을 누르면 대상이 시험 거리에 배치되고 창이 닫히며 정상 속도로 공격한다. **보스 HP40% · 강화**를 먼저 누르면 강화 패턴을 시험한다.
4. 반복 시험에는 **무적: 꺼짐**을 눌러 무적을 켠다. 실제 피해를 확인할 때는 다시 해제한다. **플레이어 회복**과 **대상 초기화**로 시험을 이어갈 수 있다. 사망한 캐릭터는 Play를 재시작한다.
5. **자동 AI 전투 시작 / 정지**로 실제 AI 전투를 시험하고, **소환한 테스트 몬스터 모두 정리**로 이 창에서 생성한 대상만 제거한다. F6 또는 ESC로 창을 닫는다.

창을 여는 동안 단독 플레이는 일시정지한다. 공격/자동 전투를 실행하면 창을 닫아 이동·회피·조준 입력을 되돌린다. 소환 공간이 막혀 있으면 이동 후 다시 시도하라는 메시지를 표시한다. 가장 최근에 소환한 살아 있는 몬스터가 조작 대상이다.

작은 에디터 뷰포트에서는 F11로 뷰포트를 확대하거나 Play 메뉴의 새 에디터 창을 사용한다. 화면 가독성은 1280×720 이상을 권장한다.

## 적용 범위

- 새 `Rogue10mCombatTestComponent`: 소환·대상 소유권·패턴·회복·무적·pause 복원·정리.
- 새 `Rogue10mCombatTestWidget`: 별도 에셋 없이 C++ UMG로 생성하는 한국어 창과 버튼.
- PlayerController: 기본 컴포넌트·F6 입력·blocking/focus·기존 메뉴 전환 최소 연결.
- BasicMonster: 테스트 인스턴스의 EXP·금화·토벌 보상 차단. 일반 몬스터의 보상 규칙 유지.
- AttackPatternComponent: 표시된 테스트 대상에게만 지정 공격을 실행하는 진입점. 원본 프로필과 실제 공격 판정·애니메이션·경고 시간을 사용한다.
- 전용 `Rogue10mCombatTestUIRuntimeTest.cpp`와 `Scripts/ValidateCombatTestUI.ps1`.

테스트 몬스터 처치는 영구 성장 보상을 지급하지 않는다. 일반 스킬 사용 수련은 기존 게임 규칙을 따른다. 무적은 캐릭터 교체·맵 종료 시 원래 피해 설정으로 복원한다. Shipping 및 네트워크 플레이에는 UI나 실행 기능을 활성화하지 않는다. `.uasset`·`.umap`·원본 능력치·스튜디오 설정 변경 없음.

## 검증

검증은 실제 Development Editor DLL로 실행한 단독 게임에서 수행했다. 버튼 delegate 경로·Slate 마우스 hit-test/누름/뗌·F6 컨트롤러 입력·Slate ESC·실제 몬스터 공격을 확인한다. 검증용 캐릭터/성장/키 저장 슬롯은 사용자 슬롯과 분리한다.

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\ValidateCombatTestUI.ps1 -Label fresh_label -Width 1280 -Height 720
```

증거: `Feature/doc/evidence/monster-test-ui-20261011/`, 빌드 로그 `Feature/doc/evidence/monster-test-ui-build-*-20261011.log`.

- 초기 기능 확인: 1280×720 71확인, 실패0.
- 입력·수명·안전 경계 추가 확인: 1920×1080 80확인, 실패0.
- F6 최종 재확인: 1280×720 80확인, 실패0. 최종 빌드9.86초 성공.
- 실제 6종/10m 이내/소환 제한/세 공격 피해/강화/회복/무적/AI 새 공격/보상 차단/일반 보상 보존/기존 몬스터 보존/안전 지면 없을 때 실패/pause 소유권/캐릭터 전환 정리/버튼 화면 경계를 확인했다.
- F8은 UE의 PossessEjectPlayer와 충돌하므로 F6으로 변경했다. 엔진 Editor 명령에서 F6 단독 충돌이 없는 것을 확인했다.

초기 빌드의 UButton API·지역 변수 이름 오류를 수정했다. 첫 QA는 기존 프로필 테스트 슬롯의 허용 접두사를 잘못 사용하여 시작 조건에서 중단했으며, 이후 허용되는 `Rogue10m_Town_QA_CombatUI_` 슬롯을 사용한다. 추가 마우스 QA는 좌표 없는 키보드 WidgetPath를 마우스 경로로 전달해 엔진 assertion으로 종료했다. 엔진 소스를 검토해 실제 hit-test로 얻은 PointerPath로 수정했고 80확인이 통과했다. 실패 로그는 이력으로 보존한다.

## Reviewer

GC 대상은 UPROPERTY/TObjectPtr, 비소유 액터·컴포넌트는 Weak 참조이다. Tick을 추가하지 않았으며 버튼 callback은 UFUNCTION이다. 소환은 deferred로 보상 차단 표식을 BeginPlay 전에 지정하고 지면 경사·고도·시야·capsule overlap을 검사한다. 정리는 생성한 액터/AI만 대상으로 한다. 패턴의 테스트 진입점은 standalone/authority/표식/프로필/재질을 검사한다. 원본 Data Asset은 변경하지 않는다. UI 입력과 기존 메뉴 전환은 Controller의 공통 blocking/focus 경로를 사용한다.

## 한계

Editor Development의 단독 플레이용 도구이다. Shipping 패키징·멀티플레이·모든 맵의 배치 장소·장시간 GPU 안정성은 이번 검증 범위가 아니다. 실제 UI와 기능 구현은 완료했으며 테스트 전용 맵이나 순간이동 포탈을 추가하지 않았다. 커밋·푸시는 수행하지 않았다.

## Exit Gate

최종 F6 빌드와 실제 게임 80확인, 실패0. 실제 재시작한 Editor의 OpenRunMap PIE에서도 컴포넌트 생성·창 열기·pause/blocking·정상 닫기를 확인하고 공식 MCP로 화면을 캡처했다. 최종 OpenRunMap·PIE 종료·미저장 콘텐츠/맵 없음·새 컴포넌트 로드를 `exit-gate.json`으로 확인했다. 관련 파일 diff --check 및 CheckGeneratedChanges 종료 코드0. 과거 바이너리 변경 경고는 기존 작업의 변경이며 이번 UI 작업에서 바이너리 에셋은 수정하지 않았다. 구현·빌드·집중 QA·검토·문서 완료, 커밋·푸시 없음.
