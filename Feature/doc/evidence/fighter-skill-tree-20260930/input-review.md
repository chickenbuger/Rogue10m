# 격투가 스킬 키 입력 독립 소스 검토

검토일: 2026-09-30  
역할: UI 구현 담당자가 전투·입력·컨트롤러 변경을 독립 검토. 자신이 구현한 UI는 검토 대상에서 제외.

## 결론

현재 단일 로컬 플레이어와 기존 동기식 공격 GameplayAbility 경로에서, 이번 변경에 의한 중대한 실행 차단·저장 손실·GC·입력 수명주기 회귀는 발견하지 못했다. 구현 계약과 권한 범위에 부합한다. 이는 소스 검토 결과이며 빌드 또는 실제 키 입력 테스트 통과를 의미하지 않는다.

## 확인 범위와 근거

- `Rogue10mCombatComponent.{h,cpp}` 변경과 주변 공격·무기 전환 코드, 신규 `Rogue10mSkillBindingComponent.{h,cpp}` 전체, `Rogue10mPlayerController.{h,cpp}` 변경을 읽었다. 기존 `Rogue10mGameplayAbility_Attack.cpp`의 동기 실행 및 캐릭터 사망 시 `CancelCombatVisuals()` 호출도 교차 확인했다.
- 단축키 실행은 현재 트리·해금 여부·공격 노드·로컬 캐릭터를 확인하고, 죽음·메뉴·일시정지와 자원·공유 재사용 대기·콤보 조건을 통과해야 한다. 점프 슬롯은 공중, 나머지는 지상 조건을 사용한다. 차징 슬롯은 누른 시각을 기록한 후 충분히 유지하고 놓았을 때만 실행하며 직접 `ActivateTreeSkill()` 호출로 차징 조건을 우회하지 못한다.
- 테스트 전체 해금은 현재 트리만 대상으로 하고, 권한을 가진 살아 있는 로컬 캐릭터에 한정한다. Shipping에서는 거절한다. 변경은 기존 세션 해금 집합에만 반영하며 수련·토벌 수치를 수정하거나 해금 SaveGame을 작성하지 않는다.
- 키는 키보드 단일 디지털 키만 허용한다. 현재 트리 중복과 기존 하드코딩/레거시/Enhanced Input 키를 검사하며, 물리 보조키가 눌린 상태에서의 실행도 거절한다. 저장된 스킬 경로를 임의 로드하지 않고 현재 트리에 대조한다.
- SaveGame 저장 성공 후에만 현재 설정을 교체한다. 저장 실패는 기존 설정을 보존하고 오류를 반환한다. 저장 버전·항목 수·경로·키 유효성 검사를 확인했다. 프로덕션 저장 슬롯과 QA 전용 슬롯 접두사를 구분한다.
- 입력 컴포넌트 교체·소유 폰 변경·무기 전환·종료 시 보유 입력을 취소하며, 자체 핸들과 자체 MappingContext만 제거한다. 메뉴 열기·컨트롤러 일시정지는 즉시 취소하고, 생존·공중 상태 변경은 차징 감시와 놓기 시 재검사한다. 진행 수치만 바뀐 이벤트에서는 동일한 레이아웃을 재생성하지 않아 보유 입력이 불필요하게 초기화되지 않는다.
- 저장 객체·MappingContext·동적 InputAction·컨트롤러 소유 컴포넌트는 `UPROPERTY`로 추적한다. 캐시된 입력/서브시스템/전투 참조와 보유 스킬은 약한 참조다. 스킬 에셋은 기존 프로필의 강한 참조에 의해 유지된다. 동적 델리게이트는 종료 시 해제하며 콜백은 `UFUNCTION`이다.

## 실제 수행과 한계

실제 수행: 위 파일 읽기, Git 차이 검토, 호출부 검색, SHA-256 해시 계산. 게임 소스·에셋·설정은 변경하지 않았다. 이 보고서만 작성했다.

이 검토에서 UE 빌드, Editor 실행, 키 주입, 재시작 후 저장 복원, 실제 시각·사운드 검증은 수행하지 않았다. 해당 통합 검증은 별도 런타임·빌드 증거로 판정해야 한다. 네트워크 멀티플레이와 비동기 사용자 정의 공격 Ability는 이번 로컬 플레이어 요구의 검증 범위에 포함하지 않는다.

## 검토 시점 SHA-256

|파일|SHA-256|
|---|---|
|Components/Rogue10mCombatComponent.h|DFDDACDF32A31D050A62C0C023DB85A8E9AC6E6E34F8687BAB6DFF8C733C2C77|
|Components/Rogue10mCombatComponent.cpp|EE20B8BDB8EA4EE35010549D1454825B17512FCBDB3D6465A7C81F2B938CC281|
|Components/Rogue10mSkillBindingComponent.h|0CB818CDA7A66E7764321007C7B7D2A7D6700B21BD0AC67EBD734A704662E800|
|Components/Rogue10mSkillBindingComponent.cpp|85508EF826EA2560682A2C208EB8CCDBEE35A4EF7D4337FFFCD74841469D664A|
|Core/Rogue10mPlayerController.h|F1C3A5AFEC0946616FFA29F9132D713E465D61DAE4DB0CC07C38268021E4A29E|
|Core/Rogue10mPlayerController.cpp|627B8D0647B1E67E1F475D3247DC40A362C26F1D4021EA939710703712804F50|

파일 경로 접두사: `Source/Rogue10m/`. 해시 이후 변경은 별도 재검토 대상이다.

## 공개 Enhanced Input API 교체 후 추가 검토

최신 `Rogue10mSkillBindingComponent.cpp` SHA-256: `3D58A3BAA23E5A7AB645B665BFDC9944592C8BB2200F82C0855851DFBCAB8E08`.

`GetAppliedInputContextData()`의 protected 접근을 공개 `QueryMapKeyInActiveContextSet()` 및 `GetEnhancedActionMappingsView()`로 교체한 코드를 다시 읽고 UE 5.8 엔진 구현과 대조했다. `build-tested.log`의 `Result: Succeeded`를 실제 읽었다. 빌드를 직접 실행하거나 실제 게임 입력 테스트를 수행한 것은 아니다.

자체 컨텍스트가 등록되어 있을 때 질의 API는 현재 활성 컨텍스트 집합을 직접 순회한다. `BlockingIssues=NoIssue`는 문제 목록 수집을 막지 않으므로 `BlockingContext != MappingContext`를 검사하는 방식은 타 컨텍스트 충돌 판정에 사용할 수 있다. 초기 등록 이전에는 레거시 예약 키 및 현재 effective mapping view 검사가 동작한다. 컨텍스트 추가/제거 이벤트는 다음 틱 갱신을 예약한다.

### 발견: P2 — 지연 rebuild 전 이전 세대 자체 Action을 외부 충돌로 오판

근거 위치: `Rogue10mSkillBindingComponent.cpp:154`의 `!Actions.Contains(Mapping.Action)`, `:238`의 `Actions.Reset()`, `:282`의 새 Action 생성, `:294`의 레이아웃 확정.

`RemoveOwnBindings()` 후 자체 Action 배열은 즉시 비워지고 새 Action으로 교체되지만, 엔진의 `GetEnhancedActionMappingsView()`는 마지막 rebuild 결과인 `EnhancedActionMappings`를 반환한다. `FModifyContextOptions`의 기본 `bForceImmediately`는 false이고 Add/RemoveMappingContext는 rebuild를 예약한다. 따라서 같은 프레임에 설정/해제/갱신이 연속되면 이전 세대 자체 Action은 더 이상 `Actions` 배열에 없어 외부 입력으로 판정된다. 갱신이 기존 키를 `DesiredSkills`에서 제외하고 해당 레이아웃을 확정하면 저장값이 남아 있어도 다음 갱신 전까지 키 실행 연결이 빠질 수 있다. 공개 API 교체 이전 검토의 '중대한 회귀 미발견' 결론에 대한 추가 발견이다.

권고: 현재 세대 배열 포함 여부 대신 모든 자체 Action 세대를 식별해 제외한다. 이 구현은 Action을 모두 `NewObject<UInputAction>(this)`로 생성하므로 Action의 Outer가 이 컴포넌트인지 검사할 수 있다. 동일 프레임 재설정과 해제 후 재사용, 다른 기존 키 연결 보존을 실제 입력 테스트로 검증한다. 구현 담당에게 보고했으며 이 검토자는 소스를 수정하지 않았다.

### 후속 수정 확인

구현 담당자가 fallback 조건을 `Mapping.Action && Mapping.Action->GetOuter() != this`로 변경한 소스를 다시 읽었다. 따라서 현재 배열에서 제거된 이전 세대 자체 Action도 제외되며, 보고한 P2 원인은 소스상 해소되었다. 다른 컴포넌트나 에셋이 소유한 Action은 계속 충돌 검사 대상으로 남는다. 이 재검토에서 런타임은 실행하지 않았으므로 동일 프레임 재설정 검증의 실제 통과 여부는 별도 런타임 로그를 따른다.

수정 확인 SHA-256 (`Rogue10mSkillBindingComponent.cpp`): `59197C127ABE92FC3DE48A363CB7760DD1784EBCCE1BD30F8A7A4B33F5FA11A1`.

## 최종 캡처·런타임 증거 확인

`skill-tree.png`를 직접 열어 확인했다. 입문 5개, 초식 3개, 절기 3개, 심법 3개로 총 14개 노드가 표시되며 각 노드의 진행 막대와 상태 문구가 프레임 안에 들어간다. 왼쪽의 `해금 무공 14 / 14`, 테스트 전체 해금 버튼과 세션 한정 안내가 서로 겹치지 않는다. 오른쪽의 선택 기술 `왼손 잽`, 현재 키 `F7`, 키 선택기·해제 버튼·저장 결과·사용 설명·전수 조건·해금 효과도 구분되어 읽힌다. 첫 캡처에서 발견했던 좌우 본문 겹침과 노드 밖 상태 문구는 최종 캡처에서 해소되었다. 이 시각 판정은 제공된 1920×1080 캡처에 한정한다.

`fighter-final-full.log`를 실제 읽고 PASS/FAIL 줄을 집계했다. PASS 107개, FAIL 0개이며 최종 결과는 `RESULT=FIGHTER_SKILL_TREE_PASSED mode=full assertions=107 active_executed=13 failures=0`이다. `same_frame_second_binding_edit`, `new_key_executes_once`, `old_key_no_longer_executes`, `cleared_key_no_cast`, `remap_cancels_old_held_charge`의 PASS를 확인했다. 이에 따라 동적 Action 세대 충돌의 `Outer != this` 수정은 소스 확인뿐 아니라 같은 프레임 재바인딩을 포함한 실제 런타임 증거로도 검증된 것으로 판정한다. 잠긴/패시브/예약/중복 키 거절, 메뉴·무기 전환·일시정지 차징 취소, 차징 유지 후 놓기 및 13개 활성 기술의 실제 실행·몽타주 검사도 이 로그에서 PASS다.

최종 판정: 검토한 범위에서 잔여 blocker 없음. 이전 P2는 해결됨. 이번 최종 검토자는 테스트나 Editor를 새로 실행하지 않았으며, 기존 최종 캡처와 실행 로그를 독립적으로 읽고 이 보고서만 갱신했다.
