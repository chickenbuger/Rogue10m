# 길잡이 아린 FBX 제작·검증 결과

최신 Apollo 결과: **정적 입체 초안 FBX·GLB 출력과 실제 재수입을 완료했다. 리깅과 NPC 적용은 외형·변형 검사에서 실패했다.** StartHub 기존 Quinn 안내 NPC와 대화·물약 지급은 유지했다. 아래 초기 준비 기록 뒤에 Apollo의 실제 실행·검사·전달 결과를 추가했다.

## 완료한 준비

기존 아린 콘셉트의 얼굴·갈색 땋은 머리·초록 여행복·가죽 가방·회복 물약 두 병을 유지해 모델링 참조를 제작했다. 정면 전신 T포즈, 흰 배경, 그림자 없는 구도이며 두 손은 리깅을 위해 비우고 물약을 허리와 가방에 고정했다. imagegen 내장 도구로 생성하고 전용 프로젝트 폴더에 사본과 실제 프롬프트를 보존했다.

- 참조: `Feature/doc/evidence/arin-npc-fbx-20261005/arin-tpose-reference-v1.png`
- 프롬프트: `Feature/doc/evidence/arin-npc-fbx-20261005/reference-prompt.txt`
- 스타일: 기존 성인 여성 아린과 같은 양식의 판타지 게임 캐릭터.
- 요청 형식: 리깅·재질을 가진 실제 FBX, 기존 NPC에 외형과 Idle 적용.

![아린 모델링 T포즈 참조](D:/Project/Rogue10m/Feature/doc/evidence/arin-npc-fbx-20261005/arin-tpose-reference-v1.png)

## 확인한 적용 경로

`GuideMesh`는 SkeletalMeshComponent이다. NPC 구현은 메시와 Idle의 Skeleton 객체가 정확히 같아야 동작을 적용한다. 생성 FBX의 스켈레톤을 독립 임포트하고 같은 Skeleton의 자체 Idle을 함께 임포트하거나, 기존 MM_Idle을 프로젝트의 검증된 IK Rig/Batch Retarget 경로로 복제한다. 두 데이터 참조를 함께 교체하며 기존 대화·2개 지급·수령 기록·포탈 전달을 유지한다.

재사용할 실제 코드: `Scripts/Editor/PreviewMarteloRetarget.py`, `Scripts/Editor/CreateInheritedCharacterAssets.py`, `Scripts/Editor/ApplyStartHubGuide.py`. 새 임포트 범위는 `/Game/Rogue10m/World/StartVillage/Tutorial/Arin/`로 한정하고 기존 Quinn/Manny Skeleton과 MM_Idle을 수정하지 않는다. FBX SDK와 Visual Studio는 설치되어 있어 변환 결과 검증 보조가 가능하지만 SDK는 모델 생성 도구가 아니다.

## Apollo 요청 전: Tripo 연결 확인

Process/User/Machine의 `TRIPO_API_KEY`와 관련 MCP 환경 설정을 값 노출 없이 확인했으며 설정된 키가 없다. 호출 가능한 이미지→3D 생성 MCP도 없다. 사용자에게 Tripo API 키의 로컬 환경변수 설정을 요청했다. 키가 연결되기 전 지갑 잔액 조회·모델 생성·리깅·FBX 변환 요청을 보내지 않았다. 실제 FBX 파일과 새 SkeletalMesh는 아직 없다.

Tripo 공식 인증·리깅·FBX 명세를 확인했다. 키가 연결되면 잔액과 실제 유료 단계 비용을 확인하고 승인된 참조로 생성·리깅·FBX 출력을 진행한 뒤 실제 파일을 검토하고 에디터에서 임포트/저장한다. 현재 잔액·비용·완료 시간은 확인되지 않았다.

- 공식 인증: https://developers.tripo3d.ai/en/docs/authentication
- 키 발급 콘솔: https://platform.tripo3d.ai
- 휴머노이드 리깅: https://developers.tripo3d.ai/en/docs/animations-rig
- FBX 변환: https://developers.tripo3d.ai/en/docs/models-convert

이 초기 단계의 완료 범위는 모델링 참조와 적용 경로 준비였다. 이후 사용자가 Apollo 경로를 지정해 아래의 실제 정적 FBX 생성까지 진행했다.

## 사용자가 지정한 Apollo 실행 결과

[Merchantlee99/apollo](https://github.com/Merchantlee99/apollo)의 실제 `apollo-asset-factory` 스킬을 읽고 실행했다. 브랜치는 `Sprint#5-14-open-world-monsters`이며 다른 미커밋 작업은 보존했다.

- 실제 입체 FBX: [model.fbx](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/hull-export-pack/assets/model.fbx)
- 입체 GLB: [model.glb](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/hull-export-pack/assets/model.glb)
- 모델·원본 manifest·검사·출처·한국어 설명: [arin-apollo-static-draft.zip](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/arin-apollo-static-draft.zip)
- 최종 상태: [delivery-status.json](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/delivery-status.json)

입체 FBX는659,468bytes,12,442정점·24,880삼각형, 높이1.7m이다. 닫힌 표면·양의 체적·단일 연결 등 형상11항목을 통과했다. 실제 Blender 재수입에서 같은 삼각형 수와1.7m 높이를 확인했다. **최종 FBX에는 뼈·애니메이션·완성 텍스처가 없다.**

![실제 FBX 사선 형상 검사](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/hull-shape-inspection/fbx-oblique.png)

이 이미지는 실제 FBX 형상에 회색 검사 표시를 적용한 Workbench 렌더이다. 모델에 텍스처를 추가한 결과가 아니며 원본 파일을 수정하지 않았다.

## 세팅과 실제 생성 경로

Apollo commit `8cadd481d0155589b39456a6266d88072d7111cf`를 `tmp/apollo-source`에 보존하고 전용 Python3.12 환경과 loopback127.0.0.1:8734 API/MCP를 준비했다. Apollo가 쓰는 FastMCP를 위해 MCP1.30.0을 사용했다. 공식 portable Blender4.5.14의 SHA256 `b9533d2397ac1984db4466fb23a7a4649391cca93f6e84209f9bcc60d071c8b9`를 확인했다. 전역 Python/MCP 설정은 변경하지 않았다.

provider/config POST는 Settings의 비재진입 Lock 내부 재호출로 교착했다. 작업 소유 API만 재시작하고 BLENDER_PATH를 프로세스 환경으로 전달해 우회했다. vendor 소스는 변경하지 않았다. 외부 API 키나 유료 생성 태스크는 사용하지 않았다.

1. 공식 stdio MCP health→create→upload→source quality→compile plan→compile→candidate→promote→state→Unreal export를 실제 실행했다. 단일 이미지 계획은 `textured_cutout`이었으며 정적 FBX·GLB·OBJ를 출력했다. 이 경로는 두께0.12m의2.5D 초안이고 리깅 모델이 아니다.
2. imagegen 내장 도구로 같은 아린의 정사각 정면·정측면·뒷면 T포즈를 생성했다. 세 PNG·실제 프롬프트·SHA256을 `multiview/`에 보존했다. 생성 뷰는 스캔 자료가 아니다.
3. public MCP에는 외부 뷰를 multiview로 묶는 입력이 없어, 프로젝트 스크립트에서 변경하지 않은 실제 `ApolloHullAdapter.generate`를 직접 호출했다. 정면·측면 실루엣으로1.7m 입체 GLB를 생성했다. 뒷면은 검수·텍스처 자료이며 형상 생성에는 사용하지 않았다.
4. 실제 GLB를 새 Apollo asset `ast_1m45dqnzp00aartxyz6`에 MCP 업로드했다. `goal=preview` 계획·compile이 `already_satisfied`임을 확인하고 원본 후보를 승격한 뒤 FBX·GLB를 공식 export·download로 저장했다. `auto/character` finalize의 procedural texture가 외형·스킨을 덮을 수 있어 preview 경로로 원본을 보존했다. 출력 GLB와 원본 SHA256이 일치한다.
5. 두 경로 모두 실제 FBX를 Blender로 재수입했다. 최종 native export manifest 체크섬·ZIP CRC를 확인하고 직접 hull 출처·실패 보고서·한국어 설명을 별도로 동봉했다.

Hunyuan3D·TRELLIS·SF3D·TripoSG 프로덕션 생성 서버는 연결되지 않았다. 현재 출력은 CPU `local_draft`이며 고품질 생성 모델 결과로 승격하지 않는다.

## 리깅·외형 실패와 적용 판단

실제 hull에20본 의미 영역 가중치를 지정하고 세 참조의 색상을 UV 투영했다. 팔65°·55° 내리기 모두 실패했다.

| 자세 | 변 길이 늘어남99백분위 | 최대 늘어남 | 결과 |
| --- | --- | --- | --- |
| 팔65° 내림 | 2.13배 | 7.38배 | 실패 |
| 팔55° 내림 | 1.85배 | 6.34배 | 실패 |
| 검사 상한 | 1.75배 | 4.00배 | 기준 유지 |

사선 화면에서 얼굴 중복·흰 테두리·망토와 몸통 투영 늘어짐을 확인했다. 얼굴 중복은 뼈 변형 전부터 발생하는3면 투영 문제이므로 가중치 수정만으로 해결되지 않는다. 두 자세 실패 후 Reviewer로 돌아가 추가 반복·기준 완화를 하지 않았다. 게임용 리깅 FBX와 Idle은 출력하지 않았다.

![실패한 실제 리깅 검토 화면](D:/Project/Rogue10m/Feature/doc/evidence/arin-apollo-20261005/rig/idle-oblique.png)

파일명은 검사 렌더 이름이며 완성된 Idle 애니메이션을 뜻하지 않는다. 보고서 상태는 `failed`이다. 기존 GuideMesh의 같은 Skeleton 메시·Idle 계약을 만족하지 못해 Unreal 임포트·데이터 교체·맵 저장을 실행하지 않았다.

Apollo source score1.0은 이미 메시인 입력의 준비 상태이고 export 후 `ready`는 파일 처리 상태이다. 업로드 노드는 `provider_id=upload`여서 직접 hull의 draft 출처가 자동 추적되지 않는다. `hull-report.json`과 `delivery-status.json`의 `local_draft`, `npc_applied=false`를 최종 판단으로 유지한다.

실제 스킬 규칙: “Never call local draft output Meshy-grade.” ([읽은 SKILL.md](D:/Project/Rogue10m/tmp/apollo-source/plugins/apollo/skills/apollo-asset-factory/SKILL.md)). 완성 NPC에는 일관된 표면 UV/텍스처, 망토·몸통·소품을 구분한 변형 토폴로지와 실제 리깅·Idle 검증이 필요하다.

## 검증과 재현

형상11/11, 실제 FBX 재수입, 최종1.7m·24,880삼각형·뼈0·Action0, 원본 GLB·manifest 체크섬·ZIP CRC를 확인했다. Python6스크립트 AST·하네스 경로와 독립 provider/FBX 적용 경로 검토를 수행했다. C++·엔진 콘텐츠를 변경하지 않아 빌드와 PIE는 재실행하지 않았다. 커밋·푸시는 하지 않았다.

실행 스크립트: `Scripts/ApolloMCP.py`, `Scripts/PrepareApolloBlender.py`, `Scripts/BuildApolloArinHull.py`, `Scripts/RigApolloArin.py`, `Scripts/InspectApolloFBX.py`, `Scripts/PackageApolloArin.py`. 마지막 스크립트는 실제 내보내기와 검사 증거로 전달 패키지를 재생성한다. 리깅 실패 게이트는 유지한다.

관련 자료: `Feature/architect/2026-10-05_arin-npc-fbx.md`, `Feature/doc/evidence/arin-apollo-20261005/`, `DevLog/20261005.txt`, `Docs/SprintChangeLog.md`.

다운로드와 검사 완료 후 확인된 작업 소유 Apollo 서버2프로세스만 종료했다. Unreal Editor는 유지했다. 전용 소스·환경·portable Blender와 실제 데이터는 프로젝트 tmp에 남아 있으며 사용자 전역 스킬 설치나 서버 자동 시작 설정은 하지 않았다.
