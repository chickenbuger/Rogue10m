# 필드 달리기 착지 애니메이션 기록

2026년 11월 21일 `Sprint#24-3-field-ground-motion`의 격리 사본에서 검증했다. Blender의 기존 `MannyMovingLandLeft.blend`에서 오른쪽 다리 회복을 편집하고, Unreal의 `/Game/Rogue10m/Animation/Common/Airborne/A_CommonForwardRunLanding_L`로 반입했다. `SK_Mannequin`과 `ABP_Common_Unarmed`에 연결된 현재 자세를 비교했다.

프로그램 W/Space 입력과 60Hz에서 원본 및 보정본을 비교했다. 최초 접지는 상대 프레임 0, 오른발 회복 측정은 +11~27프레임, Jog 인계는 기존 두 번째 오른발 마커다. 위치는 월드 cm, 회전은 부모 본 상대 quaternion의 각도 차이다. 이 동작은 비무장 이동이며 공격 피해나 무기 궤적은 없다. 실제 지면의 경사와 접지 속도, 표시 외형의 본 비교는 [기능 기록](../../Feature/doc/2026-11-21_field-ground-motion.md)에 있다.
