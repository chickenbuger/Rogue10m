# 대검 파쇄참 동작 기록

- `Sprint#26-1-greatsword-guard-break-motion`, 독립 작업 사본 `GreatSwordTechniqueWorktree20261121`. 기준은 `62e1f6d4`다. 정면 거리 85·110·140cm와 범위 밖의 네 사례에서 실제 Q 입력을 보냈다.
- 프로젝트에 있던 Mixamo 기반 `A_GreatSword_Primary03`과 `A_GreatSword_Idle`을 Blender 4.5.14에서 편집했다. 짧은 내려치기와 가드 복귀를 위해 동작의 시간을 다시 배분했다. 시작·끝 자세와 캡슐 기준 루트를 고정했다. Manny 뼈 89개와 시퀀스 1.666667초, 60Hz를 재반입해 확인했다.
- 파쇄참 `DA_Hybrid_GreatSword_GuardBreak`는 새 `AM_GreatSword_GuardBreak`, `A_GreatSword_GuardBreak`를 로드했다. 실제 피해 1회는 수락 틱에 들어갔고 화면상 검이 캡슐과 깊이 겹치는 시점은 110cm에서 0.767초 뒤였다. [데이터와 타이밍](../../Feature/doc/2026-11-21_greatsword-guard-break-motion.md)
- 프로그램 Q 입력의 준비, 피해 직후, 사선 타격과 회복을 측면 카메라에 기록했다. 대표 화면은 수락·첫 피해 46프레임 뒤이며, 몽타주 0.697480초의 검과 표적, 양손과 양발이 보인다. [대표 화면](downstroke.png)
