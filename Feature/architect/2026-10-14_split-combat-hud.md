# 승인 시안 기반 좌우 하단 전투 HUD

## 범위와 기준

사용자는 imagegen으로 만든 몬스터헌터 참고 분리 시안을 실제 게임 HUD에 적용하도록 승인했다. 좌측은 체력·스태미나(기존 마나 교체 포함)·레벨·경험치, 우측은 공격4칸+회피E·무기/아이덴티티·잠금 아이템4칸이다. 중앙 하단에 연결 패널이나 게이지를 두지 않는다. 실제 능력치·스킬키·쿨다운·무기별 자원·숙련 연결을 유지하고 시안의 고정 숫자를 게임 값으로 사용하지 않는다. 인벤토리·스킬·성장·특성의 별도 관리 UI와 저장은 범위 밖이다.

최초 브랜치 Sprint#5-31-identity-image-assets에서 기존 미커밋 변경을 보존한 채 Sprint#5-32-split-combat-hud를 만들었다. 담당 소스/에셋의 원본을 tmp/split-hud-20261014/before/에 보관하고 승인 PNG를 결과 증거로 복사한다. 시작 시 Unreal Editor 프로세스0이며 공식 MCP 로컬8001 연결은 connection refused였다. 빌드는 명령줄로 하고 에셋 수정이 필요한 단계에서 최신 모듈로 Editor를 연다. 이 세션에 네이티브 MCP 메타 도구가 없으므로 기존 Scripts/InvokeUnrealMCP.py를 통해 동일한 공식 loopback MCP를 직렬 호출한다. 프로젝트 설정/플러그인 설치를 바꾸지 않는다.

## Ultrawork Packets

| 패킷·담당 | 목표/수정 영역 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| S1 Architect / root·독립 리뷰 | 현재 native/Blueprint 계약 확인, 시안 정보 보존 | 좌/우 bind·DPI·실데이터 경계 합의 | 소스/스냅샷/에디터 상태 확인 | 계획·baseline |
| S2 Implementer / UI agent | Bottom/Main HUD split 반응형과 획득 피드 배치 | 두 코어 동일 scale·중앙gap·HP 초록색 유지·마나 교체 | Editor 빌드·해상도 변경 QA | 담당4소스 baseline |
| S3 Implementer / root | 전용 Split Widget 파트와 Bottom/Main 연결, 시안 장식/아이콘 | 레벨/XP좌·실제identity우·5+4slot·새 전용 파트 저장 | 공식 UMG/ObjectTools inspect→apply→compile→save | 기존 Bottom/Main baseline·신규 Split 파트 |
| S4 Builder / root, QA agent | 전용 렌더 fixture/실제 데이터·기존 연결 확인 | 720p/1080p/1440p/ultrawide/800x600·원래 크기 복원 통과 | BuildEditor.ps1, VerifySplitCombatHUD.ps1 | 검증 소스/스크립트·격리 QA 슬롯 |
| S5 Reviewer / 독립 agent, Doc Writer / root | diff·화면·에셋 저장·재로드·문서 | 디자인/전투정보/겹침 검토, 새Editor재로드, 한국어 기록 | CheckGeneratedChanges.ps1·scoped diff·화면 | Feature/doc·가이드·DevLog·SprintChangeLog |

## 반응형 계약

`bUseSplitCombatLayout` 기본 false로 이전 위젯 경로를 보존하고 변경할 Bottom의 defaults에서 true로 켠다. Optional bind는 `UI_LeftCombatScaleBox`/`UI_LeftCombatDesignSize`, `UI_RightCombatScaleBox`/`UI_RightCombatDesignSize`다. 각 SizeBox 안 고정 Canvas를 둔다. 기준 좌560×180, 우600×270, 최소중앙gap96, side24/bottom24이며 UMG 논리 좌표·상속 DPI를 유지한다. 창 폭/높이 변화에 두 코어가 같이 축소된다. 획득 피드는 우 코어 위로, 로그는 좌 코어 위로 둔다.

기존 Identity 바인딩·실제 soft icon 경로·원형 native 자원 gauge를 유지한다. 건강 초록색은 표시 View의 복사본에만 적용한다. 마법사의 마나 대신 스태미나를 강제로 넣지 않는다. 프레임과 장식은 UI 표시에만 속하며 게임플레이 컴포넌트·저장 스키마·밸런스를 변경하지 않는다. 신규 reflected field는 정식 빌드 뒤 Editor 재시작으로 로드한다.

실패가 두 번 반복되면 같은 수정을 재시도하지 않고 Architect/Reviewer에서 원인을 다시 확인한다. 실제 저장과 기존 동시 작업을 보존하며 커밋·푸시는 하지 않는다.
