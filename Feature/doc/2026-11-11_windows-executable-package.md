# Windows 실행용 Development 패키지 결과

## 실행 결과
- 최종 실행 파일: `D:/R10/Win1111A/ArchiveD/Windows/Rogue10m.exe`.
- 반드시 `Windows` 폴더 전체를 유지한다. 50파일, 1,947,609,156바이트(약 1.81GiB). EXE만 복사하면 실행할 수 없다.
- 실제 Win64 Game Target 컴파일, Windows cook, pak/IoStore, staging, archive를 완료했다. 기존 Editor DLL 빌드와 별개다.
- VS2022 Community 17.14.40 / MSVC 14.44.35207 / Windows SDK 10.0.22621.0 사용. 실제 Turnkey `VerifySdk` VALID/exit0. 새 소프트웨어 설치나 SDK 변경 없음.

## 체크포인트와 변경
공유 Source/Config/Content/Build metadata/Scripts를 `D:/R10/Win1111A/Rogue10m`에 물리 복사했다. 사용자 저장, 기존 빌드, 캐시와 Git 작업 상태는 복사·삭제하지 않았다. 원본 Source 314파일은 체크포인트와 그대로이며 패키징 중 의도적으로 변경한 Config는 DefaultGame.ini 하나다. 공유 원본에도 동일한 cook 수정과 검증 스크립트를 반영했다.

변경 파일:
- `Scripts/PackageWindows.ps1`: 실제 Game/Win64 목표, 4맵, SDK 확인만 수행, 새로운 archive/evidence 경로, exclusive Editor/build guard, 실패 보존, 실제 EXE 해시 기록. SDK 자동 설치/업데이트 옵션 없음.
- `Config/DefaultGame.ini`: native soft class/object default만으로는 cook되지 않는 실제 runtime 경로 15개 명시. Widget, 무기, 대검/공통 동작, 아이템, 스킬 카탈로그, UI 재질 등 좁은 경로를 지정. 1인칭 기능 개발/튜닝은 재개하지 않음.
- `Scripts/VerifyIoStore.ps1`: 실제 humanoid boss 프로필로 옛 기대값 교체, 필수 42개 exact ExportBundleData 확인. ChunkType 없는 약한 CSV는 거부.
- `Scripts/VerifyPackagedMenu.ps1`: missing-package/Widget/모든 Error 라인 실패 처리. 선택적 `-UseBootstrap`으로 루트 런처와 정확한 자식 EXE·UserDir·양쪽 exit0 확인.

## 실제 발견 및 보완
1. 첫 clean Game+Editor 빌드는 411동작, BuildCookRun 502.27초, exit0. 그러나 실제 packaged 메뉴가 검게 나왔다. native로 로드하는 MainMenu/CharacterLobby/HUD Widget과 무기 soft 자산이 cook되지 않았다. 첫 결과 `Archive`는 진단용으로 보존하며 사용하지 않는다.
2. Widget 등 runtime cook 규칙을 추가한 C는 41자산을 통과하고 메뉴가 보였다. 강화한 실행 검사가 header에 선언된 로비 미리보기 재질 누락을 잡았다. `ArchiveC` 역시 최종본이 아니다.
3. runtime header/cpp 203파일의 source-existing literal 81패키지를 추가 감사했다. 마지막 누락 `/Game/Material/UI/M_CharacterPreviewTransparent`의 단일 자산 폴더를 추가했다. 동적 조합 경로 전체를 증명한 감사는 아니다.
4. 최종 D는 실제 BuildCookRun 94.24초, exit0. cook 0오류/3경고. 경고는 기존 portal SM_Plane 빈 convex geometry 계열이며 해결했다고 주장하지 않는다.
5. 옛 Azathor Readability_1103 프로필은 저장된 DA가 humanoid `DA_HumanBoss_AttackProfile`로 이미 교체되어 있었다. 실제 활성화 기록·원본/체크포인트 해시·컨테이너 포함을 확인하고 검사 기대값만 정확히 이관했다. 오래된 프로필을 억지 cook하지 않았다.

## 검증
- 최종 IoStore: 필수 42/42, 누락0. ExportBundleData 2,165개, OpenRunMap WP 생성 셀80개. MainMenu/Lobby/HUD, Staff/대검/너클, native 동작, 로비 재질, Staff Primary01과 두 Niagara 및 현행 humanoid 프로필 포함.
- 4맵: L_Menu, StartHub, OpenRunMap, Prototypes/FiveVillageGraybox20261110. Hub 포털의 FName 목적지는 asset hard reference가 아니므로 네 번째 맵을 명시 cook.
- Game 내부 EXE와 루트 bootstrap EXE 모두 실제 DX12/SM6 cold 실행. inner exit0, bootstrap exit0, bootstrap child exit0. Save routing probe에서 격리 UserDir 아래 실제 Saved 경로 확인.
- 각 실행에서 지연 SHOWUI 1280x720 이미지2개, 총4개 직접 검토. 배경·제목·한글 시작/종료 버튼 정상. missing-package/UI/모든 Error 라인0. 실제 마우스 버튼 조작은 미검증.
- 원본 프로젝트 저장447개 SHA 보존, 추가/변경/누락0. Windows 기본 저장 폴더 전후0/0, 변경0. 검사는 별도 UserDir만 사용했고 프로젝트 저장을 Windows 기본 프로필로 이관하지 않았다.
- 남은 runtime 경고: 새 격리 프로필 저장 파일 부재, pawn 없는 메뉴의 null DefaultPawnClass spawn, 기존 portal plane convex 경고. 무경고 실행으로 표기하지 않는다.

## 재실행
프로젝트 루트에서 PowerShell 7:
`./Scripts/PackageWindows.ps1 -Configuration Development`
기본적으로 새 timestamp archive와 evidence를 사용한다. EngineRoot는 필요 시 명시한다. 실행 중인 Editor/build가 있으면 먼저 조율해야 하며 스크립트가 중복 실행을 막는다.

검증 명령은 새 출력 경로를 사용한다:
`./Scripts/VerifyIoStore.ps1 -PakDir <Windows/Rogue10m/Content/Paks> -CsvOut <fresh>/packages.csv -UnrealPakExe <UE5.8/Engine/Binaries/Win64/UnrealPak.exe>`
`./Scripts/VerifyPackagedMenu.ps1 -ArchiveDir <archive> -OutputDir <fresh>`
`./Scripts/VerifyPackagedMenu.ps1 -ArchiveDir <archive> -OutputDir <another-fresh> -UseBootstrap`

## 범위와 다음 단계
이번 결과는 Windows Development 패키지 및 cold 메뉴 검증이다. Shipping, 다른 깨끗한 PC에서 prerequisite 설치, 물리 입력/캐릭터 생성, Hub 포털 실 travel, 전체 런과 전투는 아직 별도 검증이 필요하다. prerequisites는 stage했을 뿐 설치하지 않았다. D3D11/다른 GPU의 실행도 별도 확인 대상이다.

자동 획득, 신규 G04/G10 및 20스킬 복제와 V2 취소 수정, NPC 마을 퀘스트, 보스 cadence, VFX 가독성 개선, 실제 map-travel QA fixture는 이번 체크포인트에 포함하지 않았다. 이후 순차 통합/Editor 회귀/Windows 재빌드를 진행해야 한다. commit/push/공개 배포는 하지 않았다.

## 증거
`Feature/doc/evidence/windows-executable-package-20261111/PackageD/verification-summary.json` 및 같은 디렉터리의 UAT 로그, exact IoStore CSV/journal, 두 실행 로그/이미지/보존 기록을 참조한다. 원본 전체 build/cook/stage는 `D:/R10/Win1111A`에 보존했다.
