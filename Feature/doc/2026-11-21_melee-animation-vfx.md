# 너클·대검 공격 VFX

너클은 주황·금색 손 잔상, 대검은 청색 검날 리본을 사용한다. 상위 기술은 같은 궤적에 층을 더하고 타격 불꽃의 수를 늘린다. 손 소켓과 화면에 표시되는 검 메시 포즈를 월드 좌표로 샘플링한다. 회복 중 잔상은 짧게 페이드되며, 스킬 교체나 피격 취소 시 새 궤적 샘플을 중단한다.

범위선은 전투 시스템이 쿼리에 사용한 박스·부채꼴·원형 도형의 수평 윤곽을 바닥에 투영한다. 아이덴티티 범위 배율을 포함한다. 반복 타격이 최초 판정의 대상 목록을 재사용하는 경우에는 새 범위선을 그리지 않는다. 섬광은 실제 피해가 들어갔을 때만, 피해 적용 직전에 얻은 손·검날과 대상 캡슐 표면의 근접점에 그린다. 일반 액터는 충돌 바운드를 사용한다. 기존 전투가 오버랩 기반이므로 적중 좌표는 대상의 시각적 접점에 대한 추정이다.

VFX 단계는 해당 기술의 카탈로그 요구 숙련에서 가져온다. 포인트제로 전환한 대검 기술은 기존 세이브 호환용 요구 수치를 사용한다. 일반 공격은 짧고 가볍게, 차지와 특수 공격은 잔상이 한 단계 더 선명하게 표시된다. 개별 스킬의 새 VFX는 AttackSkillData의 `bEnableMeleeReadabilityVfx`에서 조정할 수 있다.

[설계](../architect/2026-11-21_melee-animation-vfx.md) · [ImageGen 초안](evidence/melee-animation-vfx-20261121/melee-vfx-concept.png) · [내장 ImageGen 프롬프트](evidence/melee-animation-vfx-20261121/imagegen-prompt.txt)

## 변경 위치

- `Rogue10mCombatComponent`와 `Rogue10mAttackSkillData`는 실행 단계와 무기·기술 기준을 연결한다.
- `Rogue10mMeleeVfxComponent`는 궤적, 범위, 적중을 그린다. 짧은 VFX가 활성화된 동안에만 Tick하며 충돌을 사용하지 않는다. `Rogue10mMeleeVfxGeometry`는 범위 투영과 타격점 계산을 담당한다.
- `AuthorMeleeVfxMaterial.py`는 프로젝트 재질 `/Game/Rogue10m/VFX/Character/Combat/Melee20261121/M_MeleeReadability`을 에디터에서 만든다. UE의 ProceduralMeshComponent 플러그인을 사용한다.
- `VerifyMeleeAnimationVfx.ps1`, `Rogue10mWeaponVisualRuntimeTest`, `Rogue10mMeleeVfxGeometryTests`는 수치와 렌더를 확인하고 전투 시점별 캡처를 기록한다.

## 검증

렌더 검증 기준은 `2011e0de`다. 격리 저장 슬롯을 사용한 1280×720 StartHub 게임 세션에서 기본기, 중간 기술, 최상위 기술과 두 무기의 범위 밖 공격을 합쳐 14개 케이스를 통과했다. 실제 공격 입력 경로에 프로그램으로 입력했으며 46장의 화면을 캡처했다. 별도 기하 테스트는 2건 모두 통과했다. 720p 게임·기하 검증과 재질 저장 로그에 오류가 없었다.

대검 기본기의 검날 잔상과 너클 기본기의 손 잔상은 짧다. 백열난무와 상위 기술은 타격점을 가리지 않는 범위에서 잔상과 불꽃이 더 보인다. 두 무기의 범위 밖 케이스는 0회 피해와 0회 적중 섬광을 확인했다. 맞은 경우의 섬광 앵커는 테스트 대상 캡슐 표면과 계산상 일치했다. 패왕참은 기존 데이터가 몽타주 시작 프레임에 피해를 적용한다. 그 프레임에 섬광이 표시되고 후속 프레임부터 실제 검날 궤적이 보인다.

[대검 패왕참 실제 화면](../../DevLog/20261121_MeleeAnimationVfx/greatsword-capstone-motion.png) · [너클 무극패왕권 명중](evidence/melee-animation-vfx-20261121/knuckle-capstone-contact.png) · [너클 범위 밖](evidence/melee-animation-vfx-20261121/knuckle-capstone-miss.png)

[수치 결과](evidence/melee-animation-vfx-20261121/verification.json) · [검증 이벤트와 재생 자산](evidence/melee-animation-vfx-20261121/runtime-events.txt) · [에셋 재로딩 12종](evidence/melee-animation-vfx-20261121/asset-readback.json) · [첫 검증과 수정 기록](evidence/melee-animation-vfx-20261121/first-run-notes.md)

최신 전투 키와 콘텐츠 정리를 반영한 `455037c8` 통합 기준에서도 UE 5.8 빌드, NullRHI 전투 14개 케이스와 기하 자동화 2건을 통과했다. 당시 하드웨어 Direct3D 초기화와 WARP의 엔진 셰이더 파이프라인에서 렌더 실행이 중단되어, 화면 검수는 같은 VFX 구현의 720p 렌더를 사용했다. [최종 통합 검증](evidence/melee-animation-vfx-20261121/integration-results.txt)
