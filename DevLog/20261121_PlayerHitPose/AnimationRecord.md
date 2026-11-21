# 플레이어 피격 자세 기록

- 원본은 프로젝트의 `/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01`과 `MM_HitReact_Back_Med_01`이다. Blender 4.5.14에서 `spine_01`부터 `spine_04`의 회전만 추렸다. 원본 프로젝트 에셋은 그대로 보존했다.
- 적용 대상은 `/Game/Rogue10m/Animation/Combat/FighterStance/A_PlayerHitRecoil_Front`와 `A_PlayerHitRecoil_Rear`다. 0.25초 길이의 120Hz 가산 파생본이며 Unreal Editor로 새 패키지에 가져왔다. 작성 키의 최대 시점은 5프레임(60Hz, 0.083초)이다.
- `StartHub`에서 실제 점 피해가 확정된 뒤 양손과 흉부를 측정했다. 같은 몽타주 시각의 맨손 잽 중단을 대조했을 때, 가산 반응은 5프레임에 표시 흉부 5.80°, 머리 0.24°, 오른손 3.78cm를 더했다. 기존 잽 중단 블렌드 값과 실제 체력 처리는 일치했다.
- 렌더에서 다섯 번째 프레임의 전방 피격 자세를 같은 카메라의 대기와 비교했다. 입력은 프로그램 마우스와 실제 점 피해 경로이며 발 변위는 수치로 확인했다. [대표 피격 화면](front-recoil.png) · [대기 대조](../../Feature/doc/evidence/player-hit-pose-20261121/idle-control.png) · [포즈와 검사 기록](../../Feature/doc/2026-11-21_player-hit-pose-audit.md)
