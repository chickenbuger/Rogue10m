# 머리·목 시선 분리와 이동 포즈 보정

2026-11-19 `Sprint#10-1-head-pitch-animation` 작업 사본에서 확인했다. 게임에 연결된 `/Game/Rogue10m/Animation/Locomotion/PalmReference/BS_Unarmed_PalmReference`의 대표 샘플을 Unreal에서 내보내 Blender 4.5.14에서 비교했다. 이 네 패키지는 내보내기 프로젝트와 런타임 검증 프로젝트에서 SHA-256이 일치했다.

| 동작 | 애니메이션 | 클립 길이 | 루트 모션 |
| --- | --- | --- | --- |
| 대기 | `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle` | 7.567초 | 꺼짐 |
| 전진 걷기 | `/Game/Rogue10m/Animation/Locomotion/PalmReference/MF_Unarmed_Walk_Fwd_PalmReference` | 1.500초 | 켜짐 |
| 전진 달리기 | `/Game/Rogue10m/Animation/Locomotion/PalmReference/MF_Unarmed_Jog_Fwd_PalmReference` | 1.767초 | 켜짐 |

원본 스켈레톤은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이다. 전진 이동 클립은 10월 9일 프로젝트 파생본을 사용했다. 원본 파일과 보정 후보를 Blender의 별도 액션으로 비교했다. 포즈 공간에서 루트·골반·손·발의 위치 차이는 0 cm였고, 걷기와 달리기의 머리 루프 이음새는 각각 0.135°, 0.270°였다. [이동 클립 출처](../../Feature/doc/2026-10-09_walking-palm-reference.md)

공격 회귀는 기존 `/Game/DataAsset/AttackSkill/BasicBrawler/DA_BasicBrawler_LeftJab`의 입력 수락을 검사했다. 이 검사의 타격 준비·첫 피해·회복 실측과 손 궤적은 범위 밖이다. Blender 비교 렌더는 본과 자세를 확인하기 위한 프록시이며 게임 메시 외형은 별도 StartHub 렌더에서 확인했다. [최종 검증 결과](../../Feature/doc/2026-11-19_head-pitch-animation.md)
