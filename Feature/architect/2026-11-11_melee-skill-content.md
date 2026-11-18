# 근접 신규 기술 콘텐츠 구현

격리 사본 전용. 공유 소스·에셋·UE 에디터·UE 빌드는 사용/변경하지 않는다. 커밋·푸시는 하지 않는다. 1인칭은 보류하고3인칭을 유지한다.

## 제작 패킷

| 패킷 | 목표·변경 영역 | 완료 조건·검증 | 되돌림 경계 |
|---|---|---|---|
| ULW-M1 | AttackSkillData, Combat, Character 회피 입구, 신규 TechniqueMotion·시간축 | 기본 비활성 전진→종점 접촉 코드, 독립 시간축 `Scripts/Tests/TestAdvanceTimeline.ps1`, 소스 리뷰. UE 물리는 통합 대기 | 새 옵션과 컴포넌트, 해당 Combat/Character hunk |
| ULW-M2 | Editor 레시피·제작기와 순수/모의 테스트 | canonical20개 레시피·미완료 표기·원본 보존, `python -m unittest discover -s Scripts/Tests -p test_melee_skill_prototypes.py -v` | 새 제작 스크립트·테스트, 나중에 에디터로 만든 prototype 경로 |
| ULW-M3 | 기능 문서·검증 근거·인계 패치 | 검토/실행·미실행 분리, shared apply-check와 파일 해시, 통합 순서 보고 | 해당 문서·로그 추가분 |
| ULW-M4 | 피격 취소: Character/Combat/Basic 입력 정리·실제 UE 회귀 | finite 양수 applied hit만 중단, 지불/쿨다운 보존·반사 피해 후 추가타0·첫 명중 credit 보존, `Rogue10m.TechniqueMotion` 엔진 실행은 통합 대기 | TakeDamage 훅·Combat 중단/접촉 snapshot·Basic stop 옵션·새 테스트 |

Architect → Implementer → 독립 C++ Builder → Reviewer → Doc Writer를 격리 사본에서 진행한다. UE Builder/통합 QA는 공유 빌드·에디터 담당자가 이어받는다. 같은 실패를 반복하지 않고 source/asset 불일치는 기획/리뷰로 돌아간다.

1. G04 돌진참/G10 단공참의 실제 전진→종점 단타를 C++로 구현한다. 기본 비활성 DataAsset 옵션으로 기존 기술은 보존한다. 자원 지불 전에 설정/지상/몽타주 root-motion을 검증하고 승인된 실행ID와 공속·접촉 deadline을 보존한다. 전진은 capsule sweep, ledge 검사, 고정 평면 방향으로 수행하며 회피 기능을 재사용하지 않는다. 메뉴/사망/교체/EndPlay는 취소하고 접촉까지 MovementMode 소유를 유지한다. 새 이동 기술만 피해 차폐를 검사한다. G10의 최종 경로 관통 피해는 이번 종점 prototype과 구분한다.
2. K10은 실제 기존 Knuckle Charged/ShortBlow의 우차징·단타·비용/타이밍을 재사용해 별도 DA로 분리하는 제작 경로를 준비한다. canonical20개를 독립 프로젝트 소유 prototype DA로 복제하는 레시피/Editor 스크립트를 구현한다. G05/G06/G08의 반격/띄움/복합연계는 미완료 metadata로 표시하며 기존 profile/트리/저장/해금/카탈로그 활성화는 변경하지 않는다.
3. 엔진 독립 C++ 시간축 검증과 recipe/authoring 순수 검사를 실행하고 리뷰·패치·자료를 인계한다. UHT/UE 컴파일·실제 에셋 생성·ready/contact/recovery·벽/지형/입력·실제3P 검수는 UE 소유권 인계 후 실행한다. 실행하지 않은 항목을 통과로 기록하지 않는다.

## 재사용 선택

Docs/FabAssetReuseGuide.md를 확인했다. 신규 원본은 만들지 않는다. K10은 기존 FistAuthored AM_Knuckle_ShortBlow/기존 Charged를 재사용한다. G04/G10의 Mixamo slide/power-up FBX는 로컬에 있지만 프로젝트 수입/리타겟이 없으므로 현재 승인된 sword slash/heavy-cut을 임시 표현으로 보존한다. 이를 slide 모션 완성으로 기록하지 않는다. 기존 출처/조건 확인은 과거 근거이며 이번에 Fab 계정 소유나 라이선스를 새로 검증한 것이 아니다.

## 완료 경계

실제 실행 코드와 재현 가능한 콘텐츠 제작 스크립트가 있어야 한다. 핵심 시간축은 순수 코드로 검증하되 물리·애니메이션·UI 검증으로 확대 해석하지 않는다. rollback은 새 옵션/컴포넌트/Combat와 Dodge 연결 hunk 및 프로젝트 prototype 경로만이며 원본 에셋은 불변이다. 실제20개 콘텐츠 완료나 production opt-in은 이번 완료 조건이 아니다.

## 통합 검토 후 중단 계약 보강

비용 지불 성공은 사용·공유 쿨다운 확정으로 취급한다. 자원 delegate가 동기로 취소하더라도 쿨다운을1회 확정한 다음 취소 epoch를 검사하며, callback이 이미 설정한 더 긴 종료시각을 줄이지 않는다. 거절된 검증/결제는 비용·사용·쿨다운0을 유지한다.

전진 기술은 승인된 primary playback AnimInstance·Montage·instance ID에 종속된다. 시작·10ms step·각 sweep 전후·Walking 복구 후·Combat의 실제 접촉 진입에서 해당 인스턴스의 동일성·활성/재생 상태를 재조회한다. Stop/blend-out·일시정지·자산/AnimInstance 교체/소실이면 이동과 미발생 타격을 취소한다. 기존 single montage delegate는 덮어쓰지 않는다. 정상 blend-out보다 접촉이 먼저 끝나도록 콘텐츠를 검증한다. 동작 중단 뒤 의도적 타격 커밋을 허용할지는 별도 디자인 결정이며 이번에는 허용 옵션을 만들지 않는다.

실제 비용 delegate·쿨다운/이동·정리와 native montage instance의 중단 상태를 사용하는 UE Automation 회귀를 추가한다. 공유 에디터/UBT 소유권 때문에 작성한 회귀의 실행은 통합 담당자에게 인계하고, 소스 리뷰를 실행 통과로 부르지 않는다.

사용자는 피격 시 우선 취소하고 면역 여부는 스킬/실플레이 테스트 후 직접 결정하기로 했다. `Character::TakeDamage`의 finite 양수 AppliedDamage 확인 뒤, HP/LastStand/피드백 전에 한 번 취소한다. HP 비용·회복·0/거절된 피해는 이 훅을 타지 않는다. 기존 피해 수락 조건을 유지하며 새 면역을 만들지 않는다. 모든 승인 단계에 피해 epoch를 확인해 비용/몽타주 콜백 속 피격도 중단한다. 남은 다단타·콤보/차징/GAS 예약을 지우고 공유쿨다운 종료시각은 보존한다. 새 피격 애니메이션/경직 시간/슈퍼아머를 임의 추가하지 않는다.

반사 피해가 접촉 도중 들어오면 맵 정리가 현재 실행 참조를 지울 수 있다. 접촉은 지역 실행 snapshot으로 처리하고, 외부 필터 뒤·실피해 직전에 중단을 확인하며 살아있는 실행만 copyback한다. 양수로 이미 적용된 타격의 hit credit을 취소와 혼동하지 않도록 현재 접촉의 miss 판정은 최종 결과까지 유예한다. 승인된 AnimInstance/instance ID만 중단하고 Basic 입력 정리의 asset stop은 피격 경로에서 생략한다.
