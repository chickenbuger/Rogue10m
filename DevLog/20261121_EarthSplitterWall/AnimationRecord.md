# 벽 옆 대지파쇄 애니메이션 기록

- `Sprint#30-1-earthsplitter-wall-recovery`의 StartHub 검증이다. 아린에게 배운 철제 장검의 대지파쇄를 60Hz 프로그램 RMB로 실행했다. `Rogue10m.TestWeaponThirdPerson`에 `-RogueGreatSwordTechniqueQA -RogueGreatSwordEarthSplitterAudit -RogueGreatSwordEarthSplitterWall`을 더했다. `a.Rogue10m.EarthSplitterWallRecovery 0`으로 같은 동작의 후속 보정을 꺼 대조했다.
- `/Game/DataAsset/AttackSkill/HybridSkillTree/GreatSword/DA_Hybrid_GreatSword_EarthSplitter`의 `AM_GreatSword_Charged` 재생 시계와 피해 입력을 유지했다. `/Game/Rogue10m/Animation/Combat/FighterSkills/GreatSword/A_GreatSword_EarthSplitterRelease`는 기존 Manny 골격과 프로젝트의 Mixamo 파생 대검을 Blender 4.5.14에서 편집한 시퀀스다. 이 작업에서는 해당 시퀀스의 원본 시간과 기존 화면의 양손 골격을 사용했다. [기존 Blender 편집 기록](../20261121_EarthSplitter/AnimationRecord.md)
- 손바닥 두 점과 오른쪽 손가락 축으로 철제 장검을 복원했다. 실제 모델의 삼각형과 검증용 정적 장애물 상자를 비교했다. 통과 수치는 60Hz 각 틱의 실제 표시 자세다. 원본 몽타주 0.70~1.02초에 후속 자세로 이행하고 1.37~1.62초에 복귀한다.
- 인간 남성의 보정 전후 720p 화면에서 해제 후 1.700초, 원본 몽타주 1.128192초의 검을 비교했다. 최초 피해는 별도로 80프레임의 원본 0.010953초에서 기록됐다. 후속 장면은 두 쪽 모두 검사 182프레임에서 요청과 같은 평가 프레임에 저장됐다. [대표 화면](wall-follow-through-720p.png) · [같은 시점의 기존 화면](../../Feature/doc/evidence/earthsplitter-wall-recovery-20261121/stock-follow-through-720p.png)
