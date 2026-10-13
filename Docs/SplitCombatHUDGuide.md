# 좌우 하단 전투 HUD

2026-10-14 승인 시안을 실제 UMG에 적용했다. 중앙 하단을 비우고, 왼쪽에는 체력·스태미나/마력·레벨·경험치, 오른쪽에는 공격4칸과 회피E·무기/아이덴티티·기존 잠금 아이템4칸을 배치한다. 인벤토리·스킬·성장·특성 관리 화면과 게임 데이터는 각각 기존 경계를 유지한다.

## 실제 데이터와 표현

체력은 현재/최대 수치와 초록 게이지다. 스태미나는 노란색이며 마법사/지팡이에서 기존 조건에 따라 활성 마력이 있으면 같은 위치에 파란 마력바를 표시한다. 초록색은 표시 View의 복사본에만 적용하며 능력치를 바꾸지 않는다. LV와 EXP도 실제 성장 진행을 사용한다.

스킬 슬롯은 기존 LMB / RMB / HOLD LMB / HOLD RMB / E 연결과 해금·쿨다운·준비 강조를 유지한다. 입력을 바꾸면 실제 바인딩을 표시한다. 아이템4칸은 기존 잠금 표시이며 새 사용 키나 아이템 기능을 추가한 것은 아니다.

아이덴티티의 무기명·자원 비율·숙련·없을 때 `--` 처리를 유지한다. 원형 게이지와 글자는 native 표시다. 새 전용 위젯의 `WeaponIconOverrides`에 맨손/너클과 대검의 신규 이미지만 연결하고 나머지는 기존 아이콘·fallback을 따른다. 무기 종류·아이덴티티 타입·게임플레이 자원을 재정의하지 않는다. 그림은 SourceArt의 최종 v2 PNG를 임포트한 별도 Texture다.

## 배치와 경로

| 항목 | 기준 |
| --- | --- |
| 좌 디자인 크기 | 560×180 |
| 우 디자인 크기 | 600×270 |
| 좌우/하단 여백 | 각24 UMG 논리 좌표 |
| 최소 중앙 간격 | 96 논리 좌표 |
| 축소 | 두 코어에 같은 비율, DownOnly·상속 DPI 유지 |
| 획득 알림 | 오른쪽 코어 위24 간격, 최대3개 |
| 시스템 로그 | 왼쪽 HUD 위 |

Bottom의 `bUseSplitCombatLayout`을 켜고 `UI_LeftCombatScaleBox`/`UI_LeftCombatDesignSize`, `UI_RightCombatScaleBox`/`UI_RightCombatDesignSize`를 바인딩한다. 각 SizeBox 안 고정 디자인 Canvas를 둔다. 창 폭·높이가 바뀔 때 Main의 기존 갱신 경로에서 두 패널을 같이 축소한다. 전체 폭의 연결 배경이나 경험치 바는 없다. 작은 화면에서는 폰트와 슬롯도 같은 비율로 줄어든다.

- Main: `/Game/Widget/WBP_Rogue10mMainHUD`
- Bottom: `/Game/Widget/Parts/WBP_BottomHUD`
- 전용 파트6개: `/Game/Widget/Parts/Split/`
- 신규 아이덴티티 Texture2개: `/Game/UI/HUD/Split/`
- 프레임: `URogue10mBeveledFrameWidget`의 native 선/꼭짓점 표시

기존 Compact 파트와 원본 PNG는 덮어쓰지 않았다. 기존 Main의 Bottom 자식만 동일 바인딩 이름으로 교체하며 타이머·몬스터 정보·메뉴 등 다른 위젯 이름을 보존한다. 획득 피드는 실제 아이콘·이름·수량과 만료를 유지한다.

## 재현과 검증

HUD 코드·에셋은 독립적으로 빌드할 수 있다. 아래 7조건 통합 QA는 별도로 관리하는 마을 진행 저장 기능의 `Rogue10mProgressionSubsystem`·`Rogue10mProgressionSaveGame`과 스냅샷 API가 함께 있는 작업 환경을 요구한다. HUD 단독 체크아웃에서는 해당 QA 명령이 의존성 부재를 명시하고 임시 단일 플레이 검증 프로세스만 종료 코드2로 종료한다. 에디터 콘솔에서는 오류를 기록하고 실행을 거절하며 에디터를 닫지 않는다. PowerShell 실행기도 실행 전에 필요한 헤더를 확인한다. 기록된 1,334개 검증은 저장 기능이 연결된 실제 통합 환경의 결과다.

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\VerifySplitCombatHUD.ps1 -Map '/Game/Rogue10m/Maps/StartHub' -Label 'split_new'
.\Scripts\CheckGeneratedChanges.ps1
```

빌드 뒤 새 reflected 필드를 로드한 에디터에서 `Scripts/Editor/ApplySplitCombatHUD.py`를 실행한다. 공식 TextureTools·UMG·ObjectTools로 지정10개 에셋만 저장한다. `Scripts/Editor/ValidateSplitCombatHUD.py`는 저장된 바인딩·크기·아이콘·슬롯과 dirty 상태를 읽기 전용으로 검사한다. 기존 부모 위젯을 컴파일할 때 다른 미리보기 Blueprint가 메모리에서 dirty로 표시될 수 있으므로 SaveAll로 저장하지 말고 원본/작업 범위를 먼저 확인한다.

최종 `split02`는 1280×720,1920×1080,2560×1440,1920×800,800×600(ApplicationScale1.25),마력 없는 지팡이,아이덴티티 없는 맨손의 7조건·1,334확인·실패0을 기록했다. 실제 표시 값·키·아이콘·동일 축소·중앙 간격·획득 알림/만료·저장 보존을 검사하고 PNG7장의 크기·현재성·렌더 결과를 확인한다. 기존 .sav67개가 동일 SHA로 보존됐다.

현재 검증 범위는 로컬 Editor Development 실행이다. Shipping cook/패키징·네트워크·장시간 GPU 안정성은 별도이며, 800×600에서는 HOLD/EXP 등 작은 보조 글자의 가독성이 제한된다. [적용 결과](../Feature/doc/2026-10-14_split-combat-hud.md)에 화면·실패/수정 기록과 변경 범위를 남겼다.
