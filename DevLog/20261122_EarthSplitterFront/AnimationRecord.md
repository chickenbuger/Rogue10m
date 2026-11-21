# 정면 벽 대지파쇄 애니메이션 기록

- `Sprint#31-1-earthsplitter-front-wall`의 StartHub 외형 검증이다. `Rogue10m.TestWeaponThirdPerson`에 `-RogueGreatSwordTechniqueQA -RogueGreatSwordEarthSplitterAudit -RogueGreatSwordEarthSplitterFlushWall`을 더했다. 같은 빌드에서 `a.Rogue10m.EarthSplitterFrontWall 0`으로 이번 보정만 꺼서 대조했다.
- `/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_EarthSplitter`의 `AM_GreatSword_Charged`가 그대로 입력과 피해 시계를 구동한다. 마우스 우클릭을 0.800000초 유지한 뒤 놓았고, 첫 피해는 수락 틱과 원본 몽타주 첫 표본 0.010953초에 관측됐다.
- `A_GreatSword_EarthSplitterCharge`와 `A_GreatSword_EarthSplitterRelease`를 제작했던 Blender 4.5.14 Manny 장면을 바탕으로 검을 세운 양손 회수 자세를 별도 장면에서 편집했다. 60fps로 154프레임과 89본을 포함하며 새 Blender 프로세스에서 열어 확인했다. 게임에서는 실제 표시 외형의 팔 길이와 벽 각도에 맞춰 적용한다. [원본 편집 기록](../20261121_EarthSplitter/AnimationRecord.md)
- 표시 검은 `/Game/Weapons/Starter20261110/SM_StarterGreatSword`를 사용했다. 60Hz 내에서 벽 로컬 좌표의 실제 검 위치·회전·스케일을 당시 충돌 상자와 비교했다. 촬영에서 보이는 효과와 검 모델은 구분했다. 결과는 에디터 프로그램 입력과 720p 소프트웨어 렌더에 해당한다.
- 최종 720p의 처음 피해 요청과 저장은 같은 검사 80프레임(월드 5.583334초, 몽타주 0.010953초)에 완료됐다. 182프레임(7.283334초, 몽타주 1.128192초)의 이전 검날은 정면 검증용 벽 너머에 보인다. 이후 버전은 검을 벽 앞에 세웠다. 실제 준비 사진은 22프레임에 저장됐다. 촬영용 장면을 설치하는 11프레임에는 검의 직전 표시 변환이 한 번 기록됐고, 그 뒤 표시 준비·차징·공격·회복은 벽 안쪽으로 들어가지 않았다. [대표 화면](front-wall-follow-through-720p.png) · [검증 결과](../../Feature/doc/2026-11-22_earthsplitter-front-wall.md)
