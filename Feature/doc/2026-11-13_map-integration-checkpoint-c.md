# 맵 통합 체크포인트 C — 소스 통합과 실제 native 43검사

## 완료한 변경

동일 work branch `Sprint#9-32-landscape-world-blockout`에서 사용자 WIP를 보존했다. 현재 소유 helper2개 외에 독립 검토된 G08+VFX 누적29파일과 NPC full30파일을 **실제로** 설치했다. NPC delta나 개별 G08/VFX 패치를 중복 적용하지 않았다. 사전 raw/정규화 blob·신규 경로·패치 SHA·물리 백업을 확인하고, 통합 postimage는 누적33개 raw SHA와 NPC30개 정규화 blob이 일치했다. 기존 원본 2478개 중 의도적으로 바뀐 파일은 패킷 대상 Source7개뿐이다. 나머지2471개, 특히 기존 Content2139개와 Config9개를 보존했다.

통합 첫 UHT는 배열 속성의 `Units` 두 곳에서 실패했다. 원래 패킷과 실패 로그를 남긴 뒤 `AuthoredContactTimes`와 `ObjectiveLocations`의 배열 Units만 제거하고 주석/ToolTip에 seconds·centimetres를 명시했다. 필드 이름·타입·편집 플래그·런타임 값은 유지했다. 독립 검토상 해당 테스트도 Units에 의존하지 않는다. 원래 패킷의 pin을 완화하지 않았으며 이 수정은 별도 derivative로 기록했다.

최종 정상 UHT/Editor 빌드는 exit0, **104.31초**(UBT103.83초)다. 현재 DLL SHA256:

`180DA46E668B12C82DCF3A85B4EB51DB27DB14DBF3E7131FADE312CBB56CB76D`

NPC 작성 안내2개와 G08 architecture/result2개도 새 경로에만 복사했다. 이전 격리 결과 문서의 미실행 표시는 당시 기록이며, 이번 통합 결과는 이 문서와 아래 실행 근거가 기준이다.

## 실제 검증

| 검사 | 이번 실제 결과 | 범위 |
| --- | --- | --- |
| G08 접촉/회복 | 23 + 인수 파서1 통과 | transient native world·physics·timer·몽타주 instance 모델; 실제 Montage_Play·포즈 아님 |
| VFX placement | 1 통과 | 벡터/CDO 기본값; Niagara 렌더 아님 |
| NPC 퀘스트 | 18 통과 | 정의·선택·좌표·보상·delegate 재진입·수명 모델 |
| 전체 native | **43/43 성공**, 누락/추가/중복0, exit0·136.02초 | 엔진 보고서37 clean +6 warning cases, 실패0·미실행0 |
| NPC 작성 도구 | 오프라인30 통과 | fake Unreal 작성/guard 모델, 실제 에셋 생성 아님 |
| 후보 cold reload | 공식 Python commandlet exit0·25.67초 | 같은 최종 DLL, 지형256/충돌256/프록시16·레이어1 |
| 지형 기준값 | 이전/재로드 semantic·metadata BLAKE3 일치 | RG높이·paint·메타데이터; raw B/A 제외 |

native43 실행 전후 DLL과 Source/Config 내용·파일 집합이 모두 같다. 입력 모방·GUI·스크린샷을 호출하는 console fixture는 실행하지 않았다. 6개 warning case의 경고12개는 transient fixture의 Unarmed profile 부재 또는 수집물 Destroy 시 world context 부재다. 경고를 억제하거나 지우지 않았다. Engine `LevelActor.cpp:1022..1034`에서 context 경고 후 RemoveActor가 계속됨을 확인했고 해당 테스트는 Destroy 반환값도 검사한다. 이 역시 실제 런 월드/네트워크/물리 겹침 검증을 대신하지 않는다.

## 현재 맵과 미완료

후보는 여전히 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`, **1800×1800m**, 로드된 editor actor1360개다. 신규 QuestDirector와 새 퀘스트 DataAsset은 아직 배치·생성하지 않았다. 기본 플레이맵과 포털/cook 목록을 전환하지 않았다. 소스와 테스트가 통합됐다는 사실을 게임 콘텐츠 배치 완료와 구분한다.

후보 GameMode/Pawn 에셋 이름은 `BP_FirstPersonGameMode`/`BP_FirstPersonCharacter`지만 실제 CDO는 각각 `Rogue10mGameMode`/`Rogue10mCharacter`를 상속하고 GameState/PlayerState도 Rogue10m이다. 이름만으로 1인칭이라고 단정하지 않는다. 카메라·입력 코드는 변경하지 않았으며 실제 3인칭 플레이 검수는 미완료다.

GUI 금지 이후 **Computer Use/GUI 입력0회**를 유지했다. 이전 창 Raise1회와 상태/화면 조회 외에 Recovery 선택·지형 삭제·저장 입력은 없었다. 기존 Recovery 선택 내용은 알 수 없지만 이후 dirty0과 원본/후보 SHA 일치를 확인했다.

native Landscape Manage/Delete는 사용자가 금지한 GUI 조작이 필요하여 보류다. public Import도 raw EditLayer B/A, 새 GUID, 브러시 소유권 이동, PCG·foliage·spline·외부 참조 보존이 입증되지 않아 적용하지 않았다. private EdMode나 단순 Component 파괴로 우회하지 않았다. 9×5(1012.5×562.5m)안은 선택 preview와 수치 계획일 뿐 최종 채택·실제 축소가 아니다. 원본과 후보 복구용 물리 checkpoint는 그대로다.

Windows delta는 도구44파일 해시/경로·JSON22/PythonAST7/PowerShellAST4를 검토한 **UNSEALED 준비물**이다. 게임 빌드가 아니다. 기존 Quit Source7은 B2와 동일하여 재설치하지 않았고 새 최종 DLL의 quit7 증거는 없다. 기존 fixture의 OnClicked/Slate/InputKey를 GUI 우회로 실행하지 않았다. Compact cook 선택·최종 입력 봉인·Supplement derivative·Game/cook/package·실제 패키지 UI/저장/재시작 검증이 남았다. 이전 ArchiveE/B/B2 봉인을 재사용·수정하지 않았다.

또한 NPC의 미귀속 몬스터 사망 시 encounter receipt가 먼저 소비되는 기존 전투 목표 문제는 미해결이다. 이번 초급 예제는 전투 목표가 없어 이 문제를 해결했다고 볼 수 없다. 물리 직행3회/성장15분/완주, 실제 몽타주·접촉/회복·VFX31장·편집 저장/cold readback 검증도 남아 있다.

## 다음 허용 작업

1. GUI 없이 가능한 raw B/A와 brush/PCG/spline/foliage/외부 참조 인벤토리 및 공개 데이터 API의 명시적 매핑 설계를 먼저 확보한다. 일반 Import 교체를 강행하지 않는다.
2. 보존 경로가 입증된 별도 후보에서 실제 축소·저장/재로드·통행을 검증한 뒤 NPC의 새 DataAsset·접지 앵커·Director를 연결한다. 최신3거점+5초소와 우회/재방문·책 보상 규칙을 따른다.
3. 최종 후보와 소스/에셋이 동결되면 Windows 입력 봉인과 비GUI 검증을 이어간다. 실제 UI gate는 미충족으로 명시한다.

커밋/푸시/승격은 하지 않았다. 미완료 기능과 기존 사용자 WIP를 섞어 완료 체크포인트로 공개하지 않는다. 공유 원본·빌드 사용권은 부모가 이 Work에 배정한 경계를 유지하며, 마지막 호스트 프로세스 확인은 별도 final status 근거를 따른다.

## 근거와 전달물

- [소스 설치·백업](evidence/map-resume-20261113/PacketIntegrationA/result.json), [UHT 수정](evidence/map-resume-20261113/PacketIntegrationA/uht-array-metadata-fix/change.json)
- [최종 빌드](evidence/map-resume-20261113/PacketIntegrationA/editor-build-uht-fix-result.json)
- [43검사 이름/결과/경고 검토](evidence/map-resume-20261113/NativeTestsA/reviewed-result.json), [엔진 원본 보고서](evidence/map-resume-20261113/NativeTestsA/report/index.json)
- [재로드 비교](evidence/map-resume-20261113/TerrainSnapshotReloadA/comparison.json), [액터/기본 GameMode](evidence/map-resume-20261113/TerrainSnapshotReloadA/actors.json), [실제 상속](evidence/map-resume-20261113/candidate-mode-defaults.json)
- [public Import 보존 공백](../implementation/map-resume-20261113/landscape-rewrite-audit/public-landscape-rewrite-review.md)
- [Windows 도구 검토](evidence/map-resume-20261113/WindowsCandidateAuditA/assessment.json)
- [NPC 작성 안내](../../Docs/VillageQuestAuthoringGuide.md), [NPC 통합 안내](../../Docs/IntroVillageQuestIntegrationGuide.md), [VFX 편집 안내](../../Docs/EditableCombatVfxGuide.md)

- [최종 콘텐츠/Source 무결성](evidence/map-resume-20261113/checkpoint-c-final-integrity.json), [호스트 UE/빌드/게임0](evidence/map-resume-20261113/checkpoint-c-final-processes.json)

마지막 기록(2026-11-13 12:44 UTC): 보호 대상 원본 Content2139/후보1419 파일 SHA불변, Source·Config366개와 DLL이 native43 실행 때와 동일하다. 호스트 UE/UBT/AutomationTool/dotnet/MSBuild/게임 프로세스0이며 타인 프로세스 종료0이다. 사용권 기록은 OS잠금이 아닌 부모 배정 상태의 advisory 기록으로 유지했다.


## 후속 정정 — 엔진 시작 smoke 오류와 최종 v4 증거

위 native43 성공/37 clean+6warning은 선택한 테스트 결과다. 테스트 시작 전 엔진 smoke의 Condition failed15줄을 앞선 체크포인트에서 별도 집계하지 못했다. 해당 NativeTestsA 기록과 NOT_APPROVED 검토는 보존했다. 같은180DA DLL의 영어culture/language/locale startup probe는 전체Error/Fatal0이었으나 원래15개 CHECK 각각의 귀속을 단정하지 않는다. 새v3/v4 DLL의 영어명시 NativeTestsB/C는 각각43/43·6warning과 전체RED0을 실제 확인했고 새 Culture-D validator는 정확18인수·startupRED0을 강제한다. 원래15줄을 허용·마스킹한 것이 아니다. 최종활성DLL은이제 v4 92CB712BBC20F8299B9474FE704306089005C9760D4A1EC4E0E99BDDEEB49490이며 아래D후속기록이기준이다.
