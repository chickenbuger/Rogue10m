# 대검 스킬트리

검 탭의 중앙 기본기 거미줄에 기술 아이콘, 읽기 쉬운 이름표, 선행에 따른 연결 밝기를 적용한다. 공용 성장 포인트로 선택하는 전수 방식과 공격·연계·방어 가지를 사용한다. 다른 탭에서 설정하는 아이덴티티 성질은 검의 공명 상세에서 출처를 표시한다.

| 작업 단위 | 수정 위치 | 완료 조건 | 검증 | 되돌리는 범위 |
| --- | --- | --- | --- | --- |
| 대검 그래프 | 기존 K 창 네이티브 위젯 | 중앙과 각 기술이 1080p 및 720p에서 구분되고 전수·대체 연결을 읽을 수 있다. | Editor 빌드, StartHub 게임 검증 및 화면 | 검 그래프 표시와 입력 영역 |
| 경로 안내 | 스킬트리 선행 조건 표시 | 실제 카탈로그의 대체 조건을 경로별로 표시하고 묶음의 선행을 모두 확인할 수 있다. | 전수 및 대체 조건 자동화·게임 검증 | 대검의 선행 표시 |
| 통합 | 격리 게임과 기록 | 1P 전수·거미줄 우회·잠금·입력·저장 복원과 너클 회귀를 확인한다. | HybridSkillTree 자동화, 출력 로그, 독립 재시동 | 회귀 검사와 증거 |

기존 대검 공격 데이터와 UI의 대검 아이콘, 무공 창 배경을 재사용한다. 기술별 아이콘이 없는 다섯 노드는 기술 동작을 압축해 Slate에서 선으로 구분한다. 카탈로그의 노드·연결은 검증 기준이며 시안은 색·아이콘·가독성 기준이다. [대검 시안](../doc/evidence/greatsword-skill-web-20261120/concept.png)

## 시안 생성 기록

기본 ImageGen에서 이전 실제 검 탭의 720p 캡처를 구조 참고로, 승인된 너클 수련 시안을 표현 참고로 사용했다. [대검 시안](../doc/evidence/greatsword-skill-web-20261120/concept.png) · [너클 수련 시안](../doc/evidence/knuckle-training-tree-20261120/approved-concept.png)

```text
Create ONE widescreen 16:9 concept mockup for Rogue10m's Greatsword Skill Tree (a custom game). Reference 1 is the existing rendered game and the structural reference — preserve its top navigation, left detail panel, approximate 20-node web layout and anchor placements, all original project-specific labels from the list below, and the radial branches centered on a basic attack hub. Reference 2 is the aesthetic reference — adapt its tactile charcoal and brass medallion UX, with cooler forged iron and silver sword pictograms. Improve legibility at gameplay scale, don't recreate any existing proprietary game's surface. Edges are thin with clear crossings; active path burnished gold, viable route lighter cool steel, locked routes quiet slate. Use only a few broad hairline circles as backdrop, no extra decorative dots. Take a realistic isometric game's darkened world sliver outside a controlled compact window. Top tabs exactly `검` (selected gold), `공통`, `너클`, `마법`. Heading `대검 스킬트리`. On the left details panel show simply `공용 성장 1P`, selected skill `대지파쇄`, the short actual requirement line `공격 단련 또는 회전참 + 파쇄참`, and action `전수 · 1P`. The web's medium center medallion `대검 기본기` connects into three branches: upper-heavy melee, left/bottom agile sweeping, right defensive. Five larger unique in-world sword technique medallions only: `분쇄참` (diagonal heavy slash), `회전참` (sweeping crescent), `파쇄참` (sword splitting guard), `대지파쇄` (downward blade and floor fissure), `패왕참` (radiant double-edge core at very top). Smaller skill-stat sockets and four subdued resonance nodes keep original screenshot positions. Discrete matte label plaques. Preserve a visible central starting path, clean branches and optional dashed links. Keep the original project design hierarchy, no filler subtitles, no logo treatments, no fictional names, no duplicate captions, no price badges for passive stats.
```
