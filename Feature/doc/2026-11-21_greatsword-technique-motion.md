# 대검 회전참의 전신 스윙

회전참은 몸 뒤쪽 표적까지 맞히는 원형 기술이다. 기존에는 파쇄참과 같은 짧은 훅을 재생했고 표시 외형의 어깨는 15.42° 움직였다. 프로젝트에 있는 Mixamo 기반 대검 동작 `A_GreatSword_AltSlash04Repair1`을 Blender 4.5.14에서 편집했다. 원본의 디딤과 검 경로를 쓰면서 회전이 가장 급한 구간에 프레임을 더 줬다. 골반과 IK에 회전을 옮겨 루트를 고정하고 `A_GreatSword_Idle`에서 시작해 같은 자세로 돌아오게 했다. [설계](../architect/2026-11-21_greatsword-technique-motion.md) · [Blender 재반입](evidence/greatsword-technique-motion-20261121/fresh-blender-reimport.json) · [Unreal 반입](evidence/greatsword-technique-motion-20261121/unreal-import.json)

새 `A_GreatSword_Whirlwind`와 `AM_GreatSword_Whirlwind`를 `DA_Hybrid_GreatSword_Whirlwind`에 연결했다. Manny 89개 뼈를 60Hz로 기록했다. 몽타주는 `DefaultSlot`, 1.666667초 길이, 재생률 1.0, 진입과 회복의 0.25초 Hermite Cubic 블렌드를 기존과 동일하게 사용한다.

## 60Hz 실제 입력 검증

StartHub의 격리 검증에서 철제 장검을 실제 인벤토리로 장착했다. 안내원 앞에서 4점을 써 분쇄참, 회전참, 파쇄참, 대지파쇄를 배웠다. 컨트롤러에 Q와 우클릭 차징 입력을 보내 각 기술의 쿼리 범위 안·밖을 검사했다. 기존 `ad94906d`와 새 회전참 모두 8건을 통과했다. 회전참 뒤 사선의 표적은 수락과 같은 틱에 피해를 한 번 받았으며, 범위 밖 표적은 피해를 받지 않았다. 몽타주 활성 관측은 양쪽 모두 109프레임이다. [전후 수치](evidence/greatsword-technique-motion-20261121/runtime-comparison.json)

| 회전참 후방 표적 검증 | 기존 | 수정 |
| --- | ---: | ---: |
| 표시 외형 어깨의 누적 방향 범위 | 15.42° | 409.37° |
| 검 중심축 최대 이웃 프레임 각도 | 16.43° | 12.25° |
| 첫 피해 시 검 축과 표적 캡슐 사이 거리 | 65.81cm | 66.25cm |
| 활성 몽타주 전체의 최소 간격 | −31.55cm | −26.32cm |

수정 후 원본 Manny의 어깨 방향 범위는 412.22°였다. 회전 중 시전자 캡슐의 이동과 액터 방향 변화는 각각 0cm 및 0°였다. 검의 축 양 끝은 캡슐 바닥에서 최소 100.59cm 위에 있었다. 표적과의 거리는 검 중심축과 캡슐 기준이다. 음수는 이 기준에서 두 형태가 겹쳤음을 뜻한다. 회전참의 첫 피해는 즉시 발생하며, 검이 표적 가까이 지나는 것은 스윙 중이다.

파쇄참은 정면 110cm 표적에 피해를 줬고 두 독립 기준 실행과 수정본에서 검 축의 최소 간격이 47.77cm였다. 별도 기술의 재현 기록으로 남긴다. 분쇄참과 대지파쇄에는 기존 `MM_ChargedAttack`이 계속 재생됐다. [실행 기록](evidence/greatsword-technique-motion-20261121/runtime-results.txt)

## 실제 화면

`ad94906d`에 회전참을 적용한 독립 StartHub에서 실제 Q 입력으로 1280×720 화면 네 장을 저장했다. 첫 구도는 뒤쪽 표적에 가까운 게임 카메라가 몸을 잘라 기록으로 남겼다. 임시 측면 카메라를 넓힌 최종 구도에서 캐릭터의 두 손과 두 발, 검과 뒤쪽 표적이 화면에 들어왔다. 대표 화면은 첫 피해 75프레임 뒤의 스윙이며 몽타주 시각은 1.020426초다. NVIDIA 탐색에서 DXGI 887A0004 한 건이 기록됐고, Microsoft Basic Render Driver에서 모든 프레임이 처리됐다. 수치 실행과 에셋 반입의 새 오류는 0건이었다. 입력은 프로그램으로 보냈다. [초기 구도 실패](evidence/greatsword-technique-motion-20261121/rejected-default-camera-sweep.png) · [준비](evidence/greatsword-technique-motion-20261121/ready.png) · [최초 피해 직후](evidence/greatsword-technique-motion-20261121/after-first-damage.png) · [뒤쪽 스윙](../../DevLog/20261121_WhirlwindMotion/rear-sweep.png) · [회복](evidence/greatsword-technique-motion-20261121/recovery.png)

## 재현

StartHub에서 `Rogue10m.TestWeaponThirdPerson`을 실행한다. 네 기술의 수치 검사에는 `-RogueGreatSwordTechniqueQA -RogueMeleeVfxRun -RogueMeleeVfxLogicOnly -NullRHI -UseFixedTimeStep -FPS=60`을 사용한다. 새 이름을 `-RogueWeaponLabel=<label>`로 지정한다. `-RogueProfileSaveSlot=Rogue10m_Town_QA_MeleeVfx_<label>`, `-RogueProgressionSavePrefix=Rogue10m_Progression_QA_MeleeVfx_<label>`, `-RogueSkillKeySaveSlot=Rogue10m_Fighter_QA_MeleeVfx_<label>`에도 같은 이름을 사용한다. 회전참 단독 렌더에서는 `-RogueMeleeVfxLogicOnly -NullRHI` 대신 `-RogueGreatSwordWhirlwindCapture`와 사용 가능한 그래픽 장치를 지정한다.

Blender 원본을 재생성할 때 Unreal Editor에서 계획 문서의 `AltSlash04Repair1`과 대검 `Idle`을 프리뷰 메시 없이 각각 `alt04.fbx`, `guard.fbx`로 내보낸다. Blender 백그라운드 실행에서 저작 스크립트에 원본 폴더와 새 출력 폴더를 넘기고, 별도의 Blender 실행에서 검증 스크립트에 출력 폴더를 넘긴다. [저작 스크립트](evidence/greatsword-technique-motion-20261121/AuthorWhirlwind.py) · [독립 재반입 스크립트](evidence/greatsword-technique-motion-20261121/VerifyWhirlwind.py)
