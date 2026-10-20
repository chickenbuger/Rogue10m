# 이동 중 상체 공격과 하체 보행 분리

- 날짜: 2026-10-09
- 작업 브랜치: Sprint#5-20-moving-upper-body-attack
- 상태: 구현·빌드·QA·문서화 완료

## Scope Gate
플레이어가 이동 중 공격하면 DefaultSlot의 전신 몽타주가 최종 Root를 덮어 하체 보행이 멈춘다. 실제 이동과 공격 판정은 계속되고 있으므로 애니메이션 합성만 수정한다. 코드, AnimBP, 검증 스크립트와 문서를 변경한다. 기존 미커밋 작업을 보존하며 커밋·푸시는 하지 않는다.

## Ultrawork Packets

| 작업 | 목표와 수정 영역 | 완료 조건 | 검증 | 롤백 경계 |
|---|---|---|---|---|
| ULW-1 런타임 | AnimInstance에 이동 공격 분리 조건을 C++로 추가 | 지상 이동 공격만 분리; 회피·공중·제자리 전신 유지 | Editor 빌드, 실제 런타임 조건 검사 | 이번 C++ 증분 |
| ULW-2 AnimGraph | ABP_Common_Unarmed의 기존 locomotion/FootIK/guard/DefaultSlot을 보존하고 상체 합성 추가 | 하체는 보행, spine_01 이상은 공격; 캐시로 보행 한 번 평가 | 실제 그래프 핀/속성 검사, 컴파일, 재로드 | 현재 uasset 체크포인트 |
| ULW-3 QA | 실제 이동 중 하체와 상체 포즈 및 기존 공격 흐름 검증 | 하체 보행 지속, 공격 상체 변화, 회피/공중/정지 회귀 없음 | 새 런타임 테스트 및 관련 기존 테스트 | 새 테스트/스크립트 |
| ULW-4 기록 | 독립 리뷰, 결과 문서, 한국어 개발 로그 | 변경 범위와 검증 결과 기록 | SprintChangeLog, DevLog, generated changes 검사 | 이번 문서 항목 |

## 설계
- 공격 몽타주와 Data Asset, DefaultSlot, 공격 판정/콤보 시간을 보존한다.
- FootIK와 기존 전투 가드 이후 포즈를 새 캐시에 저장해 base와 montage source에서 공유한다.
- Layered Blend per Bone: base=캐시된 보행 포즈, blend=캐시된 DefaultSlot 결과. spine_01과 ik_hand_root 후손에만 공격을 합성한다. 선택 TwoWayBlend는 조건 알파가 0일 때 원본 DefaultSlot 전신 결과, 1일 때 상체 합성 결과를 출력한다.
- 보행 캐시와 별도 몽타주 결과 캐시를 각각 사용해 재생과 평가를 중복하지 않는다. 원본 노드 11개에 새 노드 9개를 추가한다.
- 이동 조건과 공격 몽타주 식별은 native AnimInstance가 관리한다. 회피 및 공중 동작은 전신 출력한다.
- 실제 현재 모델/리타깃 출력에서 걷기와 공격이 동시에 보이는지 검증한다.

## 보존 범위
기존 NPC 아린, 월드, HUD, 전투 스킬과 이미 진행된 미커밋 작업은 이번 변경의 롤백 대상이 아니다. .uasset은 Unreal Editor API로만 변경한다.

## Exit Gate

- ULW-1~4 완료. Editor 빌드, 실제 source/외형 1100프레임 및 렌더 1100프레임, 가드640/기존 전투840프레임 검증 통과.
- 원본 전신 그래프 대조는 하체 검사474회 실패했고 수정 후 실패0으로 문제 재현과 해결을 확인했다.
- 저장·별도 프로세스 재로드·과거 가드/FootIK 감사 통과. Content6366파일 중 허용 AnimBP1개만 변경했다.
- Native 읽기 전용 감사로 Python에 노출되지 않는 weak cached-pose 참조를 직접 검사한다.
- 독립 런타임/그래프 리뷰 완료. 결과 문서, Sprint 이력과 한국어 일일 로그 갱신. 커밋·푸시 없음.
