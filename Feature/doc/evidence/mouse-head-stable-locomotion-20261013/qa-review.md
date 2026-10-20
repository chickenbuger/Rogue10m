# 머리 안정화·안전 모션 독립 수치 QA 검토

- 검토일: 2026-10-13
- 작업: Sprint#5-30-mouse-head-stable-locomotion
- 검토자: 기능 검증 1 (`stable_qa_review`)
- 현재 판정: 최종 build 6의 runtime04 및 render02 집중 수치 검증, Fist02 production 착지·그림자·실제 피해 회귀, Moving01 원본 자세·하체·피해 회귀가 통과했다. 동일 production의 negative01은 머리 분리를 끈 상태를 예상대로 거절했다. render02의 실제 PNG 1,800장 완전성과 원본 대표 9장의 머리·목·관절 연결을 확인했다. 요청한 머리/카메라 상한·속도 연동·활성 위험 모션 배제 범위는 완료로 판정한다. 아래의 1인칭 가림과 표면·지면 관찰 범위를 구분한다.
- 실행 주체: 엔진·빌드·자산 저장은 통합 담당이 직렬 실행했다. 검토자는 결과 CSV·로그·소스를 읽고, 신규 집중 QA fixture의 판정 오류를 수정했다.

## 판정의 독립성

집중 검증은 실제 플레이어의 Animation Source와 retargeted Appearance 메시를 사용한다. Source/Appearance 머리의 local bone들을 부모 순서대로 component pose로 합성한 뒤 reference skeleton과 실제 actor/control quaternion으로 비교한다. 카메라 구현 helper를 호출하여 동일한 값을 기대값으로 복사하지 않는다.

late mouse 기대값은 측정한 Appearance head delta에서 평가 시선 기준을 제거하고 현재 상태의 최종 quaternion 상한을 독립적으로 적용한 뒤 새 마우스를 한 번 합성한다. 같은 프레임 반복 read·CalcCamera(0), 평가 포즈를 그대로 둔 component yaw 변경, capsule의 원자적 100cm 이동을 별도 검사한다.

공격은 실제 입력 처리, 승인된 montage instance, 실제 속성값 변경과 타깃 체력 감소를 사용한다. 재생률·montage clock과 첫 타격 시각을 함께 검사한다. 이동은 실제 입력과 CharacterMovement 속도에서 얻으며, 실제 compiled BlendSpace player의 재생률과 발 움직임을 검사한다.

## 최종 집중 실행 runtime04 결과

`tmp/mouse-head-stable/stable_runtime04`의 frames/results/metrics/failure-reasons와 새 실행 로그를 직접 확인했다. frames.csv의 0~1799 프레임은 연속이며, 결과 24행 모두 75프레임이다. 총 489,428 assertion, 실패 0개, failure-reasons는 비어 있다. fixture는 production Source→CharacterMovement prerequisite가 존재함을 assert하며 Source 순서를 직접 추가하거나 제거하지 않는다. 이전 runtime02의 강제 prerequisite 한계를 최종 실행의 근거로 사용하지 않았다.

원래 21개 사례에 현재 기본 시야인 -20도의 idle·1인칭 잽·V 검사 잽을 추가했다. 두 추가 잽은 실제로 각각 한 번 승인되어 montage rate 1.5로 재생됐고 타깃에 각각 한 번 피해가 발생했다. 머리·카메라 최대 잔여는 약 2.999983도였다.

사례별 최대값을 그대로 공격 상한과 비교하면 공격 전후 달리기 구간의 4도 상태가 섞인다. 아래 표는 각 프레임의 실제 현재 상태 상한으로 분리했다.

| 현재 상한 | 프레임 | Source 머리 최대 | Appearance 머리 최대 | 카메라 최대 | 카메라 local 위치 잔여 최대 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 정지·걷기 2도 / 1cm | 1,272 | 2.000000도 | 2.000000도 | 2.000000도 | 0.999999cm |
| 공격·차징·회피·공중 3도 / 1.5cm | 322 | 3.000000도 | 3.000000도 | 3.000000도 | 1.499999cm |
| 달리기 4도 / 2cm | 206 | 3.805321도 | 3.805321도 | 3.805321도 | 1.994945cm |

CSV의 Source/Appearance/camera 회전 상한 및 camera 위치 상한 위반은 각각 0개다. late mouse oracle 최대 오차는 0.000002415도, 같은 프레임 반복 위치 오차는 0cm, 반복 회전 오차는 0.000002958도다. capsule 100cm 즉시 이동과 늦은 component basis 운송의 오차는 모두 0이다.

공격속도 0.5/1/2/3의 잽은 실제 montage rate 0.75/1.5/3/4.5를 보였다. 각 사례에서 한 번 승인되고 실제 타깃에 한 번 피해가 발생했다. 첫 피해 시각 오차는 각각 0.013333/0.006667/0.020000/0.024444초이며 기존 허용 범위 안이다. 승인 후 live 공격속도를 1.4배로 변경해도 승인된 재생률과 타격 예약 snapshot은 유지됐다.

실제 이동속도 300/600/900/1200cm/s에 도달했으며 ground player rate는 1/1/1.5/2였다. 600 이하에서는 기존 walk/jog sample blend를 유지하는 설계이므로 300의 재생률을 다시 0.5배로 만들지 않는다. 발의 component-space 이동량도 각 이동 사례에서 0보다 충분히 컸다. 이 검증이 속도별 정확한 발 주기 비율이나 접지 미끄러짐까지 직접 측정했다고 해석하지 않는다.

paired Source lower-body 회전 오차는 최대 0.000002958도, render-bone local translation·scale 오차는 0이다. Source 손목·손가락 회전 오차는 0.000003818도다. Appearance의 8개 팔다리 segment 길이 변화는 최대 0.000005157%였다. 이는 Source의 local 본 보존과 target의 해당 길이 검증이며, Appearance의 모든 손가락 local 회전·T/S 보존 또는 실제 표면 변형 품질을 증명하지 않는다.

최종 실행의 독립 계산, Fist02·Moving01 회귀 요약과 입력 파일 SHA-256은 `independent-runtime04-analysis.json`에 남겼다. runtime02·03 분석은 이전 단계 증거로 별도 보존했다.

## 첫 실행 실패와 판정 보정

runtime01은 61개 실패로 보존했다. 성공으로 다시 분류하지 않았다.

- montage rate 실패 37개: UE5.8 `UAnimInstance::Montage_GetPlayRate`는 active instance만 검색한다. QA가 실제 contributing inactive blend-out instance를 얻은 뒤 이 getter를 호출하여 0을 비교했다. 실제 `FAnimMontageInstance::GetPlayRate/GetPosition`을 읽도록 수정했으며 active montage clock과 공격속도 snapshot 검사는 유지했다.
- late mouse 실패 8개: 기대값이 측정된 약 3도 residual을 현재 2도 camera 상한으로 최종 제한하지 않았다. 독립 quaternion clamp를 추가했다.
- Source/Appearance 머리 실패 각각 8개: montage blend-out 종료 직후의 상태 snapshot 시점 문제였다. production proxy가 PreEvaluate에서 상태 상한을 다시 복사하도록 수정했고 runtime02에서는 같은 상태 전환 비교를 완화하지 않은 채 통과했다.

## 실제 production 평가 순서의 착지 회귀와 수정 확인

runtime02 fixture는 Source에 CharacterMovement tick prerequisite를 추가했다. 이 설정은 같은 이동 상태의 paired pose 비교를 안정화했지만, production의 누락된 순서를 검증 단계에서 고쳐 버리는 한계가 있었다.

강제 prerequisite가 없는 Fist01은 840프레임을 완료했으나 착지 frame 781에서 실패 1개를 보고했다. frame 780은 공중 상태, frame 781은 이미 지상 상태였다. 이때 Appearance head residual은 2.995104668도, 현재 지상 상한은 2도였다. 카메라는 즉시 2도로 제한했고 다음 frame 782의 머리는 2도가 됐다. Source가 movement landing보다 먼저 평가되는 실제 순서가 원인으로 확인됐다.

production Source→CharacterMovement 순서를 StylizedCharacter의 실제 component lifecycle에 추가했다. 신규 집중 fixture는 해당 prerequisite 존재를 assert하며, Source 순서를 임의로 추가하거나 제거하지 않도록 변경했다. 머리 비교에 이전 상태 상한을 허용하거나 전역 3도 허용치를 넣지 않았다. 수정 후 runtime03·04와 Fist02에서 재확인했다.

Fist01의 부분 통과로 당시 실패를 상쇄하지 않았다. 새 Fist02는 840프레임, 실패 0개였다. camera.csv의 Appearance 머리·카메라 상태 상한 위반은 모두 0개다. 이전에 실패한 착지 frame 781도 현재 2도 상한을 지킨다. Shadow의 head/hand/foot 위치 오차는 0cm, 회전 오차는 최대 0.000002958도였다. damage.csv에서 실제 피해를 받은 스킬 9개와 총 피해 pulse 14개를 독립적으로 셌으며, 태산압정의 6회 피해를 포함한 기존 예상과 일치했다.

## 이동 공격 회귀

Moving01은 1,100프레임, 258,033 assertion, 실패 0개였다. 실제 공격 중 Source 하체 회전 오차는 최대 0.000002415도, translation 오차는 0cm, retarget Appearance 하체 회전 오차는 0.000002415도였다. 정지 상태에서 재생 중인 원본 sequence와 비교한 자세 오차는 최대 0.051944도였다. 실제 이동 거리 1,714.300089cm, 발의 component-space 누적 이동 3,625.436654cm, 점프 높이 89.988889cm를 기록했다. 실제 피해 CSV의 timing 최대 오차는 0.033178초였다.

이 회귀의 neutral 모드는 Source/Baseline의 머리 보정을 끄고 원래 raw full-body pose·이동 공격 하체·FootIK·피해 검사를 유지한다. 마우스 머리 분리의 완료 근거는 머리 보정이 켜진 runtime04/Fist02로 구분한다.

Moving01은 build 4에서 production 순서 수정 전 실행했지만 fixture가 이미 동일한 Source→CharacterMovement prerequisite를 사용했다. build 5의 해당 증분은 이 순서를 production에 추가한 것이므로 같은 조건의 raw pose 검사를 이유 없이 반복하지 않았다. build 6은 fixture의 촬영 일정과 기본 시야 사례만 변경했다. 실제 production 순서 자체는 runtime03·04의 prerequisite assertion과 강제 prerequisite가 없는 Fist02의 착지 회귀가 검증한다.

## Negative control과 실패한 첫 촬영 보존

negative01은 동일 production 동작의 build 5에서 원래 21개 사례·1,575프레임에 머리 분리만 껐다. 예상 실패는 3,151개였다. Source와 Appearance 머리 상한 검사가 각각 모든 1,575프레임을 거절했고, 평가된 mouse basis를 전제로 하는 component-rotation transport 검사 한 건도 실패했다. native basis가 없으므로 camera는 authored fallback을 사용하며 이 transport 기대값을 만족할 수 없다.

negative의 카메라 회전·위치 상한 위반은 모두 0개였다. 카메라만 제한한 구현이 실제 머리 방향 검사를 통과하지 못한다는 감지 증거다. 다른 입력·공속·실제 피해·본 보존 관련 실패는 보고되지 않았다. build 6은 같은 negative branch나 production을 변경하지 않고 기본 시야 사례와 촬영 일정만 추가했으므로 이 결과를 24개 negative 사례로 확대하지 않는다.

render01은 756장의 실제 PNG를 생성했지만 입력·공격 관측 assertion 5개가 실패했다. 75프레임을 36프레임으로 줄인 촬영 최적화가 이전 공격의 실제 cooldown/busy 기간과 우클릭 charge 유지 기간을 손상한 fixture 오류였다. 모든 756프레임의 머리·카메라·위치 상한은 지켰지만 실패한 촬영을 성공 증거로 제공하지 않는다.

새 render02는 runtime과 동일한 75프레임·우클릭 release 43프레임을 사용하고 실제 피해 assertion도 유지한다. cooldown을 임의로 초기화하거나 승인 개수 검사를 없애지 않았다. 촬영은 24개 사례·1,800프레임이며 손·팔 구조 확인을 위해 render 프로세스의 `r.MotionBlurQuality`만 0으로 설정했다. 프로젝트 설정은 변경하지 않았다. negative01/render01 독립 분류는 `independent-negative01-render01-analysis.json`에 보존했다.

## 최종 렌더 수치와 실제 화면 검토

render02는 24개 사례·1,800프레임·489,428 assertion에서 실패 0개였다. Source/Appearance/camera 회전과 camera local 위치 상한 위반은 모두 0개다. 기본 -20도 idle과 1인칭/V 잽을 포함하여 runtime과 동일한 실제 입력·속도·타격 검사를 유지했다.

`Saved/Screenshots/WindowsEditor/MouseHeadStable/stable_render02`에는 frame_0000부터 frame_1799까지 끊김 없는 PNG 1,800장이 있다. 모든 PNG header와 1280×720 크기를 확인했다. fixed 30fps의 60초 실제 프레임이며 그리지 않은 모형 이미지로 대신하지 않았다.

원본 PNG의 걷기 frame 255, V 걷기 1305, 현재 기본 idle 1595, 기본 1인칭 잽 1666/1669/1682, V 대기·잽·guard 1734/1743/1770을 직접 보았다. V 장면에서 머리와 목, 어깨·팔·다리의 연결은 유지됐고 검토한 대표 프레임에서 관절 길이 붕괴나 뒤돌기를 관찰하지 않았다. V 걷기에서는 팔 swing과 다리 cycle이 보인다. 정상 마우스 시선과 공격 제스처를 분리하여 머리를 고정한 범위의 결과다.

1인칭 -20도 idle/잽에서는 팔이 시야 아래에 있거나 기존 하단 HUD에 상당 부분 가려졌다. 따라서 실제 1인칭 손가락 표면을 전부 시각적으로 인증했다고 표현하지 않는다. Appearance Mesh와 머리 카메라를 유지하는 현재 framing/HUD의 관찰 한계로 기록하며 이번 검증에서 카메라 높이나 HUD를 추가 변경하지 않았다.

fixture의 충돌 floor는 기존 테스트 맵보다 1,000cm 위에 있고 보이는 floor mesh가 없다. 화면의 기존 바닥이나 노란 구조물과 발 사이 거리는 실제 collision floor의 접촉 여부를 뜻하지 않는다. 이 렌더를 모든 외형의 실제 지면 접촉 인증으로 사용하지 않는다. 수치 하체/FootIK 검사는 별도 보존했고, 화면의 관절 연결 검토와 구분한다.

독립 render CSV 계산, PNG 연속성·크기 검사, 원본 대표 샷 SHA-256과 관찰은 `independent-render02-analysis.json`에 기록했다. QA 영화의 사례 전환은 마우스 pitch/yaw와 V 모드를 의도적으로 바꾸므로 이런 시점 전환을 authored head-spin으로 해석하지 않는다.

## 자산과 저장 보존 확인

새 Editor 프로세스에서 다시 로드한 `fresh-asset-validation.json`은 PASS였다. 스킬 74개, 외형 profile 13개, 활성 montage 69개, 활성 source 26개를 감사했고 unsafe active range는 0개다. montage 외 공격 필드, profile 연결, 지정한 ground graph 수정 범위가 보존됐다.

`post_application_preservation02.json`은 PASS이며 기존 content 37개 변경과 새 파생 자산 40개를 기록했다. 예상 외 content 변경은 0개, production save 3개의 hash 불일치는 0개, 새 production save도 0개다. 원본을 직접 삭제하거나 덮어쓴 것으로 해석하지 않는다.

## 검증 범위의 한계

이 문서는 완료된 수치 실행, 선택 원본 렌더 검토와 파일 보존 검토다. 모든 1,800장의 미세 표면 변형, 메시·LOD 전환, 모든 외형·무기의 실제 플레이, 사망 상태까지 검증했다고 주장하지 않는다. 전체 Appearance 손가락 local 본 보존, 모든 피부 weighting과 모든 지면 접촉도 인증하지 않는다. 카메라/머리 상한과 실제 속도·입력·하체 보존에 대한 완료 판정과 이 한계를 구분한다.
