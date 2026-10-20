# 상체 시선 보정 런타임 독립 리뷰

- 날짜: 2026-10-11
- 역할: 기능 기획 1 완료 후 기능 검증 1로 전환. 런타임 구현자는 별도 에이전트다.
- 검토 범위: `Rogue10mViewAlignedUpperBodyProxy.h/.cpp`, `Rogue10mFighterCombatStanceAnimInstance.h/.cpp`의 이번 증분, 신규 QA fixture 초안.
- 현재 판정: 코드 정적 검토에서 구현을 막는 결함은 발견하지 않았다. 실제 실행·외형 리타깃·렌더 통과는 아직 확인하지 않았다.

## 확인한 계약

| 항목 | 근거와 판정 |
|---|---|
| 기존 그래프 평가 | 실제 root에 대해서만 `EvaluateAnimationNode_WithRoot`를 한 번 호출한다. 재귀하는 `EvaluateAnimation_WithRoot`를 사용하지 않으며 다른 연결 root는 기본 평가에 맡긴다. |
| 스레드 분리 | `PreUpdate`에서 소유자·컨트롤러·메시와 조정값을 읽고 값 타입으로 복사한다. `ApplyViewAim`은 pose와 snapshot만 사용한다. |
| 같은 프레임 입력 | UE5.8은 proxy PreUpdate 이후 NativeUpdateAnimation을 호출한다. 구현은 PreUpdate에서 현재 조준을 직접 계산하여 NativeUpdate 값의 한 프레임 지연을 피한다. |
| 좌표와 pitch 부호 | actor 기준 FRotator의 전체 조준 quaternion을 실제 source mesh component basis로 변환한다. Unreal quaternion pitch 부호와 Manny +Y 전방을 혼동하지 않는다. |
| 분배 | 전체 quaternion 하나의 Slerp를 척추 5%/8%/12%, 양쪽 upperarm 각 75%에 적용한다. pitch/yaw를 각각 쪼개 생기는 비가환 오차를 피한다. |
| 하체와 렌더 본 | root·pelvis·다리·발은 회전하지 않는다. 척추/팔 보정과 목/머리 복원은 회전만 수정하며 render local translation·scale을 보존한다. |
| 카메라 회전 | 원본 neck/head component 회전을 저장하고 부모 역회전으로 복원한다. 머리 위치는 실제 포즈를 따른다. 소스 수준에서 상체 조준이 머리 회전에 다시 더해지지 않는다. |
| 손 IK | FK 손의 최종 포즈를 helper gun→left/right 순서로 옮기고 갱신된 부모 공간으로 환산한다. helper 위치 변경은 렌더 뼈 길이 변경과 구별한다. |
| 캐시와 LOD | bone container serial에 맞춰 compact 인덱스를 다시 구성한다. Initialize에서 캐시를 무효화하고 필요한 체인이 누락되면 보정을 건너뛴다. |
| 수명 | 기본 UAnimInstance DestroyAnimInstanceProxy는 virtual destructor를 통해 proxy를 삭제한다. 별도 Destroy override가 없어도 누수가 발생하지 않는다. |
| 예외 문맥 | 비활성·사망·차단 UI·회피·비유한 입력을 보호하고 기본 pitch ±75/yaw ±45로 제한한다. |

## 실제 검증 대기 항목

- 실제 source·Appearance에서 상/정면/하향 조준과 actor world yaw 0/90/180/270도를 측정한다. source의 머리 보존만으로 외형 리타깃 머리/카메라 결과를 단정하지 않는다.
- source와 실제 외형의 하체 포즈, render local translation·scale, 손목·손가락 상대 회전, FK/IK 위치를 aim-disabled 원본 그래프 대조와 비교한다.
- 몸/컨트롤 yaw가 같은 world heading 시험과, 잔여 yaw ±30 및 ±45 clamp 시험을 구분한다.
- 실제 왼손·오른손 콤보가 모두 수락되었는지 스킬 사용 횟수/수락된 스킬 ID로 확인한다. 단순히 활성 몽타주가 있다는 검사만으로 두 입력 수락을 주장하지 않는다.
- 과거 MovingAttack fixture는 raw montage와 상체까지 직접 비교하므로 neutral aim 0도 선택으로 기존 포즈 검사를 유지한다. 새 조준으로 의도적으로 달라진 upperbone을 대조에서 제외하지 않는다.
- 기존 FistCombat·FighterStance·AppearanceCamera의 구조·판정·카메라 검사는 기본 -20도 조준을 유지한다. 카메라 최대 8도 검사는 상체 조준의 작은 중복을 단독으로 검출할 수 없으므로 머리 원본 대조와 함께 해석한다.
- 초기 fixture 검토에서 위의 잔여 yaw·외형 하체·콤보 수락 증거 보강을 개발 에이전트에 요청했다. 확인 전 완료 주장에 포함하지 않는다.
- Editor 빌드, 집중 runtime/negative/render, 관련 회귀와 콘텐츠 해시 보존 결과를 확인한 뒤 최종 판정한다.
