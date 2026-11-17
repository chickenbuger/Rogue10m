# G08 + VFX 누적 통합 제안

공유 프로젝트를 변경하지 않고 만든 물리 독립 사본이다. 적용할 파일은 `combined-g08-vfx-full-20261113.patch` 하나이며, SHA256은 `7175e6f0e54250f79720f0864d883c2213513c38ddcc95612f54880a30ac035b`이다. 원래 G08/VFX 패킷을 이 패치와 중복 적용하지 않는다.

- G08 full8 → VFX full23 순서의 check/실제 적용이 모두 성공했다. 수동 충돌 해결은 없었다.
- 변경은 기존 5개와 신규 24개, 총 29개다. 별도 보존 helper 4개까지 `combined-manifest.json`에 33개 경로의 원본/결과 raw SHA를 기록했다.
- 새 `CombinedReplay` 사본에 단일 누적 패치를 실제 적용한 뒤 33개 raw SHA가 모두 일치했다. 원래 두 패킷 각각과 누적 패치의 reverse check도 성공했다.
- 현재 공유 preimage와 새 경로 부재를 재확인했다. 공유 Source/Config/Content/Git, UE/Editor/UBT/Cook/게임, commit/push 변경은 없다. 독립 Git 저장소만 Sprint 작업 branch로 초기화했으며 commit은 만들지 않았다.

## 보존 확인

독립 VFX reviewer가 공통 두 파일을 검토했다. CombatComponent.cpp는 G08 후보에 VFX include 1개와 `SpawnCastEffect`/`SpawnImpactEffect` 함수만 합친 내용과 정확히 같다. 두 함수는 VFX 원본 후보와 같다. AttackSkillData.h는 G08 후보에 VFX 속성 4개 블록만 추가한 내용과 같다. G08 전용 파일의 정규화 내용과 VFX 전용 파일의 raw SHA도 원래 후보와 일치한다.

G08의 contact/recovery/rate/aim 수락 snapshot, 실행 ID/arm generation/ordinal 검사, 실제 양수 AppliedDamage 이후 credit, 피격·Stop·몽타주 교체 시 잔여 접촉 취소를 유지한다. VFX는 표시 함수만 수정하며 첫 cast는 기존 수락 경로, impact는 기존 양수 AppliedDamage 경로에서 발생한다. 면역과 production profile/growth/save를 추가·활성화하지 않는다.

## 기존 pin과 readiness 결과

원래 `original-vfx-full-manifest.json`과 checker는 수정하지 않았다. 원래 VFX manifest를 누적 사본에 검사하면 **already 25 / conflict 2, blocked(exit 2)**이다. G08을 추가한 CombatComponent.cpp와 AttackSkillData.h의 postimage가 달라진 결과이며, 통과로 바꾸지 않았다. 원래 pin과 새 pin은 combined manifest에 함께 남아 있다.

새 combined manifest의 검사 결과는 baseline에서 add24/update5/already4, 누적 사본에서 already33이다. 이는 파일 배치 readiness이며 UE/런타임 승인과 다르다. 실제 통합 후 G08 필드 2개도 보호하는 새 전체 reflected snapshot과 새 VFX inspect/capture lineage를 생성해야 한다. 이전 activation/inspection 보고서를 재사용하면 안 된다.

Windows `core.autocrlf=true`로 적용하면서 G08 신규 LF 파일 4개가 CRLF로 바뀌었다. 정규화 내용은 동일하며 manifest에 실제 raw SHA와 이 차이를 기록했다. VFX 전용 raw pin은 모두 일치한다.

## Root 적용 순서

1. 원본 preimage/신규 경로 부재와 패치 SHA를 다시 검사한다.
2. 원본 작업 branch에서 `git apply --check <이 패치>` 후 이 단일 패치를 적용한다. 후보 Source 전체 복사는 하지 않는다.
3. UHT/Editor 빌드, 소유 Editor 재시작, G08 contact23+parser1 및 VFX placement/Staff 회귀를 수행한다.
4. G08 authoring plan을 확인한 뒤 실제 측정 contacts/rate/recovery로 별도 후보를 만들고 cold reload한다. VFX도 새 inspect와 보호필드 snapshot을 사용한다.
5. 실제 3인칭 ready/접촉/회복, 저FPS·hitch, 피격·Stop·동일 asset 재생 교체, VFX 가림/위치/수명, 물리 입력과 풀런을 확인한다.

G08 회복 마감보다 1µs 넘게 늦은 pending contact는 버린다. 마지막 접촉과 회복 끝을 같게 잡으면 프레임 지연으로 타격이 빠질 수 있으므로 실제 재생 검증이 필요하다. 애니메이션 포즈/무기 궤적의 실제 접촉 이벤트를 측정한 구현은 아니다.

독립 portable 검사는 G08 MSVC C++17 /W4 /WX 24시나리오·230검사, VFX unittest 53테스트, Python 15파일 AST 검사가 통과했다. VFX 테스트는 fake-MCP/합성 fixture이며 UE 검증이 아니다. `portable-validation.json`과 로그를 확인한다. UE 컴파일·실행·렌더·실제 asset 생성·활성화는 하지 않았다. 실패했던 첫 준비 사본 `run-20261113-a`와 portable 실행 환경 오류도 삭제하지 않고 기록했다.
