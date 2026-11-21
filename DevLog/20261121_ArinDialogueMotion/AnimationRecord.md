# 아린 대화 전환 애니메이션 기록

작업 사본 `D:/Project/Rogue10m/tmp/ArinMotionWorktree20261121`, 브랜치 `Sprint#17-5-arin-dialogue-motion`. 실제 `StartHub`에서 `2011e0de` 렌더와 `2666013d` 최종 통합 후보의 수치 검사를 2026-11-21 수행했다.

## 원본과 실제 경로

- 설정 `/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide`, 아린 `RefinedV3/SK_Arin`과 `RefinedV3/SK_Arin_Skeleton`을 사용한다.
- 대기 `/Game/Rogue10m/Characters/Arin/Standing20261110/A_Arin_BreathingIdle`은 Mixamo Breathing Idle에서 Manny를 거쳐 아린 체형과 접지에 맞춘 기존 파생본이다. 30fps로 9.933333초이며 대화에 들어가기 전 1배속으로 반복 재생한다. [기존 대기 자세 저작 기록](../20261110_HumanoidBossNpc/NPC/AnimationRecord.md)
- 대화 `/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Talk`와 보행 `/Game/Rogue10m/Characters/Arin/RefinedV3/Animations/A_Arin_Walk`은 기존 `Art/NPC/Arin/Animation/Arin_Motions.blend`에서 아린 전용으로 만든 클립이다. 대화는 1배속이며 보행률은 기존 속도 설정을 따른다. [Blender 저작 기록](../../Feature/doc/2026-10-09_arin-walk-talk-animation.md)
- 기존 활성 애셋 여섯 개와 `StartHub` 맵의 Git 개체가 두 기준과 실제 작업 사본에서 동일하다. 이번 변경은 클립 선택 시 직전 출력 자세에서 새 클립까지 0.18초 동안 이어지는 블렌드다. 애셋 키와 경로, 대화 서비스, 입력과 보상은 기존 설정을 따른다.

## 기존 자세와 이번 실측

보존된 에디터 표본에서 `Talk` 0.8초와 `BreathingIdle` 첫 자세 사이의 위치 차이는 왼손목 31.037cm, 오른손목 9.689cm였다. 루트·양발은 같았다. 실제 게임에서 관측한 연속 프레임 차이는 다음 방식으로 따로 검사한다. [기존 포즈 표본](../20261110_HumanoidBossNpc/NPC/ViewportA/manifest.json)

`Scripts/VerifyNpcDialogue.ps1 -Label ArinMotionIntegratedA -Motion -NullRHI`는 격리 프로필로 실제 아린과 대화를 열고 닫는다. NPC의 네이티브 메서드를 호출한 뒤 60Hz 게임 시간의 본 갱신마다 메시 컴포넌트 좌표(cm)를 읽는다. 루트, 머리, 양손과 양발의 위치, 클립 위치와 전환 전후 시각을 기록한다. 대화 제스처라 공격 접촉과 피해 시점은 없다.

대화 0.816667초의 실제 손목 이동은 최대 30.964251cm였다. 새 클립을 요청한 뒤 첫 본 갱신의 손목 이동은 네 전환 모두 0cm였다. 가장 큰 한 프레임 이동은 닫기 중 4.429038cm와 재열기 중 3.924785cm다. 루트와 양발은 최대 0.000006cm였으며 39개 조건을 통과했다. D3D11 소프트웨어 렌더에서 LOD 2에서 1로 바뀌는 대화 시작, 닫기 시작과 0.10초 중간 자세, 재열기를 검증했다. 캡처는 기준 `2011e0de`에 아린 코드를 적용한 `ArinMotionWarpB`에서 얻었다. [수치와 화면](../../Feature/doc/2026-11-21_arin-dialogue-motion.md)
