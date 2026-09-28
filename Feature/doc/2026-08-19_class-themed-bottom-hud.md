# 직업별 통합 하단 HUD 결과

## 결과

마법사, 전사, 권사, 도적이 동일한 기능 배치를 공유하면서 직업별 재질과 강조색을 사용하는 하단 전투 HUD를 적용했습니다. 기존 Main HUD에 흩어져 있던 체력, MP/스테미나, 아이덴티티, 스킬, 아이템, 레벨/경험치 요소는 `WBP_BottomHUD` 하나로 조립했으며, Main HUD에는 `BottomHUDWidget` 한 개만 배치됩니다.

## 직업별 디자인

| 직업 | 프레임 | 중앙 문장 | 자원 표현 |
| --- | --- | --- | --- |
| 마법사 | 흑요석·은, 청보라 비전선 | 수정·룬 | 파란 MP |
| 전사 | 흑철·황동, 적금 강조 | 방패·대검 | 황금 스테미나 |
| 권사 | 흑칠·청동, 비취·금 강조 | 권갑·기 순환 | 황금 스테미나 |
| 도적 | 흑철·은, 독녹·자주 강조 | 교차 단검·그림자 | 녹금 스테미나 |

- 공통 기능 좌표는 직업 전환 시 움직이지 않습니다.
- 좌측에는 HP와 스킬 5칸, 중앙에는 아이덴티티, 우측에는 MP/스테미나와 아이템 5칸을 배치했습니다.
- 외곽 날개 장식이 스킬·아이템 바깥 빈 공간을 채웁니다.
- 52px 초록색 경험치 바는 하단 전체 폭에 밀착합니다.

## 구조 변경

### 이전

`WBP_Rogue10mMainHUD`가 아래 요소를 각각 직접 소유하고 갱신했습니다.

- `HealthBarWidget`
- `StaminaBarWidget`
- `IdentityBarWidget`
- `ProgressionWidget`
- `SkillSlotPanelWidget`
- `IdentityWidget`
- `ItemSlotContainer`

### 변경 후

```text
WBP_Rogue10mMainHUD
└─ BottomHUDWidget : WBP_BottomHUD
   ├─ HealthBarWidget
   ├─ StaminaBarWidget / ManaBarWidget
   ├─ IdentityWidget / IdentityBarWidget
   ├─ SkillSlotContainer
   ├─ ItemSlotContainer
   └─ ProgressionWidget
```

- `URogue10mMainHUDWidget`는 View 데이터를 수집해 `BottomHUDWidget`에 전달합니다.
- `URogue10mBottomHUDWidget`가 하단 파트의 소유자 연결, 슬롯 생성, 표시 전환을 담당합니다.
- 몬스터 정보와 시스템 로그처럼 하단 밖의 기능은 Main HUD에 유지했습니다.

## 자동 테마 판별

- Staff 또는 Mana 아이덴티티: 마법사
- GreatSword 또는 Rage 아이덴티티: 전사
- Knuckle/Unarmed 또는 StoneFist/Vigor 아이덴티티: 권사
- Dagger/DualBlades/Bow 또는 Focus 계열: 도적
- Blueprint의 `ThemeOverride`로 `Auto`, `Wizard`, `Warrior`, `MartialArtist`, `Rogue`를 강제 선택할 수 있습니다.
- 마법사이면서 Mana View가 활성화된 경우 우측 자원 위치는 MP를 표시하며, 그 외에는 스테미나를 표시합니다.

## 생성 자산

- `/Game/Widget/Parts/WBP_BottomHUD`
- `/Game/Widget/Debug/WBP_ClassBottomHUDShowcase`
- `/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Wizard`
- `/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Warrior`
- `/Game/UI/HUD/ClassThemes/T_HUD_Bottom_MartialArtist`
- `/Game/UI/HUD/ClassThemes/T_HUD_Bottom_Rogue`

디자인 시안과 실제 적용용 PNG 원본은 `Content/UI/HUD/ClassThemes`에 보존했습니다.

## 검증

- UE 5.8 `Rogue10mEditor Win64 Development` 전체 및 증분 빌드 성공
- 네 테마 Texture2D: 1920×208, UI Group, NoMipmaps, NeverStream 확인
- `WBP_BottomHUD` 필수 바인딩 14개 누락 0개
- 스킬 미리보기 5칸, 아이템 미리보기 5칸 확인
- `ProgressionWidget`: 전체 폭, 하단 -52px, 높이 52px 확인
- Main HUD: `BottomHUDWidget` 1개, 이전 하단 직접 자식 0개 확인
- `Saved/ClassBottomHUDValidation.txt`: `RESULT=PASSED`
- `Scripts/CheckGeneratedChanges.ps1`, `git diff --check` 통과

## 시각 QA

- 이미지 생성 시안 네 종을 실제 적용용 색상·문양 기준으로 사용했습니다.
- Unreal에 임포트된 동일 프레임 PNG와 1920×1080 UMG 좌표로 `Saved/ClassBottomHUDPreview` 적용 프리뷰를 렌더링했습니다.
- Windows Editor 화면 자동화는 로컬 ACL 오류로 사용할 수 없었지만, Widget Blueprint 컴파일과 UMGToolSet 구조 검증은 새 UnrealEditor-Cmd 프로세스에서 통과했습니다.
