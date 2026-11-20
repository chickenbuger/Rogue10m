# 공통 구르기 동작 기록

2026-11-20 KST, `Sprint#13-2-common-skill-web` 변경분을 최신 통합본 `d3629f01`의 물리적 독립 복사본에서 빌드했다. StartHub의 렌더링 세션 `CommonWeb1120B`에서 대검을 장착하고 회피 거리와 회피 회복 단축을 습득한 상태로 프로그램이 실제 Alt 입력을 보냈다.

구르기는 캐릭터의 기존 Manny 계열 골격과 이동 애니메이션 출력 위에 적용하는 네이티브 자세다. `Rogue10mViewAlignedUpperBodyProxy.cpp`에서 골반을 낮추고 몸을 접은 뒤 앞으로 회전한다. 앉기 자세와 회전은 몸의 포즈이며 캡슐은 `Rogue10mCharacterMobility.cpp`의 지면 충돌이 적용되는 이동을 쓴다. 구르기 전의 회피에서 기존 `/Game/Rogue10m/Animation/Combat/HeadSafe/AM_Safe_Dodge_Step`을 사용하고, 조건을 충족하면 기존 공통 회피 트레일도 표시된다. 기존 `/Game/Rogue10m/Animation/Common/AM_Dodge_Roll`의 연결 원본 `MM_Dash`는 구르기 후보 조사 후 제외했다. [기존 모션 검토](../../Feature/doc/2026-10-02_close-combat-animation-preparation.md)

| 시점 | 월드 시간 | 골반 높이 | 오른손 위치 |
| --- | --- | --- | --- |
| 준비 | 12.401초 | 3.56cm | X -41.01, Y 2.01, Z 36.38cm |
| 회전 중 | 12.768초 | -43.96cm | X -46.00, Y -44.51, Z -38.11cm |
| 회복 | 13.226초 | 1.51cm | X -14.83, Y 6.35, Z -0.51cm |

각 위치는 그 장면의 캡슐 중심을 뺀 월드 축 기준이다. 구르기 자체의 기본 시간은 0.42초, 회피 후 회복 단축을 습득한 실제 설정은 0.357초였다. 표의 시간은 테스트 샘플 시점이다. 구르기에는 공격 판정이 없으며, 회전 중 공격 요청을 거부하고 지면에서 297.59cm 이동한 뒤 자세가 복귀했다. 첫 실행에서 확인한 1,580.87cm 이동은 기존 회피 데이터의 초기 속도용 거리를 구르기의 직접 이동 거리로 사용했기 때문이다. 후속 검증에서는 구르기용 거리를 250cm로 설정했다.

실제 렌더링에서 몸의 회전과 회복을 확인했다. 회전 중 화면은 기본 3인칭에서 촬영되어 몸 하단 일부가 화면 밖에 있다. 골반과 오른손 수치는 캡처 요청 프레임의 값이다. `LogCore`의 PNG 캡처는 같은 프레임 번호에 기록됐다. 수동 키보드 입력과 전체 전투 구간 플레이는 별도 검증 범위다. [준비](../../Feature/doc/evidence/common-skill-web-20261120/roll-ready.png) · [회전](../../Feature/doc/evidence/common-skill-web-20261120/roll-active.png) · [회복](../../Feature/doc/evidence/common-skill-web-20261120/roll-recovered.png)
