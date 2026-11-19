# 너클 수련 트리

확정 시안대로 기초 무공 4개, 중간 수련 7개, 상위 무공 6개를 원형 노드 17개와 연결선 15개로 표시한다. K 창은 너클을 장착했을 때 너클 탭을 먼저 열고, 전수된 경로와 잠긴 경로를 색으로 구분한다. 노드를 선택하면 실제 선행 무공과 숙련을 확인한다.

| 기초 무공 | 연결 경로 |
| --- | --- |
| 왼손 잽 | 공격력 단련 → 붕권 → 파진각 또는 금강쇄. 공격력 단련은 유운수에도 연결된다. |
| 오른손 잽 | 스태미나 단련 → 유운수 → 백열난무 또는 무극패왕권. 유운수는 왼손 잽 경로의 공격력 단련도 요구한다. |
| 오른손 스트레이트 | 체력 단련 → 철산고 → 태산압정 또는 무극패왕권. 무극패왕권은 유운수와 철산고를 모두 요구한다. |
| 차지 오른손 훅 | 방어력 단련 → 금강불괴 |

너클 장착 중 유효 타격을 적중시키면 격투 숙련이 오른다. 숙련 단계와 선행을 충족한 스탯과 무공은 성장 포인트 없이 자동 전수된다. 네 기초 무공은 처음부터 전수되고 실제 공용 또는 우클릭 차징 슬롯에 놓을 수 있다. 철산고와 무극패왕권은 우클릭 차징 슬롯을 사용한다. 태산압정은 공중에서 사용한다.

이전 자동 전수 기록은 당시의 선행·숙련 조건으로도 읽는다. 기존 스킬 ID와 저장 형식을 유지한다. 네 기초 무공의 공격 데이터와 아이콘, 기존 너클·StoneFist 기술과 금강불괴를 사용했다.

| 수정 파일 | 내용 |
| --- | --- |
| `Rogue10mHybridSkillTreeWidget.cpp/.h` | 실제 카탈로그에서 너클의 원형 노드와 수련 경로 그리기, 무공 선택 및 드래그 |
| `Rogue10mSkillTreeCatalog.cpp`, `Rogue10mSkillTreeData.cpp/.h` | 기초기 네 개와 승인한 선행·배치, 숨은 진행 기록을 표시 대상과 구분 |
| `Rogue10mSkillTreeProgressionComponent.cpp/.h` | 현재 전수 조건과 구 저장의 자동 전수 기록을 함께 검증 |
| `Rogue10mSkillTreeCombatComponent.cpp` | 검 기술의 전수 방식과 별개로 기존 핵심기·최종기 공명 효과를 적용 |
| `CreateHybridSkillTreeAssets.py` | 기초기 경로와 카탈로그 작성 검증 갱신 |
| `Rogue10mHybridSkillTreeTests.cpp`, `Rogue10mHybridSkillTreeRuntimeTest.cpp`, `Rogue10mBasicRunPlaytest.cpp` | 노드·선행·저장 호환, K 창과 실제 장착 입력 경로 검증 |

[설계](../architect/2026-11-20_knuckle-training-tree.md) · [확정 시안](evidence/knuckle-training-tree-20261120/approved-concept.png)

## 검증

UE 5.8 Development Editor를 현재 프로젝트와 최신 통합본의 독립 체크아웃에서 빌드했다. StartHub의 실제 K 창에서 노드 17개, 기초 무공 4개의 슬롯 조건, 장착·해제에 따른 탭 전환, 메달 테두리 클릭, 훈련 전수, 키와 드래그를 검증했다. 검의 투자와 공명도 함께 확인했다. 새 저장의 격리 게임 검사 229개와 새 프로세스에서 기존 슬롯을 읽는 검사 91개를 통과했다. Unreal 출력 로그는 두 실행 모두 오류 0개였다.

카탈로그 자동화는 17개 노드의 배치와 15개 선행, 초기 네 기초 무공, 격투 M12·M20 숙련, 구 자동 전수 저장의 복원을 검사한다. 작업 폴더의 기존 저장 505개는 검증 전후의 SHA-256이 일치했다.

[실제 K 화면](../../DevLog/20261120_KnuckleTrainingTree/knuckle-tree.png) · [검증 결과](evidence/knuckle-training-tree-20261120/verification.json) · [검증 중 발견한 문제](evidence/knuckle-training-tree-20261120/validation-notes.md)
