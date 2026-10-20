# 특성 16종·강화 UI 적용 결과

2026-10-11, 설계한 특성 16종을 네이티브 C++ 전투·자원·보상 처리에 연결하고 아린에게 전용 강화창을 추가했다. 인벤토리·스킬·성장의 기존 화면과 상태 영역은 각각 유지한다. 특성 투자는 캐릭터 프로필에 영구 저장하며, 무료 재분배는 특성 투자만 초기화한다.

## 실제 사용

StartHub에서 아린 가까이 이동해 **F → 특성 강화 창 선택 → Enter**로 연다. 전체/전투/생존/기동/탐험 필터, 16개 아이콘 카드, 현재·다음 단계 효과, 비용과 남은 포인트, 잠금·최대 단계·부족 사유를 표시한다. 방향키로 선택하고 Enter로 강화한다. R은 무료 재분배 확인, Esc는 취소/닫기다. 마우스도 사용할 수 있다. 이용은 살아 있는 단일 플레이 캐릭터의 탐험 출발 전 상태로 제한한다.

특성의 수치·조건·튜닝 필드는 [특성 강화 가이드](../../Docs/TraitEnhancementGuide.md)에 정리했다. 기본 전체 투자량은 52포인트이며 레벨당 1포인트를 얻는다. 일반 특성 비용은 단계당 1, 불굴은 LV20 해금·3포인트다.

## 구현

- PlayerState의 TownProgressionComponent에 16개 ID별 단계·실제 지불 포인트·구매한 누적 보너스를 저장한다. 기존 활력/힘 ID와 NPC가 조정한 증가량을 보존하고 새 14종은 기본 카탈로그에서 제공한다. 전용 Data Asset으로 조정할 수 있다.
- 치명 확률/배율·최대 HP/스태미나·방어·이동·회피 비용·재생을 실제 능력치에 반영한다. 공격 실행에 연격·회피 반격·보스 피해를 적용하고 직접 처치 회복, 두 물약 경로의 회복 증가, 경험치를 유지하는 금화 보너스, 탐험당 1회 불굴 생존을 연결했다.
- 콤보·회피 버프·처치 회복 재사용 시간·불굴 사용 여부·소수 금화 잔여는 Character의 임시 런 상태다. 다단 피해는 같은 공격 실행을 중복 계산하지 않으며 새 탐험에서 초기화한다. 강제 사망과 탐험 시간 종료에는 불굴을 적용하지 않는다.
- 투자/재분배 후 현재 HP·스태미나·마나의 절대량을 유지하고 새 상한으로 제한한다. 무료 회복을 얻을 수 없다. 재분배는 구매 당시 실제 비용을 환급하며 성장·스킬·아이템·장비·재화를 변경하지 않는다.
- 저장 버전 1을 검증해 기존 맞춤 보너스를 보존한 채 버전 2로 옮긴다. 모든 영역 복원이 끝난 뒤에만 다시 저장한다. 손상·미래 버전·잘못된 GUID/ID/단계/비용/예산/NaN/Infinity는 거절하고 원본을 보존한다.
- 쓰기 실패 시 세션 투자 결과를 유지하면서 추가 투자와 재분배를 막는다. 정상 복원을 마친 동일 프로필의 저장 재시도는 허용한다. 실패한 읽기/복원을 현재 상태로 덮어쓰는 재시도는 제공하지 않는다.
- 독립 네이티브 UMG 강화창은 NPC 범위·사망·탐험 상태를 감시하고 입력 차단을 소유한 만큼만 해제한다. UI 전용 아이콘 atlas를 공식 TextureTools로 임포트하고 PIE 종료 후 해당 에셋만 저장했다. 다른 Blueprint/맵 바이너리는 이 작업에서 수정하지 않았다.

## 검증

| 검사 | 결과 | 증거 |
| --- | --- | --- |
| 최종 Editor 빌드 | 성공, 9.76초 | [빌드 로그](evidence/trait16-runtime-20261011/build/editor-rendered-qa-success.log) |
| 16종 실제 효과 | 100 PASS / 0 FAIL | [Effects02](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_effects02/effects-results.txt) |
| 저장 독립 Write / Read | 125 / 227 PASS, 실패 0 | [Write](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_save02/write-results.txt), [Read](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_save02/read-results.txt) |
| 실제 1080p·720p 강화창 | 260 PASS / 0 FAIL | [UI04](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_ui04/ui-results.txt) |
| 기존 마을 서비스 | Write60 / Read25 PASS, 실패 0 | [Write](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_trait16_reg_services01/write-results.txt), [Read](evidence/town-services-persistence-20261011/Rogue10m_Town_QA_trait16_reg_services01/read-results.txt) |
| 기존 포탈·런 | Write42 / Read15 PASS, 실패 0 | [Write](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_trait16_reg_run01/write-results.txt), [Read](evidence/town-run-persistence-20261011/Rogue10m_Town_QA_Run_trait16_reg_run01/read-results.txt) |

총 854개 검사가 통과했다. 저장 QA는 16종 최대 투자·52포인트, 기존 27HP/3.75공격력 보존, 커스텀 비용/보너스와 실제 비용 환급, 21개 손상 변형, 프로필 격리와 실제 파일 쓰기 실패/복구를 확인한다. UI QA는 실제 Slate 키·마우스 hit-test, held Enter 중복 투자 방지, 잠금·상한, 재분배 취소/확정, 쓰기 실패와 저장 버튼, 범위 이탈·외부 입력 차단 보존을 확인한다.

검증 명령은 `Scripts/ValidateTraits.ps1 -Mode Effects -Label <새라벨>`, 같은 스크립트의 `-Mode UI`, `Scripts/ValidateTraitPersistence.ps1 -Label <새라벨>`이다. 모두 별도 QA 프로필/저장 슬롯을 사용하고 기존 증거·저장을 삭제하지 않는다. `CheckGeneratedChanges.ps1`와 범위 내 diff/28개 소스의 공백 검사도 통과했다.

[독립 저장·효과 검토](evidence/trait16-runtime-20261011/review.md)에서 미해결 P0/P1/P2를 발견하지 못했다. [UI 검사 독립 리뷰](evidence/trait16-runtime-20261011/ui-review.md) 후 실제 QA도 통과했다. 실제 [720p 마지막 행](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_ui04/traits-bottom-720p.png)과 [재분배 확인창](evidence/trait16-runtime-20261011/Rogue10m_Town_QA_trait16_ui04/traits-refund-720p.png)을 확인했다.

## 수정 이력과 작업 경계

초기 프로필 이름 길이 초과, QA 배열 자기 요소를 Add한 오류, UI 이미지 요청 프레임 경합, 카드의 글자 겹침과 모달 정렬을 발견해 수정했다. ui02·ui03의 스크롤 밖 마지막 행 좌표 검사 실패가 반복되자 Architect/Reviewer로 돌아가 UE 소스를 확인했다. Slate가 화면 밖 자식의 Paint를 생략하므로 부모/자식 cached geometry가 다른 프레임에 남을 수 있었다. 카드 높이를 더 늘리는 대신 상단·하단을 각각 렌더한 뒤 경계·실제 한국어 폰트 두 줄 검사와 16개 ID 전체 확인을 수행했다. 실패 증거는 보존했다.

최초 작업 브랜치는 Sprint#5-27-trait16-runtime이며 작업 전 Source188개 파일을 baseline으로 보관했다. 다른 채팅에서 공유 체크아웃을 Sprint#5-28-monster-effects-chase로 전환했으므로 현재 공유 브랜치와 몬스터 변경을 보존했다. 이번 알려진 소스28개(신규9·기존19), 검증 스크립트2개, 원본 이미지1개·신규 Texture1개의 경로/해시는 [변경 목록](evidence/trait16-runtime-20261011/changed-files.json)에 기록했다. Character/Combat/Monster에는 동시 작업 변경도 있어 Git 전체 diff를 이번 단독 변경으로 주장하지 않는다. 모듈/Build.cs·프로젝트 구조 변경이나 커밋·푸시는 하지 않았다.

기존 에디터는 미저장 패키지0을 확인한 후 정상 종료해 DLL 잠금을 해제했다. 최종 빌드 뒤 이전 OpenRunMap으로 다시 열었고 새 네이티브 클래스3개·기본16종·UI Texture 재로드와 콘텐츠/맵 dirty0을 [실제 에디터에서 확인했다](evidence/trait16-runtime-20261011/editor-final-reload.json). 마지막 PIE는 종료 상태다. 현재 검증 범위는 로컬 단일 플레이와 Editor Development 실행이다. Shipping cook/패키징·멀티플레이 저장·장기 밸런스는 별도 검증이다.

[설계·ULW 패킷](../architect/2026-10-11_trait16-runtime.md), [마을 서비스·저장 가이드](../../Docs/TownServicesPersistenceGuide.md), [일일 로그](../../DevLog/20261011.txt).
