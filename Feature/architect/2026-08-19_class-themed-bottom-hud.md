# Sprint#4-15 직업별 통합 하단 HUD 설계

## 목표

- 마법사, 전사, 권사, 도적이 같은 기능 배치를 공유하면서 재질, 문양, 강조색이 구분되는 하단 HUD를 제공한다.
- 체력, 마력/스테미나, 아이덴티티, 스킬, 아이템, 레벨/경험치를 `WBP_BottomHUD` 하나에서 조립한다.
- `WBP_Rogue10mMainHUD`는 하단 요소를 개별 소유하지 않고 `BottomHUDWidget` 하나만 배치하고 갱신한다.

## Scope Gate

- 코드: 통합 하단 위젯 C++ 부모와 Main HUD 위임 구조를 추가한다.
- 에셋: 직업별 HUD 프레임 Texture2D, `WBP_BottomHUD`, Main HUD 배치를 Unreal Editor로 생성·변경한다.
- 문서: 기능 결과, Sprint 변경 기록, 2026-08-19 DevLog를 갱신한다.
- 빌드: C++ 변경이 있으므로 Editor target 전체 빌드를 실행한다.
- 바이너리 안전: `.uasset`은 직접 편집하지 않고 Unreal Editor Python/UMGToolSet으로만 변경한다.

## 공통 배치

```text
┌──────────────────────────── 화면 전체 폭 ────────────────────────────┐
│   [체력]       [스킬 5칸]   [직업 아이덴티티]   [아이템 5칸]   [MP/스태미나] │
│                      [선택적 아이덴티티 자원]                         │
│ [LV]==================== 초록색 경험치 ===============================│
└──────────────────────────────────────────────────────────────────────┘
```

- 기능 위젯 좌표는 네 직업이 공유한다.
- 전체 폭 경험치 바는 하단에 밀착한다.
- 마법사는 MP를, 전사·권사·도적은 스태미나를 우측 자원 바로 사용한다.
- 아이덴티티 자원이 있으면 중앙 문장과 보조 자원에 반영한다.

## 직업 테마

| 테마 | 자동 판별 | 재질/문양 | 강조색 | 자원 |
| --- | --- | --- | --- | --- |
| 마법사 | Staff 또는 Mana | 흑요석, 은, 비전 룬, 수정 | 청색·보라 | MP |
| 전사 | GreatSword 또는 Rage | 흑철, 황동, 방패, 검날 | 적색·주황 | 스태미나 |
| 권사 | Knuckle/Unarmed 또는 StoneFist/Vigor | 흑칠, 청동, 권갑, 기 흐름 | 비취·금색 | 스태미나 |
| 도적 | Dagger/DualBlades/Bow 또는 Focus | 흑철, 은, 단검, 그림자 | 녹색·자주 | 스태미나 |

명시적인 Blueprint 테마 오버라이드도 제공하여 캐릭터 직업 시스템이 확장되기 전에도 미리보기와 강제 설정이 가능하게 한다.

## Ultrawork Packets

### Packet 1 — 설계와 시안

- 목표: 공통 배치와 네 직업의 시각 언어를 확정한다.
- 입력: 기존 Gothic HUD와 보존된 Rogue10m Combat HUD 레퍼런스.
- 수정 위치: `Content/UI/HUD/ClassThemes/Source`, 본 설계 문서.
- 완료 조건: 네 직업 시안과 실제 적용용 테마 텍스처가 존재한다.
- 검증: 이미지 크기, 알파, 좌표 기준을 검사한다.
- 롤백 경계: `Content/UI/HUD/ClassThemes` 신규 파일만 제거하면 된다.

### Packet 2 — 통합 하단 C++ 위젯

- 목표: 하단 갱신 책임을 `URogue10mBottomHUDWidget`으로 이동한다.
- 입력: 기존 HUD View 구조체와 QuickSlot 클래스.
- 수정 위치: `Source/Rogue10m/UI/Widgets`.
- 완료 조건: Main HUD는 `BottomHUDWidget`에 frequent/slow View만 전달한다.
- 검증: `Scripts/BuildEditor.ps1`.
- 롤백 경계: 신규 Bottom HUD 클래스와 Main HUD의 해당 diff.

### Packet 3 — Widget Blueprint 조립

- 목표: `WBP_BottomHUD`를 만들고 Main HUD의 기존 하단 자식을 하나로 교체한다.
- 입력: 기존 파트 Widget Blueprint와 테마 Texture2D.
- 수정 위치: `Content/Widget`, `Scripts/Editor`.
- 완료 조건: Main HUD 트리에 하단 기능 자식이 `BottomHUDWidget` 하나만 존재한다.
- 검증: 전용 Unreal Validator와 Widget Blueprint 컴파일.
- 롤백 경계: 신규 `WBP_BottomHUD`와 Main HUD 배치 변경.

### Packet 4 — 통합 검증과 문서화

- 목표: 빌드, 바인딩, 테마 선택, 전체 폭 XP 배치를 검증한다.
- 수정 위치: Validator, `Feature/doc`, `Docs/SprintChangeLog.md`, `DevLog/20260819.txt`.
- 완료 조건: 빌드 성공, Validator `RESULT=PASSED`, 적용 이미지 확인.
- 검증: `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`.
- 롤백 경계: 검증/문서 신규 항목.

## 런타임 책임

- `URogue10mMainHUDWidget`
  - 게임 상태 View를 수집한다.
  - `BottomHUDWidget`에 frequent/slow 데이터 묶음을 전달한다.
  - 몬스터 정보, 로그 등 하단 밖의 UI는 기존처럼 직접 관리한다.
- `URogue10mBottomHUDWidget`
  - HP, MP/스태미나, 아이덴티티, 스킬, 아이템, 경험치 파트를 소유한다.
  - 무기/아이덴티티 기반 테마를 판별하고 테마 이미지를 전환한다.
  - QuickSlot 자식의 개수와 View를 갱신한다.
- `WBP_Rogue10mMainHUD`
  - 전체 화면 레이어와 `BottomHUDWidget` 한 개만 배치한다.

## 완료 조건

- 네 직업 디자인을 이미지로 확인할 수 있다.
- `WBP_BottomHUD` 내부에 하단 기능 요소가 모두 조립되어 있다.
- Main HUD에는 개별 하단 요소 대신 `BottomHUDWidget` 하나만 존재한다.
- 런타임 HP, MP/스태미나, 아이덴티티, 스킬, 아이템, 경험치 갱신이 유지된다.
- 전체 폭 초록색 경험치 바가 화면 하단에 붙는다.
