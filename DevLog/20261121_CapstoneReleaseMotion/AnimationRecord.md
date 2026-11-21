# 패왕참 발동과 가드 회복

2026-11-21. `Sprint#28-2-capstone-release-motion`의 물리 작업본은 `D:/Project/Rogue10m/tmp/GreatSwordUltimateWorktree20261121`이다. 기준 `55ab2ddcb4cbe99322b90b33d60902a99b45e02e`에서 같은 StartHub 프로그램 우클릭 경로로 이전 발동과 Blender 편집본을 비교했다. 실제 플레이어 입력 경로가 키 이벤트를 처리했으며, 별도의 물리 마우스 조작과 필드 완주는 실시하지 않았다.

| 항목 | 사용 경로 |
| --- | --- |
| 기술 데이터 | `/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_Ultimate` |
| 이전 몽타주 / 시퀀스 | `/Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Charged` / `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack` |
| 수정 몽타주 / 시퀀스 | `/Game/Rogue10m/Animation/Combat/FighterSkills/GreatSword/AM_GreatSword_CapstoneRelease`, 같은 폴더의 `A_GreatSword_CapstoneRelease` |
| 편집 출처 | 프로젝트에 반입되어 있던 Mixamo 검술의 `GreatSword20261110/A_GreatSword_Charged`와 같은 폴더의 `A_GreatSword_Idle` |
| 골격 | `/Game/Characters/Mannequins/Meshes/SK_Mannequin` |

Blender 4.5.14에서 프로젝트 재료의 베기 구간을 다시 배치하고 두 팔의 길이를 유지하며 손바닥을 손잡이 방향에 맞췄다. Blender 작업 파일과 FBX는 로컬 작업본의 `tmp/UltimateReleaseRuns/candidate-b/`에 있다. Manny 89본, 60Hz, 111개 키 샘플, 첫/마지막 가드 위치 편차 0cm, 루트 이동 0cm. Blender 재반입 위치 오차는 최대 0.000265cm였다. Unreal에서는 기존 몽타주의 `DefaultSlot`, 1.833333초, 0.25초 진입과 회복, 루트 정책, 빈 노티파이 목록을 원본과 대조했다.

| 실제 720p 프레임 | 검증 프레임 | 월드 시간 | 몽타주 시간 |
| --- | ---: | ---: | ---: |
| 차징 | 116 | 5.950000초 | 시작 전 |
| 우클릭을 떼고 첫 피해 | 118 | 5.983334초 | 0초 |
| 검이 몸통 캡슐 중심에 가장 가깝게 교차 | 209 | 7.500000초 | 1.007706초 |
| 가드 회복 | 286 | 8.783334초 | 종료 후 |

세 사진은 Microsoft 소프트웨어 D3D11 렌더러에서 1280×720으로 생성했다. 각 촬영 요청과 완료의 게임 프레임 차이는 0이었다. 손목과 손바닥은 보이는 몸의 컴포넌트 좌표에서 cm로 측정했다. 검날은 실제 장착 메시의 양 끝을 월드 좌표로 옮겨 표적 캡슐과 거리 및 바닥 간격을 계산했다. 60Hz 수치 검증에서 같은 110cm 표적에 대한 첫 칼 교차는 0.492900초였으나 피해는 공격 수락 시점에 들어간다. [대표 사진](contact-110cm.png)

StartHub를 별도 저장 프로필에서 `-game -RogueMeleeVfxRun -RogueCapstoneBraceQA -RogueCapstoneReleaseQA -UseFixedTimeStep -FPS=60 -ExecCmds=Rogue10m.TestWeaponThirdPerson`으로 실행한다. `-NullRHI -RogueMeleeVfxLogicOnly`를 더해 차징 자세 설정 전후 전체 2건을 기록한다. `-RogueCapstoneReleaseRender`는 720p 하드웨어 또는 소프트웨어 검증의 캡처 분기를 연다. 측정 예제와 단계별 결과는 기능 증거 폴더에 있다. 첫 Blender 점검 탈락과 Unreal Python 노티파이 접근 두 건의 실패도 집계에 보존했다. [검증 결과](../../Feature/doc/evidence/greatsword-capstone-release-motion-20261121/runtime-results.txt)
