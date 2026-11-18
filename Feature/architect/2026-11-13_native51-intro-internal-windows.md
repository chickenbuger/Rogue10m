# Native51·초급 NPC 포함 내부 Windows 빌드

목표는 처치 귀속 수정과 실제 저장한 Compact 초급 의뢰를 포함한 별도 Win64 Development 패키지다. 정상 공유 프로젝트에서 수집한 최종 입력과 동일 Editor DLL의 Native51B 증거를 결속한다. 기존 E5·strict·B/B2를 대체하거나 릴리스 완료로 표시하지 않는다. `root-startup-approval-A.json` 생성과 그에 의존하는 metadata/clone QA는 이 작업 범위 밖이며 계속 중단한다.

| 패킷 | 작업·완료 조건 | 검증·보존 경계 |
| --- | --- | --- |
| Scope | 게임·에디터·빌드 프로세스 없음, 외부 변경 및 전체 입력 확인 | 기존 10,460개 불변과 신규 Content 5개만 허용, 저장 2루트 보존 |
| Tool review | Native51Smoke zip SHA·33개 파일 및 실행 code 검토 | 도구는 Feature/implementation 별도 사본, 기존 봉인 runner 수정 없음 |
| Capture | Source/Config/Content/Build/Scripts/uproject와 선택 5맵·WP 외부 패키지·도구·저장 전체 캡처 | 현재 CA647 DLL·Native51B 51이름·경고19·전체 RED0을 정확히 pin |
| Freeze | 현재 콘텐츠 상태를 내부 빌드 입력으로 고정 | 1800×1800m, NPC1·회수2·미축소·기본맵/Hub 미전환·시각 문제를 명시; 승인/QA 완료 영수증이 아님 |
| Build | 정확한 소유 PS7 세션으로 정규 Game Rebuild/Cook/Stage/Archive | 새 Temp 물리 출력 6개, 기존 E5 양쪽 archive와 비중첩, SDK/UAT 실패는 실패로 보존 |
| Review | 실제 Game/stage/archive EXE 동일성 및 공식 IoStore 맵 identity | 입력·Editor·저장 전후 대조, 실행된 단계와 미검증 UI·실제 패키지 플레이 분리 |
| Doc | 결과·실패·미검증 및 산출물 경로 기록 | 한국어 DevLog/Notion 후보와 Sprint 갱신, 사용자 WIP commit/push 안 함 |

게임 QA는 별도 QA profile/UserDir의 `UnrealEditor.exe -game`이었다. 초급 수락·2회수·재방문·한 번 보상·중복 거부·새 RunId 초기화는 관찰됐지만 packaged EXE·UI7·종료 실패/재시작·15분 플레이 인증이 아니다. 후보 진입은 일반 허브의 기존 원본 포털 baseline 뒤 개발용 `open`을 사용했으며 일반 허브→Compact 연결은 아직 없다. 새 패키지는 이 한계를 그대로 유지한다.

Source/DLL을 바꾸지 않는 content-only 패킷이므로 Native51B의 동일 DLL 증거를 재사용한다. 소스 또는 DLL이 달라지면 이 결속을 재검토하고 새 빌드/관련 회귀를 수행한다. 지형·vendor 자산·기존 Actor를 추가 변경하지 않고 현재 저장 콘텐츠를 고정한다. 패키징 중 공유 소스/에셋을 수정하지 않는다.


실제 cook 선택은6맵이다. L_Menu·StartHub·OpenRunMap·FiveVillageGraybox20261110·WorldLayout20261112·Compact20261113A를 명시한다. 일반 Hub의 원본 WorldLayout 포털을 실제로 관찰했으므로 원본 맵도 선택에 추가해 포털 목적지를 누락하지 않는다. 이는 Hub 포털이나 기본 맵 변경이 아니며 Compact의 일반 진입 부재는 그대로 남는다. 전체 입력은 콘텐츠5개+정확히 귀속된 생성 캐시1개로10466개이며, 기존10460개와 보호 저장2루트/E5 보존이 확인됐다.


## 실제 패키지 시작 실패 후 Architect 복귀

Game/Cook/Stage/Archive0과 postflight·IoStore7개 identity는 통과했다. 실제 bootstrap 첫 실행은 창/로그 없이 사라졌고, process-group 분리와 반환코드 보존을 추가한 두 번째 정상 실행은777006으로 끝났다. 같은 무창 실패2회에서 단순 반복을 중단했다. Epic/설치UE소스의 CrashDuringStaticInit 분류이며 원래 SEH fault 자체의 코드는 아니다. 실제 메뉴/프로필/저장 routing probe는 도달하지 못했다. SourceConfig·맵·렌더링 설정을 추측 수정하지 않는다.

새 진단 패킷은 exact Game EXE99E843…15969와 일치 PDB GUID1fb36454-19bd-488e-b1e7-6337f82e59ea/age1에 대해 소유 프로세스만 Debug API로 시작해 원래 예외·OutputDebugString·레지스터를 수집하는 것이다. 기존 debugger CLI가 없어 helper만 별도tmp에 준비하며 실제 실행은 Root가 검토 후 판단한다. 신규 QA 루트,30초 이하 event 관찰, no-privilege-change·no-unrelated-attach·no-force-kill·정상detach가 경계다. Source/Config/Content/Binaries/Saved 직접 수정은 없다. 최초 fault 위치 또는 정확한 도구 차단을 얻으면 Reviewer로 원인과 최소 수정 범위를 정하고, 실제 소스가 바뀔 경우 새 Editor/회귀/입력 동결/후속 패키지 버전이 필요하다. 현재 Native51IntroB와 이전 E5는 보존한다. 거절된 Root 승인/metadata clone QA는 여전히 별도 중단이다.


## 명시 재개 D 패킷

상위담당의명시재개지시후C PID11792부재·UE/UBT/Game0·원문pre_sdk/null상태를재확인했다. C를재사용하거나승인사실을새로꾸미지않고새물리D영역/소유세션으로진행한다. 범위는FixF 도구(컴파일-NoLiveCoding 제거)와새capture/build/cook/archive/실제48/48·패키지메뉴/저장routing/초급의뢰/종료/재시작QA다. Source/Config/Content 변경없이현재입력을이전실제보존manifest와대조하고차이가나면외부변경으로보존해중단한다. Native51EditorCA/DLL과SourceCfg368이같으면해당기존회귀증거를정확히결속하고새UE51실행으로계산하지않는다. 신규ABI·startup·UI는실제관찰해야만통과한다. 별도Root승인/metadata cloneQA는중단유지다.
