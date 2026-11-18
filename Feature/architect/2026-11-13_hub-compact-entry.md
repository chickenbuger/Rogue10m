# Hub에서 Compact 후보로 가는 추가 입구

이 패킷은 현재 공유 프로젝트의 정상 콘텐츠 제작이다. 이미 실행 중인 D 게임창의 입력·접근 승인을 대체하거나 거절된 metadata/clone QA를 수행하지 않는다. D 패키지는 이 변경 전 버전으로 보존한다.

## Ultrawork Packet

- 목표: StartHub의 기존 포털 3개와 기본 맵을 보존하고 Compact 후보로 가는 별도 포털 1개를 에디터에서 편집 가능한 외부 Actor로 저장한다.
- 입력: 현재 work branch, Source/Config/Content, 기존 Hub·후보 맵, IntroSupply DA, Editor DLL, D의 봉인 입력과 독립 payload.
- 변경 위치: 새 Hub 외부 Actor 패키지만. Source/Config·기존 umap·기존 Actor·기존 DA는 변경하지 않는다. 모든 도구/증거는 이번 Feature 폴더와 별도 tmp 경로에 둔다.
- 완료 조건: 실제 Hub/후보·GameMode/PlayerStart/지원 가드 확인, 새 입구 저장과 별도 프로세스 재로드, 기존 3포털의 직렬화와 모든 이전 파일 SHA 보존.
- 검증: 정식 PythonScript commandlet의 읽기 검사 → API 저장 → 새 프로세스 읽기 검사. 물리 충돌 query는 허용되는 범위에서 별도 기록하며 실제 포털 travel/사용자 입력을 대신하지 않는다.
- 복구 경계: 최초 저장은 이전에 없던 새 Actor의 Hub 외부 패키지로 제한한다. 후속 보정은 이번 소유 신규 패키지를 다시 물리 백업한 뒤 그 집합 안에서만 허용한다. 작업 시작 전에 존재한 파일을 쓰려 하면 중단한다. 원본/사용자 변경을 자동 롤백하지 않는다.

## 현재 제약과 보존

현재 D bootstrap14384/Game54244는 독립 물리 payload와 별도 QA UserDir에서 실행 중이다. 접근 승인이 시간 초과된 창에는 추가 getApp·입력·다른 API 우회를 하지 않는다. 새 게임·공유 빌드를 실행하지 않는다. 새 콘텐츠 commandlet은 null RHI·별도 UserDir로 원본 프로젝트만 다루고, D payload/저장과 경로가 분리됨을 먼저 검증한다.

기존 root-startup-approval-A.json 생성 및 의존 metadata/clone QA는 계속 별도 승인 대기다. 이 파일이나 기존 거절된 런처를 사용하지 않는다.

## 설계

기본 Rogue10mRunPortal의 새 인스턴스를 사용한다. target_level_name은 WorldLayoutCompact20261113A, start_run_timer_on_travel=true, additional_travel_options는 빈 문자열로 두어 기존 StartRun=1 경로를 사용한다. FiveVillage subclass/GameMode 또는 특정 월드 가드를 변경하지 않는다. 기존 보급·통화 baseline 저장, 1200초, RunId·귀환·재방문 계약을 유지한다.

제안 위치는 기존 북쪽 포털 행에서 x=-1440cm, y=4270cm다. 실제 지면/접근 통로·기존 trigger bounds를 읽은 뒤에만 높이와 배치를 확정한다. 기존 플레이어 시작점과 후보 NPC/목표는 움직이지 않는다. 기존 세 입구보다 후보임을 분명히 알 수 있도록 영어 표식 COMPACT CANDIDATE를 사용한다. 월드 한글 TextRender 문제를 해결했다고 주장하지 않는다.

Engine BasicShapes Cube와 프로젝트 소유 WarmStone/PortalGlow 재질을 재사용한다. 새 Fab 가져오기·구매·vendor 원본 수정은 없다. 로컬 재질 사용을 Fab 계정 소유권 확인으로 확대하지 않는다. Actor마다 고유 label/tag를 주고, 새 folder 객체 생성 없이 정확한 6개 외부 Actor 패키지만 저장하는 방향을 우선한다.

## 검사 범위

기존 세 포털의 class/path/label/target/timer/options/전체 transform, 후보 WorldSettings/CDO/GameState·PlayerStart·Director·DA 지원 맵을 실제 API로 읽는다. clean 상태에서만 새 Actor를 만들고, save_packages 직전에 새 Actor desc의 정확한 패키지 집합과 dirty 집합이 같은지 확인한다. 원래 존재한 파일 경로는 저장 집합에서 거부한다.

전체 입력 SHA와 물리 Hub/후보·Source/Config·보호 세이브 백업을 남긴다. D의 기존 archive와 전달본 두 물리 사본은 새로 덮어쓰지 않고 각각 해시로 보호한다. 저장 후 모든 이전 입력 파일이 동일하고 허용된 새 패키지만 늘었는지 검사한다.

실제 일반 포털 overlap·profile/save IO·quit/restart·20분 런은 현재 승인 차단 때문에 미실행으로 남길 수 있다. Static/API 저장·재로드를 런타임 완주로 표시하지 않는다. 맵은 여전히1800m이며 이번 입구 추가는 축소 작업이 아니다.

## 실제 사전 검사와 저장 전 검토

첫 inspect01은 지면 query가 모두 None이어서 저장 전에 중단했다. 이것만으로 바닥 부재나 commandlet의 물리 미지원이라 판정하지 않았다. 설치 엔진의 StaticMeshCompiler.cpp와 AsyncCompilationHelpers.cpp에서 공식 Editor.AsyncStaticMeshCompilationFinishAll 경로를 확인했다. 준비 완료가 StaticMeshComponent::PostStaticMeshCompilation의 RecreatePhysicsState로 이어지므로, 다음 독립 Source commandlet에 이 barrier를 넣었다. 실행 중인 D 게임에 명령을 보내지 않았다.

inspect02에서는 HitResult가 반환됐지만 Python에 노출되지 않은 blocking_hit 속성을 사용한 검사기 오류가 있었다. UE5.8 HitResult의 HasNativeBreak와 GameplayStatics::BreakHitResult 선언을 읽고18개 반환값의 공식 to_tuple 바인딩으로 교정했다. 첫 두 실패의 process/error/log는 보존했다.

inspect03은 exit0/RED0이다. 실제 Hub685개 descriptor의 Actor가 전부 로드됐고, 세 지점에서 바닥Z0/normalZ1, radius42·half-height96cm의 Visibility capsule 접근 query는 막힘 없음이다. 이는 실제 플레이어 이동 테스트가 아니다. Hub/후보의1200초·prototype override=false·자동시작=false, 후보 PlayerStart/Director/지원DA와 기존3포털을 확인했다.

첫 author01은 메모리에서6Actor를 만든 뒤 get_actor_descs에 신규Actor가 있어야 한다는 조건에서 저장 전에 중단했다. save-intent/result는 없고 save_packages까지 도달하지 않았다. 원문 오류에는 신규 descriptor의 실제 수가 없으므로0개라고 단정하지 않는다. 설치 ActorDescContainer::OnObjectPreSave와 WorldPartitionBlueprintLibrary의 디스크 snapshot 설명을 읽어 첫 저장 전 descriptor를 소유권 근거로 쓴 전제가 잘못됐음을 확인했다.

후속 author02는 전체 입력 불변을 먼저 재확인한 뒤 공식 Actor.is_package_external/get_package로 소유권을 검증한다. 새 Actor6개 각각의 실제 외부 Package, 고유 이름/path, DataLayers없음, 엄격한 Hub 하위 경로·디스크에없는파일·이전manifest에없는파일을 모두 요구한다. dirty Package 객체/이름집합이 그6개와 정확히 같아야 하며 그 Package객체만 저장한다. 기존 map 저장·강제external 전환·기존 파일 덮어쓰기는 허용하지 않는다. descriptor는 저장후와 cold reload에서 확인한다.
