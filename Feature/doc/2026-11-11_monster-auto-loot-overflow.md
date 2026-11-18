# 몬스터 보상 자동 입고와 초과 수량 드롭 구현

## 구현 결과

몬스터 처치 시 기존 피해 instigator의 현재 플레이어 캐릭터에 아이템을 먼저 입고한다. 같은 아이템·같은 강화 값의 그리드 스택을 채운 뒤 빈 그리드 공간을 사용하고, 들어가지 못한 수량만 바닥에 드롭한다. 18/20개 스택과 빈 칸이 없는 가방에 물약 5개를 주면 기존 스택은 20개가 되고 바닥에는 3개만 생긴다.

기존 TryAddItemStack과 플레이어의 직접 버리기·줍기 동작, 경험치·골드 계산, 인벤토리 저장 형식은 유지했다. 3인칭을 유지하고 1인칭·에셋·빌드 설정을 바꾸지 않았다. 공유 적용 전의 독립 구현이며 실제 UE 검증은 아직 수행하지 않았다.

## 중복·손실 방지

- 처치 처리의 기존 권한·Standalone·실제 사망·테스트 개체·런 종료·1회 가드를 유지한다. 경험치가 0이어도 아이템 입고를 수행한다. 보상 대상이 없거나 생존/권한/월드 조건을 만족하지 않으면 전량 드롭 경로를 사용한다.
- 새 TryReceiveMonsterLoot API는 유효 입력과 정상 용량 부족을 구분한다. 기존 GUID는 유지하고 입력 GUID를 가진 새 항목은 잔량 전부가 들어갈 때만 만든다. 부분 병합 후 바닥에 남는 수량은 원래 입력 GUID를 유지한다.
- 추첨 결과 전체를 인벤토리·생성 콜백 전에 월드 서브시스템의 GC 추적 보관 목록에 인계한다. 시체가 파괴돼도 보관 목록은 남는다. 지면·생성 실패 시 잔량을 버리거나 다시 추첨하지 않고 1초 간격으로 재시도한다.
- 보관 처리는 재진입을 막고 콜백 이후 receipt를 재조회한다. 바닥 액터 초기화 후 보관 소유권을 먼저 넘기고 FinishSpawning하므로 즉시 겹침 획득이 일어나도 같은 보상을 다시 만들지 않는다.
- 인벤토리가 거부한 잘못된 데이터·중복 GUID는 격리 상태로 유지한다. 이후 수령자가 없어져도 같은 거부 GUID를 바닥 아이템으로 바꾸지 않는다. 항목별·콜백 복귀 후 런 종료와 월드 teardown도 다시 검사한다.
- 보관 중 초과 전리품은 기존 미획득 바닥 아이템과 같은 월드/런 수명이다. 월드 전환·종료 이후의 미획득 아이템 저장은 추가하지 않았다. 입고된 수량은 기존 변경 이벤트와 자동 저장 경로를 사용한다.

## 변경 파일

| 영역 | 파일·내용 |
|---|---|
| 인벤토리 | Components/Rogue10mInventoryComponent.cpp/.h: 전용 입고 API와 1회 알림·획득 피드백 |
| 분배 정책 | Components/Rogue10mLootStackTransfer.h: 실제 런타임이 사용하는 독립 C++ 수량 계획 |
| 처치 연결 | Enemy/Rogue10mBasicMonster.cpp: 기존 피해 instigator의 현재 pawn을 명시적으로 전달 |
| 처치 가드 | Components/Rogue10mMonsterLootComponent.cpp/.h: 한 번 추첨 후 보관 인계, 실제 드롭 수·자동 입고 수량 구분 |
| 보관·드롭 | World/Rogue10mLootDeliverySubsystem.cpp/.h: 월드 수명 보관, 재시도, receipt 중복 방지 |
| UE 검증 | Tests/Rogue10mMonsterLootTests.cpp: 신규 AutoLoot 5개; Tests/Rogue10mMonsterLootRuntimeTest.cpp: 직접 입고 저장·재시작과 잔량 보관 검증으로 갱신 |
| 독립 검증 | Scripts/Tests/LootStackTransferTests.cpp, Scripts/TestLootStackTransfer.ps1 |
| UE 실행기 | Scripts/VerifyAutoLoot.ps1: 새 QA 저장 슬롯·실행 전 조율 가드·정확한 5개 결과 확인 |

모든 Source 경로는 `Source/Rogue10m/` 아래다. 새 UE 소스가 있으므로 통합 후 UHT 포함 Editor 빌드가 필요하다. Build.cs·모듈 의존성·프로젝트 파일 생성 설정 변경은 없다.

## 실제 수행한 검증

clang 19.1.5로 런타임과 동일한 수량 정책 헤더를 `-std=c++17 -Wall -Wextra -Werror -pedantic -O2` 옵션으로 컴파일해 실행했다.

```text
RESULT=LOOT_STACK_TRANSFER_PASSED cases=9 randomized=50000 checks=2103255
```

전체 입고, 부분 병합, 새 항목, 여러 스택, 유효하지 않은 수량, 최댓값·오버플로, 수량 보존을 검증했다. 기존 TryAddItemStack 본문은 기준 사본과 동일함을 확인했다. 별도 Sol 6.1 Ultra 검토에서 격리 상태 유지와 콜백 이후 런 종료 검사, 테스트의 빈 목록 접근·항상 참인 검사를 찾아 수정했다.

UE 실행기 구문 분석과 기본 계획 출력은 수행했다. 신규 UE 5개는 **작성만 했고 실행하지 않았다**. 순수 C++ 수량 검사 통과는 UObject·실제 월드·충돌·저장의 UE 검증을 대신하지 않는다. 렌더·실제 입력·전체 런 검증도 수행하지 않았다.

준비한 UE 회귀:

1. `Rogue10m.AutoLoot.DirectDeathAndPersistence`: 실제 처치자·0 XP·지면 없는 전체 입고·스냅샷·반복 사망.
2. `Rogue10m.AutoLoot.PartialStackDropsOnlyRemainder`: 18/20+5 → 20+바닥 3, 기존 GUID·총수량·중복 방지.
3. `Rogue10m.AutoLoot.GridMergesMetadataAndInvalidInput`: 병합 후 새 항목, 강화 불일치·보존, 중복·잘못된 입력 무변경.
4. `Rogue10m.AutoLoot.PendingOverflowOutlivesCorpse`: 지면 부재→시체 파괴→지면 복구 후 같은 GUID·수량 드롭.
5. `Rogue10m.AutoLoot.RejectedReceiptStaysQuarantined`: 중복 거부 이후 수령자가 사라져도 월드 중복 생성 금지.

임의 UObject 콜백으로 런을 종료하는 전용 회귀와 강제 SpawnActor 실패 주입은 아직 없다. 해당 분기는 코드 검토를 했으며 UE 실행 결과로 표현하지 않는다.

## 통합과 남은 검증

작업 공간: `C:/Users/PC/AppData/Local/Temp/Rogue10m-AutoLoot-20261111`

부모 담당자가 공유 소스·에디터·빌드 순서를 확보한 뒤 `monster-auto-loot-runtime.patch`를 적용한다. 이 패치는 이전 자동 획득 재시도가 이미 통합된 현재 파일을 기준으로 한다. 공동 일지와 Sprint 기록은 별도 추가문을 병합하고, 신규 문서·독립 컴파일 증거는 docs 패치로 적용한다.

```powershell
git apply --check 'C:/Users/PC/AppData/Local/Temp/Rogue10m-AutoLoot-20261111/monster-auto-loot-runtime.patch'
git apply 'C:/Users/PC/AppData/Local/Temp/Rogue10m-AutoLoot-20261111/monster-auto-loot-runtime.patch'
.\Scripts\BuildEditor.ps1
.\Scripts\TestLootStackTransfer.ps1 -RunLabel IntegratedAutoLootA
.\Scripts\VerifyAutoLoot.ps1 -Label IntegratedAutoLootA -Execute
.\Scripts\VerifyLootPickupRetry.ps1 -Label ExistingLoot1111 -FullSuite -Execute
.\Scripts\VerifyMonsterLoot.ps1 -Label AutoLootDisk1111
.\Scripts\CheckGeneratedChanges.ps1
```

기존 전리품 8개와 신규 5개를 모두 실행하고 수정된 저장 Write/Read를 재검증해야 한다. 실제 전투에서 장비·재료·물약, 가방 가득 참·부분 스택·잔량 줍기를 확인한 뒤 완료 상태로 올린다. 공유 소스·에셋·세이브·Git 상태는 이 작업에서 수정하지 않았으며 UE·빌드 프로세스·커밋·푸시를 실행하지 않았다. 최종 패치·경로·해시 결과는 작업 공간 `validation-summary.json`을 따른다.
