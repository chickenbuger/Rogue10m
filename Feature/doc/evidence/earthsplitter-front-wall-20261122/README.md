# 정면 벽 대지파쇄 검증

기존 `Rogue10m.TestWeaponThirdPerson`에 `-RogueGreatSwordTechniqueQA -RogueGreatSwordEarthSplitterAudit -RogueGreatSwordEarthSplitterFlushWall`을 더하면 StartHub에서 실제 입력과 정면 벽의 검 위치가 기록된다. `-RogueGreatSwordEarthSplitterAppearance=HumanMale`을 외형에 맞게 바꾸면 같은 조건에서 여섯 외형을 선택할 수 있다. 기본 벽 간격은 캡슐 앞 2cm다. `-RogueGreatSwordEarthSplitterFlushGap=66`은 반지름 34cm인 캡슐에서 캐릭터 중심과 벽 사이를 100cm로 만든다. `a.Rogue10m.EarthSplitterFrontWall 0`은 비교 실행에서만 정면 보정을 끈다.

결과 폴더 명은 `-RogueWeaponLabel`로 지정한다. 따로 확보한 이전·이후 폴더 명을 이 디렉터리의 `measure_front_wall.py 기존이름 수정이름 --compare --expect-clear`에 전달하면 검 모델과 벽의 교차, 세계에 놓인 검의 최대 틱 변화, 최초 피해 틱을 확인한다. 열린 공간이나 옆벽 두 실행을 대조할 때는 `--compare-controls`를 쓴다.

실제 720p 촬영에서는 캐릭터 가까이 보이는 렌더용 벽과 게임의 검증용 충돌 상자를 같은 위치에 두었다. 수치 표본은 준비·차징·공격·회복 전체이며 화면은 각 단계에서 고른 검사 프레임이다. [애니메이션 기록](../../../../DevLog/20261122_EarthSplitterFront/AnimationRecord.md)

최종 Editor 빌드는 `Candidate03`이다. 여섯 외형과 좁은 정면 벽, 100cm 추가 대조, 열린 공간·오른편 벽, 발동 후 D 및 일반 무기 회귀에서 검사 로그의 새 오류는 0개였다. `rejected-recovery-candidate.json`에는 가드 복귀 때 관통한 첫 후보의 독립 실행을 보관했다.
