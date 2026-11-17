# 내부 Windows 출력 경로 격리 패킷

실제 run04는 Game fresh link/metadata와 Cook 2,323개를 완료했지만 staging 종료 코드 1로 실패했다. 새 Windows 패키지는 아직 없다. 이전 run02의 Editor fixture 컴파일 실패는 수정되어 정규 Editor/native43D/Game에서 통과했다.

| 패킷 | 목표·범위 | 완료 조건·검증 | 복구 경계 |
| --- | --- | --- | --- |
| E1 진단 | 실제 UAT/Cook/Stage 및 Core FileOpenLog 소스와 원시 로그 대조 | CookOutputDir와 staging Windows 하위 경로 분리, Build/Windows/FileOpenOrder/EditorOpenOrder.log 단일 변경 귀속; Source/Config/Content/Editor6/저장 별도 검사 | 원본 파일을 되돌리지 않으며 기존 실패·새 생성 로그·옛 manifest 보존 |
| E2 격리 구현 | Feature의 새 internal-smoke-cook-fix-e만 작성 | CookOutputDir=<새 run>/cook/Windows, FOOBASEDIR=<새 run>/cook_user, Game UBT -Rebuild; 기존 검증 gate 유지, 오프라인 반례·독립 리뷰 | 원본 strict/B/B2/UTC runtime과 프로젝트 Source/Config/Content/Build/Scripts 수정 0 |
| E3 새 봉인 | 정상 Cook로 갱신된 Build 로그와 현 Game 제품을 포함한 새 전체 capture/seal | Source/Config366·Editor6·native43D가 같은 사실 확인, old seal 재사용 금지, 출력6/lease 새 이름 | 기존 capture01/03, run01/02/03/04, 기존 Game 제품을 삭제하지 않음 |
| E4 실제 검증 | 허용된 정규 UAT/IoStore/archive 및 목록 CLI | Source/Config/Content/Build/도구/저장 모두 새 봉인과 일치, fresh Game link/metadata, Cook/Stage/Archive 종료0, archive EXE 일치, 요구 맵 IoStore 확인 | 실패 시 완료·release 통과로 표시하지 않고 산출물 보존 |

설치 UE 5.8 근거: CookCommand.Automation.cs는 CookOutputDir를 cooker outputdir로 그대로 전달한다. 단일 CookByTheBook은 [Platform] 없는 override에 Windows를 자동 추가하지 않지만 Stage는 CookPlatform을 덧붙인다. 단일 Win64의 정규 교정은 출력 root의 Windows 하위 경로를 명시하는 것이다.

KeepFileOpenLog 기본 true로 -fileopenlog가 추가된다. Core IPlatformFileOpenLogWrapper.cpp는 FOOBASEDIR가 없으면 ProjectDir의 Build/Windows/FileOpenOrder/EditorOpenOrder.log를 append=false로 기록한다. 실제 run04에서 이 생성 로그가 3,638,816→6,325,417B로 바뀌었다. 정상적 빌드 생성물이지만 old full-input seal은 더 이상 일치하지 않는다. 이 파일을 ignore하거나 복구하지 않고 현재 값을 새 capture에 포함하고 이후 로그는 cook_user로 격리한다.

최상위 UAT -clean은 project Saved/Cooked/Windows를 삭제하므로 사용하지 않는다. 공식 UbtArgs -Rebuild는 Game descriptor의 CleanMode 및 재빌드를 같은 UBT에서 수행한다. -nocompileeditor로 Editor agenda를 제외하지만 공통 Intermediate/Build/SourceFileCache.bin도 정규 clean되므로 Game 파일만 바뀐다고 단정하지 않는다. Editor6의 실제 raw SHA와 전체 보호 입력·저장 gate를 유지한다. 수동 touch/소스 무의미 변경/생성 파일 삭제/manifest 복사로 통과시키지 않는다.

이 내부 결과는 1800m 미축소 후보, NPC 미배치, 기본맵 미전환, UI7/시각/물리 플레이 미검증이며 release_eligible=false다. Computer Use·GUI/입력 우회와 게임 실행은 금지한다. 1인칭 개발 보류·3인칭 계약을 유지한다.
