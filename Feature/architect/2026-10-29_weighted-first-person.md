# 묵직한 1인칭 프로토타입
현재 Appearance 기반 1인칭을 유지하고 선택형 표현 프로필을 추가한다. 사용자 확인으로 1인칭 방향 구현은 승인됐으며 Dead by Daylight는 느낌 참고만 사용한다. 현재 프로젝트의 활성 Manny/외형 retarget·기존 공격 montage를 그대로 쓰며 새 에셋·Fab 활성화·구매는 하지 않는다.

- 기준 a426d95f, Sprint#9-10-weighted-first-person. 세이브204개+증거312개 총516파일을 preservation-20261029-122009에 복사·해시 검증했다. D/Content는 실제 독립 디렉터리다.
- Rogue10m.FPHeavy 기본0으로 기존 카메라를 보존하고 1로 시험한다. FPHeavyMotionScale 0..1, FPHeavyFOV 0=profile/70..110. 초기 추천86도, 머리 흔들림 걷기.75도/.35cm·달리기1.25도/.65cm·공격1도/.45cm, 첫 중립 시선0도. 기존 마우스 입력과 후속 aim을 지연/덮어쓰지 않는다.
- 새 USTRUCT에서 노출값을 검증하며 Appearance camera·source proxy가 같은 머리 예산을 사용한다. POV-only 위치/회전 shake는 추가하지 않아 combat camera 원점·방향과 렌더 정합을 유지한다. FOV만 최종 POV에서 바꾸며 원본 Camera property/에셋은 보존한다.
- 상완의 작은 guard/이동 sway, 안전한 단일구간 공격의 준비·회수 표현을 검토한다. 손 endpoint/피해 시점·Montage 재생속도·CD·자원·이동 속도·세이브 계약은 변경하지 않는다. 미지원 공격은 기존 authored pose로 돌아간다. 병렬평가에는 GT에서 복사한 plain 값만 전달한다.
- 검증: Editor 빌드, 전용 새 Profile/Progression/Skill 슬롯으로 on/off 복원·comfort0·입력·명중·카메라/trace·무기/메뉴/사망, 기존 전투 회귀. 실제 viewport PNG를 별도 DevLog에 남기고 물리 입력/수동 완주 여부를 구분한다. 기존 wrapper의 profile 미격리는 그대로 실행하지 않는다.
- 종료: 기능 코드/문서만 로컬 커밋, push/병합 금지. rollback은 프로필OFF 또는 이 코드 묶음이며 기존 세이브/증거/카메라를 삭제하지 않는다.
