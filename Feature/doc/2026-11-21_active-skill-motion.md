# 태산압정의 공중 내려치기

태산압정은 현재 너클 스킬트리에서 실제로 전수되는 공중 무공이다. 기존에는 지상 훅을 재생해 공격 중 공중에 있어도 두 발이 지상 공격 자세에 멈췄다. 《A_Safe_Hook》과 기존 Manny 낙하 클립을 Blender 4.5.14에서 비교한 뒤, 오른팔을 모아 아래로 뻗는 60Hz 상체 가산 클립을 제작했다. 실제 낙하와 착지에서 하체를 계속 평가하도록 골반·다리·루트에 가산 값을 넣지 않았다.

태산압정 데이터 DA_Attack_StoneFist_JumpSlam의 몽타주 참조를 새 상체 동작으로 교체했다. 원래 몽타주에서 길이 1.666667초, DefaultSlot, 구간 속도 및 블렌드 설정을 가져왔다. Unreal Editor의 60Hz 반입과 동일 Manny 골격을 확인했다. [계획](../architect/2026-11-21_active-skill-motion.md) · [Blender 재반입](evidence/active-skill-motion-20261121/fresh-blender.json) · [Unreal 반입](evidence/active-skill-motion-20261121/unreal-import.json) · [최종 트랙 연결](evidence/active-skill-motion-20261121/montage-bind.json)

## 실제 입력과 시간

StartHub에서 수련용 너클을 인벤토리로 장착하고 격리 숙련 스냅샷을 복구했다. 안내원 앞에서 공중 제어와 이단 점프를 포인트로 전수하고 자동 전수된 태산압정을 Q에 연결했다. 프로그램이 실제 컨트롤러 경로로 지상 Q, Space 한 번 뒤 Q, Space 두 번 뒤 Q를 보냈다. 지상에서는 발동하지 않았고, 두 공중 실행은 같은 입력 수락 프레임에 표적에게 피해를 한 번 적용했다. [런타임 결과](evidence/active-skill-motion-20261121/runtime-results.txt) · [프레임별 비교](evidence/active-skill-motion-20261121/comparison.json)

| 기본 인간 남성 60Hz | Space 한 번 | Space 두 번 |
| --- | ---: | ---: |
| 수락·첫 피해 | 17프레임 | 30프레임 |
| 접지 | 62프레임 | 84프레임 |
| 수락 100~200ms 사이 표시 오른손목의 골반 기준 내림 | 12.01cm | 13.68cm |
| 원본의 같은 구간 손목 이동 | 위로 10.45cm | 위로 1.05cm |

관측 205프레임에서 이동 모드, 수직 속도와 위치, 두 번째 도약의 가산 자세, 몽타주 재생 위치와 가중치가 기준과 일치했다. 두 번째 Space 다음 늦은 공중 구간에 이전 외형의 발은 멈췄고, 변경 후 왼발과 오른발은 각각 5.63cm와 6.89cm 움직이며 착지로 이어졌다. 오크 여성으로도 Space 두 번 뒤 같은 30프레임에 피해를 한 번 적용했고 84프레임에 접지했다. 외형 오른손목은 같은 구간에 12.7cm 내려갔다.

## 현재 스킬 경로 확인

Unreal Asset Registry에서 원래 인벤토리 무기 데이터로 확인된 것은 수련용 너클과 대검 타입 철제 장검이다. 대검 기본 프로필에는 공중 슬롯이 없다. 분쇄참, 회전참, 파쇄참, 대지파쇄의 데이터와 몽타주가 있으며 이번 입력·자세 검증은 태산압정을 대상으로 했다. Staff 프로필에 다섯 기존 공격 데이터가 있지만 해당 타입을 가진 무기 아이템은 발견되지 않았고 마법 스킬트리 탭에는 준비 중 화면이 표시된다. [실제 배포 프로필과 몽타주](evidence/active-skill-motion-20261121/deployed-skill-assets.json)

## 실제 화면

기준 수치는 678e22c5에서 기록했고, 수치 수정본은 몬스터 사망 개선을 포함한 41d5a224에서 재실행했다. 세 수치 실행과 에셋 반입 로그의 새 Unreal 오류는 0건이었다. 촬영은 678e22c5에 수정한 태산압정을 적용해 동일한 입력과 측면 카메라를 사용했다. NVIDIA 장치 탐색 중 DXGI 887A0004가 기록된 뒤 Microsoft Basic Render Driver가 1280×720 화면 네 장을 완료했다. 내려치기는 첫 피해 프레임에서 10프레임 뒤에 촬영했다. 입력과 저장은 QA 세션에서 처리했다. [준비](evidence/active-skill-motion-20261121/aerial-ready.png) · [첫 피해](evidence/active-skill-motion-20261121/aerial-first-damage.png) · [내려치기](../../DevLog/20261121_MountainSlamMotion/aerial-downstroke.png) · [접지](evidence/active-skill-motion-20261121/aerial-landing.png)

StartHub의 게임 컨트롤러 입력 QA에서 Rogue10m.TestHybridSkillTree와 새 QA 프로필·성장·스킬키 슬롯을 함께 사용한다. -RogueAerialSkillJumpCount=1 또는 2, 새 -RogueHybridEvidenceLabel, -UseFixedTimeStep -FPS=60을 지정한다. -RogueAerialOrcFemale은 오크 여성 외형을 고르며, 별도 720p 렌더에서 -RogueAerialCapture로 네 시점을 저장할 수 있다.
