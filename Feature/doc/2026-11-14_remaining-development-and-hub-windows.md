# 남은 개발과 Hub 입구 포함 Windows 아티팩트

재연결 뒤 원본 작업 브랜치와 기존 dirty 변경을 확인했다. 새 Hub 입구를 포함한 별도 내부 Windows 아카이브의 정규 Build/Cook/Stage/Archive·실행기 사후 검사·새 바이너리와 cooked 콘텐츠 검사·최종 보존과 독립 전달본 복사를 완료했다. 새 게임 실행은0이다. 04:33:53UTC 최종 관측의 UE·빌드·게임 프로세스는0이다.

## 완료와 미완료의 경계

기존 완료 범위는 동일 Editor DLL의 Native51B 51/51, Compact 초급 의뢰의 Editor-game 수직 동작, Source Hub 포털1+표시Actor5 저장·정밀 cold reload, 이전 D 패키지의 Build/Cook/Archive·6맵+DA identity·샘플 ABI와 실제 시작 메뉴다. Editor-game의 위치 설정 검증을 자연 보행이나 packaged EXE 검수로 확대하지 않는다. D에는 새 Hub 입구가 없다.

| 남은 묶음 | 실제로 해야 할 일 |
| --- | --- |
| 정상 진입 | 새 Hub 입구 포함 패키징·정적 검증은 완료. 정상 프로필에서 실제 포털 여행·동일 RunId·1200초 시작 확인은 남음 |
| 월드 동선 | 현재 1800×1800m 후보의 편집층을 보존하며 크기·밀도 개선안을 확정하고 3큰거점+5초소·도로·충돌·NavMesh·보스 입구 연결. 전체 필드 배율이나 이동속도로 직행 시간을 맞추지 않고 실제 경로 측정 |
| 지역 콘텐츠 | 실제 연결된 서린 초급 의뢰1개에서 지역 NPC·상점·수련·재료 사용처·입문 보수와 탐험 비급 획득처로 확대. 카탈로그 정의만으로 런타임 연결 완료라고 하지 않음 |
| 몬스터 | 존재하는 정의·스포너·AI·전투 기반을 최신 맵의 일반15종 이상·중간4종·최종1종으로 연결하고 행동·추적/복귀·전리품·보스 전투 검수 |
| 스킬·연출 | 우선 너클·대검20기술과 마법 입문 콘텐츠를 독립 데이터·학습/장착·입력·모션·VFX로 완성. 구현된 피격취소의 실제 준비/접촉/회복 및 취소 화면 검수. 면역·슈퍼아머 도입은 보류 |
| 통합 QA | 패키지 프로필·NPC/F·보상·세이브·종료/취소/재시작·저장 실패·UI7, 물리 직행3회·성장 약15분·20분 상한 전체 런. 한글 표식·회색 재질·누락 의존 등 시각 결함도 수정 |

근거: [월드 기준](../../Docs/WorldMapProductionGuide.md), [현재 Hub](2026-11-13_hub-compact-entry.md), [초급 의뢰](2026-11-13_compact-intro-quest-vertical-slice.md), [이전 D](2026-11-13_native51-intro-internal-windows.md), [기술 제작표](../../Docs/SkillProductionTracker.md). 최신 획득 카탈로그의 runtime ID/path null·통합 false 및 배치 카탈로그의 runtimePlacementVerified=false는 실제 전종 콘텐츠 연결 근거가 부족함을 표시한다. 기반 C++ 전체가 미구현이라는 뜻은 아니다.

## 이번 패킷 진행 상태

독립 현황 문서를 실제로 읽고 후속 구현 순서를 좁혔다. 새 입구를 이용한 첫 의뢰 여행 → W 일반 몬스터1종의 실제 공격·귀속·전리품 → Q 현장 너클과 W 붕권 책의 획득/발견/학습 → M 장터 서비스1개 → B 아자토르의 승리 정산 연결을 권장한다. 이는 이번 패키징에서 새로 구현한 내용이 아니다.

전투·VFX의 후속 인수도 별도다. G08·편집형 VFX 소스는 통합됐지만 지정 G08 신규 후보는0/5, 근접20종 지정 DA는0/20이며 Staff 신규31장 검수도 확인되지 않았다. 현재 DLL의 피격취소9개 재회귀, Staff Primary01 후보1개 저장·cold reload와 3인칭 검수를 다음 작은 단위로 삼는다. Ctrl→첫Q 고정기한→QQQ 자동 시전 및 Circle/Release·새 지속 Charge/Trajectory의 실제 입력·이벤트 연결도 남는다. 이 문서 작성에서 새 테스트나 엔진 실행을 한 것은 아니다.

준비 결과: 전체10472개와 Source/Config368·Editor6·Native51 원문 pin이 일치했고 보호 저장·E5/B/D 양쪽 payload·C 원문/사본을 보존했다. 새 입력 봉인 SHA256은 `69F8CDF34E8ADE875F5D35CC1CE54C061A9C1A0C09A009C7A20C8AE485D63EB4`다. 실제 owner PID11500 세션의 SDK verify0, BuildCookRun0/332.93초, wrapper 사후 검사0이다. 시작04:05:16부터 종료04:23:58UTC까지 전체 입력 검사를 포함한 소유 세션을 기록했다.

공식 UnrealPak 목록과 봉인 identity validator가0으로 완료됐고 맵6+초급 DA·2252 export 행·158 WP 셀을 확인했다. `UnrealPak.exe IoStore -Describe=...`로 새 Actor6와 기존 포털3의 실제 export 이름·native class·숫자 Outer 체인·필수 component·Create/Serialize·직렬화 SHA1을 확인했다. 외부 Actor6개가 독립 cooked package로 남았다고 추정하지 않았다. target/timer/text/NoCollision 등의 typed FProperty 값은 이 도구가 해석하지 않으므로 Source cold03·불변 입력/cook 계보와 구분했다.

새 EXE는 `FA0F95D9724D09A11B4325B26855CB269A9901A0C5BE149B726802989E7061E6`, PDB는 `9E3E63C90040F71A73AFD9EAEB07C6D0227A651F020DCBC8E35347B7CFB4C1E4`다. project/stage/archive에서 각각 동일하며 D와는 다르다. 실제 새 PDB의 global→array96→const/forward→row48, 두 Info32, Version16·필드 오프셋을 조회하고 새 PE의 table/count2→consumer→registry 인자/+48·감소·backedge를 Root와 독립 리뷰가 읽었다. PE/PDB GUID는54374c23-9f87-414c-bb5f-7b1836818451/age1이다. generated→forced include→link12파일과 nested response4파일/1edge에서 actual compile 계보와 reload macro override 부재를 보강했다. 전체 ABI/모든 TU/CRT·registry 전체 본문 인증이나 게임 시작 성공은 아니다.

수집 도구 실패도 보존했다. native query 자체는 성공했지만 기본형 주석 `(unsigned)`를 받지 못한 파서가 중단됐고, 원문 pin을 재사용해 파서만 수정했다. 첫 Describe 호출은 IoStore dispatcher token 누락으로 usage 오류1이었다. 설치 소스의 정식 진입점을 확인한 뒤 새 B 출력만 호출해0으로 완료했다. 최초 실패 코드·로그·수정 전 스크립트를 지우거나 성공으로 바꾸지 않았다.

근거: [소유 실행 결과](evidence/hub-entry-windows20261114A/owner-result.json), [IoStore·허브 export](evidence/hub-entry-windows20261114A/iostore-hub-result.json), [새 ABI 제한 검토](evidence/hub-entry-windows20261114A/new-binary-abi-review.json), [nested response](evidence/hub-entry-windows20261114A/compile-response-supplement.json), [최종 보존·전달 증거](evidence/hub-entry-windows20261114A/final-result.json).

최종 전달본은 `D:/Project/Rogue10m/tmp/InternalWindows20261114HubEntryA_UNVERIFIED/Windows`다. 50파일/1,957,709,382B, payload SHA256 `B9C2ED1B46F045C2CA80A2D6844EED5C1AA023B1AE9F47F2010311EB8A80A60B`이며 [전달 manifest](../../tmp/InternalWindows20261114HubEntryA_UNVERIFIED/DELIVERY.json)와 한글 README를 함께 보존했다. 전체10472입력·Source/Config368·Editor6·보호 저장(Local0/프로젝트458)·E5/B/D양쪽 물리 사본이 동일하다. 기존 기록14핀과 C원문/복사6쌍도 보존했다. AST7/7, PowerShell구문0, 범위diff0, CheckGeneratedChanges0이다. 기존 WIP binary/줄바꿈 경고는 보존했고 commit/push0이다.

후속 사용자 지시는 실제 맵 축소와 배치 준비다. 위 Windows 아티팩트는 축소 전1800m 버전으로 동결하며 후속 맵 변경을 포함했다고 표시하지 않는다. 최신 후보/NPC/DA/Hub를 새 물리 체크포인트로 보존한 뒤 별도 후보의 정상 Landscape 편집 경로를 검토한다.

게임 창 timeout 이후 접근 재요청·다른 도구 입력·대체 창 실행을 하지 않는다. 별도 거절된 root-startup-approval-A.json 생성과 metadata/clone QA는 별도 승인 대기를 유지한다.
