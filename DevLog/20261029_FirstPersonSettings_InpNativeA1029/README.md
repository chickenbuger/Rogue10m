# 렌더링 입력 검증 — InpNativeA1029

2026-10-29T15:23:53.906851+00:00, D:\Project\Rogue10m, Sprint#9-12-input-validation, 기준11a15011. 최종 커밋은 ../20261029_InputFrameValidation/verification.json 참조.

실제 D3D11 viewport PNG8장이다. 01~06은 StartHub 설정/UI, 07~08은 메모리에만 만든 격리 충돌 바닥의 실제 프로그램 입력 이동이다. native 창인지 Offscreen인지는 라벨/verification에 구분한다. 8장 모두1280×720 디코딩을 확인했다. 대표 설정·확인창·달리기 화면의 가독성을 검토했다.

확인항목569개 PASS(프레임 반복 포함). OFF→ON→ON→OFF 네 구간 min/max920cm/s, 순XY1380cm 이상, 원래 판정 유지. 측정 중 flush/별도viewport key/시야편차/기록누락0. native 라벨의 FocusProbe는 실제 엔진 API를 프로그램 호출한 양성 대조군이며 OS 물리 조작이 아니다.

재현: Scripts/VerifyFirstPersonSettings.ps1 -Label <새ASCII라벨> -Rendered -InputTrace -FocusProbe. 기존라벨/슬롯을재사용하지않는다. 사진을 수동 플레이·일반 필드 완주 증거로 표현하지 않는다. 사진·로그·실제 세이브는 Git에 추가하지 않는다.


## 대표 사진 보관 상태

사용자 요청에 따라 중복·이전·보조 사진을 정리했다. 현재 `20261029_FirstPersonSettings_InpNativeA1029` 묶음에는 대표 0장을 보관한다. 이 문서의 촬영 개수·파일명·시각 판정은 당시 실행 이력이며, 현재 사진 목록은 [대표 사진 목록](../README.md)을 따른다. 수치 결과·텍스트 실패 기록은 그대로 유지했다.
