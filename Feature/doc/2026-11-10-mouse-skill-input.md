# 마우스 스킬 입력 중재 결과

## 구현
- Character의 기존 LMB/RMB delegate를 별도 MouseCombatInputComponent로 전달한다. 기본 공격·비용·해금·쿨다운·실제 타격은 Combat에 남겼다.
- 좌+우 슬롯이 등록됐을 때만 기본 LMB press를 최대 80ms 보류한다. 빠른 단독 클릭은 release에서 확정한다. 80ms는 조절 가능한 시험 가정이다.
- RMB는 기존 Skill Data의 ChargeSeconds를 기준으로 탭/차징 중 하나만 실행한다. 양쪽 순서의 동시클릭은 전용 슬롯 한 번으로 소비하며 release에서 추가 공격을 만들지 않는다.
- 메뉴·일시정지·죽음·빙의해제·스킬 변경에 보류 입력을 취소한다. 보류/홀드 동안 무기 변경도 차단한다.
- Ctrl 고유 스킬을 별도로 연결하고 이동 배율이 스프린트/스탯 갱신 후에도 유지되게 했다. 마법사의 향후 문자 명령 확정/취소 방식은 구현하지 않았다.

## 검증 상태
- C3 및 D 통합 native 빌드 성공. 새 입력 컴포넌트와 13개 InputKey 회귀 fixture가 컴파일됐다.
- TP_C1은 카메라/이동/회피 단계까지만 실행됐다. 잘못 둔 QA 회피 거리 상한에서 멈췄으므로 마우스 13개는 0개 실행이었다.
- D fixture는 제작 DodgeSkill/Character 설정값을 읽고 기록하며 실제 설정을 기준으로 회피를 검사한다. gameplay 회피 수치는 수정하지 않았다.
- 최종 케이스는 처음 신생 캐릭터에서 읽은 실제 /Game 주먹 Primary와 원래 GAS 클래스를 보존·복구한다. 실제 LMB 수락/사용 횟수와 Q/E/R·RMB·Ctrl 무기 능력 미발동을 일시 스킬 검사와 구분한다.
- TP_D1 실제 Controller InputKey: 13/13 경로 성공, 전체786개 체크 실패0, 엔진 Error/Ensure/Fatal0. 각 케이스에서 기본/탭/차징/동시 스킬 사용 횟수가 정확히 일치했다.
- 실제 신규 고정 주먹 `/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab` 사용 횟수 +1. 원래 DefaultAttackAbilityClass를 복구한 실제 콘텐츠/GAS 경로이며 Q/E/R·RMB·Ctrl 무기 능력은 실행되지 않았다.
- 물리 마우스/키보드 및 저장 후 재시작은 검증하지 않았다.

## Fixture 범위
- 나머지 12개 케이스는 일시 GreatSword 프로필의 zero-cost/짧은 cooldown/no-montage 스킬 네 개를 사용한다. 장비/모션/타격 품질을 증명하는 검사는 아니다.
- 케이스: LMB 탭/홀드, RMB 탭/홀드, 좌→우와 우→좌 동시클릭, 늦은 두 번째 버튼, 설정창 중 보류 LMB/RMB 취소, 차징 슬롯 제거/교체, 동시클릭 슬롯 없을 때 즉시 기본 공격, 실제 뉴비 주먹과 무기 능력 미발동.
- 새 저장 네임스페이스와 일시 객체만 사용하며 기존 저장/맵/원본 Skill Data Asset을 변경하지 않는다.
## 결과 파일
- `DevLog/20261110_ThirdPerson/TP_D1/results.txt`에 실제 주먹과 일시 스킬 네 개의 사용 횟수를 구분해 기록했다.
- 같은 폴더 PNG5장은 카메라/이동/회피 증거이며, 주먹 접촉이나 모든 입력의 모션 품질을 증명하는 자료가 아니다.
- 기존 저장390개 해시 보존, 새 QA 저장2개만 추가. 원본 애니메이션/스킬 에셋과 맵 변경 없음.
