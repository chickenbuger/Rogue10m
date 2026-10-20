# 특성 16종·강화 UI 실제 적용

- 사용자 승인: 설계한 특성 16종과 강화 UI를 게임에 적용한다. 인벤토리·스킬·성장은 기존처럼 별도 관리한다.
- 브랜치: Sprint#5-27-trait16-runtime. 기존 미커밋 변경을 보존하고 작업 전 Source 188개 파일을 tmp/trait16-runtime-baseline-20261011/에 복사했다. 해당 baseline을 이번 변경만 검토/되돌리는 기준으로 삼는다.
- 적용 범위: 네이티브 C++ 특성 정의·투자·무료 재분배·16종 효과, 캐릭터별 저장 v1→v2 이전, 아린의 독립 강화창, 실제 이미지 아이콘 atlas 신규 임포트, Editor 빌드·격리 런타임·화면 검증.
- 디자인: 이전 researched-traits-v4/DESIGN.md와 PNG 3장. 현재 Vitality/Power ID와 실제 구매 보너스 보존, 신규 일반 3랭크·불굴 1랭크/3포인트/LV20, 최대 예산 52포인트.

## Ultrawork Packets

| 패킷·역할 | 목표 | 수정 영역 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- | --- |
| R1 Architect/Implementer | Data Asset 기본 16종·가중 예산·강화/재분배 | TownServiceDataAsset, 새 TraitCatalogDataAsset, TownProgressionComponent | 랭크/잠금/비용/기존 보너스 검증·총52 | 네이티브 저장/투자 QA | 이 파일들의 baseline 대비 변경 |
| R2 Implementer | 능력치·조건부 효과를 실제 이벤트에 연결 | 새 TraitRuntimeComponent, Character/Stats, Combat, Monster, Inventory, Regeneration | 16종 모두 영향·이벤트 중복 방지·런 상태 초기화 | 실제 피해/회피/처치/자원/보상 QA | 담당 소스 변경 |
| R3 Implementer | 독립 네이티브 강화 UI와 NPC 진입 | 새 TraitWidget, TutorialNPC, Controller, 신규 UI texture | 16카드·필터·효과 비교·잠금/부족·재분배·닫기 입력 복구 | 720p/1080p offscreen 렌더·입력 검사 | 담당 소스/신규 atlas asset |
| R4 Implementer/Reviewer | 프로필 v1→v2 이전·격리·손상 거절 | SaveGame/ProgressionSubsystem·전용 RuntimeTest | 기존 단계/튜닝·다른 관리영역 보존, invalid 원본 byte 보존 | 독립 Write/Read 프로세스·실패 케이스 | 담당 저장 소스/검증 파일 |
| R5 Builder | 합친 코드 Editor 빌드·통합 QA | 빌드/검증 스크립트·증거 | 실제 컴파일 성공, 정해진 QA 통과 | BuildEditor.ps1·전용/기존 서비스 검사 | 생성 코드 수동 수정 금지 |
| R6 Reviewer/Doc Writer | 독립 리뷰·결과·일지 | Feature/doc·Docs·DevLog | 범위·근거·제한·조작 기록 | CheckGeneratedChanges.ps1·diff 검사 | 신규 문서·추가 로그 |

## 구현 계약

영구 투자 상태는 PlayerState의 TownProgressionComponent에 둔다. 순간 콤보·회피 버프·처치 회복 쿨다운·불굴 횟수·금화 소수 잔여는 Character/런 상태이며 저장하지 않는다. Data Asset 정의는 CDO로 기본 제공해 기존 NPC 바이너리 변경 없이 동작하고, 선택한 게임 데이터로 조정할 수 있게 한다.

체력/공격력의 기존 ID 및 구매 당시 NPC 튜닝 누적 보너스를 보존한다. 새 투자는 rank·실제 사용 포인트·구매 누적 bonus를 저장한다. v1은 검증 후 메모리 이전하며 전체 복원 성공 전에는 디스크를 덮어쓰지 않는다. 미래/손상/초과 예산/중복 ID/NaN/미정의 ID는 거절한다. 재분배는 생존·마을·런 시작 전 조건을 서버에서 검증하고 무료로 실제 사용 포인트를 환급한다. 최대 체력/스태미나 변경은 현재 절대값을 상한으로 제한해 무료 회복을 막는다.

회피 반격은 실제 회피 시작 후 2초 안의 첫 공격 실행에 적용한다. 연격은 같은 적에게 3초 내 서로 다른 공격 실행 3회 적중, 다단/다수 타깃의 반복 적중을 실행 ID로 제한한다. 처치 회복은 직접 처치자에게 공통 2초 재사용, 불굴은 정상 전투 치명상에서만 런당 1회 25% HP 생존이다. 보스 피해는 MidBoss/FinalBoss만 판정한다. XP와 특성 금화 배율은 독립 계산한다.

## 검증 환경

라이브 MCP 연결에서 현재 PIE가 실행 중임을 확인했다. 새 reflected 클래스는 별도 빌드/게임 프로세스로 검증하며 실행 중인 PIE의 오래된 코드를 결과로 간주하지 않는다. MCP 호출은 root만 직렬 수행한다. 일반 프로필 저장 대신 기존 하네스의 제한된 QA 슬롯/프리픽스를 사용한다. 다른 창의 코드·Blueprint·레벨·성장/스킬/인벤토리 메뉴는 필요한 효과 훅 외에는 변경하지 않는다. 커밋·푸시는 사용자 별도 승인 전 수행하지 않는다.

## UI 검사 재설계 게이트

ui02·ui03에서 마지막 행의 카드 내부 좌표 실패가 반복되어 높이를 더 키우는 수정을 멈추고 Reviewer/Architect로 돌아왔다. 실패는 스크롤 밖 마지막 행에 집중되며, UE5.8 Widget.h의 GetCachedGeometry 계약은 미틱·미페인트 및 이전 프레임 좌표가 남을 수 있다고 명시한다. 화면에 표시한 각 행을 안정된 렌더 프레임 후 검사하고 16개 ID의 검사 완료를 확인하는 방식으로 QA를 재설계한다. 프로덕션 UI 기능과 전투·저장 코드는 이 단계에서 추가 변경하지 않는다.

공유 원본 체크아웃은 다른 채팅의 몬스터 작업에서 Sprint#5-28-monster-effects-chase로 전환됐다. 최초 5-27 작업의 baseline을 유지하며 현재 공유 브랜치를 다시 바꾸거나 다른 작업을 되돌리지 않는다.
