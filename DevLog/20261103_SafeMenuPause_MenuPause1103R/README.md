# 실제 게임 화면 — 안전 메뉴 정지

2026-11-03의 D3D11 RenderOffscreen 실제 QA 화면3개다. 합성/재구성/영상이 아니다.

- 01_inventory_paused.png: 출발 전 실제 인벤토리.
- 02_nested_menus_paused.png: 인벤토리+장비 중첩. 기존 스탯 일부 글자 잘림은 후속 UI 과제다.
- 03_active_run_menu_continues.png: 같은 QA Hub에서 명시적으로 런을 시작한 비안전 fixture의 설정. 실제 필드 완주 사진이 아니다.

사진은 UI 상태를 보여주며 시간 정지/진행은 같은 실행의 world time/TimerManager 검사로 확인했다. 실제 사람 입력·다음 판·모션 품질 검증을 주장하지 않는다. 모든 PNG1280x720 디코딩·SHA256·Root 시각 검토를 마쳤다. 검사 결과는 Feature/doc/evidence/safe-menu-pause-20261103/VERIFICATION.json과 각 Label/results.txt를 따른다.

## 대표 사진 보관 상태

사용자 요청에 따라 중복·이전·보조 사진을 정리했다. 현재 `20261103_SafeMenuPause_MenuPause1103R` 묶음에는 대표 1장을 보관한다. 이 문서의 촬영 개수·파일명·시각 판정은 당시 실행 이력이며, 현재 사진 목록은 [대표 사진 목록](../README.md)을 따른다. 수치 결과·텍스트 실패 기록은 그대로 유지했다.
