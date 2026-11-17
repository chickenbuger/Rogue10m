# 활성 무기 VFX 가독성 개선 준비 결과

## 현재 상태
새 gain8 MI/NS 제작 및 두 DA의 제한 적용/검증/rollback 스크립트를 준비했다. 이 문서 작성 시점에는 실제 새 파생 제작·DA 적용·빌드·실제 게임 PNG 판정은 루트의 직렬 실행을 기다리고 있다. 작업자가 UE/MCP/빌드를 실행하지 않았다.

## 파일
- Scripts/Editor/AuthorActiveWeaponReadability20261110.py
- Scripts/Editor/ActivateActiveWeaponReadability20261110.py
- Source/Rogue10m/Tests/Rogue10mWeaponVisualRuntimeTest.cpp
- Feature/architect/2026-11-10_active-weapon-vfx-readability.md

## 정적·오프라인 검증
Python AST 검사 및 Unreal/MCP 없는 stub 테스트를 통과했다. 실제 프로젝트 원본에서 59개 보호 패키지를 읽고 기존30 후보의 원본4 NS 해시가 일치하는지 확인했다. 기존 VFX activation과 대검 motion 기록의 67개 필드 reconciliation, 보호된 damage 변동 차단, 각 DA 네 허용 필드만 변경/63개 동결, 두 scoped save, cold readback, 중복 실행 차단과 rollback 경로를 mock으로 확인했다. 이는 실제 에디터 API 호출이나 저장 성공을 의미하지 않는다.

## 제한
- 선형 RGB 8배이며 체감 밝기나 전체 효과 개선이 검증된 것이 아니다.
- 입력·피해·몬타주·charge·dilation·FP override는 변경하지 않는다. 기본 scale 변경은 FP 배율 결과에도 반영되므로 수치 상한만 확인하며 FP 화면 검수는 대기한다.
- 원본30 후보, Magic8, 기존 gain8 독립 prototype과 모든 원본 자산을 수정하거나 활성화하지 않는다.
- 실제 game effect는 타깃 bounds 기준이며 정확한 표면 충돌 이펙트가 아니다.
- runtime actual age와 관측 frame gate는 같은 프레임 즉시 캡처 문제를 줄이지만 GPU 렌더 픽셀/가림/접촉 타이밍 승인 자체는 아니다.
- 저장이 모호하면 자동 재시도하지 않고 manifest 마지막 단계와 현재 상태를 먼저 확인한다.

## 실행 후 갱신 항목
새 author manifest 경로·8개 실제 자산 해시·compile/stack 결과, activation before/after·cold readback, 빌드 결과, strict34 및 4개 VFX gate, vfx-components.csv, 실제 이미지 판정과 최종 활성/rollback 상태를 이 문서에 추가한다.

## 실제 적용·회귀 결과
- 공식 MCP에서 ActiveReadGain8A의 기존4효과 파생 Niagara4개/MIC4개를 저장·컴파일·readback했다. 원본·30후보·마법을 포함한 보호59개 SHA는 동일했다. 두 기존 스킬만 VFX 참조·cast/impact scale4필드를 변경하고 나머지63필드는 보존했다. 별도 백업과 Editor 필드 롤백 경로가 있다.
- ActiveReadCombatA에서 시전 component가 3인칭인데 비활성 FP 메시와 축소 배율을 쓰는 것을 발견했다. GetEffectAttachMesh에 3인칭일 때 렌더 본체를 반환하는 분기만 추가했다. FP 기능 개발/튜닝이 아니며 기존 FP 경로는 보존한다.
- 빌드22.66초 및 3인칭 경로 수정 빌드15.02초 성공. ActiveReadCombatB: 실제 게임10스킬,34/34PNG, 첫 피해 대검5접촉 proxy, 기존444개 save SHA 보존, exit0. cast 부모=실제 GetMesh, cast/impact 실제3P scale(.25/.36 대검,.20/.30 너클), 개별 component>=2프레임·실제 simulation age 및 첫 impact 확인도 통과했다.
- 네 VFX 캡처를 직접 확인했다. 적중 링은 보이나 시전 효과는 여전히 후방 화면에서 약하다. 캐릭터 가림·대비·짧은 수명/캡처 지연을 더 검수해야 하므로 최종 시각 승인은 보류한다. 이 두 연결 개선은 미연결 30후보의 완성/활성화를 의미하지 않으며 연속 ribbon도 없다.
- 근거: evidence/sword-knuckle-20261110/active-readability/ActiveReadGain8A 및 ActiveReadCombatB. 물리 입력/풀런/패키지 QA는 하지 않았다.
