# 대검 스킬트리

검 탭의 중앙에 대검 기본기를 배치하고 공격·연계·방어로 뻗는 경로를 유지했다. 작은 스탯 노드와 기술 메달리온, 외곽 공명을 구분한다. 분쇄참·회전참·파쇄참·대지파쇄·패왕참은 각각의 공격 동작을 본뜬 그림으로 표시한다. 스탯과 기술 이름에 배경을 두어 교차 연결 사이에서도 읽을 수 있다.

기술은 실제 선행 경로를 충족한 뒤 공용 성장 1P로 전수한다. 상세 카드에서 대체 경로마다 완료 여부를 확인할 수 있다. 같은 이름의 스탯은 숙련 단계로 구분한다. 대지파쇄는 숙련 M8의 공격력 단련을 거치거나, 회전참과 파쇄참을 모두 전수해 열 수 있다. 공명 카드에는 공통 탭에서 요구하는 실제 성질을 표시한다.

| 변경 파일 | 내용 |
| --- | --- |
| `Rogue10mHybridSkillTreeWidget.cpp/.h` | 검 탭의 메달리온, 기술 그림, 이름표, 상태별 연결선과 선택 강조 |
| `Rogue10mSkillTreeProgressionComponent.cpp` | 검 기술의 대체 선행과 공통 탭 공명 조건을 상세 카드에 표시 |
| `Rogue10mHybridSkillTreeTests.cpp` | 한 포인트로 시작하는 공격 경로, 네 포인트로 완성하는 우회, 부족한 예산과 선행 검증 |
| `Rogue10mHybridSkillTreeRuntimeTest.cpp` | 실제 장착·전수, 경로별 상세 카드, 두 해상도의 화면 검증 및 저장 복원 |

[작업 계획](../architect/2026-11-20_greatsword-skill-web.md) · [디자인 시안](evidence/greatsword-skill-web-20261120/concept.png)

## 검증

최신 통합본을 물리적으로 분리한 체크아웃에서 UE 5.8 Development Editor를 빌드했다. 1920×1080과 1280×720에서 실제 K 창의 이름표와 다섯 기술 그림을 확인했다. 기술 카드는 실제 입력 규칙을 읽어서 장착 슬롯을 안내한다. 대지파쇄의 우회 선행을 하나만 배운 상태에서 전수를 시도하면 포인트가 그대로 남고, 양쪽 선행을 갖추면 경로가 완료된다.

자동화 3개와 StartHub 게임 검사 242개, 새 프로세스에서 저장을 읽는 검사 91개를 통과했다. 게임 검사는 사용자 편집 세션과 별도 저장 슬롯에서 자동 입력으로 실행했다. 세 검증 로그의 오류는 각각 0개였다.

[실제 게임 화면](../../DevLog/20261120_GreatSwordSkillWeb/greatsword-skill-web.png) · [720p 검증 화면](evidence/greatsword-skill-web-20261120/greatsword-720p.png) · [검증 결과](evidence/greatsword-skill-web-20261120/verification.json)
