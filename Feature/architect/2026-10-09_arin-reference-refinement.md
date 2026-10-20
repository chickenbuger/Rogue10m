# 아린 v3 — 사용자 FBX·이미지 기반 개선

- 작업 브랜치: `Sprint#5-18-arin-reference-refinement`
- 요청: 사용자 FBX와 전신 T-pose 이미지를 기준으로 현재 아린을 최대한 개선한다. Blender 편집 원본과 Unreal용 FBX를 전달하고 기존 안내 NPC 기능을 보존한다.
- 기준선: `Art/NPC/Arin/Refined/` v2, 기존 RefinedV2 엔진 콘텐츠. 원본 첨부 파일과 v2는 변경하지 않는다.
- 입력 FBX SHA256: `9ec4b4fb034071c8b6c46d015991c636be4f11544f03f913d4697eed0b5ab6b9`.

## 설계 결정

실제 FBX 조사 결과 단일 90,186삼각형 메시, 2K 임베드 Color atlas, 뼈/가중치/Action 없음이다. 얼굴·물결 머리·땋기·후드와 자수 의상이 기존 v2보다 참조 이미지에 가깝다. 이 표면과 UV를 새 v3의 기반으로 채택한다. 동일한 형상을 원시 도형으로 다시 만드는 대신 실제 입력 메시를 정리·수리하고 재질과 리깅을 개선한다.

높이 1.70m, 발바닥 Z=0, 정면 -Y, 적용된 양의 단위 변환, FBX -Z forward/Y up을 사용한다. 기존 23본 의미 이름/2초 stationary looping Idle을 유지하되 관절 위치를 도너에 맞춘다. 필요하면 망토/땋기 보조본을 추가한다. 얼굴·개별 손가락 애니메이션이나 Manny 호환은 이번 계약에 포함하지 않는다. 도너의 얼굴과 자수를 보호하기 위해 LOD0 예산은 100,000삼각형, 최대 4재질/4영향, 4K PBR atlas로 정한다. 2K 원본의 실제 정보량은 별도로 명시한다.

## Ultrawork Packets

| 패킷 | 목표·영역 | 완료 조건 | 검증 | 롤백 경계 |
|---|---|---|---|---|
| R1 | 첨부 FBX 실제 조사·입력 보존 | 원본 SHA/UV/토폴로지/재질·정면/측면/후면/얼굴 증거 | 새 Blender factory-startup 입력 조사 | 새 Art ReferenceRefined 및 전용 evidence만 |
| R2 | 형상 정리·국소 구멍 수리·참조 적합성 | 기존 상세 형상·UV 보존, 실제 얼굴/헤어/의상 검토 | 동일 카메라 textured/clay 렌더·수리 전후 ledger | 새 v3 source만 |
| R3 | 올리브/갈색 팔레트·표면 개선 | 피부/머리/천/가죽 재질 반응, portable 실제 PBR PNG | 원본 색 이미지와 shader bake·실제 렌더 | v3 Textures와 surface module |
| R4 | 도너 맞춤 리깅·Idle·FBX | normalized weights·최대4영향, 2초 loop, 관절 자세 검토 | fresh BLEND/FBX/GLB inspector 및 bend/reach/twist/knee 증거 | 새 rig/내보내기만 |
| R5 | Unreal 새 소유 폴더 적용 | 새 V3 에셋·guide 외형만 수정, 저장·재로드·LOD/PBR 확인 | 실제 UE 저장 검사·PIE NPC 회귀·Content SHA 감사 | V2 참조로 guide 복원; 다른 콘텐츠 보존 |
| R6 | 독립 검토·최종 전달 | 실제 결과/제한/경로·검증 보고서·한국어 로그 | source/export SHA·실제 이미지 개봉·diff/harness | 문서/evidence만 |

## Exit Gate

수치 합격은 시각 합격을 대신하지 않는다. 얼굴·손·헤어·후드·부츠와 변형 자세를 실제로 열어 확인한다. 자동 리깅/도너 재사용으로 생긴 clipping을 발견하면 source를 고치고 같은 검사를 반복한다. 뒷면은 입력 FBX의 형상을 기준으로 하며 정면 이미지에 없는 상세를 정확한 원본으로 주장하지 않는다. 기존 대화·포션2개·중복 방지·만석 재시도·포탈 뒤 잔여수량은 보존한다. C++를 바꾸지 않으면 Editor 컴파일 대신 asset/runtime 검증을 실행한다. 커밋·푸시는 사용자 승인 전 수행하지 않는다.

## R4 재설계 — 실제 두 번의 변형 실패 이후

첫 후보는 Z=1.235m 경계에서 인접 3.6mm 정점이 몸통/팔로 완전히 분리되어 최대 약92배 edge 신장이 발생했다. 두 번째 후보는 Z를 부드럽게 바꿨지만 망토/팔을 Y 깊이로 나누어 소매와 가죽 팔보호대 뒷면이 몸통에 남았다. 같은 실패 유형이 두 번 발생했으므로 반복 적용을 멈추고 Architect/독립 Reviewer 진단으로 돌아왔다.

재설계는 어깨에서만 연속적인 Z/X drape 보간을 사용하고, X 절댓값 .34m 바깥의 전체 팔 단면은 깊이와 무관하게 팔 본에 바인딩한다. 도너의 망토와 팔은 Y 영역이 겹치므로 Y threshold를 부위 판정으로 쓰지 않는다. 실패 렌더/BLEND를 attempt1/attempt2로 보존하고 동일 Idle 및 극단 자세의 실제 재수입 결과를 검사한다. 무릎은 도너의 ±.15m, 발목 ±.17m에 맞춰 기존 좁은 v2 관절 위치를 수정한다.

## R3·R5 검토 후 국소 복구

무릎 변형은 좌우 앞치마/바지 경계의 연속 가중치로 수정했다. 목의 삼각형 패치는 무채색 렌더에서 없고 textured 렌더에만 나타나, 그림자 픽셀을 다른 재질로 분류한 문제로 확인했다. 원래 얼굴·UV·색상을 보존하면서 목의 연속 공간 마스크를 사용한다. 새 내보내기는 새 프로세스와 정확한 파일 SHA로 다시 검사하며 이전 후보 합격을 자동 승계하지 않는다.

첫 UE v3 가져오기는 FBX 45296f9fcbd0d182406c85d29f8627409e090dc0c97ec97815a8035221448f00의 3개 핵심 에셋과 4개 텍스처만 생성한 뒤 Roughness 32×32 임시 리소스 검사에서 멈췄다. 아직 재질·guide 참조 변경과 명시 저장은 없다. 이 정확한 7개 범위, 원래 guide 체크포인트, 입력 경로·SHA가 일치할 때만 복구한다. 재가져오기/덮어쓰기/삭제 없이 기존 texture resource를 갱신하고 실제 4096×4096을 확인한 뒤 2개 누락 재질·LOD를 준비한다. 9개 의존 에셋을 모두 저장·검증한 후 별도 적용 단계에서 guide 외형을 교체한다. 이전 v2의 동일 proxy 문제를 단순 반복하지 않고 Epic 로컬 TextureCompiler/AsyncCompilationHelpers 소스와 독립 Reviewer를 통해 완료 명령과 deferred resource 처리 한계를 확인한다.
