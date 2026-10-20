# 길잡이 아린 FBX 제작·적용 계획

사용자 요청: 직전 아린 콘셉트를 바탕으로 FBX를 제작하고 기존 StartHub 안내 NPC에 적용한다. 현재 `Sprint#5-14-open-world-monsters`의 NPC 후속 작업으로 진행하며 다른 미커밋 변경을 보존한다.

## 선택한 Ultrawork Packets

| 패킷 | 목표 | 수정 위치 | 완료 조건 | 검증 | 되돌릴 수 있는 경계 |
| --- | --- | --- | --- | --- | --- |
| A1 참조 준비 | 기존 외형을 유지하는 흰 배경·그림자 없는 전신 T포즈 | 전용 참조 PNG·프롬프트 | 전신·수평 팔·물약/가방 식별, 불필요한 배경 제거 | PNG 검사·육안 검토 | 신규 이미지·문서 |
| A2 생성·리깅·FBX | 실제 이미지 기반 3D 모델·UV/재질·휴머노이드 리깅·FBX 출력 | 전용 아트 소스·API 결과 | 다운로드한 실제 FBX와 텍스처/스켈레톤 확인 | 공식 API 최종 상태·파일 구조·3D 검토 | 신규 모델 폴더 |
| A3 Unreal 연결 | 전용 SkeletalMesh와 같은 Skeleton의 Idle을 임포트/리타깃하고 NPC 데이터에 연결 | 신규 NPC 외형 패키지·DA_StartVillageGuide·기존 NPC 인스턴스 | 외형·Idle 표시, 기존 대화·지급 유지 | 에디터 저장·재로드·실제 화면·대화/지급 집중 PIE | 신규 에셋과 기존 데이터/메시 참조만 원복 |
| A4 검토·기록 | 출처·참조·수령 데이터 보존·실제 결과 기록 | Feature/doc·DevLog·SprintChangeLog | 실제 완료/미완료 범위 정확히 기록 | diff·하네스·허용 콘텐츠 검사 | 문서·검사 산출물 |

## 기술 계약과 시작 상태

- `GuideMesh`는 SkeletalMeshComponent이며 `GuideSkeletalMesh`와 `GuideIdleAnimation`은 정확히 같은 Skeleton 객체를 참조해야 한다. 기존 Quinn MM_Idle을 임의의 생성 스켈레톤에 그대로 지정하지 않는다.
- 새 리깅과 자체 Idle을 함께 준비하거나, 기존 프로젝트의 검증된 IK Rig/Retarget 경로로 MM_Idle을 새 Skeleton에 복제한다. 스켈레톤·기준 자세·root 이동·메시 단위와 방향을 실제 파일에서 확인한다.
- 외형만 바꾸고 F 대화·회복 포션 2개·세션 수령 기록·포탈 보급품 전달은 유지한다. `.uasset`/`.umap`은 에디터로만 저장하며 기존 파일을 직접 바이너리 편집하지 않는다.
- 2026-10-05 시작 시 호출 가능한 3D 생성 MCP가 없고 TRIPO_API_KEY가 Process/User/Machine 및 관련 MCP 환경 설정에 없다. 키 값 자체는 출력하거나 문서에 기록하지 않는다. 사용자에게 로컬 환경변수 설정을 요청했다.
- Tripo API가 연결되면 공식 최신 API로 잔액을 먼저 조회하고 실제 생성/리깅 비용과 참조 이미지를 확인한 뒤 유료 작업을 시작한다. 현재 잔액·유료 태스크·FBX가 존재한다고 주장하지 않는다.
- 설치된 codex-3d-asset 스킬에는 SKILL.md만 있으며 상대 경로의 데이터 파일이 없다. API 명세는 Tripo 공식 문서로 검증한다.
- C++/스키마 수정 없이 에셋 연결이 가능하면 불필요한 전체 빌드는 생략한다. 실제 새 메시가 확보되기 전 기존 NPC 외형을 교체하지 않는다.

공식 명세: https://developers.tripo3d.ai/en/docs/authentication , https://developers.tripo3d.ai/en/docs/animations-rig , https://developers.tripo3d.ai/en/docs/models-convert

## 사용자가 지정한 Apollo 경로

Tripo 연결 대신 사용자가 지정한 `Merchantlee99/apollo`를 내려받아 `apollo-asset-factory/SKILL.md`를 읽고 실행한다. 프로젝트 tmp 아래 전용 Python 환경과 loopback API/MCP, 공식 portable Blender를 준비한다. 아린 참조 업로드→source quality→compile plan→compile→candidate/state/repair→FBX/GLB export와 재임포트 검증 순서를 따른다.

소스 검토상 Apollo는 정적 에셋 제작 도구이며 기본 단일 이미지 compile은 두께를 붙인 textured cutout이다. 자체 리깅·애니메이션은 지원하지 않는다. local draft를 정상적인 360도 리깅된 NPC로 주장하지 않는다. 실제 품질/뼈/Idle 검증을 만족하지 못하면 기존 안내 NPC를 교체하지 않고 실제 초안과 한계·필요한 후속 단계를 남긴다. 공유 프로젝트 설정과 구매 에셋은 수정하지 않는다.

## Apollo 입체 프로토타입 후속 패킷

단일 이미지 경로의 실제 FBX 확보 후, imagegen으로 정사각 정면·측면·뒷면 참조를 생성한다. 생성 뷰는 스캔이 아니라 형상 추정 자료임을 기록한다. Apollo public upload는 외부 뷰를 multiview 노드로 묶지 않으므로 원본 ApolloHullAdapter를 별도 프로젝트 스크립트에서 직접 호출하고 실제 결과 GLB를 다시 공식 upload/finalize 경로로 검증한다. vendor 소스를 수정하거나 기본 compile이 외부 참조를 사용했다고 주장하지 않는다.

| 패킷 | 목표 | 범위 | 완료 조건 | 검증 | 원복 경계 |
| --- | --- | --- | --- | --- | --- |
| A2b | 실제 두께가 있는 3D 아린 초안 | 전용 뷰 PNG·Apollo hull 스크립트·GLB | 폐쇄 형상·참조 실루엣·1.7m 단위 확인 | 실제 메시 QA·정면/측면 렌더 | 신규 파일만 |
| A2c | 간단한 정지 NPC 리깅 | Blender 스크립트·신규 FBX·자체 Idle | 뼈·가중치·Idle 실재, 망토/가방 변형 수용 가능 | 재수입·중립/Idle 실제 렌더 | 신규 파일만 |
| A3b | 검증 통과 시 외형 적용 | 전용 신규 Unreal 에셋·가이드 데이터 참조 | 같은 Skeleton Idle·대화/지급 유지 | 에디터 저장·재로드·PIE | 외형 참조 원복 |

직접 hull 호출은 Apollo의 CPU 초안이며 프로덕션 생성 모델 결과로 승격하지 않는다. 정면/측면 투영 텍스처와 간단한 정지 Idle은 프로토타입으로 한정하며 걷기·공격 토폴로지는 별도 범위이다. 중립 자세나 소품이 심하게 변형되면 기존 NPC를 유지하고 실패한 실제 렌더를 함께 기록한다.

## Exit Gate 판정

A1·A2b의 참조와 입체 정적 초안·공식 MCP FBX export·실제 재수입은 완료했다. 형상11/11,24,880삼각형·높이1.7m·닫힌 단일 연결 메시를 확인했다. A2c는 실제 팔65°/55° 두 자세의 늘어남과 사선의 얼굴 중복·망토 투영 실패로 불합격이다. 같은 실패를 반복하지 않고 Reviewer가 가중치 수정만으로 얼굴/UV 문제를 해결할 수 없다고 판정했다. 상한을 완화하거나 게임용 FBX/Idle 성공으로 표시하지 않았다.

A3/A3b는 불합격 모델을 적용하지 않는 경계에 따라 실행하지 않았다. 기존 NPC·엔진 콘텐츠를 유지한다. A4 기록과 정적 초안 전달 패키지는 완료한다. 전체 원래 목표인 리깅된 NPC 적용은 미완료이며 실제 부족한 표면 UV·의상 분리/변형 토폴로지·리깅 검증이 후속 작업이다. Apollo upload의 score1.0/ready는 파일 처리 상태이므로 별도 local_draft 출처와 실패 보고서를 유지한다.
