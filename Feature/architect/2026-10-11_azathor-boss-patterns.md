# 아자토르 보스 전용 패턴

브랜치: `Sprint#5-23-azathor-boss-patterns`. 사용자 지시: 스튜디오 없이 직접 개발(2026-10-11). 다른 채팅이 Studio 제거를 담당하므로 Studio와 제거 대상 실행기를 수정하거나 재개하지 않는다. 기존 미커밋 게임 변경을 보존한다.

## 목표와 보존 경계

OpenRunMap의 `AbyssOverlordAzathor`에 회피 가능한 정면 공격, 시작 시 위치가 고정되는 범위 공격, 체력 50% 이하 강화 단계를 추가한다. 기본 공격과 Beholder 소유 애니메이션, HP10000·기본 피해70·EXP3000·이동240cm/s, 단일 보스 배치와 지역 스포너는 유지한다. 다른 5종 몬스터와 플레이어/NPC/HUD/보행/카메라를 변경하지 않는다. 보스 처치→런 승리와 영구 저장은 제외한다.

## Architect 결정

기존 `URogue10mMonsterAttackPatternComponent`와 프로필을 확장한다. 보스 패턴은 프로필의 명시적 opt-in으로 켜며 기본값은 false다. 기존 기본 공격 프로필은 그대로 둔다. 아자토르 전용 프로필을 새로 생성하고 현재 아자토르 Data Asset 한 곳의 참조만 교체한다.

- 정면 파동: 시작 시 잠근 방향의 직사각형 경로를 예고한다. 예고 후 경로 안의 살아 있는 플레이어를 한 번 타격한다. 방향은 예고 중 추적하지 않는다.
- 심연 영역: 시작 시 대상 위치를 잠그고 원형 경고를 투사한다. 예고 후 해당 영역 안에서만 피해가 발생하며 영역은 대상을 따라 움직이지 않는다.
- 강화: HP 비율 50% 이하에서 진입, 남은 전투 동안 유지한다. 특수 공격의 예고·회복·쿨타임과 피해 배율을 노출 설정으로 조정한다. 기본 능력치는 바꾸지 않는다.
- 기본→정면→기본→영역 순환과 패턴별 쿨타임을 사용한다. 다음 실제 사용 가능한 공격의 교전 거리를 반환해 AI가 기본 공격 거리 밖에서 멈춰 서지 않게 한다.
- 전용 Data Asset의 material과 런타임 `UDecalComponent`로 게임 화면에 경고를 표시한다. 디버그 Draw에 의존하지 않는다. 타이머를 사용하고 Tick은 비활성 유지한다.
- 타격 시 유효 대상·거리·방향·높이·가림·중복을 검사한다. 타깃 상실/사망/Cancel/프로필 교체/EndPlay 시 상태·경고·타이머를 정리한다.

## Ultrawork Packets와 역할

1. 런타임 구현: `Source/Rogue10m/Enemy/Rogue10mMonsterAttackPatternProfile.h`, `Rogue10mMonsterAttackPatternComponent.h/.cpp`. 기본 경로의 행동·수치를 보존하고 특수 선택/타격/단계/가시 경고를 구현한다. 완료 조건은 빌드와 실제 런타임 검증이다. 롤백 경계는 이번 편집 델타이며 기존 전체 파일을 되돌리지 않는다.
2. 에셋 연결·QA: `Scripts/Editor/ApplyAzathorBossPatterns.py`, `ValidateAzathorBossPatterns.py`, `InspectAzathorBossPatterns.py`, 필요 시 별도 native Automation Test. 실제 Editor API로 전용 프로필과 경고 material만 생성하고 아자토르 DA 참조를 연결한다. 기존 콘텐츠 SHA를 기준으로 변경 경계를 검사한다. 저장·fresh load·2특수 타격/회피/가림/높이/단계/쿨타임/취소/사망/EndPlay/기본 공격을 검사한다. 에디터 조작은 root만 순차 실행한다.
3. Builder: `Scripts/BuildEditor.ps1 -NoHotReload` 실제 빌드. 새 reflected 스키마는 에디터 재시작 후 확인한다. 빌드 실패는 해당 변경의 컴파일 문제만 수정한다.
4. Reviewer: 실제 변경과 QA 증거를 독립 검토하고 문제를 수정한다.
5. Doc Writer: `Feature/doc/2026-10-11_azathor-boss-patterns.md`, 한국어 DevLog와 SprintChangeLog에 최종 상태·검증·한계를 기록한다. 다른 채팅의 공통 문서 편집이 끝난 것을 확인한 뒤 추가한다.

## 검증·Exit Gate

에디터 초기 연결과 PIE/dirty 상태를 읽기 전용으로 확인한다. 실제 저장 전후 콘텐츠 해시와 아자토르 연결을 비교한다. 빌드 성공 후 실제 PIE/native 피해를 검증하고 화면 증거를 확보한다. 새 에디터 재로드에서 보스 프로필·기본 수치와 다른 5종 기본 프로필 보존을 확인한다. 테스트 미실행·실패를 완료로 표시하지 않으며 전체 런/패키징/네트워킹/최종 밸런스는 이번 집중 검증의 보장 범위가 아니다. 커밋·푸시는 하지 않는다.
