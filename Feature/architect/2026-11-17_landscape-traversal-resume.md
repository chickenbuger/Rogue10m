# Landscape 통행·도로 표현 재개

Sprint#9-32. 사용자의 11/17 재개 지시에 따라 기존 work branch에서 진행한다. 11/16의45개 지형·1,012.5×562.5m·21지점/29양방향도로·중앙300m를 유지한다. 새 참조 scanner와 프로필 PossessedBy 수정 DLL을 사용하고 과거 실패와 QA_A 저장을 보존한다.

| 패킷 | 목표/수정 범위 | 완료/검증 | 롤백 경계 |
|---|---|---|---|
| R1 | 새 세션·기준점·공개API 관측 | 정규DLL/맵/PIE정지·strict scanner·retained 비교·dirty0 | 원본/후보/저장 백업 유지, 지형 수정0 |
| R2 | 기존 QA_A로 fresh PIE3회 | 이름CropWalkQA·기본620·시간배율1·접지·S→B각60초 이내 | 실패 시 원문 보존, 속도/위치/가드 완화0 |
| R3 | 도로 clipping 원인 분리와 후보 표현 보정 | 현재geometry/충돌/실제화면대조→한정편집·시각연속/충돌 확인 | 후보변경전 새 물리본, 원본/벤더/지형raw보존 |
| R4 | 통합 검토·결과·한국어DevLog | 저장·재로드·strict보존/통행/로그·독립리뷰 | 미검증/무관WIP 통합 금지 |

R2는 기존 메뉴 UI로 만든 테스트 프로필을 재사용한다. fresh candidate PIE의 PlayerStart로 시작하고 공개 GameState.StartRun은 측정 전 준비로만 호출한다. 보행은 Pawn.AddMovementInput이며 물리 키보드/허브 포털/15분 성장·20분 완주 검증과 구분한다. 자료 경로만11/17새폴더로 분리한다. 보호 SaveGame 필드 접근이나 임의 프로필 생성은 하지 않는다.

도로663개는 기존 프로젝트 소유 Cube 조각이다. 지형 Auto Layer에 도로 전용 paint는 없다. 곡면과 얇은 판의 clipping 및 원거리LOD를 구분한다. existing project material/geometry를 우선 사용하고 신규 팩 반입·vendor 수정은 하지 않는다. 현재 로컬 Fab 팩의 존재를 새 계정 소유 인증으로 확대하지 않는다. 높이/paint 재저작이나 지형 전체 reimport는 이번 원인 조사 대안이 아니다.

같은 실패2회면 Architect/Reviewer로 돌아간다. 새로그의 실제 AbsLog를 작업 전후 검사하고 과거오류와 신규오류를 분리한다. 무관한 기존 Source/Config/Content WIP는 보존한다.

## 이번 체크포인트의 실제 결과

R1의 새 strict cold26항목, R2의 중앙3회48.7379/48.8019/48.5884초와 외곽8간선144.2785초, R3의 두 구간 시험 후29개 데칼 표면과663개 기존 조각 숨김을 완료했다. R4는 독립 null-RHI 재로드47항목과 별도 디스크 범위9항목이 통과했다. 지형45개·raw/composite/페인트/B/A와 보호 입력3,951개·비QA 저장458개가 보존됐다. 실제 보행은 도로 표현 편집 전이며, 이후 지형·충돌 불변과 데칼 비충돌을 새 cold에서 확인했다.

실제 머티리얼은 기존 프로젝트 EarthPath의 StarterContent Gravel 텍스처와 tint를 사용한다. 후보 전용 재질1개와29개 DecalActor·정리 폴더 외부 오브젝트1개를 만들었다. 기존663개 도로의 위치·재질·NoCollision을 유지하고 표시만 껐다. 시각 시험 후 사용자가 현재 에디터 변경 패키지 전체 저장을 별도 명시 승인했다. 저장 결과는 기존663개 외부 액터 변경·새30개 패키지 추가·삭제0이다. 새 폴더1개는 저장된 AssetRegistry의 ActorFolder 클래스와 공개 태그로 검증했다. 카탈로그의 지점·연결·획득·전투 규칙과 좌표는 바꾸지 않아 두 원본 JSON을 유지했다.

추가 C++/Build.cs 변경은 없으며 기존 정규 DLL을 사용했다. Python/PowerShell 구문 검사와 실제 에디터·독립 재로드가 이번 Builder 검증이다. 과거 Undo/Redo 즉시 composite 실패와 E19/E27 정밀 point-ray 실패, 정상 허브 포털·성장/전투·20분 완주 인수는 남아 있다. 전체 맵의 인수 통과나 Git 통합 완료로 확대하지 않는다. [최종 결과와 증거](../doc/2026-11-17_landscape-traversal-resume.md)를 따른다.
