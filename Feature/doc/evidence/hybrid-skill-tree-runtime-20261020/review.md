# 혼합 성장 스킬트리 최종 소스 검토 기록

작성일: 2026-10-20. 대상: `Sprint#5-38-hybrid-skill-tree-runtime`의 격투가·대검술사 성장, 저장, 전투 연결과 K 메뉴 관련 변경.

이 문서는 정적 검토 범위와 확인한 실행 증거를 구분한다. 아래 수정 사항의 코드 검토와 HybridAuto02·HybridRun05·별도 read 실행 결과를 확인했다. 확인하지 않은 드래그 실행·장시간 플레이·패키징·멀티플레이로 통과 범위를 확대하지 않는다. 최종 결과는 [개발 결과 문서](../../2026-10-20_hybrid-skill-tree-runtime.md)에 통합한다.

## 검토 범위

- `Rogue10mSkillTreeData`, 기본53노드 카탈로그, `Rogue10mSkillTreeProgressionComponent`의 숙련·선행 AND/OR·투자·활성·환급·저장 검증.
- PlayerState·TownProgressionComponent의 실제 지출 공유와 ProgressionSaveGame/Subsystem의 v1/v2 이전, v3 복원·롤백·이벤트 저장.
- CombatComponent의 관리형 전수 검사, 실제 명중 실행별 XP, 임시 테스트 전수·장착과 정상 저장의 경계, 기존 기본 입력 복원.
- Character의 현재 무기 숙련 스탯 합산, SkillTreeCombatComponent의 아이덴티티 허용 계열·게이지·재사용 시간·MP 소비.
- K 메뉴의 구매·활성·단계 환급 요청이 백엔드 검증을 사용하는지와 안내인 거리 제한. 실제 Slate 클릭 경로와 실행한 UI 거래는 아래 증거로 확인하며, 다른 해상도와 드래그 실행은 별도 검증 대상이다.

새 애니메이션 원본 제작, Shipping 패키징, 네트워크 플레이, 다른 직업의 혼합 트리, 장기 플레이의 성장 속도 균형은 이 검토 범위에 포함하지 않는다.

## 검토 과정에서 보완한 사항

|항목|문제와 조치|현재 확인 범위|
|---|---|---|
|신규 프로필 기본 입력|비어 있는 트리 상태에서 Combat 기본 바인딩을 먼저 구성하면 관리형 기본 공격이 아직 미전수여서 누락될 수 있었다. 신규 분기를 `Tree 초기화 → 무료 기본 노드 전수 → Town 초기화 → Combat Reset` 순서로 바꿨다.|`Rogue10mProgressionSubsystem.cpp`의 신규 프로필 순서 확인. 기존 저장 바인딩을 기본값으로 덮어쓰지 않는다.|
|실제 카탈로그 DA 연결|C++ 기본 정의만 읽으면 생성한 카탈로그 에셋의 튜닝이 실행에 반영되지 않는다. 컴포넌트의 reflected soft reference가 `/Game/DataAsset/SkillTree/DA_HybridSkillTreeCatalog`를 읽도록 연결했다.|일반 실행·정적 저장 검증·legacy 이전이 같은 프로젝트 기본 카탈로그를 읽는 코드 확인. 에셋 부재 시 native bootstrap fallback을 사용한다. 저장 형식은 공용 카탈로그 하나를 기준으로 한다.|
|마나의 실제 비용|마나 피해 변형을 무상 발동하지 않도록 `ManaProcCost=5`를 추가했다. 게이지·재사용 시간·MP 조건을 통과한 발동에서 실제 MP를 소비한다.|MP 부족 시 발동하지 않고 게이지를 보존하는 분기, `ConsumeMana` 호출, 카탈로그 수치 검증 확인. HybridRun05의 전투 어댑터 검사에서도 MP 부족·실제 소비·공명 범위 변형을 확인했다.|
|공유 포인트 원장|특성과 트리에서 각각 `LV-1`을 잔액으로 쓰면 중복 지출할 수 있다. 양쪽 모두 특성·트리의 구매 당시 실제 비용을 합산한다.|중복 NodeId·음수 비용·획득량 초과 저장 거절, 공개 Restore의 실제 Town 지출 확인. 도메인 교체용 비용 인수는 persistence 전용 private 거래 API로 제한한다.|
|legacy 이전과 롤백|과거 사용 카운터에는 허공 공격이 섞여 있으므로 숙련 XP로 바꿀 수 없다. v1/v2 공격은 `LegacyGranted`로 보존하고 M1에서 시작한다.|기존 전수·특성 비용 보존, legacy 기술만 선행 예외 허용, 숙련 스탯 소급 지급 없음, 성장·인벤토리·트리·Town·Combat 복원 실패 시 이전5영역 롤백 순서 확인.|
|단계별 부분 환급|계열 전체 재분배만으로는 입문을 남기고 유료 심화만 환급할 수 없었다. `RefundNode`를 추가했다.|입문·심화·성질만 환급 시작점으로 허용. 유효 선행 OR 그룹을 모두 잃는 하위 단계·공명을 고정점까지 철회한다. 후보 전체 원장을 검증한 뒤 반영하며 무료 숙련·스탯·기술·XP는 유지한다.|
|임시 테스트 장착의 정상 저장 분리|테스트 전용 미전수 공격을 정상 입력 슬롯에 미리보기로 올리면 기존 정상 바인딩이 저장에서 사라질 수 있었다.|`DurableSkillBindings`에 정상 선택을 보관하고 Capture에서 실제 전수 공격만 저장한다. 미리보기 이동은 정상 선택을 덮어쓰지 않으며 사용자가 명시적으로 빈 슬롯을 선택하면 그 선택을 유지한다. Restore/Reset은 테스트 전수 이름을 제거한다.|
|맨손 기본 점프 공격 복원|기존 맨손 프로필의 JumpPrimary/JumpSpecial은 표시 트리 밖의 정상 기본 공격이었다. 기존 Restore가 `IsSkillInActiveTree`만 허용하여 정상 snapshot을 거절했다.|현재 활성 프로필의 **같은 입력 슬롯에 등록된 정확한 기본 공격**만 추가 허용한다. null·슬롯 enum 상한·RestoredNames·ActiveTechnique 검증은 유지한다. 타 무기나 다른 슬롯의 기본 공격을 포괄 허용하지 않는다. 관리형 공격의 실행은 트리 전수 상태를 다시 검사한다.|

부분 환급에서 입문이 남으면 활성 계열을 유지한다. 유료 심화를 환급한 뒤 숙련 대안을 만족하더라도 심화를 자동으로 다시 전수하지 않으며, 사용자가0P 경로를 확인해 선택해야 한다. 철회한 성질과 공명은 성질 재구매 전까지 복원하지 않는다. 환급량은 현재 카탈로그 비용이 아닌 실제 구매 기록에서 결정한다.

## 확인한 테스트 증거

`HybridAuto01`은 테스트 fixture 문제로 실패했다. BeginPlay 없는 격리 UWorld에서 ASC에 AttributeSet을 등록하지 않아 PlayerLevel setter가 ensure를 냈고, 중복 검증 데이터를 만들면서 같은 배열 요소를 직접 Add해 self-alias assert가 발생했다. 프로덕션 코드를 바꾸지 않고 fixture에서 `AddAttributeSetSubobject`·`InitAbilityActorInfo`를 호출하고 배열 요소를 로컬 값으로 복사한 뒤 Add하도록 수정했다.

[HybridAuto02 로그](automation-HybridAuto02.log)와 [Automation 결과 JSON](automation-report/index.json)을 확인했다. **보고서 기준3개 Success, 실패0, 경고0이며 각 테스트 errors=0**이다.

|테스트|확인한 실행 결과|검사 범위|
|---|---|---|
|`Rogue10m.HybridSkillTree.MasteryAndRewards`|Success|385XP/M12·1,045XP/M20, 순차 자동 전수, 최종 숙련 스탯, 무기 계열 격리, 복원·맨손/너클 교체 중복 방지, 숙련 상한|
|`Rogue10m.HybridSkillTree.SharedLedgerAndValidation`|Success|공유 포인트 중복 지출 거절, 실제 구매 비용과 후속 튜닝, 중복·음수·미달·알 수 없는 노드 저장 거절, 유료 심화 대안, 선택 성질·허용 계열|
|`Rogue10m.HybridSkillTree.MasteryAlternativesAndLegacy`|Success|영구 성장 출처를 요구하는 무료 심화, 대검의 공유 투지 대안, 무기 교체 효과 격리, NPC 경계 밖 환급 거절, legacy 전수와 미지급 숙련 스탯 분리|

이 Automation은 GameInstance·프로필·디스크 저장을 사용하지 않는 격리 월드 검사다. 실제 NPC 거래·피해·게이지·MP 소비·디스크 재로드·K 메뉴 조작의 통합 검증을 대신하지 않는다.

## 최종 통합 실행 결과와 한계

|실행|확인한 결과|근거|
|---|---|---|
|HybridRun05 runtime|**50 PASS / 0 FAIL**, `RESULT=HYBRID_RUNTIME_PASSED failures=0`|[runtime 로그](runtime-HybridRun05.log), [검사 결과](runtime-results.txt)|
|HybridRun05 별도 read 프로세스|**14 PASS / 0 FAIL**, `RESULT=HYBRID_READ_PASSED failures=0`|[read 로그](read-HybridRun05.log), [read 검사 결과](runtime-read-results.txt)|

두 로그에서 `RequestExitWithStatus(0, 0, ...)`와 정상 종료 기록을 확인했다. runtime의 PASS50개와 read의 PASS14개는 각각 결과 파일의 행을 집계한 값이다.

runtime은 신규 프로필의 정상 기본 입력, 임시 테스트 전수·장착의 저장 격리, 기본 점프가 포함된 정상 snapshot 복원, 실행당 명중 XP, 훈련·무효 피해 제외와 막타 인정, 특성과 트리의 실제 공용 지출, NPC 거리, 두 숙련의 분리와 실제 기술 전수를 검사했다. 전투 어댑터를 통해 마나 MP 부족·실제 MP 소비·공명 범위·전환 시 게이지/재사용 시간 계약도 확인했다.

실제 K 창과 Slate 마우스 경로를 거쳐 내공의0P 심화와 성질 투자가 실행됐으며, 기술 키 지정·잠금 기술 거절·대검 탭 열람 시 장비 미변경을 확인했다. 유료 심화 부분 환급으로 입문을 유지하고 실제 비용을 돌려받는 검사, 성질·공명 철회, 마나 심화의 포인트 전용 경로와 재구매도 runtime 결과에 포함된다.

별도 read 프로세스는 기존 QA 프로필을 정상 `AttachCharacter` 경로로 복원했다. 저장 GUID·v3, 성장·인벤토리·특성, 격투 M12·대검 M20의 정확한 XP, 트리 실제 지출8P·공용 잔액5P, 마나 확산·공명, 대검10공격·기본 입력과 격투 기술F7 키를 확인했다. 임시 게이지·재사용 시간은 새 프로세스에서 비어 있는 상태로 시작했다. 두 실행은 생산 저장과 분리한 QA 저장 경계를 사용했다.

### UI 실행 중 확인하고 수정한 사항

- HybridRun03에서 전수 버튼이 실제 마우스 hit 경로 밖으로 잘리는 문제를 확인했다. 긴 설명·조건만 스크롤하고 거래 버튼은 하단 고정 영역에 남도록 UI를 보완했다.
- HybridRun04에서는 hit 경로에 버튼이 있어도 클릭 거래가 실행되지 않았다. 테스트 helper가 hover 상태를 갱신하는 `RoutePointerMoveEvent` 없이 down/up만 보내는 것을 확인했다. 기존 특성 UI QA helper와 같은 move→down→up 순서로 보완했다.
- HybridRun05에서 `real_mouse_hit_reaches_button`, `real_UI_free_mastery_deepening`, `UI_property_spends_remaining_two_points`가 모두 PASS하여 보완한 실제 Slate 경로와 거래 실행을 확인했다. HybridRun02의 `restore_clears_test_unlocks_and_restores_normal_loadout` 실패도 최종 실행에서 PASS로 바뀌었다.

실제 HUD 슬롯으로의 **드래그 실행은 별도로 검증하지 않았다.** 기존 Drag & Drop 연결이 남아 있다는 소스 사실과 실행 통과를 구분한다. 여러 해상도의 전체 UI 회귀,20분 이상 실제 플레이, 성장 속도·전투 밸런스 평가, 패키징·Shipping·멀티플레이도 이번 통과 범위에 포함하지 않는다.

소스 검토와 위 실행 범위에서 추가 치명적 차단 사항을 발견하지 못했다.

## 별도 archive-v2 후속 통합 경계

별도 애니메이션 작업 채팅에서 진행하는 무기별 로드아웃 보관·복원의 `archive-v2`는 후속 통합 범위다. 이번 Save v3의 Combat snapshot은 현재 활성 입력 슬롯을 저장하며 무기별 archive 구현 완료를 주장하지 않는다. 후속 작업은 장비가 결정한 현재 무기·프로필, 신규 맨손 기본 전투, 관리형 전수 검사, 임시 테스트 장착과 정상 저장의 분리, 기존 기본 점프 exact-slot 복원 계약을 유지한 채 별도로 검증해야 한다.
