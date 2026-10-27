# R02/R03 조건 데이터·공통 평가 기반 결과

`RequiredAll`을 획득 경로 바깥의 공통 AND gate로 추가했다. 기존 선행 그룹 내부 AND/그룹 간 OR, MasteryAlternatives 행 간 OR, 포인트/숙련 선택 경로는 유지한다. 기존 노드의 새 배열 기본값은 빈 배열이며 기존 enum 값과 순서는 바꾸지 않았다.

## 지원과 연결 상태

| 항목 | 이번 구현 범위 |
|---|---|
| 무기 숙련 조건 | `MasteryAtLeast` + `GreatSword`/`Brawler`. 실제 숙련 snapshot에서 평가 입력을 구성한다. |
| 계열 조건 | `DisciplineRankAtLeast` + `Discipline.Mage`. 명시적 C++ 평가 입력으로 지원하며 live adapter는 기본0만 공급한다. |
| 공통 결과 | 유효성, 필수 조건/부족 목록, 선행 그룹, 포인트·숙련·자동 경로 가능 여부. |
| UI 연결 | 기존 widget이 호출하는 노드 상태·요구조건 텍스트·구매 가능 API가 공통 결과를 사용한다. 새로운 기술 노드나 UI 화면은 배포하지 않았다. |
| 실제 획득 | 구매 명령과 자동 해금에 공통 AND 적용. 포인트/골드가 충분해도 우회 불가. 후보 snapshot의 위조 경로도 거부한다. |
| 미지원 | 다른 무기/계열 ID, 알 수 없는 유형. 음수/중복 조건·카탈로그 숙련 상한 초과를 오류로 처리한다. 새 조건의 중첩 OR는 추가하지 않았다. |
| 미연결 | MageRank 획득·변경·영구 저장, 두 신규 검술의 실제 카탈로그/효과/장착, 퀘스트·보상·랭크 이벤트. Mana identity를 MageRank로 환산하지 않는다. |

새 구조는 성장 수명·정산·검술 상호배타·전투 중 교체 정책과 분리했다. 아래 사용자 확정 사항은 요구사항으로 기록하며 정책 구현은 포함하지 않는다. R05와 저장 버전/필드, 생산 맵, Content/Config는 변경하지 않았다.

### 최신 사용자 확정 방향

**20분 초과 처리**는 사용자 답변으로 확정됐다. 상세설계의 숙련 영구 성장 추천은 폐기한다. 런 중 숙련/능력치와 로비 영구 강화를 분리하고, 20분을 넘으면 게임 오버와 함께 해당 런에서 오른 숙련/능력치를 초기화하며 로비용 재화를 정산한다. 로비 재화로 구매한 영구 강화 수치는 보존한다. 게임 마무리 후 재화로 영구 강화한다.

이는 문서 요구사항으로만 기록했다. 이번 코드에 시간초과 초기화·신규 재화 정산·영구 성장 저장을 구현하지 않았다. 기존 snapshot을 읽는 adapter는 호환성용이며 신규 런 성장의 영구성을 약속하지 않는다. 사망·승리·장비·기술 해금의 지속성과 재화 산정 세부값은 미확정으로 남긴다. 기존 실제 저장은 삭제/초기화/이행하지 않았다.

## 검증 결과

- UE5.8.3 CL58210709 Editor 빌드 성공. 첫 컴파일은 테스트 보조 함수 `Internal`의 엔진 namespace 충돌로 실패했으며 이름 수정 후 8.04초 성공. 임시 서비스 fixture 수정 후 증분 빌드 8.21초 성공. 최신 규칙에 맞춘 수명 중립성 주석까지 포함한 최종 빌드 41.67초 성공.
- [새 조건 자동화](evidence/progression-requirements-20261026/ProgressionRequirements_servicefixed/report/index.json): 4/4 Success, errors=0, warnings=0.
- [R11 자원 결제 회귀](evidence/progression-requirements-20261026/AttackResources_r02final/report/index.json): 4/4 Success, errors=0, warnings=0.
- [기존 숙련 회귀](evidence/progression-requirements-20261026/HybridSkillTree_r02final/report/index.json): 3/3 Success, errors=0, warnings=0.
- [원본 보존](evidence/progression-requirements-20261026/original-preservation.json): `D:/Project/Rogue10m` SHA e32e5abe672dd9554b3f51e90b672f6039077d29, Git clean, 기존 저장98개 해시 변경0.

검사 범위는 Sword M2/M3 × Brawler M2/M3 × Mage0/1/2의 12행, 부족 목록, 빈 필수/선행 배열, 잘못된 ID/유형/음수/중복/상한, 기존 숙련 OR와 선행 그룹, 실제 마을 서비스 내 구매 실패/성공, 자동 해금 멱등성, snapshot 위조 거부다. 100000 골드와 충분한 포인트도 AND를 우회하지 못했다. 새 골드 획득 경로를 만든 것은 아니다.

최초 새 테스트 실행은 3건 성공/서비스 fixture 1건 실패였다. 엔진 `GetMapName()`이 World 이름 대신 패키지 이름을 사용하여 마을 gate가 닫혔다. `/Temp/Rogue10mRequirementsQA/StartHub` 임시 패키지로 수정하고 실제 service gate 진단을 추가했으며, 생산 StartHub 맵은 로드/저장하지 않았다. [초기 실패 보고서](evidence/progression-requirements-20261026/ProgressionRequirements_first/report/index.json)를 보존한다.

## 실제 공격 통합과 한계

구체적 기본 Blueprint Character, PlayerState, GAS, GameState, NPC를 갖춘 격리 월드에서 실제 구매와 자동 해금 후 `ExecuteAttackSkill`을 실행했다. 공격 수락 성공, 스태미나 100→90, 사용 횟수1을 확인했다. 테스트 command line에는 기존 QA 슬롯 override를 사용하지만, fixture에는 GameInstance가 없어서 실제 QA 프로필 파일을 로드하거나 저장하지 않았다. 복제 SaveGames 파일 수는0이다.

NullRHI 검사이며 렌더링 플레이·실제 입력·적중/피해 pulse·신규 검술 효과·20분 런·cook/Shipping·성능은 검증하지 않았다. UI의 backend 연결을 시각적 화면 검증으로 표현하지 않는다.

## 재현과 롤백

`Scripts/VerifyAttackResourceCosts.ps1 -Suite ProgressionRequirements -RunLabel <새이름> -EvidencePacket progression-requirements-20261026`로 재실행한다. 같은 옵션의 Suite를 AttackResources/HybridSkillTree로 바꾸면 회귀 검사를 실행한다. 기존 증거 경로는 덮어쓰지 않는다.

작업 경로는 독립 물리 clone `C:/Users/PC/Documents/Codex/2026-10-27/task/Rogue10m-r11`, 브랜치는 `Sprint#9-3-progression-requirements`, 기반은 R11 로컬 커밋 e91eb711eb0c1fff204b7af4eb60baf82f9187a9이다. 롤백은 이 packet만 되돌리면 된다. 원본/실제 저장을 reset·삭제할 이유가 없다. 자동 병합/push/PR/생산 맵 변경은 하지 않는다.


## 추가 사용자 확정 방향 — 조합별 호환성과 보스

검술 변형 전체를 한 슬롯만 활성화한다는 이전 추천은 폐기한다. 한 캐릭터가 여러 무기를 배우는 것은 허용한다. 조합별 호환성으로 표현해야 하며 마나+내공 동시 사용은 불가, 내공+투지는 허용되는 예시다. 자원 보유·동시 축적·전환 시 잔량 처리는 미정이다. RequiredAll 조건 평가와 호환성/장착 정책을 결합하지 않는다.

이번 새 타입에는 단일 장착 슬롯/상호배타 채널/자원 전환 정책을 추가하지 않았다. 기존 legacy 아이덴티티 선택·자원 동작은 호환 기준선으로 남아 있으며, 이를 새 조합 호환성 시스템으로 구현 완료했다고 주장하지 않는다. 새 호환성 데이터와 전투 효과 연결은 별도 packet이다.

보스는 시작 직행 상태의 승리가 현실적으로 매우 어렵지만 불가능하지 않으며 여러 기믹/기술 대응을 요구한다. 이번 packet에서 보스 밸런스/AI/생산 맵을 수정하거나 직행 승리 가능성을 플레이 검증하지 않았다.


### 사용자 답변6 — 전투 중 무기·사전 세팅 전환
무기별 사전 스킬 세팅은 비전투 중 편집하고 전투 중 무기 교체 시 연결된 세팅으로 전환한다. 교체는 무적을 부여하지 않는다. 전투 중 교체 전면 금지 추천은 적용하지 않는다. 교체 시간/공격 취소/쿨다운 공유/자원 잔량 세부는 미정이다. 이번 조건 기반 코드에는 구현하지 않았다. 최신 확정/미정 규칙의 통합본은 `Docs/RapidFieldRules.md`를 따른다.
