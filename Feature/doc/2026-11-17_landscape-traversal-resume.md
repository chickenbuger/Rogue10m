# Landscape 통행 검증과 도로 표면 보정 재개

2026-11-17 · Sprint#9-32 · 작업 브랜치 `Sprint#9-32-landscape-world-blockout`.

**45개 Landscape와 29개 도로 표면의 후보 저장·독립 cold 보존 검증을 마쳤다. 중앙 3회·외곽 1회 프로그램 입력 보행도 통과했다.** 재개 계획 R1~R4의 지형·도로 체크포인트를 완료했으며, 맵 전체 콘텐츠와 남은 게임플레이 QA·Git 통합의 완료를 뜻하지 않는다.

사용자가 요청한 1차 기획은 [Landscape 제작 기획서](../architect/2026-11-16_landscape-production-plan.md), 이번 재개 범위와 패킷은 [통행·도로 표현 재개 계획](../architect/2026-11-17_landscape-traversal-resume.md)을 따른다. 11/16의 외곽211개 삭제·45개 유지 승인과 실제 편집 기록은 [앞선 제작 결과](2026-11-16_landscape-production-result.md)에 보존했다.

## 유지한 지형과 새 기준점

대상은 독립 후보 `/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A`다. 9×5 배열의 Landscape·충돌 컴포넌트 각각45개, 영역1,012.5×562.5m, 기존21지점·29개 양방향 도로와 중앙300m 동선을 유지한다. 이번 재개에서 지형 높이·페인트·충돌을 다시 저작하지 않았다.

편집 전 후보맵과 정확한 World Partition 패키지1,423개를 물리 복사하고 SHA256으로 확인했다. Source·Config·StartHub·원본맵 등 보호 입력3,951개와 비QA 저장458개의 기준 해시도 별도로 기록했다. 이 기준점은 정해진 범위의 파일 보존 증거이며 전체 파일시스템 검사로 확대하지 않는다.

새 DLL을 사용하는 두 새 에디터 세션에서 [cold 보존 비교](evidence/landscape-resume-20261117/cold-reload/comparison.json)와 [작업 세션 기준 비교](evidence/landscape-resume-20261117/workbench/cold-reload/comparison.json)가 각각26/26 통과했다. composite·raw 높이/페인트·raw B/A·레이어/GUID·지형 재질·retained 컴포넌트와 비Landscape 액터가 기준과 같고 검사 후 dirty는0이다. 새 정책은 `success=true`, `loaded_reflection_walk_complete=true`와 비잘림 조건을 모두 요구한다. 이는 로드된 반사 속성과 관측 데이터의 보존 검사이며 native/custom serializer·미로드 패키지·전역 transitive 안전성을 인증하지 않는다.

## 실제 CharacterMovement 보행

기존 정상 메뉴에서 만든 테스트 프로필 `CropWalkQA`를 재사용했다. 매회 새 후보 PIE의 정상 PlayerStart에서 시작했으며, 준비 단계에서만 공개 기본 `StartRun`을 호출했다. Pawn·PlayerState 이름, 실제 능력치와 이동속도620cm/s, 3인칭·생존·비질주·비회피, 시간배율1, 기본 런1200초를 확인했다. 위치 이동·속도·런 시간 덮어쓰기와 무적은 사용하지 않았다.

| 경로 | 실제 측정 결과 | 판정 |
| --- | --- | --- |
| 중앙 S→B 1회차 | 48.7379초 | 60초 이내 통과 |
| 중앙 S→B 2회차 | 48.8019초 | 60초 이내 통과 |
| 중앙 S→B 3회차 | 48.5884초 | 60초 이내 통과 |
| 외곽 S→J100→M→A→G→F→Z→O→BE | 약885.89m, 144.2785초 | 지정8개 간선 보행 통과 |

보행은 렌더링된 PIE에서 `Pawn.AddMovementInput`으로 실제 유한 캡슐 CharacterMovement를 진행한 프로그램 입력 시험이다. 중앙3회는 모두 접지 상태였고, 외곽 시험은 접지 관측513회·공중0회·최대 중심선 이탈106.72cm였다. 외곽 간선은 E01/E08/E15/E16/E19/E20/E21/E27이다. 물리 키보드 입력, 정상 StartHub 포털, 15분 성장·20분 완주, 사람 플레이5회, 전체 전투·NavMesh·AI 검증은 수행하지 않았다.

근거는 [3회 보행 요약](evidence/landscape-resume-20261117/runtime-walk/summary.json), [라운드1 사전 조건](evidence/landscape-resume-20261117/preflight-1.json), [라운드2](evidence/landscape-resume-20261117/preflight-2.json), [라운드3](evidence/landscape-resume-20261117/preflight-3.json), [외곽 보행 원문](evidence/landscape-resume-20261117/optional-walk.json)이다. 기존 이름 초기화 실패 당시 보행0회 기록을 소급해서 고치지 않는다.

## 실제 도로 표면 보정

기존 도로는 폭7.6m·두께2cm의 Cube 메시663개다. pitch가 구간 양 끝을 따라가지만 곡면·횡경사와 일치하지 않아 지형에 묻히는 구간이 있었다. E11의 실제 근접 캡처에서도 표면이 끊겨 보였다. 도로 전용 Landscape paint나 기존 도로 DecalActor는 없었다.

같은 작업 세션의 읽기 전용 사전 검사에서 29개 도로의 폭8m를 따라 직접 지면 ray4,998회와 ray miss에 대한 반경1cm sphere fallback2회를 관측해 투영 범위를 기록했다. fallback은 투영 상자의 유한 면적 지지 근거이며 과거 strict point-ray 실패를 통과로 바꾸지 않는다. 기존 프로젝트 소유 EarthPath 재질의 실제 StarterContent Gravel 텍스처와 tint를 재사용하고 원본 재질·벤더·지형 데이터는 보존한다.

공식 Unreal MCP의 Material/Object/Scene/Actor 도구로 E04/E11 데칼2개를 만들고 해당 기존 도로34개를 숨겼다. 근접 화면에서 갈색 표면이 지형을 따라 이어지는 것을 확인한 뒤 나머지 데칼27개·기존 도로629개로 확장했다. 최종 편집 상태는 새 데칼29개이며, 기존663개는 삭제하지 않고 `bVisible=false`·`bHidden=true`로 표현만 숨겼다. 원래 Cube·NoCollision·재질·태그·위치·회전·스케일을 보존했고 Landscape 높이·페인트·충돌은 변경하지 않았다. 읽기 전용 사전 검사와 별개로, 준비한 Python `prototype()` 함수는 실행하지 않았고 실제 저작 경로는 등록된 공식 MCP 도구 호출이다.

새 프로젝트 소유 재질은 `/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface`다. 노드9개에 기존 StarterContent `T_Ground_Gravel_D`, tint0.86/0.78/0.65, 월드 UV 배율0.002, 불투명도0.9·거칠기0.9를 사용했다. 원본 재질의 색을 더 밝게 바꾸거나 normal 수정으로 단절을 해결한 작업이 아니다.

첫 재질 생성은 Texture 속성에 package 경로를 전달해 실패했다. [원래 실패](evidence/landscape-resume-20261117/road-material.json)를 보존하고, 생성된5개 노드를 관측한 뒤 정확한 전체 object 경로로 재개했다. [재개 결과](evidence/landscape-resume-20261117/road-material-resume.json)는 기존 노드를 삭제하거나 자동 재시도하지 않고 노드9개·연결·컴파일을 확인했다. 초기 검은 캡처는 비동기 shader/viewport 안정화 전 화면으로 보존했으며, opacity0 진단 뒤 원래0.9를 복원하고 안정화된 화면으로 검토했다.

시각 근거는 [E04 근접](evidence/landscape-resume-20261117/E04-prototype-camera-warmed.png), [E11 근접](evidence/landscape-resume-20261117/E11-prototype-settled.png), [전체 도로 원거리](evidence/landscape-resume-20261117/road-full-overview.png) · [최종 사선 화면](evidence/landscape-resume-20261117/final-oblique.png) · 저장 후 열린 에디터〔중간·보조 자료 정리로 삭제됨; [보존 목록](evidence/README.md)〕다. [두 구간 저작](evidence/landscape-resume-20261117/road-prototype.json)과 [27개 확장](evidence/landscape-resume-20261117/road-extend.json)도 보존했다.

첫 액터별 저장은 새 external actor package를 기존 asset으로 찾지 못해 중단됐다. 이어 후보맵·새 재질의 명시적 저장은 true를 반환했지만, [중간 파일 비교](evidence/landscape-resume-20261117/road-save-interim-files.json)에서 액터 패키지 변경0개를 확인해 전체 저장 성공으로 처리하지 않았다. [첫 실패](evidence/landscape-resume-20261117/road-save.json)는 그대로 보존했다.

전체 dirty 저장 호출은 자동 승인 검토에서 거절되어 실행하지 않았고, 이후 사용자의 명시적 승인을 받아 후보 소유 범위를 확인한 뒤 저장했다. [완료 후 파일 비교](evidence/landscape-resume-20261117/road-save-completed-files.json)에서 기존 도로 external actor663개 변경, 새 도로 external actor29개와 폴더 external object1개 추가, 삭제0개를 확인했다. 후보 패키지 수는1,423개에서1,453개로 늘었고, 별도 새 재질1개가 디스크에 존재한다. 후보 `.umap`과 기존16개 지형 external actor는 기준 바이트를 유지했다. 이후 독립 cold 검증에서692개 액터와 새 폴더의 native 패키지를 실제 파일 변경 목록에 대조했다.

사전 근거: [29개 투영 범위와 기존663개 도로](evidence/landscape-road-surface20261117/preflight.json), [보정 전 E11](evidence/landscape-resume-20261117/E11-before-near.png), [보정 전 E04](evidence/landscape-resume-20261117/E04-before-near.png), 원거리 기준 화면〔중간·보조 자료 정리로 삭제됨; [보존 목록](evidence/README.md)〕.

## 독립 cold 검증과 최종 변경 범위

별도 Editor-Cmd 프로세스에서 저장된 정확한 후보와 전체 편집 영역을 다시 로드한 뒤 읽기 전용 [최종 비교47/47](evidence/landscape-road-surface20261117/final-cold/comparison.json)을 통과했다. 기존 모든 액터 경로를 보존하고 원래 도로663개의 변경은 visibility로 한정됐다. 신규 액터는 계획한29개 DecalActor뿐이며, 추가 external object는 `RoadSurface20261117` 라벨의 `/Script/Engine.ActorFolder`임을 확인했다. 각 데칼의 투영 위치·회전·크기·재질·fade0과 저장 패키지를 대조했고, 새 재질의 노드9개·scalar3개·입출력 연결도 저장본에서 확인했다.

지형45개·충돌45개, semantic/metadata·composite·raw 높이/페인트·raw B/A·레이어·GUID·원점/스케일/회전·컴포넌트가 작업 세션 기준과 같았다. strict loaded-reflection 검사와 읽기 전용9개 조건을 유지했고 dirty0이다. 보호 입력3,951개는 변경·누락0, 비QA_A 저장458개는 해시와 파일 집합이 모두 같았다.

별도 [최종 파일 범위 대조9/9](evidence/landscape-resume-20261117/final-scope-report.json)도 통과했다. 변경663개·추가29개 데칼과 폴더1개·삭제0개 및 재질 저장을 native 패키지 허용 목록과 일치시켰다. 이 결과는 정한 CPU 데이터·액터·파일·저장 범위의 보존이며, cold 프로세스가 새로운 시각·게임플레이·전역 참조 안전성을 검증한 것은 아니다.

[headless 실제 로그 검사](evidence/landscape-road-surface20261117/cold-headless-RoadSurface20261117A/log-check.json)는 Error0이다. loader는 [complete 결과](evidence/landscape-road-surface20261117/cold-loader-RoadSurface20261117A/result.json)를 기록했고, 제한시간 전 후속 관측에서 해당 프로세스의 부재를 확인했다. 프로세스 ExitCode는 관측하지 못했으므로 exit0으로 보고하지 않는다. 이번 재개에 C++·`.Build.cs` 변경이 없어 새 컴파일은 하지 않았고 앞서 검증된 정규 DLL을 사용했다. [독립 구현 검토](evidence/landscape-road-surface20261117/implementation-review.md)도 함께 보존했다.

## 남은 실패와 통합 조건

- 11/16 Undo/Redo 직후 composite AutoLayer seam `(378,693)` 보존 실패는 그대로 남는다. 한 차례 공개 native 재합성 후 저장·cold 보존이 통과한 사실과 구분하며 이번 재개에서 Undo/Redo를 반복하지 않았다.
- 기존 E19/E27 정확한 격자선 point-ray miss2개와 인접 capsule 미실행4개는 실패 관측으로 보존한다. 이번 실제 캡슐 보행 성공으로 해당 정적 실패나 전체 삼각형12° 조건을 통과 처리하지 않는다.
- 첫 보행 세션에서 StylizedCharacter `SKEL_Hu_M`의 오래된 `/Engine/EngineMeshes/Humanoid` 의존성 LoadErrors가 신규1줄 재발했다. [로그 검사](evidence/landscape-resume-20261117/walk1-log-check.json)에 보존했으며 벤더 원본을 수정하지 않았다. 전체 로그 오류0으로 보고하지 않는다.
- 별도 작업 세션의 [저장 시점 로그 검사](evidence/landscape-resume-20261117/workbench-save-log-check.json)는 Error3줄이다. 같은 벤더 LoadErrors1줄과 과도한 읽기 전용 MCP 요청의 `socket_send_failure`2줄이며, 후자는 작은 요청으로 바꾼 뒤 정상 연결·편집·저장을 확인했다. 재질 Texture 경로 실패는 API의 `isError` 결과로 보존하며 이3개 Error줄 수에 중복 합산하지 않는다.
- [게시 의존성 감사](evidence/landscape-road-surface20261117/publication-dependency-audit.json)에서 main/origin/main `fdbe8c8c`에 Crop 기반1,423개·최종1,453개 패키지와 실제 맵의 Director/DataAsset 정의가 없음을 확인했다. 기존 Source·Config·Content의 누적 미완료 변경과 미추적 의존성이 많다. 이번 도로 자산만 분리 커밋하면 검증한 작업 트리의 필수 맵·도구가 누락될 수 있어 독립적으로 완결된 의존성 범위와 남은 통합 조건을 확인하기 전 전체 WIP를 stage하거나 `develop→test→main`으로 승격하지 않는다. 지형·도로 체크포인트의 검증 성공과 Git 통합 완료는 구분한다.

식생·마을 내부 환경·세부 조형·모든 NPC 서비스·비급 획득·일반15종/중간보스4종/최종보스1종의 실제 배치는 이번 보행 성공의 범위가 아니다. 현재 기획의 개수·획득·입력·진행 규칙은 그대로 유지한다.


2026-11-18 자료 정리: 위 실행 결과·수치·실패 설명은 당시 기록이다. 중간·보조 사진/JSON의 현재 보관 여부는 [검증 자료 보존 목록](evidence/README.md)을 따른다. 삭제 자료를 다른 성공 증거로 대체하지 않았다.
