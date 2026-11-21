# 패왕참 차징 동작 기록

2026-11-21, `Sprint#20-2-capstone-charge-brace`의 독립 작업 사본에서 검증했다. 실제 `DA_Hybrid_GreatSword_Ultimate`를 습득하고 장착한 뒤 StartHub에서 우클릭 입력을 재현했다. `A_GreatSword_CapstoneChargeBrace`는 기존 프로젝트 Manny 대검 대기와 발동을 기초로 Blender에서 키를 편집한 파생 클립이다. `/Game/Characters/Mannequins/Meshes/SK_Mannequin`에 120Hz 89키, 0.733333초로 가져왔다. 첫 포즈 기준의 로컬 가산 클립이며 프레임당 루트·골반·발목·발끝 변위는 0이다. [Blender FBX 재가져오기와 Unreal 가져오기](../../Feature/doc/2026-11-21_capstone-charge-brace.md)

정지한 3인칭에서 기존 대검 가드 위에 최대 0.40 가중치로 준비 자세를 얹는다. 몽타주 슬롯이 들어오면 가중치를 낮춘다. `AM_GreatSword_Charged`의 `MM_ChargedAttack`, 0.25초 진입·이탈 블렌드, 기술 재생률 0.62와 기존 피해 시점을 사용한다. 우클릭을 0.20초가량만 누르는 경우 일반 특수기를 쓰고, 유지 중 좌클릭이 들어와 양 버튼 입력으로 들어가면 취소한다.

표본은 표시 메시의 뼈 갱신 번호, 60Hz 게임 시간, 몽타주 시계, 메시 컴포넌트 좌표의 양손과 발, 검 기점과 검끝을 기록했다. 최초 피해는 대상의 실제 체력 감소 이벤트로 확인했다. 기존 자세와 파생 자세 모두 몽타주 0초에 피해를 적용했다. 다음 새 표시에서 몽타주 0.010953초에 왼손목 0.84→0.86cm, 오른손목 0.62→0.51cm, 검 각도 2.32→2.42°가 움직였다. 각 경우 19개의 발동 표본으로 몽타주 시계 0.20초 경계를 확인했다. 손목의 검 축 이동은 그립 위치의 대리 지표다. [최종 동작 표본](../../Feature/doc/evidence/capstone-charge-brace-20261121/runtime-poses.csv)

프로그램 입력으로 0.333333초 유지 자세를 1280×720 WARP에서 촬영했다. 기존/보정 촬영은 각각 프레임 81/163에서 요청과 저장이 같은 프레임에 끝났다. 두 검증 유형 모두 설정된 테스트 입력을 사용했다. 촬영은 유지 단계이며 피해나 접촉 순간은 수치 기록으로 확인했다. [렌더 결과](../../Feature/doc/evidence/capstone-charge-brace-20261121/render-results.txt)
