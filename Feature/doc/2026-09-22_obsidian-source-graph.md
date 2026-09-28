# Obsidian Feature 근거 기반 기능·소스 그래프

- 브랜치: Sprint#4-29-obsidian-source-graph
- 목표: Feature 설계·결과 본문을 참고하여 기능 노드에서 관련 소스 파일로 연결한다. README·DevLog·Feature 원문은 그래프에 표시하지 않는다.

## 결과

- 소스 노드 121개와 정적 include 방향선 292개를 유지했다.
- 기능 노드 10개: 전투, 스킬, 인벤토리·장비, 체력·성장, 캐릭터·프로필, 애니메이션·1인칭, 몬스터·AI, HUD·메뉴, 월드·상호작용, 입력·게임흐름.
- Feature/architect와 Feature/doc의 본문 170개를 검사했으며 기능 주제에 해당하는 문서 157개를 사용했다.
- 기능→소스 연결 397개를 생성했다. 모든 선에 문서 경로·행 번호·언급 문자열·설계/결과 구분 근거가 있다.
- 파일 이름만으로 임의 분류하던 기존 방식을 교체했다. 근거가 없는 소스는 기능선을 만들지 않지만 기존 소스 그래프에는 유지한다.
- 기능 노트에는 소스별 대표 근거 최대 3개를 표시하고 전체 근거는 Docs/Obsidian/ArchitectureMetrics/functions.json에 저장한다.
- Feature 원문 경로는 일반 텍스트로 표시하여 근거 문서 자체가 새 그래프 노드나 허브로 생기지 않는다.

## 연결 기준

Scripts/ObsidianFunctionGroups.json은 소스 파일 패턴 대신 기능별 Feature 문서 주제를 관리한다.
Scripts/GetObsidianFeatureEvidence.ps1은 본문에서 다음 참조를 현재 소스와 대조한다.

- 실제 파일명: 명시한 .h 또는 .cpp 파일만 연결한다.
- .h/.cpp, .cpp/.h, .*: 실제 존재하는 파일 쌍을 연결한다.
- 클래스/구조체: 현재 헤더의 정의를 확인하고 해당 헤더와 같은 이름의 구현 파일에 연결한다. 전방 선언은 소유 근거로 사용하지 않는다.
- CombatComponent 등의 유일한 축약명: 프로젝트 접두어를 제외한 충분히 긴 이름을 현재 소스와 대조한다.
- 모호한 중복 이름과 존재하지 않는 이름은 선을 만들지 않는다.

예: 스킬 Feature 본문의 URogue10mWeaponSkillProfileDataAsset은 실제 정의가 있는 Data/Rogue10mSkillLoadoutDataAsset.h와 대응 .cpp에 연결된다.
공유 파일은 여러 기능에 연결된다. 기능→소스는 문서에서 언급한 연관성이고, 소스→소스는 include 관계다.

## 사용과 갱신

프로젝트 루트의 OpenObsidianArchitecture.bat가 생성·검증·설정 저장을 수행한다.
전체 그래프 검색:

~~~text
(path:"Docs/Obsidian/ArchitectureMetrics/Files/" OR path:"Docs/Obsidian/ArchitectureMetrics/Functions/") -path:"Feature/"
~~~

기능 노트에서 관련 소스와 근거 행을 확인한다. 기능 노트를 연 상태의 로컬 그래프 깊이 1로 해당 기능 주변을 볼 수 있다.
실행 중인 Obsidian 화면에 설정이 반영되지 않으면 위 필터를 입력하거나 앱을 다시 불러온다.
현재 앱 화면 반영은 직접 확인하지 않았다.

## 검증

- TestObsidianArchitectureMetrics.ps1: 파일 수·include 방향선·기능 소스 집합·노트 링크·근거 문서와 실제 행·언급 문자열·Canvas 검증 통과.
- TestObsidianFeatureEvidence.ps1: 파일명과 다른 클래스 소유 파일, 양방향 축약 파일 쌍, 명시적 확장자, 중복 이름 거부, 존재하지 않는 소스, 전방 선언 배제, 설계/결과 구분 회귀 검사 통과.
- 문서 근거 해석은 정적 분석이다. 과거 결과·설계·제거 계획의 언급도 포함될 수 있어 현재 런타임 호출이나 구현 완료를 보증하지 않는다.
- README, evidence 하위 폴더, Obsidian 도구 자체 문서는 근거 수집에서 제외한다. 선택되지 않은 문서와 근거가 없는 소스 목록도 functions.json에 기록한다.
- Unreal 소스·자산 변경 없음. 엔진 빌드 생략. 커밋·푸시 없음.
- 전체 행별 근거 2079 건, 기능 근거 없는 소스 9 개.
