# 파진각 애니메이션 기록

- 출처는 프로젝트 제작 `A_Knuckle_Uppercut`의 기존 Manny 리그와 가드 자세다. 발차기의 본 궤적은 Blender 4.5.14에서 작성했다. 기존 `F.BreakingKick`의 `DA_Attack_Knuckle_Special`에 새 `AM_Fighter_BreakingKick`과 시퀀스가 연결된다.
- 동일한 60Hz 게임 프로그램 Q 입력에서 장착한 수련용 너클의 첫 피해는 74프레임이었다. 표시 오른쪽 발볼 위치는 카메라 좌표와 분리해 캐릭터 및 대상 캡슐 기준으로 기록했다. 실제 피해와 클립의 입력 재생을 같이 관측했다.
- 최종 720p 실행 `BreakingKickWarpFinal55`에서 준비 61, 첫 피해 74, 회복 146프레임을 확인했다. [첫 피해 대표 화면](first-damage-720p.png)
- 새 클립은 Blender 재가져오기와 Unreal Editor에서 검증했다. `.blend` 편집본은 로컬 `tmp/BreakingKickBlender20261121/candidate-c/MannyBreakingKick.blend`에 있다. [작업과 게임 검증](../../Feature/doc/2026-11-21_breaking-kick-motion.md)
