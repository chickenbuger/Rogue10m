# 두 번째 점프 다리 애니메이션

2026-11-21 KST, 독립 작업 사본 `DoubleJumpMotionWorktree20261121`, 브랜치 `Sprint#21-1-double-jump-motion-audit`. 기준은 `531a7668`, 후보 빌드 기준은 `3b1e8047`이다. [설계와 검증](../../Feature/doc/2026-11-21_double-jump-motion-audit.md)

- 프로젝트의 Epic Manny `/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump` 0.867초와 `MM_Fall_Loop` 3.0초를 스켈레톤 전용 FBX로 내보냈다. Blender 4.5.14에서 첫 점프 0.10초의 발 밀기 자세와 같은 시각의 낙하 다리 자세를 비교했다. 이 차이를 약하게 반영해 왼쪽 주도 다리와 오른쪽 지지 다리의 회전 곡선을 편집했다.
- Unreal Editor에 반입한 프로젝트 파생 시퀀스는 `/Game/Rogue10m/Animation/Common/Airborne/A_CommonSecondJumpImpulse`이다. 스켈레톤은 `/Game/Characters/Mannequins/Meshes/SK_Mannequin`이며 60Hz로 15간격·0.25초다. 첫·마지막 키는 같다. 루트, 골반, 상체의 변위를 합치지 않는다. 새 Blender 세션에서 FBX를 불러왔고 Unreal 반입에서도 해당 루트와 다리 궤적을 확인했다. [Blender 재반입](../../Feature/doc/evidence/double-jump-motion-audit-20261121/blender-fresh-import.json) · [Unreal 반입](../../Feature/doc/evidence/double-jump-motion-audit-20261121/unreal-import.json)
- 프로그램 입력으로 첫 Space, 두 번째 Space, 착지를 기록했다. 두 번째 도약은 40프레임에 시작했고 하체 보정은 41~54프레임, 왼발 최대 변화는 47프레임에 적용됐다. 길이는 메시 컴포넌트 좌표 cm다. 실제 발, 지면, 공격 대상의 물리 접촉 시각은 이 수치의 발 위치와 구분한다. 현재 원래 Niagara 포인터는 `/Game/Rogue10m/VFX/Character/Movement/NS_Motion_DoubleJumpBurst`다.
- 최종 소프트웨어 렌더에서 두 장 모두 각 시나리오의 47프레임에 캡처를 요청했다. 게임 월드 시간은 대조 0.950000초, 두 번째 도약 2.116667초다. 1280×720 PNG 두 장의 완료를 확인했고 렌더 콜백의 세부 지연은 계측 범위 밖이다. [대표 화면](second-jump-720p.png)
- 편집 가능한 Blender 원본과 FBX는 해당 독립 작업 사본의 `tmp/DoubleJumpMotion/second-jump-v1`에 보존했다. 원본 클립은 기존 프로젝트 패키지에서 읽었다.
