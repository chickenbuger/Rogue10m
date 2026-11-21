# 이동 중 대검 장착·해제 기록

- 작업 사본 `tmp/AvatarMotionWorktree20261121`의 `Sprint#25-4-moving-unequip-motion`에서 통합 기준 `62e1f6d4`로 검증했다. 기존 Manny `SKM_Manny_Simple`, `ABP_Common_Unarmed`, 방어 자세 `A_GreatSword_Idle`, 보행 `BS_GreatSword_Ground`·`BS_Unarmed_PalmReference`를 사용한다.
- StartHub 평지에서 프로그램 W로 이동하고 기존 철제 장검 인벤토리 동작으로 대검을 장착·해제했다. 첫 평가와 이후 8프레임의 손목·발 위치는 메시 컴포넌트 좌표의 cm이고 회전은 각도다. 피해가 발생하지 않는 장비 동작이다.
- 최종 보정은 포즈 차이를 팔과 상체 0.22초, 다리 0.16초 동안 줄인다. 자산과 재생 위상은 기존 방식을 사용한다. [동작 기록](../../Feature/doc/2026-11-21_moving-sword-locomotion-handoff.md)
- `MFinalDwarfMaleFullWarp` 720p에서 해제 직전 67프레임, 첫 평가 69프레임, 맨손 보행 복귀 82프레임을 요청한 프레임에 저장했다. [본문 검증 수치](../../Feature/doc/evidence/moving-sword-locomotion-handoff-20261121/validation.json)
