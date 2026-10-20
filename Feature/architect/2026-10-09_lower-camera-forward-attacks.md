# 카메라 높이와 공격 전방 방향 점검

- 요청: 시작 마을 스크린샷보다 1인칭 카메라를 조금 낮추고 플레이어 공격의 전방성을 전체 점검한다.
- 작업 브랜치: `Sprint#5-21-lower-camera-forward-attacks`. 커밋·푸시는 수행하지 않는다.
- 기준 상태: 이전 걷는 중 상체 공격 분리 완료 상태와 기존 미커밋 변경을 보존한다.

## Architect / Scope Gate

실제 PIE 플레이어는 `BP_Rogue10m_HumanMaleCharacter`, 카메라 설정은 `EyeOffset=(8,0,4)cm`, 초기 pitch=-20도다. 카메라와 타격 원점은 같은 AppearanceCameraComponent에서 계산한다. 위치 조정 요청은 높이 약7cm 하향으로 구현하고 시야각과 마우스 입력은 유지한다. 3인칭 검사 카메라는 별도 캡슐 피벗을 사용한다.

공격 애니메이션은 Manny 로컬 +Y가 전방이며 실제 보이는 외형으로 리타게팅된다. 타격 판정은 카메라 전방을 사용한다. 공격별 접촉 위치·궤적·실전방 명중을 확인하며 훅·어퍼컷·광역 스킬의 의도된 횡/상향/반경 특성은 별도로 해석한다. 읽기 전용 전체 사용 스킬 인벤토리로 범위를 확정한다.

## Ultrawork Packets

| Packet | 목표 / 수정 영역 | 완료 조건 / 검증 | 롤백 경계 |
| --- | --- | --- | --- |
| A | AppearanceCameraComponent.h EyeOffset.Z 4→-3 | 실생성 캐릭터 및6종 BP 기본값, head view·pitch·V 회귀 확인 / BuildEditor.ps1 -NoHotReload | 이번 작업 직전 헤더 백업의 상수·주석 |
| B | 실제 공격 DA/몽타주/시퀀스 전방 접촉과 사용 범위 audit | 접촉시 source/retarget 손 전방 위치, 잽·직선 excursion, 타격시각/포워드 판정 보고 / Editor Python read-only audit + runtime fixture | 새 audit/test 파일; 발견된 문제만 별도 수정 |
| C | 통합 QA와 결과 기록 | 기본1인칭 화면/공격 화면, 실제 전방/후방 타겟 damage, 이동 상하체 분리 회귀, 콘텐츠 hash·diff check / 새 Verify wrapper 및 기존 MovingAttack test | 새 evidence/docs만 |

## Production / Validation 제약

- ROOT만 live UE와 빌드 프로세스를 제어한다. 조사·검증 도구 작성·리뷰는 분담한다.
- 바이너리 직접 수정 금지. 추가 애니메이션 수정이 필요하면 현재 파일 백업 후 Editor에서만 변경한다.
- 콘텐츠 인벤토리와 타격 데이터·몽타주 타이밍을 비교해 관련 없는 자산 변화를 막는다.
- GUI Editor의 dirty packages를 확인한 뒤 종료하고 native 빌드한다. 검증 후 StartHub를 다시 연다.
- Builder/Reviewer/Doc Writer 결과는 paired feature doc, SprintChangeLog, 한국어 DevLog에 기록한다.

## Exit Gate (완료)

최종 카메라 기본값 `(8,0,-3)`과 여섯 Blueprint 상속, pitch=-20도 유지 확인. 74공격/13프로필 인벤토리·현재 연결23공격/16시퀀스 감사, 실제13종+회전2건의 전투·렌더15건과 기존 카메라/3인칭/이동 공격 회귀 통과. 방향 문제와 범위 밖 콘텐츠 변경은 발견되지 않았다. 최종 빌드·StartHub 복원·기록 완료. 상세 수치와 검증 한계는 [결과 문서](../doc/2026-10-09_lower-camera-forward-attacks.md)에 기록했다.
