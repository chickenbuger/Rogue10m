# 공통 스킬트리 화면 개선

공통 탭 전체를 승인 시안의 명암과 배치에 맞췄다. 실제 게임 화면을 흐리고 어둡게 한 위에 검은 패널과 금속 테두리를 배치했다. 제목과 선택 탭에는 따뜻한 금빛을, 습득 경로와 선택 노드에는 밝은 금빛을 적용했다. [승인 시안](evidence/common-skill-web-20261120/approved-concept.png) · [이전 게임 화면](../../DevLog/20261120_CommonSkillWeb/common-skill-web.png)

그래프의 16개 지점을 시안 비율에 맞춰 다시 배치했다. 공중·이동·회피마다 다른 금속 장식과 입체 아이콘을 사용하고, 이름표의 베벨은 창 테두리와 같은 방향에서 빛을 받게 했다. 선택 카드에도 같은 아트를 큰 크기로 표시한다. 공용 성장 포인트는 헤더로 옮기고 습득과 아이덴티티 버튼은 왼쪽 카드 하단에 배치했다.

| 파일 | 역할 |
| --- | --- |
| `Rogue10mHybridSkillTreeWidget.cpp/.h` | 공통 창 레이아웃, 아틀라스에서 능력별 메달리온 선택, 연결과 색, 상세 카드 |
| `Rogue10mCommonSkillChrome.cpp/.h` | 금속 모서리, 명암이 있는 프레임, 탭, 이름표와 글자 표시 |
| `Art/UI/CommonSkillTree`, `Scripts/Editor/ImportCommonSkillTreeArt.py`, `Content/UI/CommonSkillTree` | 원본과 재생성 스크립트, UE용 UI 텍스처 2종 |
| `Config/DefaultGame.ini` | 패키징 시 공통 스킬트리 아트 포함 |
| `Rogue10mHybridSkillTreeRuntimeTest.cpp` | 두 해상도에서 메달리온과 선택 탭의 실제 표시, 기존 선택과 전수 흐름을 검사하는 화면 검증 모드 |

메달리온은 승인 시안을 참조해 ImageGen으로 제작하고, 패널에는 승인 시안의 숯빛 조각을 사용했다. 한국어는 게임에 포함된 글꼴과 따뜻한 명암으로 표시한다. [원본 및 생성 프롬프트](evidence/common-skill-tree-art-20261120/art-source.md) · [작업 계획](../architect/2026-11-20_common-skill-tree-art.md)

## 검증

최신 통합본 `a507255e`를 반영한 독립 복사본에서 UE 5.8 Development Editor 빌드와 UI 텍스처 두 종 임포트를 확인했다. StartHub의 실제 게임 세션에서 274개 항목을 통과했다. 실제 클릭으로 이동 속도 증가를 습득했고 교차 연결로 이단 점프를 선택했다. 16개 노드와 상세 카드가 아틀라스를 사용하고 선택 탭이 별도의 색으로 표시되는 것도 검사했다. 1080p와 720p 실제 화면, 자동화 4개를 확인했다. [최종 실제 화면](../../DevLog/20261120_CommonSkillTreeArt/common-skill-tree-art.png) · [720p 화면](evidence/common-skill-tree-art-20261120/common-720p.png) · [검증 수치](evidence/common-skill-tree-art-20261120/verification.json)

첫 시도에서 테두리의 정점을 배열에 추가하는 과정이 충돌해 독립된 값으로 복사해 수정했다. 이후에는 좁은 이름표의 줄바꿈을 줄이고 활성 탭과 이름표의 바탕이 실제로 그려지도록 조정했다. 전체 이동 검사 후속 실행에서는 점프 샘플과 구르기 회복 시점 검사가 일관되지 않았다. 기존 이동 검사는 별도로 유지하고 최종 실행에서는 UI 표시와 클릭, 습득 및 기존 무기 탭 입력을 검증했다. 화면 검사 플래그는 `-RogueHybridCommonPresentationQA`다. 이미 열려 있던 에디터 로그의 PIE 중 현재 레벨 조회 오류 4개는 별도 세션에 기록됐다.
