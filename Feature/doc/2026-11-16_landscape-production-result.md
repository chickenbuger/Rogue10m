# Landscape 제작 결과 — native 외곽 정리 및 후보 저장

2026-11-16 · Sprint#9-32. **실제 지형 편집·후보 저장·독립 cold 재로드 확인 완료, 정적 도로 검사 strict 실패2지점과 정상 프로필 보행 준비 실패**인 체크포인트다. 최종 지형 인수 완료나 전체 맵 콘텐츠 완성을 뜻하지 않는다.

후속 C++ 수정에서 로드된 반사 참조 검사와 프로필 초기화의 회귀 검증이 통과했다. 아래 지형 작업 당시의 실패·보존 기록은 유지하며, 최신 코드 결과는 문서 끝의 후속 결과를 따른다. 실제 지형 보행·도로 시각 연속성은 여전히 인수하지 않았다.

사용자 요청대로 [1차 기획서](../architect/2026-11-16_landscape-production-plan.md)를 먼저 제시한 뒤, “외곽 211개 삭제 후 검증 진행” 승인을 받아 2차 Landscape 편집을 실행했다.

## 실제 변경

- 대상은 독립 후보 `/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A`다.
- 에디터의 native Landscape Manage/Delete로 기존 외곽 컴포넌트 211개를 삭제하고, 9×5 배열의 지형·충돌 컴포넌트 각각 45개를 남겼다.
- 지형 영역은 **1,012.5×562.5m**, 월드 X `-33750..67500cm` / Y `22500..78750cm`다. 기존 Landscape 원점·스케일과 중앙 300m 동선의 좌표를 유지했다.
- 최종 후보의 retained 높이·페인트·레이어와 비Landscape 액터를 비교한 뒤 지형 external actor package 16개를 저장했다. 저장 반환은 성공, 잔여 dirty package는 0개였다.

StartHub·포털 목적지·런타임 코드·원본 Compact 맵·기존 패키지는 이번 native 지형 편집의 수정 대상이 아니다. 기획의 NPC·책·몬스터 표식은 실제 서비스·획득·AI 구현과 구분한다.

## 데이터 검증과 Undo/Redo 실패

| 단계 | 관측 결과 | 판정 범위 |
| --- | --- | --- |
| 삭제 직후 | 지형·충돌45개, retained 비교26/26 통과 | native crop 후 관측 데이터 보존 |
| Undo 직후 | 컴포넌트256개 복원, composite AutoLayer seam `(378,693)` 검사 실패 | **Undo 즉시 composite 보존 실패** |
| Redo 직후 | 컴포넌트45개 복귀, 동일 composite 오류 지속 | **Redo 즉시 composite 보존 실패** |
| 공개 native 재합성 | 정확한 후보 Landscape에서 `force_layers_full_update()` 1회 호출 | derived composite 갱신이며 읽기 전용 검사가 아님 |
| 재합성 후 | retained 비교26/26 통과, 원본 retained semantic digest와 일치 | 최종 상태의 데이터 복구 확인 |
| 후보 저장 | 지형 external actor16개만 저장, dirty0 | 선택 저장 확인 |
| 독립 cold 재로드 | 저장 에디터 정상 종료 후 새 프로세스에서 유지45개·비교26/26·dirty0 확인 | 저장본 데이터·액터 보존 확인, 전역 참조·runtime과 별도 |

Undo/Redo 실패 중에도 raw layer height·paint 및 raw B/A는 원본과 같았다. 실패 snapshot을 보존했고, 재합성 후 성공을 이유로 앞선 실패를 합격으로 바꾸지 않는다. **native Undo/Redo가 즉시 모든 composite 데이터를 보존한다고 안내할 수 없다.**

재합성 후 비교에는 composite/raw height·paint, raw B/A, layer metadata/GUID, Landscape GUID·actor, retained component, 비Landscape 액터가 포함됐다. retained semantic digest는 `623961bac1faadd4138952f15694b5c54ae2226f51afba9acd5b7fd5857a9e1c`다. 26개 비교는 데이터·액터 보존 검사 묶음이며 gameplay 테스트26개를 뜻하지 않는다.

## 파일 보존과 독립 검토

이번 baseline에 포함한 **5,370개 파일 중 저장한 지형 external actor16개만 변경**됐다. 누락0개, 예상 밖 변경0개이며 보호 입력 **3,947개가 SHA256 기준으로 불변**이다. baseline 범위의 비교 결과이며 전체 파일시스템 전수 검증으로 확대하지 않는다.

독립 검토는 후보 검증·native refresh·선택 저장 스크립트와 기록된 증거를 읽기 전용으로 확인했다. 검토 범위에서 추가 코드 수정이 필요한 차단 결함은 발견하지 않았으나, 전역 dependency inspection은 `success=false`를 유지한다. unsupported native/editor property, custom serialization, 미로드/다른 맵·문자열 참조까지 안전하다고 인증하지 않는다.

## 정적 도로 검사와 ray miss 추가 진단

- 새 에디터의 정적 검사에서 29개 도로의 폭 방향 지면2,076점·50cm 이하 중심선6,532점을 질의했다. 양방향 capsule 결과13,006개 중 실제 질의13,002개·미실행4개다. 중앙선 지면 miss1개에 인접한 양방향4구간을 임의 높이로 진행하지 않고 미실행 실패로 남겼다. 양성 대조 capsule1회는 별도다.
- 지면 ray miss는 E19의5m 중앙 표본22와 E27의50cm 중심선 표본52다. 두 점 모두 정확한 Landscape X 격자선이며, 직접 관측된 지면 법선의 최대 경사는11.8805169°였다. **표본 법선 결과는 전체 삼각형12° 통과를 뜻하지 않으며**, 과거 합성경사15.095° 기록을 해결한 것으로 처리하지 않는다.
- 추가 진단에서도 정확한 중앙 좌표의 full/short ray4회는 miss했다. X를±0.001/0.01/0.1/1/10cm 이동한 ray40회와 정확한 XY의 radius1/5/42cm sphere sweep6회는 실제 Landscape hit를 관측했다. short 범위는 기존 실제 이웃 hit 높이±200cm를 사용했으며 독립적인 중앙 높이 검증이 아니다.
- 이는 ray-gridline 경계/정밀도 가설과 부합하는 관측이지만 엔진 원인을 확정하지 않는다. 추가 진단에서 지형 수정·collision rebuild·저장0, 원래 실패 report와 맵 파일 SHA 불변·dirty0을 확인했다. **원래 strict 실패는 그대로 보존**하며 실제 통행·보행 성공으로 치환하지 않는다.
- cold log의 line2509 이후 추가 진단 출력 확인은 새 오류0개였다. 기존 한국어 smoke 오류15개는 유지된다. 전체 로그 오류0이나 전체 Automation 통과로 표시하지 않는다.

## 아직 진행 중인 인수

- 기본 장비 직행3회60초 이내, 서로 다른 성장 경로의 사람 플레이 최소5회, 15분 성장·20분 완주, NavMesh·AI·전체 전투·패키지 검증은 별도다. 과거48.537초1회 프로그램 입력 기록을 이번 후보의 합격 근거로 재사용하지 않는다.
- 이번 산출물은 지형 외곽 정리 및 기존 편집 데이터 보존 단계다. 식생·마을 내부·지역별 세부 조형·모든 NPC/비급/몬스터의 완성을 포함하지 않는다.

## 증거

- [native crop 체크포인트](evidence/landscape-production-20261116/native-crop-checkpoint.json)
- [구현·저장 독립 검토](evidence/landscape-production-20261116/implementation-review.md)
- [저장 후 파일 보존 대조](evidence/landscape-production-20261116/post-save-files-verified.json)
- [독립 cold 재로드 대조](evidence/landscape-production-20261116/cold-reload/comparison.json)
- [정적 도로 검사 — strict 실패 보존](evidence/landscape-production-20261116/terrain-collision-check.json)
- [정확한 ray miss와 작은 sphere 추가 진단](evidence/landscape-production-20261116/ray-miss-diagnostic.json)
- [추가 진단 후 로그 신규 오류 확인](evidence/landscape-production-20261116/cold-output-after-diagnosis.json)
- [삭제 직후](evidence/landscape-production-20261116/after-delete/), [Undo 실패](evidence/landscape-production-20261116/after-undo/), [Redo 실패](evidence/landscape-production-20261116/after-redo/), [native 재합성 후](evidence/landscape-production-20261116/after-native-refresh/), [후보 저장](evidence/landscape-production-20261116/save-candidate/)

## 정상 프로필 보행 준비 결과와 최종 상태

테스트 전용 세 슬롯으로 PIE를 시작하고, 준비용 개발 여행으로 L_Menu에 들어갔다. 정상 로비 콜백으로 CropWalkQA를 생성한 뒤 실제 화면의 게임 접속 버튼을 눌러 StartHub 진입을 확인했다. 후보로 향하는 허브 포털이나 후속 개발 여행은 실행하지 않았다.

QA 도구는 SaveGame의 비노출/보호 속성 접근2회와 Python에 없는 Blueprint 내부 서브시스템 API1회에서 실패했다. 동일 실패를 반복하지 않고 공개 typed ObjectIterator로 현재 GameInstance 소유의 기존 subsystem을 관측하도록 재설계했다. 보호 필드를 읽거나 C++/런타임 계약을 바꾸지 않았다. 공개 관측은 프로필 수1·정상 HumanMale Pawn·실제 이동속도와 장비 능력치620cm/s였지만, 표시 이름은 기대한 CropWalkQA 대신 **로그**였다. 이름 조건을 완화하지 않아 보행 준비가 실패했고 **실제 중앙 보행0회, 완료 라운드0회**다. 이 관측만으로 이름 문제의 근본 원인을 확정하지 않는다.

캐릭터 미리보기에서 StylizedCharacter의 SKEL_Hu_M이 `/Engine/EngineMeshes/Humanoid`를 찾지 못하는 LoadErrors1건과 디버그 재질의 SkeletalMesh 사용 경고도 발견했다. 해당 엔진 Humanoid 파일의 실제 부재를 확인했으며 이번 지형 작업에서 vendor·런타임 코드를 수정하지 않았다. QA 슬롯의 생성·저장은 정상 게임 경로에 의한 것이고 운영 저장 슬롯을 지정하지 않았다.

로그의 시작 오류15줄과 QA 준비 중 신규46줄(도구 바인딩/이름 게이트4개 traceback 및 캐릭터 의존성 LoadErrors1건)을 모두 보존했다. [오류 기록](evidence/landscape-production-20261116/cold-output-runtime-gate-failures.json)과 [실제 캐릭터 관측](evidence/landscape-production-20261116/runtime-walk/runtime-state-inspection.json)을 참조한다. PIE를 공식 도구로 정상 종료한 뒤 [최종 보존 비교26/26](evidence/landscape-production-20261116/after-reload/comparison.json)·dirty0을 재확인했다. 2,736줄 이후 최종 보존 검사·화면 캡처에서 신규 오류는0이다. 첫 뷰포트 캡처는 MCP 선택 인수 기본값 요구로 거절됐으며 명시 인수로 교정한 실제 캡처를 보존했다.

최종 파일 재검사도 baseline5,370개 중 정확히 지형16개만 변경·보호3,947개불변·누락0·예상 밖 변경0이다. [최종 범위 보고서](evidence/landscape-production-20261116/final-scope-report.json)를 기록했고 에디터는 저장된 Crop 후보의 전체 지형 확인 화면으로 남겼다. 검증 실패와 미인수 항목이 있어 commit/push/develop→test→main 승격은 수행하지 않았다.

![실제 저장 후보의 에디터 뷰포트](evidence/landscape-production-20261116/landscape-crop-final.png)

캡처는 +Y가 위, +X가 왼쪽인 에디터 카메라 방향이다. 기획도의 좌표 표기와 구분한다. 현재 산출물은 native 지형 범위 정리와 기존 블록아웃 보존이며 세부 환경 미술·전체 콘텐츠 완성이 아니다.

최종 상부·사선 캡처에서 도로 표현이 여러 구간 끊겨 보이는 시각적 한계도 남았다. 높이 투영·가림·페인트 중 원인을 확정하지 않았고, 29개 도로의 시각적 연속성 합격으로 보고하지 않는다. [사선 캡처](evidence/landscape-production-20261116/landscape-crop-oblique.png)를 함께 보존한다.

마지막 정적 검사는 소유 Python13개 parse/compile, PowerShell2개 parse, 범위 diff 공백 검사 통과다. CheckGeneratedChanges도 통과했고 기존 바이너리 변경·줄바꿈 경고는 별도 유지했다. 새 C++ 수정이 없어 복구 때 성공한 정규 Editor 빌드 이후 추가 빌드는 수행하지 않았다.

## 출력 로그 진단 전달 후 보완

사용자가 전달을 승인한 별도 [출력 로그 조사](2026-11-16_output-log-error-triage.md)를 대조했다. Undo/Redo의 합성 AutoLayer 공유 경계 실패와 이미 완료된 native 재합성 후 데이터 보존 결과가 일치한다. 재합성이나 지형 검증을 반복 실행하지 않았다.

`VerifyLandscapeProduction20261116.py`의 불완전 snapshot 오류에 원래 `snapshot_errors` JSON을 함께 출력하도록 보완했다. 다음 실패부터 좌표·합성 채널·레이어 경로가 즉시 드러난다. 성공 조건은 그대로다. [보존된5단계 증거를 사용한 검사](evidence/landscape-production-20261116/snapshot-error-diagnostic-check.json)에서 Undo/Redo는 계속 실패하고 좌표 `(378,693)`·composite/weight·AutoLayer를 표시하며, 재합성·cold·최종 보존 단계의 조건은 계속 통과함을 확인했다. Unreal import·에디터 호출·자산 변경·새 빌드는0이다. ExportCandidateRetainedTiles의 추가 실행과 엔진 근본 수정은 수행하지 않았다.

## 후속 참조 검사·프로필 초기화 수정 결과 연결

별도 소유 작업의 [C++ 수정 결과](2026-11-16_reference-scanner-compatibility.md)와 [검증 요약](evidence/reference-scanner-20261116/verification-summary.json), 실제 `inspection-first/dependencies.json`을 읽어 대조했다. 새 DLL의 별도 headless 후보 검사에서는 객체90,810개·속성1,093,395개, 미지원0·잘림없음·`success=true`·`loaded_reflection_walk_complete=true`, 보존9항목과 이전 cold 비교7항목이 통과했다. 참조 검사 회귀5개는 Error/Warning0, 프로필 회귀1개는 Error0·Warning13이다. 경고13개를 전체 경고0으로 바꾸지 않는다.

후속 프로필 수정은 선택한 class로 초기 스폰한 뒤 PlayerState에 프로필 이름을 적용하지 않던 경로를 `PossessedBy`의 PlayerState 연결 직후 적용으로 보완했다. 실제 native Possess/BeginPlay·이름/GUID·교체/거부·메모리 저장 격리 회귀가 통과했지만, 이 지형 작업의 실제 메뉴→필드·중앙3회 보행을 다시 실행한 것은 아니다. 당시 보행0회 기록은 그대로다. vendor Humanoid import는 별도 자산 호환성 범위로 남는다.

새 검사 정책에 맞춰 이쪽 `VerifyLandscapeProduction20261116.py`에서 과거 미지원 오류를 허용하던 예외를 제거했다. 다음 실행은 `success is True`와 `loaded_reflection_walk_complete is True`를 모두 요구하며 잘림 검사·기존9개 보존·높이/페인트/원시B/A 비교도 그대로 유지한다. 보고서에 정책 식별자와 loaded walk 결과를 명시한다. [저장된 신규·과거 결과 및 실패 변형6개에 대한 AST 표현식 검사](evidence/landscape-production-20261116/scanner-policy-update-check.json)에서 신규 기록만 통과하고 과거 미지원·실패·완료 필드 누락·잘림은 거부함을 확인했다. 독립 리뷰에서도 판정 강화와 범위 한계를 확인했다. 이 보완에서 에디터·빌드·지형·타일 export를 실행하지 않았다.

기존 `cold-reload/comparison.json` 등의26항목 통과는 당시 retained 보존 정책의 역사적 결과이며 새 strict 정책의 재실행 결과로 소급하지 않는다. 기존 baseline5,370개·보호3,947개 불변 대조도 지형 저장 체크포인트 당시의 결과다. 이후 두 C++와 fixture가 별도 소유 작업에서 바뀌었으므로 그 과거 해시를 현재 Source 전체 불변으로 해석하지 않는다. 신규 loaded walk 통과도 native ARO/custom Serialize·미로드 package·문자열 경로·전체 전이 참조나 삭제 안전성을 인증하지 않는다. `project_wide_dependency_complete=false`·`crop_reference_safety_verified=false`는 유지된다. 새 세션에서 실제 새 DLL을 사용할 때 위 정책을 적용하며 현재 작업 전체의 커밋·승격은 미완료다.
