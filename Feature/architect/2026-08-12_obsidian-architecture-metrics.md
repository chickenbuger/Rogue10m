# Obsidian 아키텍처 지표 연동 계획

## Scope Gate

- 변경 대상: PowerShell 분석·검증·실행 스크립트, 생성형 Obsidian Markdown/Canvas, 기능 문서와 로그
- 분석 대상: `Source/Rogue10m` 아래의 `.h`, `.cpp`와 프로젝트 내부 `#include` 관계
- 보호 대상: 기존 Vault 문서, `.obsidian`, C++ 원본, Unreal 바이너리 에셋과 생성·캐시 디렉터리
- Unreal 빌드: 불필요. C++를 수정하지 않으므로 정적 분석 결과와 생성물 검증으로 대체
- 작업 브랜치: `Sprint#4-3-obsidian-architecture-metrics`

## Ultrawork Packets

### Packet 1 — 지표 모델과 Obsidian 정보 구조

- 목표: 파일 결합도와 폴더 응집도의 계산식, 출력 위치, 링크 모델을 고정한다.
- 수정 위치: `Feature/architect/2026-08-12_obsidian-architecture-metrics.md`
- 완료 조건: 지표 정의와 오해 방지 문구가 설계 문서 및 대시보드에 존재한다.
- 검증 명령: 문서 검토
- 롤백 경계: 이번 설계 문서

### Packet 2 — 분석 및 Vault 생성기

- 목표: 프로젝트 내부 include 그래프를 Obsidian 노트, 대시보드, Canvas로 변환한다.
- 수정 위치: `Scripts/BuildObsidianArchitectureMetrics.ps1`, `Docs/Obsidian/ArchitectureMetrics`
- 완료 조건: 모든 C++ 파일 노트가 생성되고 각 노트에서 유입·유출 관계를 Wiki Link로 탐색할 수 있다.
- 검증 명령: `Scripts/BuildObsidianArchitectureMetrics.ps1`
- 롤백 경계: 분석 스크립트와 생성 출력 폴더

### Packet 3 — 실행 진입점과 자동 검증

- 목표: 한 명령으로 최신 지표를 생성하고 Obsidian에서 대시보드를 열 수 있게 한다.
- 수정 위치: `Scripts/OpenObsidianArchitecture.ps1`, `Scripts/TestObsidianArchitectureMetrics.ps1`
- 완료 조건: 생성물 수, JSON, Wiki Link, 수치 범위 검증을 통과한다.
- 검증 명령: `Scripts/TestObsidianArchitectureMetrics.ps1`
- 롤백 경계: 신규 실행·검증 스크립트

### Packet 4 — 결과 기록

- 목표: 사용법, 한계, 검증 결과를 프로젝트 문서와 개발 이력에 남긴다.
- 수정 위치: `Feature/doc`, `Docs/SprintChangeLog.md`, `DevLog/20260812.txt`
- 완료 조건: 기능 결과, Sprint 상태, Notion 요약 후보가 한국어로 기록된다.
- 검증 명령: `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`
- 롤백 경계: 이번 기능의 신규 문서와 기존 로그의 신규 항목

## 지표 정의

- `Ca`(afferent coupling): 해당 파일을 include하는 프로젝트 파일 수
- `Ce`(efferent coupling): 해당 파일이 include하는 프로젝트 파일 수
- `C = Ca + Ce`: 변경 영향과 의존 부담을 함께 보는 총 결합도
- 파일 로컬 의존 비율: 같은 폴더로 향하는 유출 include 수 / `Ce`
- 폴더 응집도: 폴더 내부 include 수 / (폴더 내부 include 수 + 다른 폴더로 나가는 include 수)
- 폴더 불안정도: 외부 유출선 수 / (외부 유입선 수 + 외부 유출선 수)

include가 없는 파일의 비율은 임의로 0 또는 100%로 간주하지 않고 `N/A`로 표시한다. 이 값들은 구조적 신호이며 설계 품질의 절대 점수가 아니다.

## 출력 구조

- `Dashboard.md`: 폴더 지표, 고결합 파일, 폴더 의존 Mermaid 그래프
- `Files/**/*.md`: 원본 C++ 파일과 1:1로 대응하는 탐색 노트
- `Rogue10m Architecture.canvas`: 폴더 간 의존 관계를 보여 주는 Obsidian Canvas
- `metrics.json`: 외부 도구와 후속 분석용 원시 지표
