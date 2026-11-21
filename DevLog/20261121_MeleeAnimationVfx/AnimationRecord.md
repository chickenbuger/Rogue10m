# 너클·대검 공격 애니메이션 VFX 기록

- 2026-11-21, 별도 물리 작업 사본 `tmp/MeleeAnimationVfx20261121`의 `Sprint#17-4-melee-animation-vfx`. 720p 렌더 기준 `2011e0de`, 최종 통합 수치 검증 기준 `2666013d`.
- 기존 Data Asset과 Montage 사용. 대검 기본기 5개는 `/Game/Rogue10m/Animation/Combat/GreatSword20261110`의 프로젝트 파생 몽타주이며, 너클 기본기는 `/Game/Rogue10m/Animation/Combat/FistAuthored`의 기존 몽타주를 사용한다. 패왕참은 기존 `/Game/Rogue10m/Animation/Combat/GreatSword/AM_GreatSword_Charged`에서 `MM_ChargedAttack`을 재생한다. 실제 선택한 공격별 몽타주·소스 시퀀스는 [런타임 이벤트](../../Feature/doc/evidence/melee-animation-vfx-20261121/runtime-events.txt), 공격 DA·몽타주·스켈레톤 경로는 [에셋 재로딩 기록](../../Feature/doc/evidence/melee-animation-vfx-20261121/asset-readback.json)에 있다.
- 기존 UE 5.8 공격의 배속·블렌드·타수·피해 지연을 사용한다. `frames-720p.csv`에는 60Hz 검증 프레임, 월드 초, 몽타주 초, 양손과 검의 월드 좌표 cm, 최초 피해, 경과 시간과 공격 데이터의 지연을 기록했다. 캡처 요청과 완료의 프레임 및 월드 시각은 [보관 화면 기록](../../Feature/doc/evidence/melee-animation-vfx-20261121/captures-retained.csv)에 있다.
- 패왕참은 몽타주 0초에 피해가 들어간다. 실제 검날 샘플이 움직이는 프레임에서 후속 리본이 만들어지므로 캡처에서 최초 피해와 잔상 구간을 차례로 확인했다. 공격 판정은 3D 오버랩을 사용하며 흰 섬광은 맞은 대상의 캡슐 표면을 기준으로 표시한다.
- 검증 명령 `./Scripts/VerifyMeleeAnimationVfx.ps1 -Label MeleeVfxB -RequireAuthoredSword`. 격리된 720p 실제 게임 세션에서 프로그램 입력 14개 상황, 46장, 실패 0건. 자동화 테스트 2건과 출력 로그 검사를 따로 통과했다. 실물 키보드·마우스와 필드 완주 검증은 이 기록의 범위 밖이다.
- 준비·최초 피해·회복 화면은 기능 증거 폴더에 보관했다. 이 DevLog에는 실제 런타임에서 캡처한 [패왕참 잔상](greatsword-capstone-motion.png) 한 장을 남겼다.
- 키 변경, 콘텐츠 정리, 회피 애니메이션을 포함한 최종 통합본은 `./Scripts/VerifyMeleeAnimationVfx.ps1 -Label MeleeVfxRollIntegrated -LogicOnly -RequireAuthoredSword`로 실행했다. 동일한 StartHub, 실제 공격 입력 경로와 14개 케이스를 NullRHI에서 확인했다. 그래픽 오류와 수치 검증 결과는 [통합 결과](../../Feature/doc/evidence/melee-animation-vfx-20261121/integration-results.txt)에 기록했다.
