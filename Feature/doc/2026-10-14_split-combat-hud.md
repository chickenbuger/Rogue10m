# 승인 시안의 좌우 하단 HUD 적용

2026-10-14, 승인한 imagegen 분리 시안을 실제 게임 UMG에 적용했다. 왼쪽에는 초록 체력·노란 스태미나/파란 마력·LV·EXP, 오른쪽에는 공격4칸과 회피E·무기/아이덴티티·잠금 아이템4칸을 배치했다. 중앙 하단 연결 패널을 제거하고 창 크기에 따라 두 코어가 같이 축소되도록 했다.

인벤토리·스킬·성장·특성의 별도 관리 화면, 저장 스키마와 밸런스는 변경하지 않았다. 시안의 고정 숫자를 넣지 않고 실제 캐릭터 값·입력 바인딩·쿨다운·숙련·아이덴티티 자원을 사용한다. 세부 경로와 튜닝은 [사용 가이드](../../Docs/SplitCombatHUDGuide.md)에 정리했다.

## 변경

- Bottom/Main native 코드에 분리 모드와 좌우 Optional bind, 실제 경계 조회·반응형 배치를 추가했다. 기준 좌560×180/우600×270,여백24,최소중앙gap96이며 상속 DPI를 한 번만 적용한다. 획득 알림은 오른쪽 HUD 위로 올리고 시스템 로그는 왼쪽 위에 둔다.
- 생존 표시·글자·슬롯·원형 자원 게이지를 기존 native 연결로 유지했다. 새로운 native 프레임은 청동/상아 선과 모서리로 시안의 형태를 구현하며 UMG 내부 내용과 같이 축소된다. 별도 장식 PNG로 전체 HUD를 굽지 않았다.
- 원본 v2 맨손·대검 PNG를 새 Texture2개로 공식 도구에서 임포트했다. `WeaponIconOverrides`는 전용 SplitIdentity에만 적용한다. 맨손/너클과 대검의 그림만 바꾸며 원래 게임 View의 아이콘 경로·자원 타입·수치·무기명은 보존한다. 나머지 무기는 기존 이미지/fallback을 따른다.
- 전용 Split 파트6개와 기존 Main/Bottom2개만 구성했다. 다른 Main 위젯 이름은 동일하게 유지했다. 이전 Compact 파트는 보존했고 기존 ApplyCompact 도구가 분리 모드를 끄도록 호환 처리했다.

## 실제 검증

| 검사 | 결과 | 증거 |
| --- | --- | --- |
| Editor 빌드 | 최초45.12초, 프레임 수정 후 최종 링크3.88초 성공 | [최초](evidence/split-hud-20261014/build01.log), [최종](evidence/split-hud-20261014/build03-success.log) |
| 최종 게임 화면/데이터 | 7조건·1,334확인·실패0 | [split02 결과](evidence/split-hud-20261014/Rogue10m_Town_QA_split02/runtime-contract.txt) |
| 기존 저장 보존 | .sav67개 동일 SHA·예상 외 신규 저장 없음 | [저장 검사](evidence/split-hud-20261014/Rogue10m_Town_QA_split02/save-preservation.txt) |
| 저장 에셋 계약 | 지정10개·12bind·5+4slot·dirty0 | [계약 결과](evidence/split-hud-20261014/saved-contract.json) |
| UI 에셋 보존 | 기존80개 중78개 동일, Main/Bottom2개 변경·8개 추가·삭제0 | [해시/경로](evidence/split-hud-20261014/asset-preservation.json) |
| 독립 정적/시각 검토 | 발견한 비율 글자 P2 수정 후 미해결 항목 없음 | [리뷰](evidence/split-hud-20261014/review.md) |

7조건은 720p·1080p·1440p·울트라와이드·800×600 ApplicationScale1.25,마력 없는 지팡이와 아이덴티티 없는 맨손이다. 실제 건강/자원 표시를 두 번 갱신하고 원형 게이지 값·현재 무기 그림·`--` 조건, 스킬5개 입력명, 좌우 경계·같은 축소비·중앙 여백, 획득 알림 겹침/만료를 검사했다. 확인 수에는 기존 저장 파일별 바이트 보존 검사도 포함된다. PNG7장의 실제 해상도·현재 생성 여부·유효 렌더링을 검사했다.

root와 독립 검토자가 [대검1080p](evidence/split-hud-20261014/Rogue10m_Town_QA_split02/SplitCombatHUD_01_1920x1080.png), [마력1440p](evidence/split-hud-20261014/Rogue10m_Town_QA_split02/SplitCombatHUD_02_2560x1440.png), [작은 화면](evidence/split-hud-20261014/Rogue10m_Town_QA_split02/SplitCombatHUD_04_800x600.png)을 직접 열어 확인했다. 코드/스크립트 공백·Python 문법·생성 출력 경로 검사도 통과했다. 최종 [새 에디터 재로드](evidence/split-hud-20261014/fresh-editor-validate.json)에서도 저장 계약이 통과했다. 마지막 StartHub 에디터는 [PIE 종료](evidence/split-hud-20261014/final-is-pie.json)·dirty0 상태다.

## 발견한 문제와 수정

공식 Texture 스키마의 `lODGroup` 약어 표기 때문에 첫 적용이 멈춰 실제 프로퍼티 이름을 발견/읽은 뒤 설정하도록 보완했다. 새 프레임 꼭짓점 배열에 자기 요소를 Add해 썸네일 저장 중 assert가 발생했고, 지역 값 복사로 고쳤다. 충돌 보고 프로세스의 DLL 잠금으로 중간 링크가 실패한 로그와 수정 증거를 보존했으며 최종 링크가 성공한 뒤에 에셋 적용을 완료했다.

첫 split01은 화면/데이터 검사가 통과했지만 시각 검토에서 원 테두리가 퍼센트 글자 하단을 가리는 문제를 발견했다. 이름을y99,비율을y123/h18로 옮겨 하단141 안에 두고 그림을1.12배로 표시했다. 전용 Identity 파트 하나만 수정·저장한 뒤 새 split02로 전체7조건을 검증했고 독립 캡처 리뷰에서 해결을 확인했다. 실패/수정 기록을 삭제하지 않았다.

부모 Bottom Blueprint 컴파일로 기존 디버그 showcase가 메모리에서만 dirty가 됐다. 최초 원본 SHA가 그대로임을 확인하고 해당 패키지만 디스크 버전으로 재로드해 저장 없이 보존했다. 이 과정에서도 기존 미리보기 파일은 동일 SHA이며 마지막 dirty 목록은 비어 있다.

## 작업 경계

Sprint#5-32-split-combat-hud에서 이전 미커밋·아이덴티티 이미지·전투/저장 작업을 보존했다. 수정 C++5개, 신규 QA CPP1개, 신규 스크립트3개와 이전 Compact 적용기1개, 새 에셋8개/기존2개가 이번 범위다. 담당 소스·기존 Main/Bottom의 원본을 tmp/split-hud-20261014/before/에 보관했다. 두 원본 v2 PNG의 SHA도 동일하다. 모듈/Build.cs·프로젝트 설정·맵·게임 데이터는 이 작업에서 수정하지 않았다. 커밋·푸시는 하지 않았다.

검증은 로컬 Editor Development 실행에 한정한다. Shipping cook·네트워크·장시간 GPU 안정성은 별도다. 작은800×600 화면의 보조 입력·EXP 글자는 축소되어 가독성이 제한된다. 승인 시안의 배치·색·형태를 native UI로 구현했으며 래스터 시안의 모든 금속 질감이나 픽셀을 동일하게 복제한 것은 아니다.

[ULW 계획](../architect/2026-10-14_split-combat-hud.md), [이전 중앙 HUD 기록](../../Docs/CompactCombatHUDGuide.md), [일일 로그](../../DevLog/20261014.txt).


## 사용자 승인에 따른 HUD 커밋·푸시 준비 (2026-10-14)

이번 HUD 변경의 커밋·푸시를 사용자가 명시적으로 승인했다. 전체 작업 트리를 추가하지 않고 HUD 네이티브6개, 전용 에셋10개, 임포트한 원본 PNG2개, 적용/검증 도구와 관련 문서·증거를 대상으로 잡았다. Main 헤더와 공통 Compact 작성 도우미는 이전 미커밋 HUD 표현 기능이지만 현재 레이아웃의 획득 로그 클래스·무기명·잠금·HOLD 표시를 재현하는 데 필요하므로 포함한다. 별도 특성·진행 저장·전투·카메라·맵·모듈 설정은 제외한다. SprintChangeLog도 HEAD의 기존 내용과 이번 Sprint#5-32 항목만 스테이징해 이전 작업 기록을 로컬에 보존한다.

새 7조건 통합 테스트의 진행 저장 스냅샷 의존성을 명시하고 조건부 컴파일을 추가했다. 저장 기능이 없는 HUD 단독 체크아웃에서는 동일 명령이 오류를 표시하고 임시 standalone 프로세스만 종료 코드2로 종료한다. 에디터에서는 실행을 거절하며 종료하지 않는다. PowerShell도 실행 전 필수 헤더를 확인한다. 기존 통합 테스트 본체·게임 HUD 표현·에셋은 이 준비에서 바꾸지 않았다.

HEAD의 추적 Source/Config/uproject와 스테이징한 HUD 소스7개만 별도 폴더로 추출해 빌드했다. 진행 저장 헤더와 다른 미커밋 소스가 없는 환경의 Editor 빌드가 통과했고, 종료 보호를 보완한 최종 QA 파일도 실제 재컴파일·링크했다. [독립 빌드](evidence/split-hud-20261014/commit-clean-build.log), [최종 빌드](evidence/split-hud-20261014/commit-clean-build-final.log)에 원본 로그를 보관했다. 실제 통합 환경의 7조건·1,334검사 결과는 앞선 split02 증거다. 독립 커밋 범위·종료 보호 검토에 미해결 항목은 없다.

사용자 승인 범위의 작업 브랜치 커밋과 일반 원격 푸시를 진행한다. 최종 포함 파일은 [커밋 파일 목록](evidence/split-hud-20261014/commit-files.txt)에 기록한다.
