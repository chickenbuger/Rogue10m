# Walking.fbx 기반 플레이어 보행 손바닥 보정 결과

## 1. 요청과 적용 결과

사용자가 제공한 `C:/Users/PC/Downloads/Walking.fbx`를 참고해 플레이어가 걸을 때 손바닥이 몸 안쪽과 뒤쪽으로 자연스럽게 기울도록 보정했다. 작업 브랜치는 `Sprint#5-22-walking-palm-reference`이다.

16개 Walk/Jog 클립과 전용 BlendSpace를 새로 만들고, `ABP_Common_Unarmed`의 `Walk / Run` 상태가 새 BlendSpace를 사용하도록 연결했다. 초기 v1은 외형 retarget 이후 손 IK가 이동하는 문제가 발견되어 수용하지 않았다. `hand_l`/`hand_r` 회전만 수정하고 원래 IK track 전체를 보존한 v2의 에셋 감사·render·여섯 외형 QA·독립 검토 및 이동 공격·카메라 회귀가 통과했다. 새 에디터 프로세스에서 저장된 에셋을 읽기 전용으로 다시 검사했으며 시작 마을을 열어 둔 상태로 작업을 완료했다.

## 2. 레퍼런스와 좌표 확인

FBX는 65개 bone을 가진 Mixamo 걷기 골격이며 피부 메시가 없다. 30 FPS, 프레임 1–30의 약 0.967초 동작을 117개 위상에서 분석했다. 한 사이클의 전진량은 약 162.032cm이며 원본 SHA-256은 분석 전후 `a28690ec43f03af63c61566fa1293d6931044fa812e8e91e8371861547a4939b`로 동일하다.

손목→중지 MCP와 새끼→검지 MCP로 손바닥 평면을 구하고, 엄지 위치와 네 손가락의 실제 굽힘으로 palmar normal 부호를 확인했다. 임의 bone-local 축이나 Euler 값을 손바닥 방향으로 가정하지 않았다. FBX의 Blender 축은 -Y 전방/-X 오른쪽/+Z 위, 현재 Manny의 component 축은 +Y 전방/-X 오른쪽/+Z 위이다. 실제 source mesh의 yaw -90°까지 반영해 actor의 전방/오른쪽/위 기준으로 비교했다.

양손 normal은 모든 참조 위상에서 몸 안쪽과 뒤쪽을 향했다. 중복 마지막 프레임을 제외한 사이클 평균을 정규화한 목표 벡터는 좌 `[-0.604, +0.755, -0.256]`, 우 `[-0.506, -0.770, -0.389]`이다.

[손 확대 참조 이미지](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/reference/reference-hand-contact-sheet.png) · [분석 수치와 한계](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/reference/reference_notes.md)

## 3. 보정 방법과 적용 범위

소스의 손목→중지 방향을 회전축으로 삼아 클립별·손별 고정 roll을 적용했다. 프레임마다 원래 팔 흔들림과 손가락 자세를 사용하며 소스의 손 길이 축을 보존한다. 보정은 ±45° 이내이고, 평균 참조 각도 오차가 줄어드는 후보를 선택했다. 모든 평가 위상에서 `보정 후 inward dot ≥ min(원래 inward dot, 0.35)`를 확인한다. 전방 Walk는 좌 -15°/우 +10°, 전방 Jog는 좌 -15°/우 +20°이며 좌우 대각선도 같은 값을 사용한다. 측면·후방은 해당 클립에 맞는 제한된 값을 사용한다.

v2의 변경 허용 채널은 원본을 복제한 클립의 `hand_l`, `hand_r` 회전 두 개뿐이다. 두 손의 원래 local 위치·scale 및 `ik_hand_gun`, `ik_hand_l`, `ik_hand_r`의 위치·회전·scale key 전체를 float 허용 오차 내에서 보존한다. UE controller의 `FQuat4f` 변환이 있으므로 byte 동일성을 주장하지 않는다. 손 IK의 회전이나 parent 상대 위치를 재계산하지 않는다. 프레임 수·rate·타이밍·curve·notify·sync marker·track 순서와 다른 bone 채널도 원본을 사용한다. 원래 BlendSpace의 27개 표본 좌표/설정 및 Idle 참조를 복제했다. 새 에셋은 `/Game/Rogue10m/Animation/Locomotion/PalmReference/`에 있다.

MF 소스의 손 위치는 translation retarget를 적용하면 약 26.14cm에서 27.30cm로 바뀔 수 있다. 원본 local key를 보존하기 위해 RAW 평가에서 `should_retarget=False`로 authoring하고, 기존 런타임 외형 retarget 경로로 재생한다. 초기 retarget된 pose를 그대로 key로 쓰는 방법은 채택하지 않았다.

## 4. 확인된 측정 결과와 한계

v2 에셋의 33개 위상을 다시 평가한 전방 손바닥과 참조 평균 사이의 각도 오차는 다음과 같다.

| 클립 | 왼손 보정 전 → 후 | 오른손 보정 전 → 후 |
| --- | --- | --- |
| Walk Fwd | 28.14° → 17.97° | 20.67° → 16.87° |
| Jog Fwd | 30.47° → 22.91° | 27.37° → 15.37° |

엄격한 native audit에서 16개 클립의 101,953개 transform pair를 비교해 통과했다. 보호 채널 위치 오차 최댓값은 0cm, scale 차이는 0, 회전 오차는 `0.000116414°`이다. 허용한 손목 회전 변화는 최대 `45.000027°`이다. 별도 raw pose 평가에서는 손목 위치 차이 0cm, MCP 위치 차이 최대 `3.690×10⁻⁶cm`, IK 위치 차이 최대 `0.000106785cm`, 추가 손목 loop 차이 최대 0.338°였다. Content 6,366개 중 기존 수정은 공통 AnimBP 한 개, 새 에셋은 17개이며 허용 범위 밖 변경은 0개다.

독립 CSV/기하 재계산에서 안정 보행 220프레임의 source와 visible body에 있는 세 hand IK의 world 위치 차이는 모두 0cm였다. 보행 시계의 위상 차이와 프레임 이동도 0이다. 실제 110·210·600cm/s 및 yaw 90° 보행의 source/body × 좌우 손, 16개 조합 모두 위상별 평균 normal의 참조 각도 오차가 개선됐다. 예를 들어 visible body의 210cm/s 걷기는 좌 23.24°→13.79°, 우 31.95°→27.50°, 600cm/s 조깅은 좌 28.15°→18.24°, 우 21.46°→9.80°이다.

이 작업은 Mixamo의 전체 보행·루트 이동·팔 스윙을 이식한 결과가 아니다. 서로 다른 동작의 위상을 그대로 대응시키지 않고 참조의 손바닥 방향을 제한된 roll로 반영했다. 따라서 모든 프레임의 참조 각도가 일치하거나 오차가 각각 줄어든다고 주장하지 않는다. 전방 Jog 왼손은 일부 raw 위상에서 약 10.21° 더 멀어지지만 사이클 평균은 개선됐다. 측면 이동처럼 원래 손의 길이 축이 목표 normal과 가까운 경우 큰 회전을 강제하지 않으며, 원래 동작 일부의 바깥쪽 기울기가 남을 수 있다. 외형 retarget의 실제 손 길이 방향은 원본 대비 최대 약 5.09° 차이가 있어 raw 소스의 축 보존과 구분해 검토했다.

v1에서는 raw local key/CS 검사가 통과했지만 visible retarget의 `ik_hand_l`이 원본 대비 약 33.25cm 이동했다. raw 좌표에서 IK 회전과 위치를 보상하는 방법이 외형 retarget까지 보존한다는 가정을 폐기하고, 모든 원래 IK key를 복원했다. `walking-palm-after-render-v1` 및 `walking-palm-six-runtime-v1-*` 증거는 superseded된 초기 접근의 관찰 기록으로 남긴다.

## 5. 빌드와 QA 현황

| 검증 | 현재 결과 | 근거 |
| --- | --- | --- |
| Editor target 빌드 | v2 통과, 27.97초 | `tmp/walking-palm-reference-20261009/build-editor-v2.log` |
| 파생 클립/BlendSpace/원래 IK/보호 Content 검사 | v2 통과, 16클립/101,953 transform pair | `apply-audit.json`, `native-key-audit.json` |
| 원본 기준 render fixture | 650프레임/95,090검사, 실패 0, 스크린샷 9장 | `walking-palm-before-render-v1` |
| 최종 보정 후 render fixture | 650프레임/95,970검사, 실패 0, 스크린샷 9장 시각 검토 완료 | `walking-palm-after-render-v2` |
| Human/Dwarf/Orc × Female/Male 런타임 | 각각 650프레임/95,976검사, 실패 0 | `walking-palm-six-runtime-v2-*` |
| 독립 기하/CSV/원래 IK 비교 | v2 PASSED, 실패 목록 0 | `reference/runtime-review-v2.json` |
| 이동 중 상하체 분리 공격 회귀 | 1,100프레임/258,066검사, 실패 0, 실제 피해 16건 | `walking-palm-moving-regression-v2` |
| 카메라 높이/전방 공격 회귀 | 1,572프레임/4,978검사, 15case·13스킬·21hit pulse 통과 | `Scripts/VerifyCameraForwardAttacks.ps1 -Phase runtime` |
| 최종 새 에디터 재시작/저장 상태 및 strict native 검사 | 통과, StartHub·PIE false·dirty Content/Map 0·120 FPS | `final-editor-state.json`, `reload-audit.json` |

v2 render/runtime fixture는 실제 `DoMove`/CharacterMovement로 110·210·600cm/s 이동, actor yaw 90° 회전, 이동 중 jab, 정지/Idle 복귀를 재실행했다. 여섯 외형 모두 약 3,029.415571cm를 이동했고 관측 카메라 오차는 0cm였다. 독립 에이전트가 정면 보행·양손 확대·조깅·yaw 90°·이동 jab·Idle/정지 복귀의 스크린샷 9장을 모두 검토했다. 검증용 임시 충돌 바닥의 높이는 실제 시작 마을 지형이나 높이 변경을 뜻하지 않는다.

이동 공격 회귀의 하체 회전 오차 최댓값은 `2.415×10⁻⁶°`이며 기존 7cm 카메라 높이 보정도 오차 0cm로 유지됐다. 검사 수는 fixture 로그의 최종 `RESULT` 표식을 기준으로 한다. `metrics.txt`는 마지막 결과 출력 직전 저장되어 검사 수가 하나 작다.

독립 재계산은 CSV의 손목/MCP 좌표와 parent hierarchy에서 normal 및 actor 축 투영을 다시 구했다. 여섯 외형에서 보고된 normal 벡터와의 차이는 `8.92×10⁻¹⁰` 이하, parent 재합성 위치 차이는 `3.98×10⁻⁷cm` 이하이다. 초기 Idle의 서로 다른 프로세스 진입 위상으로 생기는 차이도 별도로 기록하고 안정 보행의 동일 프레임 비교와 혼동하지 않았다.

최종 읽기 전용 확인에서 여섯 외형의 EyeOffset `(8, 0, -3)cm`와 초기 pitch `-20°`를 유지했다. 에디터 재시작 시의 일시적인 FPS 값을 120으로 복원했으며 Config 파일을 변경하지 않았다.

원본·v2 render와 여섯 외형의 CSV 증거 8세트 및 비교 PNG 18장을 `evidence/runtime`, `evidence/screenshots`에 보관했다. 큰 pose CSV는 gzip으로 압축하고 `evidence-manifest.json`에 SHA-256을 기록했다.

[적용 에셋 검사](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/apply-audit.json) · [독립 runtime 검토](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/reference/runtime-review-v2.json) · [최종 에디터 상태](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/final-editor-state.json) · [증거 manifest](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/evidence-manifest.json) · [보정 후 왼손 확대](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/screenshots/walking-palm-after-render-v2/walk210_left_hand_close.png) · [보정 후 오른손 확대](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/screenshots/walking-palm-after-render-v2/walk210_right_hand_close.png) · [superseded v1 에셋 검사](D:/Project/Rogue10m/Feature/doc/evidence/walking-palm-reference-20261009/apply-audit-v1-superseded.json)

## 6. 변경 파일과 복구 경계

- 에셋: 기존 `ABP_Common_Unarmed.uasset`의 보행 BlendSpace 참조, 새 `PalmReference` 경로의 16개 클립과 `BS_Unarmed_PalmReference`.
- Authoring/검증: [ApplyWalkingPalmReference.py](D:/Project/Rogue10m/Scripts/Editor/ApplyWalkingPalmReference.py), [Rogue10mWalkPalmAuthoring.cpp](D:/Project/Rogue10m/Source/Rogue10m/Editor/Rogue10mWalkPalmAuthoring.cpp), [Rogue10mWalkPalmRuntimeTest.cpp](D:/Project/Rogue10m/Source/Rogue10m/Tests/Rogue10mWalkPalmRuntimeTest.cpp), [VerifyWalkPalm.ps1](D:/Project/Rogue10m/Scripts/VerifyWalkPalm.ps1), [AuditWalkPalmRuntime.py](D:/Project/Rogue10m/Scripts/AuditWalkPalmRuntime.py).
- 참조 분석: [InspectWalkingPalmReference.py](D:/Project/Rogue10m/Scripts/Blender/InspectWalkingPalmReference.py), `Feature/doc/evidence/walking-palm-reference-20261009/reference/`.
- 계획: [Architecture 문서](D:/Project/Rogue10m/Feature/architect/2026-10-09_walking-palm-reference.md).

복구는 Unreal Editor에서 `Walk / Run`의 BlendSpace 참조를 원래 `/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run`으로 되돌리는 범위로 분리했다. 원래 클립과 BlendSpace는 보존되어 있으며 적용 전 AnimBP checkpoint는 `tmp/walking-palm-reference-20261009/checkpoint/ABP_Common_Unarmed.uasset`에 있다. 커밋이나 푸시는 실행하지 않았다.
