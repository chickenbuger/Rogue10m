# 오크 남성의 대지파쇄

작업일은 2026-11-21이다. 브랜치 `Sprint#29-3-orcmale-earth-pose`와 물리 작업 사본 `D:/Project/Rogue10m/tmp/OrcMaleEarthWorktree20261121`에서 편집했다. 공식 `9cd829f3`에 드워프 표시 자세 소스 05를 적용해 검증한 뒤, 벽 접근에 쓰는 62cm 시작 간격의 06 자세를 촬영했다. 실제 게임에 설정된 애니메이션 블루프린트는 별도 실행으로 반입과 시작을 확인했다.

| 항목 | 자산 및 경로 |
| --- | --- |
| 실제 외형·골격 | `/Game/StylizedCharacter/Meshes/Character/Orc/Male/SK_Or_M_FullBody`, 같은 폴더의 `SKEL_Or_M` |
| 편집한 프로젝트 자산 | `/Game/Character/Customization/Retargeting/ABP_Retarget_Or_M` |
| 유지한 IKR | `/Game/Character/Customization/Retargeting/IKR_Manny_To_Or_M` |
| 활성 기술 | `/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_EarthSplitter` |
| 시간·피해를 정하는 활성 몽타주 | `/Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Charged` |
| 표시 손 자세의 Blender 시퀀스 | `/Game/Rogue10m/Animation/Combat/FighterSkills/GreatSword/A_GreatSword_EarthSplitterCharge`, 같은 폴더의 `A_GreatSword_EarthSplitterRelease` |

새 몸체의 포즈 노드는 프로젝트 스크립트 `Scripts/Editor/CreateInheritedCharacterAssets.py`가 만든 `RetargetPoseFromMesh`와 `Root`다. 두 노드, 부모 메시 입력, 대상 골격, IKR, 두 LOD 한계 −1을 보존했다. 원본 파일 SHA256 `d5f58ae1466fab8db96296e79b273da474e53ac2d03e6ddc5fb3e17efbbc5517`, 변경 뒤 `dc0e00dbd867d882aa9423ccb7b5b24a07c937547c07ab0b1aa01c0339430de0`. 초기 검증에 쓴 복제 자산은 작업 폴더 바깥의 로컬 증거에 보관했다.

| 720p 촬영 단계 | 검사 프레임 | 월드 시간 | 몽타주 시간 |
| --- | ---: | ---: | ---: |
| 준비 | 22 | 4.616667초 | 공격 시작 전 |
| 차징 중 | 49 | 5.066667초 | 공격 시작 전 |
| 최대 차징 | 78 | 5.550000초 | 공격 시작 전 |
| 최초 피해와 배치된 검날 | 80 | 5.583334초 | 0.010953초 |
| 기존 자세 복귀 | 248 | 8.383334초 | 공격 종료 뒤 |

기존 피해 수락 틱에 실제 피해가 들어갔다. 표시 검과 캡슐을 월드 공간에서 따로 비교해, 시전자로부터 110cm에 둔 표적 캡슐 안쪽으로 15.35cm 닿았다. 같은 자세에서 검 끝은 시전자 캡슐 바닥보다 18.20cm 높았다. 손바닥은 표시 몸의 손목과 중지 첫 소켓 사이 중점이며, 거리와 검날 바닥 여유는 cm다. 기록 프레임과 화면 요청·완료 지연을 같은 시간 기준으로 비교했다. [최초 피해 장면](first-damage.png)

StartHub의 분리 프로필에서 실제 장비 장착 후 `Rogue10m.TestWeaponThirdPerson`을 `-RogueMeleeVfxRun -RogueGreatSwordTechniqueQA -RogueGreatSwordEarthSplitterAudit -RogueGreatSwordEarthSplitterAppearance=OrcMale -UseFixedTimeStep -FPS=60`과 함께 실행한다. `-NullRHI -RogueMeleeVfxLogicOnly`는 수치용, `-RogueGreatSwordEarthSplitterCapture`는 720p 프로그램 입력 캡처용이다. `-RogueGreatSwordEarthSplitterMoving`은 늦은 D, `-RogueGreatSwordEarthSplitterWall`은 회전 벽 시험을 연다. 벽 파일에서 `-RogueOrcEarthFastWallQA`로 0.18초 회전도 시험했다. 뒤 구르기는 실제 Ctrl+S 프로그램 입력이 들어가는 `Rogue10m.TestHybridSkillTree` 경로를 사용했다. 물리 키 조작과 필드 완주는 이번 검증 범위에 포함되지 않았다.
