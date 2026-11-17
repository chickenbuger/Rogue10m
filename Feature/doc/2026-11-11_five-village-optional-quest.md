# 다섯 마을 선택 회수 의뢰 구현 결과

2026-11-11 UTC. **격리 C++ 구현·오프라인 검증 완료, 공유 Source·맵 반영과 UE 검증 대기.**

V1 게시판에서 선택 의뢰를 수락하고 V3 쪽 분실 보급품을 회수한 뒤 V1에 돌아와 물약을 받는 흐름을 구현했다. 미수락 통과·수락 전 회수·재방문을 지원하며 퀘스트가 길이나 보스 입구를 잠그지 않는다. 다섯 마을의 상점·던전·보스 콘텐츠 전체 완성을 뜻하지 않는다.

## 구현 파일

신규 C++는 모두 `Feature/implementation/five-village-quests-20261111/Source/Rogue10m/` 아래 격리했다. 통합 전 현재 Source를 덮어쓰지 않는다.

| 파일 | 역할 |
| --- | --- |
| `World/Rogue10mVillageQuestModel.h` | 엔진 독립 목표 사실·수락·보상 예약/확정·초기화 상태 |
| `World/Rogue10mVillageQuestDataAsset.h/.cpp` | 의뢰 ID·설명·고유 목표 ID·아이템 보상 데이터 |
| `Components/Rogue10mVillageQuestComponent.h/.cpp` | PlayerState 소유·활성 RunId/권한·소유 Pawn 검사와 실제 인벤토리 지급 |
| `World/Rogue10mVillageQuestActor.h/.cpp` | 게시판/보급품의 거리·시야·생존·모달창 검사, F 상호작용·월드 안내 |

프로토타입 표식은 충돌 없는 엔진 Cube/TextRender다. 보상은 기존 `DA_Item_HealthPotion`1개를 참조한다. 로컬 파일 존재를 확인했으며 효과·현장 가독성·실제 수령은 UE 통합에서 확인해야 한다. Fab 신규 반입이나 현재 계정 소유권 확인을 수행한 것은 아니다.

진행 상태는 현장 액터가 아니라 PlayerState에 둔다. 동일 목표 회수/보고를 반복해도 중복 지급하지 않으며, 지급 예약 이후 실제 `TryAddGridItem` 실패는 다시 시도할 수 있게 한다. 의뢰 보상은 런 임시 인벤토리이고 새 로비 보상·SaveGame 버전은 추가하지 않는다. 기존 정산·귀환이 아이템을 처리한다.

## 현재 결과와 한계

- 엔진 독립 모델을 MSVC C++17 `/W4 /WX /permissive-`로 컴파일하고 실행했다. **69검사·실패0**,1~32목표 및2만 회 초기화·재진입·오래된 ticket 검사를 통과했다. [실제 컴파일·실행 기록](evidence/five-village-quests-20261111/portable-tests.json).
- 기존 scene 명세 기반 배치·잘못된 입력/중복/막힌 접근·기존 파일 보존 **25검사·실패0**. [배치 검사](evidence/five-village-quests-20261111/placement-tests.json), [두 액터 배치 명세](evidence/five-village-quests-20261111/placement-plan.json).
- 두 액터의 XY는 기존 비충돌 예약 표식에서 가져왔다. V1의 전체 원형 여유는0.82cm뿐이므로 검증된 마을 중심 쪽 접근점을 명세에 담았고 실제 캡슐 충돌·LOS 확인은 남겨뒀다.

최종 파일 해시·범위는 [검증 기록](evidence/five-village-quests-20261111/README.md)과 `implementation-validation.json`을 따른다. 독립 portable 검사와 API/수명/입력 리뷰를 UE UHT/C++ 전체 빌드나 실제 F 입력으로 표현하지 않는다.

기존 실제 travel `TravelSpeedD`는 정상 후보 진입·프로필/RunId/인벤토리·1200초를 확인했지만 기본 신발+20 이동속도와 구 fixture600 조건이 달라 실패했다. 해당 증거와 다른 담당자의 수정본은 보존했다. 직행/재방문·정산·귀환 성공으로 계산하지 않는다.

이번 작업은 UE/UBT/MCP와 공유 Source·설정·맵/에셋을 사용하거나 수정하지 않는다. 현재3인칭과 양수 실제 피격 시 스킬 취소를 보존하며 면역은 사용자 결정 전 보류한다. 새 의뢰 표식은 메뉴를 열거나 pause하지 않으므로 안전 메뉴 pause를 구현한 것으로도 표현하지 않는다.

## 인계

[통합 절차와 검사표](evidence/five-village-quests-20261111/README.md)에 따라 신규 Source7개를 검토·설치·빌드하고, 단독 UE 사용권에서 기존 후보에 게시판/회수 표식 두 개만 additive 배치한다. 기존 예약 표식·건물·도로를 교체하지 않는다. 현재 GameMode/관측 Component가 정확한 후보 패키지를 검사하므로 임의 새 레벨에 복제하면 같은 계약으로 동작하지 않는다.

3인칭·피격 취소·기존 무기·세이브를 보존하며 커밋·푸시하지 않는다. 실제 엔진 통합이 끝나지 않은 격리 구현이다.
