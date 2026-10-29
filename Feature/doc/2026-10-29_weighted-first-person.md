# 선택형 1인칭 무게감 프로토타입

사용자의 1인칭 방향 확인을 반영해 기존 Appearance 기반 카메라·Manny→외형 retarget 경로 위에 조절 가능한 프로필을 추가했다. 현재도 이미1인칭이므로 기존 카메라를 교체하거나 삭제하지 않는다. Dead by Daylight는 느낌 참고이며 에셋·브랜딩·실제 구현을 복제하지 않았다.

## 실행과 복구

`Rogue10m.FPHeavy 1`로 시험하며 기본값0은 기존 표현이다. `Rogue10m.FPHeavy 0`으로 돌아간다. `FPHeavyMotionScale`은0..1, `FPHeavyFOV`는0(프로필값) 또는70..110이다. 초기 프로필은 FOV86, 최초 중립 시선0도, 머리 흔들림 걷기 .75도/.35cm·달리기1.25도/.65cm·공격1도/.45cm다. 이미 조작한 시선은 초기화/보간하지 않는다. 값은 UPROPERTY와 콘솔에서 조절 가능하며 finite·범위를 검증한다.

FOV만 최종 POV에서 선택하며 머리 위치·회전은 기존 공용 CameraComponent 경로를 통해 combat trace와 일치한다. 별도 POV-only shake, 마우스 입력 지연, 이동속도 변경은 추가하지 않았다. 손 보정은 상완 회전과 IK helper 동기화에 한정한다. 기존 spine/feet/렌더 bone local T/S와 원본 asset·설정 값을 보존한다. 추가 어깨 follow는 OFF/강도0에서 baseline을 사용하며 guard raise/lower는 원래 데이터 값을 덮어쓰지 않는다.

공격 준비·회수 보정은 단일 타격·단일 비반복 구간·일반 rate/비rootmotion/비projectile 근접 공격과 정상 source/body 매프레임 평가에 한정한다. 보정 제외 폭은 max(1/60,2×max(dt,.05)×MontageRate)다. 50ms 초과 간격·URO·정지/갱신 생략은 그 공격의 남은 보정을 생략한다. 타격 시각·재생속도·피해·자원·CD·입력 버퍼는 변경하지 않았다. 1프레임 이하 retarget 지연의 지원 범위이며, 갑작스런 큰 hitch에서 이미 표시된 이전 포즈를 소급 수정하지 못한다. 코드가 준비 보정을 지원해도 짧은 준비 구간은 보호 창 안에 들어가 추가 보정이 생략될 수 있다.

## 실제 검증

- UE5.8.3 CL58210709 Editor Development 최종 빌드 PASS (10.83초), MSVC14.44 / SDK10.0.22621.0.
- 기존 회귀17개 PASS: 공격 자원4·쿨타임5·무기 교체8. 이후 수정은 QA 호출·사망 기대값·촬영 순서·프레임 기반 타이밍 oracle뿐이며 생산 코드 변화는 없다.
- HeavyRenderE1029: 3400항목/716프레임, 실패0, 실제 D3D11 viewport PNG9장. HeavyFastC1029: 1666항목/314프레임, 실패0, 공격속도2/30fps 상한. 항목 수는 매 프레임 반복 검사를 포함한다.
- ON/OFF 실제 DoAim yaw17.5/pitch10 누적 일치. InputKey 이동·달리기·LMB 실제 너클 공격, 명중, 공용 camera/trace, 동일 프레임 반복 camera 조회 무진행, 후반 마우스 반영, 메뉴·V inspection·사망 플래그·인벤토리 무기 전환·원형 복원 경계를 확인했다.
- 실제 표적 피해 22.778320/21.682617, 첫 피해 오차 0.069214106/0.010696750초, 접촉 손 추가 pitch는 두 경우 모두0/0. 실제 공격 구간 추가 pose 관측 프레임은 0/2이다. 수치는 고정 QA fixture의 관측값이며 밸런스·성능 개선율이 아니다.
- 기존516파일(세이브204+증거312)의 SHA256 변경0. 현재 저장216개에는 새 QA 저장이 포함되며 삭제하지 않는다.

첫 빌드는 QA의 const ExportText API 인자, 후속 빌드는 protected UFUNCTION 이름 매크로 접근 때문에 실패해 검증 코드만 수정했다. 첫 runtime은 옛 DoLook을 호출해 실패했고 현재 실제 입력 DoAim으로 정정했다. 다음 runtime의4실패는 사망 중 Appearance camera가 정상 복구/비활성화되는 계약을 살아 있는 카메라와 혼동한 QA 문제였다. 모두 실패 기록으로 별도 보존하며 통과 수에 합산하지 않는다. 일반 runtime에서 관측된 실제 명중은 정정 전 기록도 있지만 최종 통과 판정은 RenderE와 FastC를 사용한다.

재촬영 D는 고정120ms timing 기준1개가 실패했다. 마지막 무피해 world_s8.992114801, 첫 피해9.145120207로153.005406ms frame이며 예상 due9.014914002는 그 안에 있었다(raw late130.206205ms). UE5.8은 TimerManager.Tick 후 OnWorldPostActorTick을 호출하고 피해도 동기 적용된다. 독립 리뷰에 따라 최종 QA는 error의 최소 경계 `-(acceptObservationDelta+.001) <= error <= damageFrameDelta+.001`를 검사한다. previousDelta/acceptDelta를 late 허용량에 중복 가산하지 않는다. raw error와 각 delta·허용 경계를 그대로 기록한다. 큰 hitch 표본은 작은 subframe 지연을 구별하지 못하므로 낮은 jitter의 별도 FastC 결과와 변경 없는 공격 데이터/clock 검사를 함께 본다. D를 통과로 바꿔 기록하지 않으며 로그·사진은 중간 기록으로 보존한다.

## 외형·화면의 범위

사용자는 현재 맨손으로 진행하고 이후 얻은 너클별 외형을 붙이도록 확정했다. 현재 DA_Item_StarterKnuckle은 계열/장착 데이터만 있고 별도 장착 모델 연결은 확인되지 않는다. 이번 변경은 이 데이터나 시작 무기 규칙을 삭제하지 않는다. 기존 mesh/socket·retarget 계층을 보존하므로 미래 아이템별 부착 모델을 추가할 수 있지만, 그 부착 시스템 자체를 구현했다고 주장하지 않는다. 모델 구매·생성·import는 하지 않았다.

프로그램 InputKey/리플렉션 DoAim 검증이며 OS 물리 마우스·키보드, 실제 OpenRunMap 수동 완주·보스·강제종료·전면 clipping은 미검증이다. 사진은 StartHub 격리 fixture다. 최초 명중은 AI를 끈135cm 근접 표적이며 이후 표적을 전방450/옆100cm로 옮긴다. 최종 E는04/05/06/07 촬영 후 상태를0.6초 유지해 파일명과 상태를 맞춘다. C의 사진은 이전 촬영 순서이므로 최신 증거 대신 쓰지 않는다. 기존 sprint blur/fringe와 natural idle 팔 내림은 남아 있다. 움직임의 주관적인 무게감과 실제 맵의 가독성은 사용자 플레이로 추가 조정해야 한다.

## 변경 파일과 체크포인트

새 Data/Rogue10mWeightedFirstPersonSettings.h/.cpp는 설정/CVar·유한값 경계를, AppearanceCamera.h/.cpp와 Character.cpp·FirstPersonPresentation.cpp는 카메라/FOV/첫 시선을 담당한다. ViewAlignedUpperBodyProxy.h/.cpp와 FighterCombatStanceAnimInstance.h/.cpp는 GT snapshot/worker 손 후처리·전환·평가 결과 공개를 담당한다. 새 RuntimeTest.cpp와 VerifyWeightedFirstPerson.ps1은 고유3종 QA 저장·결과·전체 기존 저장 해시 감시를 제공한다. 계획/본 문서/규칙/SprintChangeLog/일일 DevLog를 함께 갱신한다. Content/Config/실제 세이브는 수정하지 않았다.

프로젝트 D:\Project\Rogue10m, 브랜치 Sprint#9-10-weighted-first-person, 시작 SHA a426d95f8f0fe6b2da3aa849680ff2fc27e9f4a5. 원시 증거는 Feature/doc/evidence/weighted-first-person-20261029/, 사진은 DevLog/20261029_WeightedFirstPerson_HeavyRenderE1029/, 봉인 JSON은 task/WEIGHTED_FIRST_PERSON_20261029_CHECKPOINT.json이다. 기능 소스·문서만 로컬 커밋하며 push/병합/PR은 하지 않는다.

너클 외형 질문은 사용자 답변으로 해소됐다. 입력필요 UI를 직접 읽으려던 ChatGPT 창67022 요청은 앱 접근 승인 시간초과로 AX 상태를 읽지 못했으며 재시도/승인 클릭을 하지 않았다. 이 UI 요청은 구현에 필요하지 않고 현재 별도 승인·디자인 질문 대기는 없다.
