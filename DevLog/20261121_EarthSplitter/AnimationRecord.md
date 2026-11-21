# 대지파쇄 애니메이션 기록

- 프로젝트 제작 Manny 가드와 기존 Mixamo 파생 `A_GreatSword_AltSlash05Repair1`을 Blender 4.5.14에서 편집했다. 원본과 편집본은 로컬에 보관한다. 원본 타격 몽타주는 `AM_GreatSword_Charged`, 원본 시퀀스는 `MM_ChargedAttack`이다. 파생 시퀀스 네 개는 `/Game/Rogue10m/Animation/Combat/FighterSkills/GreatSword/`에 반입했다.
- 60Hz에서 차징 44개, 전개·회복 111개 표본을 작성했다. 준비는 0.716667초에 마지막 자세에 도달한다. 전개·회복은 원본 재생 위치를 읽으며 1.833333초 범위에서 진행한다. 월드 액터 틱이 끝날 때의 실제 표시 손바닥, 검 길이축, 원본 시간, 캐릭터 캡슐과 피해를 기록했다.
- StartHub의 철제 장검과 전수 스킬 슬롯에 프로그램 우클릭을 보냈다. 수치 실행에서는 0.800초의 홀드 뒤 놓았고 대지파쇄는 수락 프레임에 한 번 타격했다. 720p 실행은 준비 22, 차징 중간 49, 차징 완료 78, 첫 피해 후 80, 회복 248프레임에서 기록했다. [첫 피해 후 대표 화면](first-damage-720p.png)
- Blender 편집본은 로컬 `tmp/EarthSplitterMotion20261121/{v3,female-v4}/MannyGreatSwordEarthSplitter.blend`에 있다. 가장 이른 후보의 루트 기준 오류와 다른 체형에서 검 방향이 틀어지는 사례도 수치로 보존했다. [작업과 검증](../../Feature/doc/2026-11-21_greatsword-earthsplitter-motion.md)
