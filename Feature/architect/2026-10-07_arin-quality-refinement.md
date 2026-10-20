# 아린 Blender 품질 개선 계획

`Sprint#5-17-arin-quality-refinement`에서 시작한다. 기존 미커밋 작업과 v1을 보존하며, UE5.8 시작마을 안내 NPC의 실제 모델 품질을 개선한다. 설치한 `blender-character-workflow`, `blender-asset-validation`, `unreal-export`, `retopology`, `blender-iterative-refinement` 지침과 imagegen 참고 보드를 사용한다.

## 선택한 Ultrawork Packets

| 패킷 | 목표 / 수정 범위 | 완료 조건 / 검증 | 원복 경계 |
| --- | --- | --- | --- |
| Q1 | 기존 BLEND/FBX/소스/렌더 동결·독립 critic·새 참고 | baseline 해시와 view별 pass/fail/unclear, 실제 참고 이미지 열기 | Versions/v1·baseline |
| Q2 | 새 head/body 소스에서 얼굴·머리·손·의상 형태 개선 | 기존170cm/23본에 맞는 입체 형태, 동일 카메라 정면·측면·뒤·클로즈업 비교 | Refined 모듈·새 씬 |
| Q3 | 실제 UV·PBR atlas와 표면 질감 | Face/Hair/Cloth/Gear 4개 섹션, UV atlas, 컬러/roughness/metallic/tangent normal PNG와 엔진 참조 | 신규 텍스처·재질만 |
| Q4 | 2초 Idle·변형·export·LOD | 새 프로세스 BLEND/FBX/GLB 검사, 중립/팔굽힘/도달/몸통비틀기/무릎 화면, 실제 지원되는 LOD API 조사·검증 | 새 출력 폴더 |
| Q5 | UE 전용 폴더 임포트·NPC 참조 교체 | 동일 Skeleton mesh/Idle, 저장·재로드·실제 대화/물약/포탈 PIE, 보호된 설정 보존 | v1 외형 참조 복원 |
| Q6 | 독립 리뷰·파일 전달·기록 | 실제 파일/뷰/해시·검증·한계 기록, 한국어 DevLog·SprintChangeLog | 신규 문서·추가 기록 |

## 제작·검사 계약

친근한 성인 여성, 갈색 땋은 머리·초록/아이보리 여행복·가죽 가방·물약2병을 유지한 스타일화된 RPG NPC다. 새 참고 보드는 형태/재질 지침이며 모델 투영 텍스처로 쓰지 않는다. 폴리곤을 추가하는 것보다 얼굴 통합·손/의상 형태·UV와 PBR의 엔진 이식성을 먼저 개선한다.

좌표 Z up/front -Y, 단위 미터, 높이1.7m. 현재23본 구조와 root stationary 30fps/1~61프레임 2초 Idle을 유지한다. LOD0 상한55,000삼각형, 4개 atlas 섹션 목표, Face2k/Hair2k/Cloth4k/Gear2k. 전용 NPC에는 메시 충돌을 추가하지 않고 기존 감지·상호작용 컴포넌트를 유지한다. lip-sync·얼굴 리그·개별 손가락 제어·Manny 자동 호환·완성된 상용 캐릭터 품질은 이번 계약에 포함하지 않는다.

양쪽 팔 내리기만으로 스키닝을 통과시키지 않는다. 팔/어깨 굽힘·도달·몸통 비틀기·무릎을 실제로 평가하고 개선 여부와 남은 제한을 기록한다. 동일 카메라/조명/재질의 기준선과 후보를 비교하고 지표와 시각 품질을 별도로 판단한다. 원인 수정 후 같은 검사를 다시 실행한다.

v1 출력은 `Art/NPC/Arin/Versions/v1`, 소스/렌더/보고서는 `Feature/doc/evidence/arin-quality-20261007/baseline`에 보존했다. 신규 출력 `Art/NPC/Arin/Refined`, 신규 Unreal 폴더 `/Game/Rogue10m/Characters/Arin/RefinedV2`를 사용한다. root만 실제 Blender/UE를 실행하며 병행 에이전트는 독립 소스/critic/검사기를 작성한다. C++ 변경이 없으면 전체 Editor 빌드를 반복하지 않는다. 커밋·푸시는 하지 않는다.

## 완료 시 결정과 Exit Gate

Q1~Q6를 수행했다. 눈/의상/두피/손/무릎 변형과 실제 UE 자동 LOD의 품질 실패는 각각 보존한 뒤 소스·웨이트·축소 설정에서 수정했다. 최종 원본은 51,634삼각형·4atlas·16PNG·23본·2초 Idle이며, 새 프로세스 BLEND/FBX/GLB 검사와 최종 22개 FBX 자세 화면을 검토했다.

UE 기본 자동 생성의 실제 retention은 1/.25/.125였다. 의상·홍채 손상 때문에 imported LOD0를 보존하고 LOD1=.85/LOD2=.65, LockEdges=True, BaseLOD=0, VolumeImportance=1로 결정했다. 세 LOD의 실제 정점 수는 40,211/35,681/28,851이며 런타임 삼각형 수는 노출되지 않아 추산하지 않는다. 최종 실제 UE 화면 9개·PIE67/67·맵 언로드/재로드·별도 Cmd 프로세스 검증을 통과했다.

Content 전체 SHA256 감사에서 안내인 설정 3개와 신규 v2 24개 외 변경은 없다. Editor-only 검토 상태는 복원했다. 산출물·단계별 원본 보고서·남은 근접 미술/리그 한계는 [결과 문서](D:/Project/Rogue10m/Feature/doc/2026-10-07_arin-quality-refinement.md)에 기록한다. 보행·전투·얼굴/손가락 애니메이션·패키징/출시 성능은 별도 작업이다.
