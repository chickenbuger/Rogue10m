# 전투 VFX 수정 위치와 작업 원칙

런타임 효과 참조와 수치는 기존 DataAsset·Niagara·Material 편집값을 계속 사용한다. 새 콘텐츠·밸런스 값은 가능한 한 이 데이터에 두고, 스킬별 숫자나 스타일을 C++에 추가하지 않는다. 연결 도구는 이번 검토 대상과 변경 필드 범위를 검사하며, 현재 편집값을 읽어 검증한다.

| 바꾸려는 내용 | 수정 위치 | 확인할 영향 |
| --- | --- | --- |
| 스킬의 Cast/Impact 효과 선택·활성화 | AttackSkillData의 CastEffect/ImpactEffect/bEnableAttackEffects | Charge/legacy 활성화 부수 영향, 현재 프로필·슬롯 유지 |
| 효과 크기·소켓·기존 재생 배속·종료 timer | AttackSkillData의 Visual/Niagara 설정 | 실제3인칭 가림, particle 수명과 game-world timer 관계 |
| 입자 수명·크기·local/world space | 프로젝트 소유 Niagara derivative | 모션 추종, 발광 구간, 반복/종료, 재생 비용 |
| 색·밝기·알파·텍스처 | 프로젝트 소유 Material/MaterialInstance와 renderer | 공유 material이면 다른 효과에도 영향; 개별 스타일은 derivative 사용 |
| selectable 기법 구성 | 기존 프로필·기법 DataAsset/catalog | 담당자와 별도 조율하며 basic/passive/legacy·저장·input 계약 유지 |

## 조정 후 검증

1. 다른 Codex 담당자의 파일·에디터 사용 범위를 확인하고 원본 vendor 자산을 보존한다.
2. 해당 DataAsset/Niagara/MIC를 에디터에서 조정·저장한다. 스킬 피해 시점이나 면역을 VFX에 맞춰 바꾸지 않는다.
3. 수정한 자산의 현재 readback과 manifest/capture SHA를 새로 기록·검토한다. 오래된 스냅샷이나 hash를 재사용하지 않는다.
4. 연결 도구의 fresh inspect로 현재 전체 schema·수명·타이머·참조·프로필을 다시 확인한다. current DataAsset 값은 도구가 임의로 덮어쓰지 않는다.
5. 기본3인칭 실제렌더와 성공/빗나감/피격취소·교체·사망·cold/cook을 검증한다. 숫자·저장 검사가 시각 승인을 대신하지 않는다.

후보 검사에는 임의의 0.5초 수명 상한을 두지 않는다. finite positive 수명과 현재 DataAsset의 배속·종료 timer를 계산한다. local/world-space는 저장·검토한 manifest의 boolean 값을 따른다. bool 타입·유한값·현재 hash·정확한 대상·전체 보호 필드 검사는 유지한다.

재질 참조도 이름 규칙이나 정확4개 목록으로 고정하지 않는다. 저장된 renderer readback에서 프로젝트 Material/MaterialInstance 참조를 수집하고 parent chain까지 class·dirty·hash를 검사한다. 개별 후보에 새 MIC를 지정한 뒤 최신 readback과 검증 자료를 갱신할 수 있다. 현재 parent 추적은 저장된 프로젝트 자산을 대상으로 하며, 전체 material function/texture/engine 의존성 그래프는 인증하지 않는다. 렌더·cook 검사는 별도로 수행한다.

수명 계산의 0.01초 timer 최소값과 0.1 배속 최소값은 기존 런타임 동작을 모델링하는 값이다. 새 효과의 스타일 기본값으로 사용하지 않는다. 입자 system tail과 실제 화면 노출은 별도 렌더 검사가 필요하다.

## 보존 계약

3인칭·피격 시 스킬 취소를 유지하며 면역은 결정 전 추가하지 않는다. 이미 생성된 Cast/Impact는 기존 timer까지 남으므로 즉시 소멸을 가정하지 않는다. Circle/Release/지속Charge의 새 authority와 신규 catalog 활성화는 별도 검토한다. 공유 Source·에셋·설정이나 다른 Codex 변경을 자동 복구·되돌리기하지 않는다.
