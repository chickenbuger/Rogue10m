# 하단 전투 HUD 시안 적용

- 작업: Sprint#5-15-compact-combat-hud
- 기준: 대화에서 승인한 두 번째 imagegen 시안. 하단으로 내린 검정·청동 패널, 붉은 체력/금색 스태미나, 중앙 무기 문장, 5개 스킬/4개 잠금 아이템, 패널 안 경험치, 오른쪽 획득 카드.
- 범위: HUD C++ 표현과 위젯 Blueprint, 재현 도구, QA와 기록. 전투 데이터/스킬 해금/맵/애니메이션은 수정하지 않는다.
- 기존 미커밋 작업을 이어받아 현재 HEAD에서 새 작업 브랜치 생성. 커밋·푸시 없음.

## Scope Gate / Architect

현재 PlayerController → WBP_Rogue10mMainHUD → WBP_BottomHUD 구조다. ReferenceMetal 생성기는 1200×208, 아래56, 별도 전폭 경험치와 고딕 장식 프레임을 쓴다. C++만 바꾸면 직렬화된 Blueprint 기본값이 남으므로 두 경로를 같이 수정한다. 공용/ReferenceMetal 파트는 보존하고 Compact 전용 파트를 생성한다. 네이티브 UMG 둥근 브러시와 간단한 기하 장식으로 구현 가능한 시안을 반영한다.

## Ultrawork Packets

| Packet / 역할 | 목표·수정 영역 | 완료 조건 | 검증 | 복구 경계 |
| --- | --- | --- | --- | --- |
| 1 / Implementer | UI native 옵션: 잠금 기호/두 줄 입력/무기명/획득 카드 클래스·데이터 | 기존 위젯 fallback 유지, 새 파트 바인딩 지원 | BuildEditor.ps1 및 실제 런타임 데이터 확인 | 이번 작업 전 UI 소스 사본/패치 |
| 2 / Implementer | Compact 파트, Main/Bottom 연결, 1200×196·아래20·경험치 내부 | 명시 패키지 컴파일·저장, 옛 프레임 제거 | 새 Editor validator, 공식 MCP 조회 | Main/Bottom 원본 패키지 사본, 신규 파트 경로 |
| 3 / Builder→Reviewer | DPI/화면비/슬롯·자원·경험치·획득 데이터 QA | 빌드 통과, 실제 렌더 캡처·독립 리뷰 | 게임 실행 및 좁은 HUD runtime 검사 | HUD 전용 변경 |
| 4 / Doc Writer | 결과/증거/일일 개발로그/Sprint 기록 | 검증 결과와 한계를 한국어로 기록 | diff·CheckGeneratedChanges | 추가 문서·해당 append |

## 배치·호환 계약

- 중앙 앵커, ScaleToFit/DownOnly와 기존 DPI 상속을 유지한다. 글자·아이콘을 먼저 줄이지 않고 세로 패딩을 줄인다.
- 디자인 코어 1200×196, 하단 여백20, 양옆24를 기준으로 실제 화면에서 확인한다.
- 경험치·레벨은 같은 CombatCanvas 안에 배치한다. 긴 HOLD 입력은 두 줄로 표시한다.
- 실제 스킬 아이콘과 잠금 상태, 쿨다운, 드래그 연결을 유지한다. 아이템4칸은 현재 잠금 동작을 유지한다.
- 중앙은 실제 무기명을 표시하고 자원이 있는 무기는 게이지 정보를 보존한다. 고정된 체력/수량/가짜 아이템을 넣지 않는다.
- 획득 카드 전용 클래스는 없으면 기존 LogLineWidgetClass로 fallback한다. 시스템 로그와 분리한다.
- Blueprint는 Editor 경로로만 수정한다. 수정 전 Main/Bottom 패키지와 HUD 소스를 체크포인트로 보관하고 변경 후 명시 저장한다.
- 새 리플렉션 프로퍼티는 정식 빌드·Editor 재시작 후 적용한다. Editor 실행/PIE 상태를 확인하며 라이브 MCP 호출은 직렬화한다.

## Exit Gate

요청 시안이 실제 HUD에 연결되고, 관련 빌드·저장 검증·실제 화면 확인·독립 Unreal 리뷰를 완료한다. 검증 불가 범위는 결과에 명시하고 기존 변경을 보존한다.

2026-10-05 Exit Gate 완료: 7개 패키지 저장·검증과 공식 MCP dirty=false, 전체 UI 빌드 및 별도 최종 모듈 빌드 성공, 실제 StartHub 6단계 922 assertion·실패0, 일반/좁은 화면 확인·독립 리뷰·개발로그·Sprint 기록 완료. 실제 HOLD 글자 높이에 맞춰 슬롯72×108/키72×42로 조정했고 레벨은y166으로 올렸다. 결과와 시행착오는 `Feature/doc/2026-10-05_compact-combat-hud.md`에 기록했다.
