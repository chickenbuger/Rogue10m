# 맨손·너클 전용 애니메이션과 안정된 머리 카메라

작업: Sprint#5-1-unarmed-knuckle-animation / 기준: b7ab6b6 / UE 5.8.2

## 구현

무기별 기획서에 따라 맨손 4종과 너클 5종의 전용 Sequence/Montage를 제작했다. 검증된 Boxing 가드를 출발점으로 120Hz 본 회전 키를 베이크하고 실제 공격 Data Asset에 연결한다. 새 모션캡처를 가져온 작업은 아니다. 기존 손을 내린 Idle과 걷기·점프 그래프는 유지한다.

|무기|동작|입력·연결|
|---|---|---|
|맨손|왼잽 → 오른잽|좌클릭 기본 콤보|
|맨손|오른 스트레이트|짧은 우클릭 후 놓기|
|맨손|오른 훅|우클릭 완충 후 놓기|
|너클|짧은 잽 → 몸통 스트레이트 → 좌우 훅 6연타|기존 Primary 3단계 스킬|
|너클|승룡권 어퍼컷|기존 Special|
|너클|축경권 짧은 강타|기존 ChargedPrimary|

너클 3단계의 6회 타격은 실제 기존 DA 정책이다. 해금 조건·입력 슬롯·콤보 연결·피해량·공격속도·쿨다운·사거리·차징 시간·히트 수는 보존했다. 너클 5종의 기존 첫 타격 지연 0초만 각각 0.15/0.16/0.18/0.21/0.22 몽타주 초로 조정했다. 실제 첫 타격 시간은 이 값을 공격속도와 재생률로 나눈 값이다. 후속 6연타 간격은 기존 정책을 유지한다.

전용 에셋은 `Content/Rogue10m/Animation/Combat/FistAuthored/`의 18개다. 공유 원본은 보존하며 `Scripts/Editor/PrepareFistCombatAnimations.py`로 재생성·검증할 수 있다. 기존 생성기 4개는 새 전용 몽타주 연결을 감지하면 자산을 쓰기 전에 중단한다.

## 본과 전환

팔은 기존 길이를 유지한 두 본 IK로 계산하고 회전만 기록한다. 손목·손가락은 검증된 주먹 자세를 유지하고 손·발 IK를 최종 본 위치에 맞춘다. 최대 팔 도달 거리는 길이의 94%로 제한한다. root·골반·다리는 공격 클립 안에서 같은 스탠스를 유지한다.

가드에서 팔꿈치 방향이 갑자기 바뀌지 않도록 IK Pole을 연속 보간한다. 반대팔이 몸통을 돌리는 중에도 해당 프레임의 손 위치를 기준으로 연결해 6연타 경계의 실제 불연속을 제거했다. 오른잽과 스트레이트는 어깨 회전·준비·회수·전진 궤적을 구분했다. 오른 어깨가 앞으로 회전하게 하고 타격점을 올려 주먹이 HUD에 가리던 문제도 수정했다. 손을 내린 Idle에서 공격 가드로 들어가는 블렌드는 맨손 0.16초, 너클 0.10~0.12초이며 실제 첫 타격과 비교한다.

## 시야

Appearance Mesh의 실제 머리 위치에 카메라를 유지한다. 맨손·너클에서만 애니메이션 머리 회전 기여를 15%, 최대 8도로 줄였다. 초기 시선은 -30도에서 -20도로 올렸다. 마우스 회전은 즉시 반영하고 V 왕복·무기 교체가 사용자의 시선을 초기화하지 않는다. 별도 First Person Mesh는 사용하지 않는다.

설정은 `Rogue10mAppearanceCameraComponent`의 `BrawlerHeadRotationWeight`, `BrawlerHeadMaxRotationDegrees`, `InitialViewPitch`로 노출했다. 화면과 공격 판정이 같은 카메라 계산을 사용한다. 머리를 포함하는 기존 VSM 그림자와 V 관찰을 유지한다.

## 검증 결과

기획 1·개발 2·검증 2 역할로 구현과 독립 검토를 나누었다. 빌드와 에디터 자산 저장은 통합 담당자가 순차 실행했다. 최종 결과는 가독성 교정까지 반영한 v2다.

|검증|결과|
|---|---|
|UE Editor 빌드|성공, 최종 증분 빌드 7.09초|
|전용 에디터 베이크|9클립·14접촉, 기존 연속성 기준 유지 PASS|
|공유 원본 보존|48개 파일 SHA256 일치|
|본/IK|변형 본 local translation·scale 보존, IK 위치 오차 최대 0.0000644cm|
|6체형 실제 실행|Human/Dwarf/Orc × 남/여, 각 840프레임 시나리오 실패 0건|
|독립 자세 검사|총 711,486행, NaN/Inf 0건, 구조 본 길이비 0.999999~1.001353|
|실제 타깃 피해|Human Male의 9종 합계 14회, 각 1/1/1/1/1/1/6/1/1 일치|
|피해 시각|첫 타격 오차 최대 0.026923초, 연타 간격 오차 최대 0.027885초: 30fps 한 프레임 이내|
|머리 카메라|모든 체형 최종 머리 회전 영향 최대 7.0021도, 8도 제한 준수|
|시점·입력 회귀|InitialHeadFraming 90프레임, ThirdPersonInspection 280프레임, AppearanceCamera 570프레임 모두 PASS|
|기존 기능|V 왕복·메뉴 입력 차단·시점 충돌·실제 카메라 캐시·무기 교체·차징 중단·걷기·점프·착지·raw 그림자 포즈 일치 확인|
|저장 경로·문법|Harness 경로 검사, diff 공백 검사, Python 7파일 AST 검사 PASS|

같은 Human Male·30fps·제어 pitch -20도·맨손 4종 구간을 비교하면, 머리 카메라의 최대 프레임 간 회전은 **57.7707 → 1.8773도**, 위치 이동은 **23.8553 → 4.9774cm**다. 기준선은 V 관찰을 함께 사용했으므로 이 수치는 화면의 3인칭 POV가 아닌 실제 머리 카메라 컴포넌트 변환을 비교한다. GPU 성능 개선 수치가 아니다.

원본 클립을 바꾼 뒤 진입 블렌드까지 완화했으나 빠른 공격의 손·손가락 각속도를 0으로 제한하지는 않았다. 오른손 가독성 개선으로 일부 너클의 손 이동량은 증가했다. 본 길이와 손목 위치의 정상성, 공격 속도, 화면 가독성을 각각 확인했다.

- [실제 1인칭 실행 영상 — 28초](evidence/fist-combat-20260929/first-person-unarmed-knuckle.mp4): 0~10초 맨손, 10~21초 너클, 이후 중단·이동·점프.
- [1인칭 대표 프레임](evidence/fist-combat-20260929/first-person-contact-sheet.jpg), [체형별 V/머리 시점](evidence/fist-combat-20260929/body-variants-contact-sheet.jpg).
- [제작 독립 리뷰](evidence/fist-combat-20260929/authored-source-review.md), [런타임 독립 리뷰](evidence/fist-combat-20260929/runtime-final-review.md), `six-body-analysis.json`, `before-after-camera.json`.
- `runtime-csv-summary.zip`에는 6체형의 카메라·프레임·스킬·피해·스켈레톤·실행 지표를 보존했다. 전체 자세 CSV는 로컬 `tmp/fist-combat/<체형>-v2/`와 `tmp/fist-combat/runtime-csv-full.zip`에 보존했다. 대용량 자세 원본을 Git 대상에 복제하지 않으며 테스트 명령으로 재생성할 수 있다.

## 범위와 한계

검증 영상은 테스트 맵의 실제 실행 프레임을 30fps로 기록한다. 특정 기기의 실시간 성능 측정은 아니다. 실제 피해 검증은 Human Male에서 수행했고, 나머지 체형은 입력·자세·카메라·전환을 검증했다. 너클 장비 메시의 외형·손가락 관통, 모든 방어구 조합, 경사면 접지, 네트워크·사망·재빙의, 극단적인 공격속도 버프, RT 그림자는 이번 실행 검증 범위에 포함하지 않는다. 기본 해금 정책을 바꾸지 않았으므로 일반 플레이에서 모든 너클 스킬이 처음부터 열리는 것은 아니다.

실행된 체형과 검증 결과만 완료로 기록하며, 본 길이 검사만으로 모든 동작의 자연스러움을 보장하지 않는다. 작업 브랜치에 보존하며 새 변경의 커밋·푸시는 하지 않는다.

## 재현

1. `Scripts/BuildEditor.ps1`로 Editor 빌드.
2. UnrealEditor-Cmd의 `-run=pythonscript -script=.../Scripts/Editor/PrepareFistCombatAnimations.py`로 생성 및 에디터 검증.
3. 게임 실행 `-ExecCmds="Rogue10m.TestFistCombat" -RogueFistLabel=<label>`. `-RogueFistFirstPersonOnly`, `-RogueFistCaptureAll`, `-RogueFistCaptureKeyframes`, `-RogueFistDamageTarget`, `-RogueFistCharacterClass=<Blueprint_C>`를 선택해 검증.
4. `Scripts/AnalyzeFistCombat.py <capture directories> --output <json>`으로 CSV 분석. 추가 리타깃 진단은 `Scripts/AnalyzeAnimationStability.py`를 사용한다.
5. `Rogue10m.TestInitialHeadFraming`, `Rogue10m.TestThirdPersonInspection`, `Rogue10m.TestAppearanceCamera` 회귀 실행 후 `Scripts/CheckGeneratedChanges.ps1` 확인.

관련 계획: `Feature/architect/2026-09-29_unarmed-knuckle-animation.md`

검증 자료: `Feature/doc/evidence/fist-combat-20260929/`
