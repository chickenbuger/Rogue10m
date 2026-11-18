# 입구 고정 대검 획득 연결

## 근거와 범위

부모의 OpenRunMap bounded 조회(1243 actor, `task/basic-play-partition-1101.json`)에서 대검 pickup/상점/퀘스트 획득 배치가 확인되지 않았다. 기존 `/Game/DataAsset/Item/Starter/DA_Item_IronLongsword`는 이름과 달리 실제 GreatSword다. 사용자 확정 고정 장소 획득을 최소 연결하며 기존 맵/원본 자산은 저장·수정하지 않는다.

## 구현

`Rogue10mOpenWorldBootstrap.h/.cpp`에 enable, 기존 대검 Item DataAsset soft reference, 조절 가능한 기준 위치 `(600,-62600,3000)`를 추가한다. 정상 OpenRunMap 게임 월드의 Standalone authority와 Running 상태에서 기존 navigation refresh timer를 재사용한다. 별도 Tick/타이머는 추가하지 않는다.

지형 streaming 완료 및 WorldStatic 수직 ground trace 성공 후에만 기존 `ARogue10mDroppedItem`을 deferred spawn하고, 기존 아이템 데이터의 분류/장비 슬롯/무기 타입/이름/설명/tint/회복값, 수량1/새 GUID/occupied를 초기화한 다음 FinishSpawning한다. 구매·새 mesh·Blueprint는 만들지 않는다. 월드 외형은 기존 드롭 메시/기존 fallback 표현을 따른다.

WorldSettings의 런타임 전용 namespaced tag `Rogue10m.EntryGreatSword.Placed`로 같은 UWorld의 배치 완료를 기록한다. 중복 bootstrap이나 pickup 이후 actor 소멸에도 같은 월드에서 재생성하지 않는다. 저장 맵에는 적용하지 않고 월드가 끝나면 자연 폐기한다. 픽업 actor에는 `Rogue10m.EntryGreatSword.Pickup` tag를 붙인다. 월드 persistent level에 배치하여 bootstrap/partition 셀 재로딩으로 인한 복제를 방지한다.

retry는 기존 registration attempt 설정의 1..600회 범위로 한정하고, 실패 끝에 warning을 한 번 기록한다. 공중 임의 스폰은 하지 않는다. 기존 pickup의 공간 부족 거부 동작을 그대로 재사용한다.

## 검증 접점

기존 신규 RunLearning runtime QA에 실제 생성 대기, world 한 개, ItemStack metadata/GUID 정합, 중복 bootstrap 이후 여전히1개, 프로그램 pickup과 인벤 GUID 보존, 같은 world 재생성 없음 검증을 추가한다. 원래 너클 장착은 변경하지 않는다. 프로그램 근접 이동/픽업 API 검증을 OS 입력이나 정상 완주로 표현하지 않는다.

Root 추가 요구인 QA 전용 저장 실패→학습 차단/불변→속성 복원→동일 프로필 재시도 허용도 같은 소유 QA 파일/스크립트에 포함한다. 파일 속성 변경은 정확히 이번 fresh QA progression 슬롯만 허용하고 Finish/스크립트 finally에서 복구한다.

소유 파일: WorldBootstrap.h/.cpp, 기존 신규 RunLearningRuntimeTest.cpp, VerifyRunLearning.ps1, 이 문서. 빌드/UE 실행/커밋은 부모가 수행한다. 소스 diff/PowerShell parse 이외에 이번 자식에서 실행 검증은 하지 않는다. rollback은 이 소스와 신규 QA 변경만 대상으로 하며 원본 맵·자산·사용자 저장·기타 변경을 reset/삭제하지 않는다.
