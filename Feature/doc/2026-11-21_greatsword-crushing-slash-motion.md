# 대검 분쇄참의 양손 내려베기

분쇄참은 기존에 `MM_ChargedAttack`의 맨손 자세를 재생해 표시 외형의 양 손목이 최대 98.43cm 벌어졌다. 골반 기준 오른손목은 한 프레임 사이 22.72cm 튀었다. 기존 Mixamo 기반 대검 `A_GreatSword_Primary02`의 들어 올리는 준비와 전방 사선 타격을 Blender 4.5.14에서 편집했다. 같은 대검 가드에서 시작해 두 손을 모아 들어 올리고 짧게 내려친 다음 가드로 돌아온다. [계획](../architect/2026-11-21_greatsword-crushing-slash-motion.md) · [Blender와 Unreal 반입 기록](evidence/greatsword-crushing-slash-motion-20261121/blender-and-import.json)

첫 편집본은 손 간격이 줄었지만 205cm의 실제 표적 앞에서 검 중심축이 캡슐에 +6.20cm 못 미쳤다. 두 번째 편집에서 왼발 디딤과 최대 26cm의 골반 전진을 넣었다. 뒤쪽 오른발목은 첫 편집본과 Blender에서 0.00006cm 이내로 일치했다. 허벅지·종아리 길이 최대 오차는 0.00004cm였다. 하체를 낮추는 정도는 검이 바닥을 지나지 않게 조절했다. 루트는 시퀀스 전체에서 고정된다.

## 실제 60Hz 검증

StartHub에서 철제 장검을 인벤토리로 장착하고 실제 전수한 분쇄참을 Q에 연결했다. 110cm, 205cm 및 범위 밖 표적에서 기존과 수정 동작을 비교했다. `DA_Hybrid_GreatSword_CrushingSlash`에 `A_GreatSword_CrushingSlash`와 `AM_GreatSword_CrushingSlash`를 연결했다. 몽타주 슬롯 `DefaultSlot`, 1.833333초 길이, 재생률 1.0과 진입·회복 0.25초 Hermite Cubic 블렌드를 복제했다. 기존 스킬 배속은 0.72다. [비교 수치](evidence/greatsword-crushing-slash-motion-20261121/runtime-comparison.json) · [실행 기록](evidence/greatsword-crushing-slash-motion-20261121/runtime-results.txt)

| 측정 항목 | 기존 | 최종 |
| --- | ---: | ---: |
| 표시 외형의 양 손목 최대 간격 | 98.43cm | 30.70cm |
| 표시 외형의 양 손바닥 최대 간격 | 94.88cm | 28.47cm |
| 표시 오른손목의 골반 기준 최대 한 프레임 이동 | 22.72cm | 10.39cm |
| 표시 검 중심축의 최대 3D 각도 변화 | 16.99° | 11.04° |
| 정면 110cm 표적의 최소 검·캡슐 간격 | −33.79cm | −33.29cm |
| 정면 205cm 표적의 최소 검·캡슐 간격 | −26.28cm | −21.74cm |

205cm 표적에서 최종 검·캡슐 간격은 수락 0.983초 뒤, 몽타주 0.763200초에서 가장 작았다. 음수는 표시 검 중심축과 표적 캡슐이 겹친 프레임을 뜻한다. 캡슐 반경을 포함하는 거리이므로 표시 검 끝의 최대 정면 투영은 191.80cm였다. 기존 232.07cm는 왼손이 떨어진 채 오른손만 뻗은 후반 프레임에서 기록됐다. 정면 판정은 285cm 부채꼴이다.

기존과 최종의 활성 몽타주 관측은 각각 129프레임이었다. 두 정면 표적은 수락과 같은 틱에 한 번 피해를 받았고, 범위 밖 표적의 피해는 0이었다. 측정한 시전자 캡슐 이동은 0cm였다. 검 중심축의 가장 낮은 점은 실제 표시 메시에 연결된 위치에서 캡슐 바닥보다 45.86cm 위였다.

## 실제 화면

기준 `af9ffb60`에 최종 동작을 적용한 별도 StartHub에서 205cm를 촬영했다. 1280×720으로 대기, 피해 직후, 머리 위 준비, 내려베기와 회복을 확인했다. 머리 위 준비는 몽타주 0.508800초, 내려베기는 0.763200초였다. 두 발과 양손, 검과 표적을 같은 측면 화면에서 확인했다. 동일한 129프레임 시계에서 디딤 보정 전후 오른발목 변화는 Manny 최대 0.055cm, 표시 외형 최대 1.35cm였다. NVIDIA 장치 탐색에서 DXGI 887A0004 한 건 뒤 Microsoft Basic Render Driver가 프로그램 입력과 화면 저장을 완료했다. [준비](evidence/greatsword-crushing-slash-motion-20261121/ready.png) · [첫 피해](evidence/greatsword-crushing-slash-motion-20261121/after-first-damage.png) · [머리 위 자세](evidence/greatsword-crushing-slash-motion-20261121/overhead-load.png) · [205cm 내려베기](../../DevLog/20261121_CrushingSlashMotion/downstroke-205cm.png) · [회복](evidence/greatsword-crushing-slash-motion-20261121/recovery.png)

## 재현과 소스

[대검 입력 검사](2026-11-21_greatsword-technique-motion.md)의 격리 프로필과 플래그를 사용한다. 205cm 비교에는 `-RogueGreatSwordCrushingEdgeQA`를 추가한다. 같은 205cm 기준의 기존 몽타주 비교에 `-RogueGreatSwordCrushingStockReference`를 추가하면 해당 검사 프로세스에서 기존 몽타주를 로드한다. 렌더 실행은 NullRHI와 LogicOnly를 빼고 `-RogueGreatSwordCrushingSlashCapture`를 사용한다.

게임 파생 에셋에는 Manny 골격을 사용한다. 원본을 재생성할 때 Unreal Editor에서 프로젝트 `A_GreatSword_Primary02`, `A_GreatSword_Idle`을 프리뷰 메시 없이 각각 `primary02.fbx`, `guard.fbx`로 내보낸다. 초기 Blender 저작, 앞발과 지지발 보정, 독립 재반입 순으로 실행한다. FBX와 편집 `.blend`는 작업 사본의 `tmp/GreatSwordTechniqueRuns/crushing-donor` 및 `crushing-b`, `crushing-d`에 두었다. [기본 저작](evidence/greatsword-crushing-slash-motion-20261121/AuthorCrushingSlash.py) · [앞발·지지발 보정](evidence/greatsword-crushing-slash-motion-20261121/ExtendCrushingSlash.py) · [FBX 재반입](evidence/greatsword-crushing-slash-motion-20261121/VerifyCrushingSlash.py)
