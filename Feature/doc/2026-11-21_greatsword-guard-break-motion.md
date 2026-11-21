# 대검 파쇄참의 정면 사선 타격

파쇄참은 정면 부채꼴 안의 표적에 피해를 주지만, 기존에 재생한 짧은 훅의 검은 정면 85·110·140cm의 표적 캡슐 근처까지 닿지 않았다. 프로젝트의 Mixamo 기반 `A_GreatSword_Primary03`과 대검 `A_GreatSword_Idle`을 Manny 골격의 FBX로 내보내 Blender 4.5.14에서 편집했다. 검을 가까이 들어 준비하고, 짧게 사선으로 내려친 뒤 가드로 돌아온다. 몸과 IK에 동작을 옮겨 루트를 고정했다. 섞은 자세에서 발볼이 바닥 아래로 내려가는 프레임은 최대 2.25cm 끌어올렸다. [설계](../architect/2026-11-21_greatsword-guard-break-motion.md) · [Blender 재반입](evidence/greatsword-guard-break-motion-20261121/fresh-blender-reimport.json)

실제 `DA_Hybrid_GreatSword_GuardBreak`에 `A_GreatSword_GuardBreak`와 `AM_GreatSword_GuardBreak`를 연결했다. 이전 `AM_Safe_GreatSword_Primary02`와 동일한 `DefaultSlot`, 1.666667초, 60Hz, 진입·회복 0.25초 Hermite Cubic 블렌드를 쓴다. 스킬의 피해 지연은 0초이며 수락과 같은 틱에 한 번 피해를 준다. [Unreal 에셋 반입](evidence/greatsword-guard-break-motion-20261121/unreal-import.json)

## 실제 Q 입력과 검 궤적

`62e1f6d4`를 기준으로 StartHub에서 철제 장검을 실제로 장착하고 파쇄참을 배워 Q에 연결했다. 같은 60Hz 전투 경로에서 세 표적을 차례로 정면에 옮겼다. 기존과 수정 모두 각 표적에 수락 틱의 피해 한 번을 관측했으며, 범위 밖 후방 표적의 피해는 0이었다. 활성 몽타주 97프레임, 시전자 캡슐 위치 및 방향이 두 실행에서 같았다. [전후 동작과 확인 결과](evidence/greatsword-guard-break-motion-20261121/runtime-comparison.json)

| 정면 표적 거리 | 기존 최소 검·캡슐 간격 | 수정 최소 간격 |
| --- | ---: | ---: |
| 85cm | +23.57cm | −31.28cm |
| 110cm | +45.46cm | −30.85cm |
| 140cm | +68.35cm | −28.72cm |

검 중심축과 캡슐 간격의 음수는 한 프레임에서 두 형태가 겹쳤음을 뜻한다. 110cm 표적에서 겹침이 가장 큰 시점은 첫 피해 0.767초 뒤다. 첫 피해 직후의 간격은 +44.47cm였다. 화면의 검 경로와 즉시 피해 시점을 각각 기록했다.

표시 외형의 어깨 방향 범위는 약 124.04°였고 검 중심축의 최대 프레임 간 각도 변화는 17.97°에서 15.91°로 줄었다. 공격 중 바닥에서 가장 낮은 검 축 끝은 캡슐 바닥보다 94.10cm 위였다. 표시 발목 표식은 최소 10.35cm 위에 있었다. 반입 전 Blender와 독립적으로 연 FBX의 89개 뼈가 101개 키 시점에서 최대 0.000297cm 차이로 일치했다. [실행 로그](evidence/greatsword-guard-break-motion-20261121/runtime-results.txt)

## 실제 화면

`62e1f6d4`에 파쇄참을 적용한 StartHub에서 110cm 사례를 측면 카메라로 1280×720에 촬영했다. 준비, 첫 피해 직후, 46프레임 뒤 사선 타격, 회복을 확인했다. 사람의 두 손·두 발, 검과 표적이 보이는 사선 타격은 몽타주 0.697480초다. NVIDIA 탐색에서 DXGI 887A0004 한 건이 기록됐고, Microsoft Basic Render Driver에서 실제 프로그램 입력과 화면 저장을 완료했다. [준비](evidence/greatsword-guard-break-motion-20261121/ready.png) · [첫 피해 직후](evidence/greatsword-guard-break-motion-20261121/after-first-damage.png) · [사선 타격](../../DevLog/20261121_GuardBreakMotion/downstroke.png) · [회복](evidence/greatsword-guard-break-motion-20261121/recovery.png)

## 재현

[대검 기술 검사](2026-11-21_greatsword-technique-motion.md)의 독립 프로필 플래그에 `-RogueGreatSwordGuardBreakAudit`를 더하면 세 정면 거리와 범위 밖을 4건 실행한다. 정면 사례는 활성 구간에서 실제 표시 검 축과 표적 캡슐이 겹치는 프레임을 요구한다. 110cm 한 건의 렌더에는 NullRHI와 LogicOnly를 빼고 `-RogueGreatSwordGuardBreakCapture`를 사용한다. [Blender 저작](evidence/greatsword-guard-break-motion-20261121/AuthorGuardBreak.py) · [독립 FBX 검증](evidence/greatsword-guard-break-motion-20261121/VerifyGuardBreak.py)
