# 준비된 보스 입력 QA와 기존 모션 관찰

2026-11-01 UTC. 프로젝트 D:\Project\Rogue10m, 브랜치 Sprint#9-21-prepared-run-input-qa, 기준1928869d. 적용·빌드·실행한 QA의 결과와 미확정 시각 문제를 기록한다. 최종 로컬 커밋은 이 문서의 Git 이력에서 확인한다.

## 먼저 볼 결과

BasicRun1101I는 실제 메뉴 → 캐릭터 생성 → NPC F 대화/보급 → 필드 → 실제 대검 overlap 획득 → 자연 처치 XP → UMG 학습 → 보스 도달 → 자연 사망 → +6 정산 → 마을 복귀를 프로그램 입력으로 통과했다. 보스 승리는 실패했고 `full_boss_completion=0`이다. 물리 키보드/마우스 증거는 아니다.

보스 HP는 시작 10000, 최저 9937.703, 마지막 sample 9939.703이다. 자연 회복이 있으므로 차이 62.297을 순수 누적 피해량이라고 부르지 않는다. 패턴을 읽은 전략의 Alt 요청 5회가 모두 실제 회피 상태·Stamina 6 소모·velocity로 수락 관측됐지만 피격 회피 성공은 별도이며 이번에는 사망했다.

실행 전 340개 저장은 전부 SHA256 보존됐고 허용된 새 QA 2개만 생겨 342개가 됐다. 종료 후 독립 검사에서도 원본31개 자산과 맵 SHA256이 전부 동일했다. 새 애니메이션/에셋/세이브 형식/게임 규칙 변경은 없다.

## 실행 방식과 재현 경계

- UE5.8, 렌더링된 standalone 게임. 소스 QA는 `WITH_EDITOR`이며 QA 전용 fresh 세 슬롯과 label을 요구한다.
- 기록된 옵션: `PreparedBossStrategy`, `AnimationEvidence` ON. 기본값은 둘 다 OFF다. field 이동은 걷기/일반 회복으로 실제 회피 비용과 기본 공격 예산을 남긴다. 공개 Windup/Recovery 상태에 맞춰 Controller InputKey/DoAim, local Slate 문자 입력과 실제 UMG delegate를 사용한다.
- 이번 프로세스 wrapper 시작 `04:55:27.4586449Z`, 종료 영수증 `05:02:26.0319808Z`; PID 43024 exit 0은 확인했다. driver `wall_seconds=400.675`, 게임 제한은 원래 1200초다. wrapper 1560초는 런 규칙이 아니다.
- 같은 label/세이브/출력은 재사용하지 않는다. 재현 후보는 새 label로 `VerifyBasicRunPlay.ps1 -PreparedBossStrategy -AnimationEvidence`를 선택하는 것이며, UI 제어권과 빌드 확인 후 실행한다. 재현 시 기존 증거와 라벨은 보존한다.
- 패턴 상태를 직접 읽는 자동 전략은 사람의 인지·반응 시간이나 밸런스 공정성 증거가 아니다. HP/XP/위치/자원 직접 설정, 텔레포트, 무적, 강제 승리, 직접 피해/보상 지급은 사용하지 않는다.

## 증거 원본과 복사 상태

원본: `DevLog/BasicRunPlayQA/BasicRun1101I/Driver/` 아래 `summary.txt`, `samples.csv`, `events.csv`, `input-frames.csv`, `movement-diagnostics.csv`, `animation-assets.txt`, `animation-events.csv`, `screenshots.csv`, `screenshots/`.

아래9개 실제 PNG를 이 폴더로 복사하고 모두1280×720 디코딩과 SHA256 검증을 마쳤다. 이미지를 직접 열어 아래 시각 문제도 확인했다. 자동 내부 입력 렌더이며 물리 플레이 결과가 아니다. 원본과 복사 영수증은 VERIFICATION.json에 기록했다.

| 파일 | 기록 목적 | 실제 판정 |
|---|---|---|
| [16_animation_attack_ready.png](16_animation_attack_ready.png) | 공격 전 준비 | processed phase/file match. 직접 이미지 관찰: 손이 보이지 않음/블러. 준비 자세 가독성 PASS 아님. |
| [19_animation_attack_first_damage.png](19_animation_attack_first_damage.png) | 첫 HP 변화 근처 | phase_unconfirmed. 직접 이미지 관찰: HUD 70/70과 블러. 접촉 사진으로 인증하지 않음. |
| [21_animation_attack_recovered.png](21_animation_attack_recovered.png) | 공격 회복 | processed 시 다음 공격 상태로 phase_unconfirmed. 회복 자세 인증 불가. |
| [17_animation_dodge_ready.png](17_animation_dodge_ready.png) | Alt 전 준비 | processed phase/file match이나 손 준비 자세 시각 PASS 아님. |
| [18_animation_dodge_active.png](18_animation_dodge_active.png) | 회피 이동 중 | processed에서 dodging=1. 직접 이미지 관찰: 기존 몸통이 화면 대부분을 가림. 가독성 실패를 보존. |
| [20_animation_dodge_recovered.png](20_animation_dodge_recovered.png) | 회피 종료 | processed dodging=0/phase match. 회복 손 자세·품질 PASS와 구분. |
| [28_boss_reached.png](28_boss_reached.png) | 실제 보스 도달 | 보스 승리 사진이 아님. |
| [29_result.png](29_result.png) | 자연 사망 정산 | +6 로비 정산을 직접 화면에서 확인. 승리/시간초과 정산 검증으로 확대하지 않음. |
| [30_returned_hub.png](30_returned_hub.png) | 저장 후 마을 귀환 | reset/Discovery gate 통과. 다른 프로필·기존 세이브 이행 증거 아님. |

Driver summary의 `processed_phase_and_file_match_decode_pending`는 사후 디코딩 이전 드라이버 상태를 그대로 보존한다. 사후 PNG 디코딩이 성공해도 phase_unconfirmed나 시각 가림을 PASS로 바꾸지 않는다. `shots`의 빈 pending/queue는 요청 큐 종료이며 자세 품질 완료가 아니다.

## 실제 로드 자산: 기존/수정 여부

이번 packet은 아래 기존 자산을 관찰만 했다. 클립·몽타주·재생속도·타격 지연·회피 거리/시간·판정·피격/충돌을 수정하지 않았다. owner 신규 팔/프레이밍은 기본 OFF이며 기존 body 표현 문제와 신규 모드 효과를 섞지 않는다. 이전 정적 source snapshot만으로 현재 pose가 맞는다고 인증하지 않는다.

| 역할 | 실제 경로/관측 |
|---|---|
| 공격 skill | `/Game/DataAsset/AttackSkill/ClassCombat/Knuckle/DA_Attack_Knuckle_Primary01.DA_Attack_Knuckle_Primary01` |
| 공격 montage | `/Game/Rogue10m/Animation/Combat/FistAuthored/AM_Knuckle_Jab.AM_Knuckle_Jab` |
| 공격 sequence | `/Game/Rogue10m/Animation/Combat/FistAuthored/A_Knuckle_Jab.A_Knuckle_Jab`, montage slot0/segment0, authored start 0/end 1초 |
| source mesh | `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple`, 실제 Animation Source Mesh에서 읽음 |
| 표시 body | `/Game/StylizedCharacter/Meshes/Character/Human/Male/SK_Hu_M_FullBody.SK_Hu_M_FullBody` |
| 회피 skill | `/Game/DataAsset/DodgeSkill/DA_Dodge_StoneFist.DA_Dodge_StoneFist` |
| 회피 motion | `/Game/DataAsset/Character/Animation/DA_CommonCharacterMotion.DA_CommonCharacterMotion` |
| 회피 montage/sequence | `/Game/Rogue10m/Animation/Combat/HeadSafe/AM_Safe_Dodge_Step.AM_Safe_Dodge_Step` / `/Game/Rogue10m/Animation/Combat/HeadSafe/A_Safe_Dodge_Step.A_Safe_Dodge_Step` |

## Authored 값과 실제 관측 시간

Knuckle Jab montage/clip 1초, authored play rate `1.340000033`, hit-delay montage time `0.150000006초`, authored cooldown `0.319999993초`. 수락 관측 frame 14614/world `109.516640초`, 실제 montage rate `1.3936`. authored rate와 실제 rate를 같은 값으로 기록하지 않는다.

첫 HP 감소 관측 frame 14621/world `109.740478초`, target HP 70 → 49.488953. 수락 관측과의 차이는 약 `0.223838초`지만 damage delegate/execution ID가 아닌 post-tick 상태 표본이다. 도중 회피와 몽타주 전환도 있어 이를 정확 타격 지연이나 GPU 접촉 시각으로 인증하지 않는다. first-damage 사진 요청/processed 모두 phase_matches=0이다.

StoneFist authored distance `250cm`, duration `0.159999996초`, cooldown `0.550000012초`, Stamina cost `6`. Safe_Dodge_Step montage/clip `0.200000003초`, segment authored rate 1. authored distance는 실제 이동 거리 측정값이 아니다. 회피 이동 종료와 cooldown 완료는 별개이며 recovery 사진에는 일반 공격 cooldown이 일부 남아 있다.

첫 활성 관측 world `109.610503초` → 이동 종료 관측 `109.788604초`는 약 `0.178101초` 간격이고 종료 때 dodge cooldown은 `0.371895초` 남아 있다. 표본 간격/틱을 포함하는 관측값이며 authored duration과 정확 일치한다고 단정하지 않는다. 보스 prepared 전략의 5회와 전체 런에서 기록된 회피 이동 종료 14개도 서로 다른 집계다.

| 사진 단계 | 요청 frame/world초 | processed frame/world초 | processed 상태 |
|---|---|---|---|
| attack_ready | 14475 / 108.053280 | 14476 / 108.061613 | phase=1, 공격 busy=0 |
| attack_first_damage | 14621 / 109.740478 | 14622 / 109.788604 | phase=0, busy=1, montage inactive |
| attack_recovered | 14663 / 110.378530 | 14664 / 110.386864 | phase=0, 다음 busy=1 |
| dodge_ready | 14477 / 108.196222 | 14478 / 108.204556 | phase=1, dodging=0 |
| dodge_active | 14619 / 109.610503 | 14620 / 109.618836 | phase=1, dodging=1 |
| dodge_recovered | 14661 / 110.236576 | 14662 / 110.244910 | phase=1, dodging=0, attack_cd≈0.118초 |

캡처 요청/processed callback은 순차 기록이며 GPU가 실제 그린 픽셀 시각과 다를 수 있다. `phase_matches=1`만으로 손/몸 자세나 화면 품질을 보장하지 않는다. 회복 사진을 얻으려고 다음 정상 입력을 멈추거나 게임 타이밍을 지연시키지 않았다.

## 손 궤적 데이터의 좌표계와 한계

`animation-events.csv`의 `hand_l_*`/`hand_r_*`는 **Animation Source Mesh의 hand_l/hand_r 소켓 world 좌표(cm)**다. 소유자 화면의 픽셀 좌표나 실제 표시 Hu_M 손의 궤적이 아니다. 카메라/캐릭터 이동·회피·스켈레톤 차이가 포함되므로 두 프레임 차이를 주먹 타격 이동 거리로 해석하지 않는다.

| 관측 | 왼손 world (cm) | 오른손 world (cm) |
|---|---|---|
| 공격 수락 frame14614 | (-47971.207, -24408.813, 3892.171) | (-48033.777, -24409.980, 3886.515) |
| 회피 활성 frame14619 | (-48032.138, -24365.410, 3917.093) | (-48056.328, -24362.796, 3906.842) |

전체 37개 animation event 행과 frame/world/wall 시간은 원본 CSV에 남겼다. 첫 HP delta는 근사 관측이며 해당 순간의 공격 판정·실제 시각 궤적을 확정하지 않는다. 원본 뼈/스킨/retarget/mesh/원래 애니메이션을 바꾼 작업이 아니다.

## 완료와 미완료

완료: 두 옵션 source/launcher 연결, C4458 지역 이름 수정 후 11.45초 빌드, wrapper 순수 텍스트 gate 10/10(순수 텍스트 검사), 정상 입력 I 자연 사망 경로, +6 저장 정산/마을 복귀/선택 런 상태 reset 및 Discovery 유지, 기존 저장 340개 SHA 보존.

미완료: 애니메이션 준비·접촉·회복 자세의 유효한 시각 증명, 기본 body 가림/블러/손 가독성, 정상 보스 승리, 실제 물리 입력, 전체 20분 시간초과, 대검 UI 장착·충전·물약 전략, 60초 맵 목표, 4액티브 슬롯/프리셋, 일반 밸런스·패키징. 원본31개 자산·맵 해시 동일과 사진9개 복사·디코딩·해시는 최종 확인했다. 자세 품질과는 별개다. 다음 실제 UI 실행은 제어 인계 확인 후 진행하며  push/병합/업로드 없음.
