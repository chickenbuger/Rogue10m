# Q·Z 동굴의 연속 암반 보완

2026-11-18 · 설계·완료대조 · 기준 체크포인트 `3a33ee21` · 전체 맵 제작 중

사용자 요구는 사각 방의 벽에 바위를 붙인 인상을 없애고, 발헤임을 참고한 자연스러운 절벽과 천장이 이어지는 동굴로 보완하는 것이다. Q/Z의 주 경로·갈림길·비밀방·귀환로와 중앙 직행로·세 마을 평지는 유지한다. 이 문서는 제작 계획이며 새 에셋·시각·통행 검증의 완료 보고가 아니다.

## 기준과 실제 관찰

[월드 제작 기준](../../Docs/WorldMapProductionGuide.md), [참조 README와 지도](../../Docs/References/WorldMap/README.md), 원본 배치·획득 JSON, [현재 동굴 카탈로그](../../Docs/References/WorldMap/caves20261118.json), [이전 구조 기획](2026-11-18_underground-caves.md), [현재 결과](../doc/2026-11-18_underground-caves.md), [Fab 재사용 가이드](../../Docs/FabAssetReuseGuide.md)를 읽었다. 지도 PNG는 배치 콘셉트이며 충돌·거리 측정의 근거로 사용하지 않는다.

이전 실제 PIE의 [Q 직각 통로](../doc/evidence/caves20261118/runtime-camera-verified/captures/02_Q_narrow_right_angle_turn.png)와 [Z 직각 통로](../doc/evidence/caves20261118/runtime-camera-verified/captures/05_Z_narrow_right_angle_turn.png)를 직접 확인했다. 긴 수직 평면, 직각 모서리, 수평으로 닫힌 천장과 따로 놓인 타원형 바위가 반복된다. 가까운 돌은 연한 회백색이고 Q는 넓은 황색 조명, Z는 청백색 조명이 퍼져 암반의 깊이를 약하게 만든다. 이전 276지점·12왕복 동선의 통행 합격은 이 시각 문제를 해결했다는 근거가 아니다.

현재 기준의 actor는1,408개이며 동굴 구조2개와 장식2개가 포함된다. 석재 바닥 보행면Z8610cm, 지붕 잔디 덮개 상단Z9602cm, 원래 천장 하단Z9200cm를 기준으로 삼는다. Landscape composite uint16LE SHA256은 `a2c8846963f7d08f7df561b6c800f2f389255e6f52c14a1453a6e5e3f7674726`, BLAKE3는 `67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc`다. 적용 전 실제 세션에서 다시 읽어 일치해야 하며 과거 보고서로 live 관측을 대신하지 않는다.

## 형상과 재질 선택

Q/Z 각각의 기존 연결 격자를 입력으로 연속된 SDF 암반 셸을 만든다. 넓은 암반 덩어리의 굴곡이 벽에서 천장으로 이어지고, 통로 폭과 천장 높이가 완만하게 달라지게 한다. 큰 볼륨·중간 크기 절리·작은 표면 질감의 역할을 구분한다. 규칙적인 구형 돌의 반복, 긴 평판, 동일 간격의 수직 홈, 매끈한 고무관 같은 단면은 시각 거절 조건이다. 벽 바위와 천장 돌의 반복 외장은 새 셸로 대체하되 기존 구조·바닥·지붕·목재·상자·유물의 역할과 경로 경계는 보존한다. 실제 숨김·교체·충돌 변경 범위는 적용 계획과 readback에 정확히 기록한다.

새 형상은 현재의 곧은 경로를 지키면서 비통행 암반 쪽으로 코너를 둥글리고 방 가장자리를 불규칙하게 넓히는 방식부터 사용한다. 비밀방의 인접 벽을 깎아 새 직통 구멍을 만들지 않는다. 셸은 천장 아래와 출입구에서 틈·뒷면·지상광 누출이 없어야 한다. 바닥의 실제 보행 높이와 중앙·마을의 Landscape를 바꾸지 않는다. 벽·천장의 실제 Camera/Pawn 응답과 새 메시의 두께·법선·충돌·항상 로드를 확인한다.

첫 V1 소스 미리보기는 벽·천장의 연결은 개선됐지만, 방의 넓은 벽이 둥근 사각 평면으로 남아 미술 후보로 거절했다. 후속 V2는 큰 벽 덩어리의 볼록·오목 변화와 접힌 천장이 실제 삼각형 미리보기에서 읽혀 **Unreal 적용 미리보기용 소스 후보**로 선택한다. 재질·조명·실제 카메라·충돌·cold가 없는 CPU 화면이므로 최종 미술이나 QA 합격은 아니다. [직접 검토한 소스 게이트](../doc/evidence/cave-organic20261118/source-gate.json)에 관찰과 남은 위험을 기록한다.

V2의 연속 표면은 벽·천장과 함께 Z8609cm의 평평한 시각 바닥을 포함한다. 기존 바닥 ISM은 화면에서만 숨기고 Z8610cm의 충돌·transform·인스턴스를 그대로 유지한다. 새 시각 바닥은 충돌 보행면보다1cm 아래이며 재질 슬롯1에 기존 `MI_CaveStoneFloor`를 재사용한다. 벽·천장은 슬롯0에 새 `MI_OrganicRock`을 쓴다. 기존 경사로의 충돌·높이·경사·통행은 그대로 두고, 같은 경사로 transform의 NoCollision 시각 컴포넌트2개만 추가한다. 따라서 새 셸2개와 경사로 시각2개, 총4개의 새 StaticMeshComponent를 기존 동굴 actor에 붙이며 actor 총수1,408개와 지붕 잔디 덮개 상단9602cm를 유지한다. 바닥을 숨기는 것은 충돌을 제거한다는 뜻이 아니며 실제2016질의와 경사로 왕복으로 구분해 확인한다.

| 기존 자원 | 확인한 근거·선택 |
|---|---|
| `M_CaveSurface` | 현재 소유43노드 그래프의 월드XY/XZ/YZ 240cm 석재 투영을 새 프로젝트 소유 parent로 복제해 재사용한다. 기존 parent와7개 동굴 재질은 변경하지 않는다. |
| `T_GroundRock_01_basecolor` | `/Game/Stylized_Village/Assets/Terrain/Rock/`의 현재 석재 텍스처다. 새 셸의 중립적인 회갈색 바탕에 사용한다. |
| `MI_CaveStoneFloor` | 기존 석재·Normal·Roughness0.92를 변경하지 않는다. V2 시각 바닥 슬롯1과 경사로 시각에 이 재질을 그대로 재사용한다. 기존8610cm 바닥 충돌은 유지한다. |
| `SM_Boulder_Medium_01`·`MI_CaveRock` | 현재 씬의 실제 사용은 확인된 기존 자원이다. 기존 BoulderUV용 재질을 새 셸에 배정하면 UV atlas 실패를 반복하므로 선택하지 않는다. |
| 기존 목재·등불·소품 | 현재 프로젝트 소유 파생 재질과 배치를 재사용한다. vendor 원본·프레임워크를 교체하지 않는다. |

새 셸 재질은 어두운 중립 회갈색의 무광 석재를 목표로 한다. 첫 후보는 선형 `OrganicTint=(0.30,0.32,0.28,1)`, Roughness0.92로 잡고 실제 화면으로 결정한다. 이는 발헤임의 재질 수치를 옮긴 값이 아니라 이 프로젝트 조명 아래의 시작 후보다. 기존43노드 그래프의 `ColorTint`는 윗면 분기이고, 옆면·천장 아래의 triplanar 분기는 고정 `Constant3Vector_0=(0.75,0.72,0.65)`를 곱한다. 따라서 MI의 `ColorTint`만 어둡게 해서는 옆면이 바뀌지 않을 수 있다. 새 소유 `M_OrganicRock`에는 기존 triplanar 합계 `MaterialExpressionAdd_4`→새 `OrganicTint` 곱→BaseColor를 직접 연결해 모든 면을 월드 투영한다. 기존43노드는 유지하고 tint·곱·중립Normal3노드만 추가해46노드로 만든다. 새 `MI_OrganicRock`과 함께2자산을 만들며 기존 parent·재질7개는 변경하지 않는다.

기존 XY투영 Normal은 새 곡면에서 늘어진 결이나 방향성 얼룩을 만들 수 있다. 새 parent의Normal에는 접선 공간 중립 상수(0,0,1)를 연결해 형상 법선이 드러나게 한다. 벽·천장 전체에 같은 큰 평면 법선을 주거나 바위UV atlas를 억지로 투영하지 않는다. 신규 Fab 구매·다운로드·팩 반입은 없다. `Stylized Village`의2026-10-19 보유 카드 기록, 로컬 존재, 현재 씬 사용은 구분하며 이번 조사에서 현재 계정 소유·약관·배포 권리를 다시 인증했다고 표현하지 않는다.

## 조명과 발헤임 참조의 범위

현재 Q6개 조명은RGB(255,193,116), Z6개는(155,204,255), 각각12,000lm·반경2,800cm이며 실내EV3이다. 첫 형상·재질 후보에서는 이를 유지한다. 회백색 포화나 전체 황색·청색 덮임이 남으면 기존12개 국소 조명의 강도·반경·채도만 별도 한정 조정하고 readback을 남긴다. 빛은 갈림길과 지지대 주변의 국소 강조로 읽히게 하며, 지상 전역EV11.5·국소 노출 경계·카메라 설정은 유지한다.

[Valheim 공식 FAQ](https://www.valheimgame.com/faq/)는 낮은 해상도 텍스처와 적은 폴리곤 디테일에 현대 재질·조명·후처리를 결합한 스타일을 설명한다. [공식 Press](https://www.valheimgame.com/press/)의 [Frost Caves 통로](https://img2.storyblok.com/fit-in/600x400/f/157036/1612x907/fb77538d7c/frostcave1.png)와 [큰 암반 공간](https://img2.storyblok.com/fit-in/600x400/f/157036/2560x1440/c9c74fadd3/frostcave6.png)을 웹과 배경 브라우저에서 확인했다. 앞 화면은 불규칙한 천장 아치와 구조물·국소 온색광, 뒤 화면은 큰 연속 암반·불규칙한 턱·깊은 음영이 관찰된다. 이를 회갈색 무광 셸과 Q의 차분한 온색·Z의 차가운 국소광으로 해석하는 것은 우리 설계 판단이다. 원본 게임 자산·지도·프로시저럴 월드·진행 잠금 규칙을 복제하지 않는다.

## Ultrawork Packets

| 패킷 | 목표·수정 위치 | 완료 조건·검증 명령 | 롤백 경계 |
|---|---|---|---|
| O1 기준 고정 | 실제 세션·현재 동굴4actor·재질·지도 보존 입력 조사. 새 `cave-organic20261118` 증거만 작성 | active `-AbsLog`와 줄/UTC 경계를 기록하고 `Scripts/CheckUnrealOutputLog.py --log <actual-log> --after-line <line>` 실행. 기존·새 오류를 분리하고 현재 clean 상태·파일 SHA·Landscape 전체 값을 고정 | `3a33ee21`과 그 기준의 한정 물리 백업. 기존 검증·원본 카탈로그는 수정하지 않음 |
| O2 연속 암반 | 신규 순수 셸 생성기·계획·Q/Z 메시, 기존 동굴의 허용된 표면 교체 | 순수 생성기에서 경로·비밀방 그래프, 폐곡면/경계, 법선·퇴화면·두께·자기교차·비용을 검사. 기존2016개 질의의 계약을 유지한 신규 검증 입력으로 실제 충돌 검사 | 새 셸·새 컴포넌트/프로젝트 소유 자산과 명시된 기존 표면 상태만. 기존 바닥 충돌·transform, 지붕 잔디·지형은 유지 |
| O3 실제 화면 | 새 석재 parent/MI, 필요 시 한정 국소 조명 | 기본 PlayerCameraManager 시점으로 이전6지점과 같은 입구·좁은 코너·비밀방6장을 촬영·직접 검토. 아래 미술 기준 전항목을 통과. 실패하면 같은 후보 반복 대신 Architect/Reviewer로 복귀 | 새 재질과 선택된12개 국소 조명 속성. 기존7개 동굴 재질·원본40개 재질은 유지 |
| O4 카메라·cold·범위 | 실제 왕복 보행, 한정 저장, 독립 재로드, 결과/DevLog/Sprint 기록 | 기존276경유점·양방향12체인, Camera14cm·거리140cm 이상/수축180cm 이하 기준,2016캡슐 질의와 신규 셸 검사를 재실행. clean 저장 후 독립 cold·파일 범위와 `Scripts/CheckGeneratedChanges.ps1` 통과 | 허용 패키지만 한정 저장. 실패 원문·대표 실패 화면은 새 증거에 보존하고 과거 결과를 바꾸지 않음 |

O2의 V2 순수 생성 명령은 `& .\tmp\apollo-venv\Scripts\python.exe Scripts/OrganicCaveMeshSculpt20261118.py`다. `Art/World/CavesOrganic20261118/V2/SM_CaveOrganicSculpt_{Q,Z}_20261118.obj`·`.mesh.json`과 [V2 형상 규격](../doc/evidence/cave-organic20261118/geometryspec-v2.json)을 만든다. 이전 `OrganicCaveMesh20261118.py`·`geometryspec.json`은 거절된 V1 소스와 수학 helper 근거로 보존하며 V2 성공으로 덮지 않는다. O3 재질 실행기는 `Scripts/Editor/OrganicCaves20261118Materials.py`의 명시적 `create_organic_cave_materials(...)`이며, [고정 재질 규격](../doc/evidence/cave-organic20261118/materialspec.json)의 SHA·실제PID·clean 상태·기준47재질 파일 SHA를 요구하고 저장하지 않는다. O4는 `Scripts/Editor/OrganicCaves20261118QA.py::validate_organic_caves(config, require_saved_clean=True)`와 `Scripts/Editor/OrganicCaves20261118Camera.py::arm_organic_cave_traversal_camera(...)`를 사용한다. 실제 메시 경로·재질 경로·geometry 규격 SHA는 주 작업자가 읽은 적용 manifest에서 넘긴다. 이 함수명의 존재나 순수 사전 검사 성공은 에디터 실행 완료가 아니다.

실제 Unreal 메시 저작은 OBJ의 FbxFactory 반입 대신 공개 GeometryScript API를 사용한다. V2 `.mesh.json`의 고정한 로컬XYZ cm·삼각형·법선·슬롯 정보를 `GeometryScriptSimpleMeshBuffers`로 전달해 DynamicMesh를 만들고, `create_new_static_mesh_asset_from_mesh`로 프로젝트 소유 StaticMesh를 생성한다. OBJ는 원본 교환·출처·독립 검증용으로 보존하며 실제 에셋을 import했다고 기록하지 않는다. 이 경로는 OBJ 반입 시 축 변환 추측을 피하면서 native vertex/triangle readback과 원본 대조를 요구한다. 생성 API의 문서에 저장 동작이 언급되므로 저장 여부·정확한 패키지·충돌 옵션은 실제 실행 receipt를 기준으로 기록하고, 재질 실행기의 `assets_saved=false`를 메시 생성에도 확대하지 않는다.

첫 native 통합QA에서 원본의 방향·좌표가 정확히 저장된 상태라도 벽·천장132질의가 실패했다. Engine Cube의 실제 앞면 관례와 소스의 수학적 면 방향을 대조한 뒤, Unreal 전달 adapter에서만 삼각형`[a,b,c]→[a,c,b]`를 적용한다. 원본 V2 좌표·삼각형 파일·저작 법선은 보존하며 실제 native 비교의 기대값에도 이 명시적 adapter를 적용한다. 두 입력 모두float32로 변환한 방향·재질별 Counter로 비교해, 이전double/float32 정렬의 잘못된 위치 대응을 반복하지 않는다. 바뀐면·누락면·재질ID오류는 계속 거절한다. NoCollision 전환이 기존 구조 벽2bin의 프로필 이름을Custom으로 바꾸는 실제 부작용만 허용 overlay에 반영하고 나머지 필드는 엄격히 보존한다. 이전QA실패는 [진행 결과서](../doc/2026-11-18_organic-cave-refinement.md)에 남기며 수정 후 새 전체검사·실제화면·cold를 요구한다.

C++·Build.cs를 바꾸지 않는 콘텐츠 작업이면 `BuildEditor.ps1` 실행을 완료 증거로 추가하지 않는다. 의미 있는 Editor/저장/PIE/cold 전후에는 동일한 실제 로그의 새 구간을 검사한다. 에디터·Git·공유 DevLog·Sprint 변경 기록은 주 작업자가 맡고 이 작성자는 새 설계·참조 기록과 명시 실행형 재질 helper만 작성한다.

## 실제 화면 인수 기준

| 항목 | 통과 기준 | 거절 사례 |
|---|---|---|
| 암반의 연속성 | 벽에서 천장으로 이어지는 불규칙한 큰 볼륨이 기본3인칭에서 읽힘 | 직각 벽·천장 판이 지배하거나 별도 타원형 돌이 벽에 반복 부착됨 |
| 형태 변화 | 코너·입구·방의 단면이 달라지고 크고 작은 면이 섞임 | 일정 간격 골, 매끈한 원통, 같은 크기 삼각 패턴, 긴 균일 평면 |
| 표면 | 회색·갈색 석재의 중간톤과 미세 질감, 곡면에서 이어지는 월드 투영 | 하얗게 날아간 큰 면, 플라스틱 광택, 늘어진 XY Normal, atlas 색 조각 |
| 빛과 가독성 | 길과 캐릭터를 읽을 수 있고 Q온색/Z냉색이 국소적으로 구분됨 | 전면 균일 발광, 짙은 색 필터, 어둠으로 틈·평판·충돌 문제를 가림 |
| 보호 계약 | 바닥8610·지붕9602·Landscape 전체 composite와 중앙/마을 평지·경로·획득 규칙 보존 | 지형 재굴착, 비밀방 직통 개방, 새로운 경로 잠금, 바닥 턱·낙하 |
| 카메라 | 기존 SpringArm320cm·Camera probe14cm·FOV90 유지, 양방향 실제 경로와 대표 화면 재검증 | 카메라 충돌 해제·오프셋/속도/시간 변경으로 시각·통행 실패를 우회 |

최종 결과에는 최종6장 중 대표 Q/Z 화면과 고유한 실패만 선별한다. 중간 RPC·캡처 묶음을 DevLog로 복제하지 않는다. 근거와 설계 추론은 [새 참조 기록](../doc/evidence/cave-organic20261118/art-direction-reference.json)에 한 번만 정리한다. 새 결과는 작업 브랜치에 기능 단위로 커밋·푸시하고, 전체 맵 제작 완료 전 develop/test/main 승격은 보류한다.

저장 후 최종형상·소스·에셋·엄격보존·cold·실제왕복 결과는 [후속 보충 카탈로그](../../Docs/References/WorldMap/cave-organic20261118.json)를 따른다. 요청한보완범위의 실제276/276경유점·12체인·6PCM화면,19정적조건·2,016질의·8패키지저장·cold20조건·파일범위24조건을통과했고 PIE종료후dirty0을확인했다. 실제보행 최소카메라거리는231.904cm·최대수축99.344cm였다. 기존원본배치·획득카탈로그·V1거절기록은보존하고, 소스미리보기는 [보존매핑](../doc/evidence/cave-organic20261118/retained-source-previews.json)을통해읽는다. 전체맵제작완료·전투·Shipping인수로확대하지않는다.
