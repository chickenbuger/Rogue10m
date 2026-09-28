# Obsidian 아키텍처 결합도·응집도 연동 결과

## 목표

Rogue10m 프로젝트 루트를 Obsidian Vault로 유지하면서 C++ 파일의 결합도와 폴더 응집도를 반복 생성하고, Wiki Link·Mermaid·Canvas로 탐색할 수 있게 한다.

## 구현 결과

- `Source/Rogue10m`의 `.h`, `.cpp` 97개를 분석했다.
- 프로젝트 내부 `#include` 관계 197개를 고유 방향선으로 추출했다.
- 원본 파일과 1:1로 대응하는 Obsidian 노트 97개를 생성했다.
- 파일 노트에 Ca, Ce, 총 결합도, 같은 폴더 의존 비율과 유입·유출 Wiki Link를 기록했다.
- 폴더 10개의 내부선, 외부 유입·유출선, 응집도, 불안정도를 계산했다.
- 폴더 의존 Mermaid 그래프와 Obsidian Canvas를 생성했다.
- 프로젝트 문서 영역 `Docs/Obsidian/ArchitectureMetrics`에서 대시보드로 바로 이동할 수 있게 구성했다.
- 더블클릭 실행용 `OpenObsidianArchitecture.bat`와 생성·검증·열기 PowerShell 흐름을 추가했다.

## 사용 방법

1. Obsidian에서 `D:\Project\Rogue10m`을 **Open folder as vault**로 한 번 연다.
2. 프로젝트 루트의 `OpenObsidianArchitecture.bat`를 실행한다.
3. 스크립트가 최신 C++ 관계를 생성·검증한 뒤 `Dashboard` 노트를 연다.
4. 대시보드의 Canvas 링크 또는 파일 링크를 따라 의존 관계를 탐색한다.
5. Obsidian Graph View에서 `path:"Docs/Obsidian/ArchitectureMetrics/Files"`를 필터로 사용하면 생성된 코드 관계만 볼 수 있다.

명령줄에서는 다음과 같이 개별 실행할 수 있다.

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildObsidianArchitectureMetrics.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\TestObsidianArchitectureMetrics.ps1
~~~

## 현재 관찰값

- 총 결합도가 가장 높은 파일은 `Character/Rogue10mCharacter.h`로 Ca 17, Ce 3, 합계 20이다.
- 유출 결합도가 가장 높은 파일은 `Core/Rogue10mPlayerController.cpp`로 Ce 18이다.
- 폴더 응집도는 `Ability` 75.0%가 가장 높고 `Enemy` 8.3%, `UI` 20.0%, `Components` 24.1%가 낮은 편이다.
- 이 값은 include 구조에 한정된 리팩터링 탐색 신호다. 낮은 점수만으로 폴더 분리나 클래스 이동을 결정하지 않는다.

## 지표 한계

- Blueprint, Data Asset, 리플렉션 문자열, 런타임 호출과 에셋 참조는 분석하지 않는다.
- 전방 선언과 include 제거 가능성을 별도로 판정하지 않는다.
- 파일 응집도는 메서드-필드 기반 LCOM이 아니라 같은 폴더로 향하는 의존 비율이다.
- 폴더 응집도는 논리적 도메인 품질이 아니라 include의 내부 집중도를 나타낸다.

## 검증

- 생성기: 파일 97개, 관계 197개, 폴더 10개 생성 성공
- 검증기: 원본 파일 수와 노트 수 일치
- 검증기: Wiki Link, Canvas JSON, 지표 범위 검증 통과
- Obsidian 실행 스크립트: PowerShell 구문 분석 통과
- Unreal 코드·에셋 변경 없음
