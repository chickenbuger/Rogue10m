---
title: Rogue10m 아키텍처 지표
tags:
  - rogue10m
  - architecture
  - generated
generated_at: 2026-09-22 02:26:53 +09:00
source_files: 121
dependency_edges: 292
---
# Rogue10m 아키텍처 지표

> [!warning] 해석 범위
> 이 대시보드는 프로젝트 내부 #include 정적 관계만 분석합니다. 런타임 호출, Blueprint 참조, 리플렉션·에셋 의존성은 포함하지 않으며 점수는 설계 품질의 절대 판정이 아닙니다.

## 요약

- 분석 파일: **121**개
- 프로젝트 내부 include 관계: **292**개
- 분석 폴더: **12**개
- 시각 탐색: [[Docs/Obsidian/ArchitectureMetrics/Rogue10m Architecture.canvas|Rogue10m Architecture Canvas]]

## 폴더 의존 그래프

~~~mermaid
flowchart LR
    F0["Ability<br/>응집도 75.0%"]
    F1["Character<br/>응집도 33.3%"]
    F2["Components<br/>응집도 31.9%"]
    F3["Core<br/>응집도 53.3%"]
    F4["Data<br/>응집도 50.0%"]
    F5["Editor<br/>응집도 0.0%"]
    F6["Enemy<br/>응집도 8.3%"]
    F7["Enemy/AI<br/>응집도 57.1%"]
    F8["Tests<br/>응집도 0.0%"]
    F9["UI<br/>응집도 20.0%"]
    F10["UI/Widgets<br/>응집도 43.9%"]
    F11["World<br/>응집도 38.5%"]
    F0 -->|1| F1
    F1 -->|1| F0
    F1 -->|8| F2
    F1 -->|4| F3
    F1 -->|3| F4
    F2 -->|5| F0
    F2 -->|11| F1
    F2 -->|6| F3
    F2 -->|6| F4
    F2 -->|3| F6
    F2 -->|1| F11
    F3 -->|2| F0
    F3 -->|7| F1
    F3 -->|4| F2
    F3 -->|2| F4
    F3 -->|1| F6
    F3 -->|1| F9
    F3 -->|4| F10
    F4 -->|1| F0
    F4 -->|6| F1
    F4 -->|1| F2
    F5 -->|1| F3
    F6 -->|2| F0
    F6 -->|1| F1
    F6 -->|2| F2
    F6 -->|3| F3
    F6 -->|2| F4
    F6 -->|1| F7
    F7 -->|1| F1
    F7 -->|1| F4
    F7 -->|1| F6
    F8 -->|6| F0
    F8 -->|10| F1
    F8 -->|16| F2
    F8 -->|17| F3
    F8 -->|5| F4
    F8 -->|5| F6
    F8 -->|1| F9
    F8 -->|2| F10
    F9 -->|1| F0
    F9 -->|5| F1
    F9 -->|1| F2
    F9 -->|4| F3
    F9 -->|4| F4
    F9 -->|1| F6
    F10 -->|3| F1
    F10 -->|6| F2
    F10 -->|6| F3
    F10 -->|4| F4
    F10 -->|4| F9
    F11 -->|1| F1
    F11 -->|2| F2
    F11 -->|3| F3
    F11 -->|1| F4
    F11 -->|1| F6
~~~

선의 숫자는 폴더 사이의 고유 include 관계 수입니다.

## 폴더 응집도와 안정성

| 폴더 | 파일 | 내부선 | 유입선 | 유출선 | 응집도 | 불안정도 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Editor` | 1 | 0 | 0 | 1 | 0.0% | 100.0% |
| `Tests` | 7 | 0 | 0 | 62 | 0.0% | 100.0% |
| `Enemy` | 2 | 1 | 12 | 11 | 8.3% | 47.8% |
| `UI` | 8 | 4 | 6 | 16 | 20.0% | 72.7% |
| `Components` | 18 | 15 | 40 | 32 | 31.9% | 44.4% |
| `Character` | 10 | 8 | 46 | 16 | 33.3% | 25.8% |
| `World` | 10 | 5 | 1 | 8 | 38.5% | 88.9% |
| `UI/Widgets` | 20 | 18 | 6 | 23 | 43.9% | 79.3% |
| `Data` | 14 | 8 | 28 | 8 | 50.0% | 22.2% |
| `Core` | 21 | 24 | 44 | 21 | 53.3% | 32.3% |
| `Enemy/AI` | 4 | 4 | 1 | 3 | 57.1% | 75.0% |
| `Ability` | 6 | 3 | 18 | 1 | 75.0% | 5.3% |

## 결합도가 높은 파일

| 파일 | Ca | Ce | 총 결합도 | 같은 폴더 의존 비율 |
| --- | ---: | ---: | ---: | ---: |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Character/Rogue10mCharacter.h|Character/Rogue10mCharacter.h]] | 29 | 3 | 32 | 100.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10m.h|Core/Rogue10m.h]] | 24 | 0 | 24 | N/A |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerController.cpp|Core/Rogue10mPlayerController.cpp]] | 0 | 18 | 18 | 38.9% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Ability/Rogue10mAttributeSet.h|Ability/Rogue10mAttributeSet.h]] | 16 | 0 | 16 | N/A |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mCombatComponent.cpp|Components/Rogue10mCombatComponent.cpp]] | 0 | 15 | 15 | 40.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mCombatComponent.h|Components/Rogue10mCombatComponent.h]] | 13 | 2 | 15 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Character/Rogue10mCharacter.cpp|Character/Rogue10mCharacter.cpp]] | 0 | 14 | 14 | 7.1% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerController.h|Core/Rogue10mPlayerController.h]] | 14 | 0 | 14 | N/A |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/Rogue10mBasicMonster.h|Enemy/Rogue10mBasicMonster.h]] | 13 | 1 | 14 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerState.h|Core/Rogue10mPlayerState.h]] | 10 | 2 | 12 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Data/Rogue10mAttackSkillData.h|Data/Rogue10mAttackSkillData.h]] | 12 | 0 | 12 | N/A |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Tests/Rogue10mBasicBrawlerRuntimeTest.cpp|Tests/Rogue10mBasicBrawlerRuntimeTest.cpp]] | 0 | 12 | 12 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mInventoryComponent.h|Components/Rogue10mInventoryComponent.h]] | 9 | 2 | 11 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/Rogue10mBasicMonster.cpp|Enemy/Rogue10mBasicMonster.cpp]] | 0 | 11 | 11 | 9.1% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mMenuWindowWidgets.cpp|UI/Widgets/Rogue10mMenuWindowWidgets.cpp]] | 0 | 11 | 11 | 27.3% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Tests/Rogue10mBoxingFeedbackRuntimeTest.cpp|Tests/Rogue10mBoxingFeedbackRuntimeTest.cpp]] | 0 | 10 | 10 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Tests/Rogue10mFirstPersonFistRuntimeTest.cpp|Tests/Rogue10mFirstPersonFistRuntimeTest.cpp]] | 0 | 10 | 10 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/Tests/Rogue10mResponsiveHUDRuntimeTest.cpp|Tests/Rogue10mResponsiveHUDRuntimeTest.cpp]] | 0 | 9 | 9 | 0.0% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Rogue10mRunHUD.cpp|UI/Rogue10mRunHUD.cpp]] | 0 | 9 | 9 | 11.1% |
| [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Rogue10mRunHUD.h|UI/Rogue10mRunHUD.h]] | 5 | 4 | 9 | 0.0% |

## 계산 기준

- **Ca**: 이 파일을 include하는 프로젝트 파일 수
- **Ce**: 이 파일이 include하는 프로젝트 파일 수
- **총 결합도**: Ca + Ce
- **파일 로컬 의존 비율**: 같은 폴더로 나가는 include / 전체 프로젝트 내부 include
- **폴더 응집도**: 내부선 / (내부선 + 외부 유출선)
- **폴더 불안정도**: 외부 유출선 / (외부 유입선 + 외부 유출선)
- 분모가 0이면 N/A로 표시합니다.

## 소스 파일 그래프 보기

Obsidian 전체 그래프의 검색 필터에 다음 값을 사용합니다. README, DevLog, Feature 문서, 대시보드와 Canvas는 그래프에서 제외됩니다.

~~~text
(path:"Docs/Obsidian/ArchitectureMetrics/Files/" OR path:"Docs/Obsidian/ArchitectureMetrics/Functions/") -path:"Feature/"
~~~

소스 노드는 C++ .h/.cpp 파일에 대응합니다. 소스 A → 소스 B는 include 관계이며, 기능 → 소스는 해당 기능의 Feature 본문에서 확인한 소스 언급이며, 근거 문서와 행은 기능 노트에서 확인합니다. 공유 소스는 여러 기능에 연결되고, 연결이 없는 소스 파일도 표시합니다.
태그·첨부파일은 숨기고 화살표를 켭니다. 기본 경로를 사용할 때 OpenObsidianArchitecture.bat가 이 설정을 저장합니다.
이미 열린 Obsidian에 반영되지 않으면 그래프 검색창에 위 필터를 입력하거나 앱을 다시 불러옵니다.

## 갱신

~~~powershell
.\Scripts\BuildObsidianArchitectureMetrics.ps1
.\Scripts\TestObsidianArchitectureMetrics.ps1
~~~
