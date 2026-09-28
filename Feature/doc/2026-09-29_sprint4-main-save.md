# Sprint4 main 저장 검증

2026-09-29 사용자 요청으로 현재 누적 작업 저장을 준비했다. 개별 변경 파일과 설명은 `evidence/main-save-20260929/changed-files.tsv`에 기록한다.

- Unreal Editor 빌드 성공(최신 상태, 2.14초).
- Studio 6개 JS 구문 검사 및 루트 실행 Node 테스트19건 통과.
- Studio/data, 엔진 생성물, 로컬 tmp 출력은 제외. 정식 제작에 필요한 tmp 입력5개만 예외 포함.
- 이전 head shadow 검증390프레임, V 회귀280프레임 결과도 함께 보존.
- git fetch 후 develop/test/main 원격과 로컬 일치 및 작업 브랜치 조상 관계 확인.

현재 게임·Studio 검증 범위는 각 Feature 문서의 한계를 따른다. 이번 저장은 모든 기능의 신규 전체 QA를 의미하지 않는다. Studio 테스트에는 cwd 의존이 있어 프로젝트 루트에서 `node --test Studio/tests/*.test.mjs`를 사용했다. 외부 Downloads의 FBX 입력은 기존 로컬 의존이며 저장소에 포함하지 않는다.

Git 승격 및 원격 결과는 완료 후 기록한다.
