# 마법 후보의 기존 스킬 연결 구현

2026-11-11 UTC. 공유 에디터와 빌드는 부모 담당자가 소유한다. 이 패킷은 기존 work branch에서 새 독립 스크립트와 테스트만 작성하며 에디터·빌드·공유 C++·바이너리 자산을 변경하지 않는다. 검증 가능한 연결 실행 코드를 준비하고 통합 담당자가 같은 코드를 실행하도록 인계한다.

## 현재 확인한 계약

- `URogue10mCombatComponent::ExecuteAttackSkill`은 비용·쿨다운 승인과 몽타주 재생 시도 뒤 `SpawnCastEffect`를 즉시 호출한다. 실제 발사 notify나 몽타주 성공 신호가 아니다.
- `ApplyAttackDamage`는 권한 측에서 유한한 양수 피해가 적용된 뒤 대상·pulse마다 `SpawnImpactEffect`를 호출한다. 새 notify·Niagara 충돌 피해 경로를 추가하지 않는다.
- 기존 DataAsset의 `bEnableAttackEffects`, `CastEffect`, `ChargeEffect`, `ImpactEffect`와 tuning을 사용할 수 있다. Cast는 렌더 skeletal mesh 소켓에 붙고, Staff impact는 대상 bounds 중심이다. 지팡이 StaticMesh tip 부착·정확한 접점은 현재 계약이 아니다.
- MagicMcp01의 네 Niagara와 재질은 저장·Entry 렌더 검수가 끝났다. world-space·FaceCamera·단회 입자이며 손 추적·실제 월드 발사 방향·지면 AoE를 구현한 자산이 아니다.
- 기존 scale·dilation·EmissionDuration은 후보 preview와 다르다. `ConfigureTransientEffect`가 월드 타이머로 즉시 삭제하므로 후보 수명보다 먼저 잘리지 않도록 조정해야 한다.

## 작은 적용 단위

첫 대상은 `/Game/DataAsset/AttackSkill/ClassCombat/Staff/DA_Attack_Staff_Primary01` 한 개다. Cast에는 Telegraph 후보를 **공격 승인 예고**로, Impact에는 DiamondStar 후보를 **양수 피해 확인**으로 연결한다. 기존 ChargeEffect, 몽타주, hit delay, 타수, 피해량, 자원, 슬롯, 프로필 바인딩, 손 소켓, 1인칭 위치·회전·크기 보정은 보존한다.

시각 제안값은 Cast/Impact scale1, Niagara time dilation1, first-person emission multiplier1, Cast 강제 종료 .35초, Impact 강제 종료 .27초다. 후보의 .30/.22초보다 긴 종료 예산이며 게임플레이 시전·판정 시간을 변경하지 않는다. 실제 전투 화면에서 최종 가독성 검수는 아직 필요하다.

Circle·Release를 임의의 Special/Charge 필드에 넣지 않는다. 실제 방출 시점·취소·중복 방지·손 추적 계약을 별도로 정한 후 연결한다. 특히 단회 Circle로 지속 charge나 주문모드를 대신하지 않는다. 나머지 Staff4개·기존 charge·프로필·character DA는 보존 대상이다.

## Ultrawork Packet

| 패킷 | 목표·변경 영역 | 완료 조건·검증 | 롤백 경계 |
| --- | --- | --- | --- |
| A Architect/Reviewer | 현재 runtime event/asset 계약과 위험 확인 | C++·기존 evidence 읽기, 독립 리뷰 | 문서만 |
| B Implementer | 순수 binding spec과 native MCP inspect/apply/verify 코드 | 파일/필드 whitelist, stale snapshot·dirty·부수 변경 방지, 단일 저장 | 새 Scripts와 테스트만 |
| C Builder/Reviewer | 오프라인 계약·실행흐름 테스트 | unittest, AST, CheckGeneratedChanges. UE/build 실행 없음 | 준비 증거만 |
| D Integration owner | 에디터 소유자가 새 live snapshot 후 pilot DA 적용 | compile/readback·전체 필드 보존·scoped save·cold reload·실제 공격/양수피해/실패 검수 | snapshot의 기존 visual 필드만 native MCP로 복구 |
| E Doc Writer | 준비 완료와 실제 연결 미실시 구분 | 결과·한국어 DevLog/Sprint·통합 명령 | 공유 로그는 짧은 append |

## 실행 원칙

기본 명령은 오프라인 준비만 수행한다. live inspect/apply/verify는 명시적 editor lease가 있어야 하며 loopback 공식 MCP만 사용한다. inspect는 최신 schema·프로젝트 AgentSkill·PIE·asset class/dirty·전체 DA properties·candidate compile·profile/default character 참조를 읽는다. apply는 inspect snapshot과 파일 해시가 그대로인 경우에만 visual whitelist를 한 번 쓰고, 전체 필드를 다시 읽어 부수 변경이 없을 때 대상 DA 한 개만 저장한다. 실패나 transport 불명확 결과는 자동 재시도·자동 복구하지 않고 기록한다.

실제 연결 완료 판정은 저장 성공만으로 내리지 않는다. coordinator가 스킬 경로·실제 캐릭터·몽타주·공격 승인·양수 피해·0피해·수명·취소·cold reload를 확인해야 한다. 현재 편집기와 빌드를 동시에 실행하지 않는 제한은 유지한다.
