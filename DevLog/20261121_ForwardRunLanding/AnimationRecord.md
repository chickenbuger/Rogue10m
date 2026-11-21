# 전방 달리기 착지·보폭 기록

격리된 `Sprint#18-1-player-locomotion-audit`에서 StartHub 기본 HumanMale의 실제 W/D/Space 실행 경로를 60Hz로 검수했다. 비교 기준은 `48de957d`의 원래 `ABP_Common_Unarmed`였고, 최종 검수는 장비·피격 변경을 포함한 `e990e106` 위에서 진행했다. 평지와 카메라만 실행 중 일시 배치했다.

- 출처는 프로젝트의 Manny `MM_Fall_Loop`, 정지 `MM_Land`, `MF_Unarmed_Jog_Fwd_PalmReference`다. Unreal에서 내보낸 같은 Manny 골격을 Blender로 편집하고 `A_CommonForwardRunLanding_L`을 `/Game/Rogue10m/Animation/Common/Airborne`에 저장했다. 60Hz 28키의 0.45초 클립이고 루트 모션은 껐다.
- `Main States`는 일반 전방 첫 착지에 `Forward Run Land`를 사용한다. 접지 전 마지막 Fall 0.350000초와 접지 첫 Fall 0.366667초를 도입부의 기준으로 삼았다. 왼발 지지와 오른발 인계의 authored 마커는 각각 0.133333초와 0.416667초다. 620/610cm/s의 표시 외형은 접지 +0.400초에 실제 `R` 0.852108/0.851830초로 이어졌다.
- 숨김 Manny와 표시 외형의 골반·무릎·발·발볼을 컴포넌트 좌표와 평지 월드 좌표에서 비교했다. 67~300ms 표시 외형의 바닥 8cm 이내 발볼 수평 이동은 620cm/s에서 174.93→40.08cm, 610cm/s에서 173.64→39.65cm였다. 각 기준·적용 쌍의 접지 프레임과 속도가 일치했다.
- 0.20/0.34초 전환 시험은 무릎이 튀어 제외했다. 첫 WARP 캡처는 렌더링 검사를 통과했으나 앞쪽 발끝이 잘려 카메라를 43°로 넓혔다. 실제 세션의 프로그램 입력, 수치 검수, 렌더 검수를 구분했다. 물리 입력과 전체 플레이는 검수 범위 밖이다.

[기능 기록](../../Feature/doc/2026-11-21_player-locomotion-audit.md)
