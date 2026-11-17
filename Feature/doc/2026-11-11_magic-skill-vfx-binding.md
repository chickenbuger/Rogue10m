# Staff Primary01 VFX 연결·실제 검증

## 현재 상태
실제 DA 한 개에8개 표현 필드를 저장했고 cold readback·실제 전투3케이스를 통과했다. 11장 실제 사진에서 예고/적중 효과가 충분히 뚜렷하지 않아 시각 완료는 보류한다. 나머지 Staff4개·Circle/Release/Charge는 추가 연결하지 않았다.

## 실제 구현
- /Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01의 Cast=Telegraph, Impact=DiamondStar, 두 scale1·공통dilation1, emission .35/.27초를 공식 MCP로 연결했다. enable을 포함한 허용8필드 외의 전체 reflected 값은 보호했다.
- 최초 actual Primary=primary 직렬화 차이를 strict alias로 교정했다. 원래 FirstPersonEmissionDurationMultiplier0.5는 FP 전용이므로 setter에서 제외하고 snapshot에서 보호했다.36개 오프라인 테스트가 통과했다.
- 원본 DA 바이트백업과 전후SHA, 나머지 Staff/프로필/캐릭터/후보자산·비시각필드 보호를 확인했다. source피해/입력/애니메이션 타이밍·Charge·FP전용튜닝은 바꾸지 않았다.

## 검증 결과
- inspectC→applyA→same-session verifyA 성공. Editor PID41608 dirty0 정상종료 후 새 PID41280에서 pinned cold verifyB 성공, 이후 다시 정상종료했다.
- 통합 Editor 빌드 성공28.15초. 첫 실패는 fixture 지역변수 C4456 한 건으로, 테스트 변수명만 교정했고 실패로그를 보존했다. 신규 맵 클래스도 함께 컴파일했다.
- StaffVfxRuntime1111A: native 실제 프로필 초기해금·기본 바인딩·native Staff 모델·기존 montage 그대로 실제 Primary3회 요청. 정상타깃 양수HP감소·Impact, 무적/100m빗나감은 피해0·Impact0을 통과했다.
- 실제 Niagara 컨트롤러 age,3인칭 parent/scale, 연속 montage/손이동,11개 실제 callback PNG와 저장/보호파일SHA 보존을 확인했다. 정확한 첫HP감소는 접촉순간이 아니며 HitStartDelay0을 변경하지 않았다.
-11장 전부 직접 판독했다. 현재 기본3인칭에서는 몸·무기·타깃 가림과 이른 캡처의 blur 때문에 cue가 뚜렷하지 않다. active/age 검사를 픽셀 시인성 승인으로 바꾸지 않았다.

## 의미와 한계
Cast는 비용·쿨다운 승인 후 montage 재생 시도 직후의 cue이며 실제 방출 notify/재생 성공을 뜻하지 않는다. Impact는 권한 측 양수 피해 뒤 대상 bounds 중심에 생성된다. 후보는 world-space·FaceCamera 일회 Sprite라 방출된 입자가 손을 계속 따르는 마법진이 아니다. 정밀 접점/연속 리본은 아니다.

다음은 기본3인칭에서의 대비/가림·시점별 적정 표본 개선, 메뉴/교체/사망 잔여효과와 물리입력·풀런 검수다. 이 작업에서1인칭 신규개발과 공개 commit/push는 하지 않았다. 우선순위에 따라 맵 통합검증을 이어간다.

## 근거
- Feature/doc/evidence/magic-skill-vfx-binding-20261111/{MagicEditor1111A,MagicEditor1111B,MagicStaffInspect1111C,MagicStaffApply1111A,MagicStaffCold1111B,UnifiedBuild1111A,PreserveFirstPerson1111A}.
- Feature/doc/evidence/staff-vfx-runtime-20261111/StaffVfxRuntime1111A/{metrics.txt,frames.csv,vfx-components.csv,launcher.json,visual-review.json}; images/StaffVfxRuntime1111A.
