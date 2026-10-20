# Lazy Codex Harness

이 문서는 LazyCodex 글에서 다루는 작업 방식 중 Rogue10m에 적용할 수 있는 부분을 프로젝트 하네스 규칙으로 정리한다.

참고 글:

- https://goddaehee.tistory.com/615

## 적용 기준

LazyCodex는 새 모델이 아니라 Codex 위에 작업 분해, 실행, 검증, 반복 흐름을 얹는 얇은 하네스 계층으로 해석한다.

Rogue10m에서는 외부 도구 의존성을 바로 추가하지 않고, 다음 개념을 로컬 규칙으로 장착한다.

- `ultrawork`: 큰 기능을 작고 검증 가능한 작업 단위로 분해한다.
- `ulw-loop`: 계획, 구현, 검증, 리뷰, 문서화를 통과할 때까지 반복한다.
- 역할 라우팅: 작업 성격에 따라 Architect, Implementer, Builder, Reviewer, Doc Writer 관점을 명확히 바꾼다.
- 비용 가드: 불필요한 대규모 탐색, 빌드, 에셋 변경, 반복 작업을 줄인다.

## Rogue10m Lazy Loop

기능 개발은 아래 루프를 따른다.

```text
Request
-> Scope Gate
-> Ultrawork Breakdown
-> Architect
-> Implementer
-> Builder
-> Reviewer
-> Doc Writer
-> Exit Gate
```

### 1. Scope Gate

작업 시작 전 다음을 확인한다.

- 코드, 설정, 문서, 에셋, 브랜치 상태 중 무엇이 바뀌는가?
- Unreal Editor가 필요한 바이너리 에셋 변경인가?
- Data Asset, PlayerState, ActorComponent 분리가 필요한가?
- 빌드가 필요한가, 문서/설정 체크만으로 충분한가?
- 오늘 DevLog에 어떤 형태로 기록할 것인가?

### 2. Ultrawork Breakdown

작업이 크면 30분 안에 검증 가능한 단위로 쪼갠다.

각 작업 단위는 다음 정보를 가진다.

```text
Work Packet
- 목표:
- 입력:
- 수정 위치:
- 완료 조건:
- 검증 명령:
- 되돌릴 수 있는 경계:
```

Unreal 작업의 권장 분해 기준:

- C++ 런타임 로직
- Widget Blueprint 또는 UI 부모 클래스
- Data Asset 스키마
- PlayerState / ActorComponent 데이터 소유권
- 맵/레벨/콘텐츠 배치
- 문서와 DevLog

### 3. Role Routing

작업 성격별 기본 관점을 명확히 선택한다.

| 작업 | 우선 관점 | 산출물 |
| --- | --- | --- |
| 새 시스템 설계 | Architect | `Feature/architect/*.md` |
| C++ 기능 구현 | Implementer | Source 변경 |
| Widget/Blueprint 연결 | Implementer + Reviewer | C++ 부모, Editor 설정 가이드 |
| 빌드 실패 수정 | Builder | 최소 컴파일 수정 |
| 위험 검토 | Reviewer | 파일/라인 기반 리뷰 |
| 기능 완료 정리 | Doc Writer | `Feature/doc/*.md`, DevLog |

## ULW Loop Exit Gate

아래 조건을 만족해야 작업을 끝낸다.

- 요청한 기능 또는 문서 보강이 실제로 반영되었다.
- 변경 범위가 설명 가능하다.
- 관련 빌드 또는 체크를 실행했다.
- 실패한 검증이 있다면 이유와 후속 작업을 남겼다.
- DevLog가 한국어로 갱신되었다.
- 2026-10-20 사용자 상시 승인에 따라 기능 완료·검증·리뷰·문서화 후 커밋하고 `작업 브랜치 -> develop -> test -> main` 순서로 병합·푸시한다. 같은 승인을 다시 요청하지 않는다. 실패한 검증이나 통합 장애는 기록하고, 강제 푸시나 무관한 변경 폐기는 하지 않는다.
- 같은 날 사용자는 현재 코드·Unreal 에셋·제작 원본·검증 자료와 향후 완료·검증된 기능을 공개 GitHub `https://github.com/chickenbuger/Rogue10m`에 게시하는 것도 명시 승인했다. 앞선 공개 목적지 승인 차단은 이 답변으로 해소되며 정상적인 기능 완료 때 같은 공개 게시 승인을 재요청하지 않는다.

## Cost And Risk Guard

토큰, 시간, 빌드 비용을 줄이기 위한 기준이다.

- 먼저 `rg`, `git status`, 관련 문서만 읽고 시작한다.
- 전체 빌드는 C++ 또는 Build.cs 변경이 있을 때 우선한다.
- 문서만 바뀐 경우 `Scripts/CheckGeneratedChanges.ps1`를 우선 실행한다.
- `.uasset`, `.umap`은 직접 편집하지 않는다.
- Unreal Editor 명령줄이 필요한 경우 목적을 좁힌다.
- 반복 실패가 2회 이상이면 설계 단계로 되돌아간다.

## Rogue10m 적용 예시

### HUD Widget Blueprint 작업

```text
Work Packet 1
- 목표: C++ HUD 데이터 Getter 확인
- 수정 위치: Source/Rogue10m/UI
- 완료 조건: Widget Blueprint에서 호출 가능한 Getter 목록 확인
- 검증 명령: BuildEditor.ps1

Work Packet 2
- 목표: 파트별 Widget Blueprint 부모 클래스 준비
- 수정 위치: Source/Rogue10m/UI/Widgets
- 완료 조건: WBP_*가 부모 클래스를 상속하고 로드됨
- 검증 명령: UnrealEditor-Cmd Python load_asset

Work Packet 3
- 목표: 메인 HUD 배치 가이드 작성
- 수정 위치: Docs/WidgetBlueprintHUDGuide.md, DevLog/YYYYMMDD.txt
- 완료 조건: Designer 작업자가 따라 할 수 있음
- 검증 명령: CheckGeneratedChanges.ps1
```

### 공격 Data Asset 작업

```text
Work Packet 1
- 목표: AttackSkillData 필드 설계
- 수정 위치: Data Asset C++ 클래스
- 완료 조건: 피해, 범위, 자원 소모, 차징, 쿨타임 필드 노출

Work Packet 2
- 목표: 공격 입력과 Data Asset 연결
- 수정 위치: Character Combat Component
- 완료 조건: Data Asset이 없으면 입력 잠김

Work Packet 3
- 목표: 디버그와 UI 피드백 확인
- 수정 위치: HUD / Log / Debug Draw
- 완료 조건: 화면에서 공격 판정과 로그 확인 가능
```

## 하네스에 추가된 규칙

- 큰 요청은 먼저 Ultrawork Packet으로 분해한다.
- 각 Packet은 완료 조건과 검증 명령을 가진다.
- 실패하면 같은 구현을 반복하지 않고 이전 역할 단계로 되돌린다.
- 기능 단위 종료 시 ULW Loop Exit Gate를 확인한다.
- Lazy Codex 개념은 로컬 하네스 운용 규칙으로만 사용하며, 외부 자동화 도구 설치는 별도 사용자 지시가 있을 때만 진행한다.
