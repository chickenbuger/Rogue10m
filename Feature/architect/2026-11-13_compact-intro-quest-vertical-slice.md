# Compact 후보 초급 의뢰 수직 연결

정상 공유 프로젝트의 `/Game/Rogue10m/Maps/Production/WorldLayoutCompact20261113A`에만 초급 의뢰를 additive로 연결한다. 기존 맵은 1800m이며 이 패킷은 지형 축소가 아니다. 거절된 `root-startup-approval-A.json` 생성과 metadata-only commandlet/Temp clone QA는 대상이 아니며 계속 중단한다. 최신 사용자 지시로 이번 작업의 Computer Use는 허용됐다.

| Packet | 목표와 수정 범위 | 완료 조건·검증 | 복구 경계 |
| --- | --- | --- | --- |
| Scope | 외부 Codex·브랜치·dirty·소유 프로세스, 입력·저장 사전 해시 | 공유 UE/빌드 충돌 없음, NPC Native51B DLL 확인 | 타 프로세스 종료·reset·사용자 변경 되돌리기 없음 |
| Backup | 후보 umap와 해당 OFPA/OFO 전체 물리 checkpoint | 파일 수·SHA 일치, Content 링크 없음 | 원본 WorldLayout20261112 및 다른 맵 보존 |
| Inventory | 정상 공유 Editor의 현재 후보·Director·DA·PlayerStart·GameMode 읽기 | exact map identity·clean packages·실제 배치 확인 | Temp clone 및 승인 의존 QA 실행 없음 |
| Author | 새 프로젝트 소유 Intro DA와 Director 연결 | NPC 1명·목표 2개·골드 40·전투/비급 보상 없음, 실제 지형에 맞는 위치, native validation 통과 | 신규 DA와 후보 신규 actor만 수정; vendor/CDO/기존 actor 보존 |
| Persist | 후보 범위만 저장, 소유 Editor 종료 후 cold reload | DA·Director·참조·좌표·비대상 해시 보존 | unrelated dirty 발견 시 저장 중단, 자동 rollback 없음 |
| Runtime | 별도 QA namespace의 정상 Run과 실제 F·회수·보고·재방문·새 Run | 진행 2/2, 보상 1회와 중복 방지, RunId 변경 시 초기화; 실제 입력/프로그램 검증 구분 | 보호 세이브 전후 검사; 파괴적 프로필 조작 없음 |
| Review/Doc | 실제 수행·실패·미검증 구분 | 결과 문서·한국어 DevLog·Sprint 갱신 | 기존 E5/strict/B/B2 봉인 고정, commit/push 없음 |

최신 제작 기준은 큰 거점 3개·전문 초소 5개, 선택 의뢰와 우회·재방문, 1200초 런, 직행 60초/성장 900초 목표다. 중상급 책은 탐험 발견이며 초급 예제에 비급 보상을 추가하지 않는다. 3인칭·피격 시 스킬 취소·면역 보류를 유지한다.

콘텐츠 참조: `Docs/WorldMapProductionGuide.md`, `Docs/References/WorldMap/README.md`, 배치/획득처 JSON과 concept PNG, `Docs/VillageQuestAuthoringGuide.md`, 격리본 `IntroVillageQuestIntegrationGuide.md`. H 낙엽 수렵촌의 실제 월드 앵커와 지면은 live inventory에서 확인한다. 기존 사용자 제공 Arin 및 로컬 프로젝트 소유 TutorialData, Engine 기본 marker를 먼저 조사하며 vendor 원본/새 Fab import·구매는 하지 않는다. 기존 로컬 자산과 Fab 현재 계정 소유 확인을 구분한다.

Director는 runtime NPC/목표를 생성하고 DataAsset에서 위치·외형·반경을 편집한다. 수동 NPC/목표를 중복 배치하지 않는다. 배치 방식은 실제 기존 Director 유무와 클래스 API를 확인한 뒤 결정한다. 이 수집 예제는 처치 귀속 버그의 실제 combat/loot 검증을 대신하지 않는다.


## 보존 검사 Reviewer 복귀

전체 입력 검사에서 예상 추가분 비교가 두 번 중단돼 반복 실행을 멈추고 좁은 인벤토리를 검토했다. 첫 구현은 authoring의 bytes와 inventory의 size 필드명을 직접 비교했고, 정규화 후에는 실제 Editor import가 만든 Scripts/Editor/__pycache__/CreateIntroVillageQuestDefinition20261113.cpython-311.pyc 1개가 추가로 확인됐다. author_intro.py:40-41의 정상 import와 생성 시점, pyc header의 원본 크기/수정시간을 대조했다. 이 캐시는 삭제·수정하지 않고 정확한 SHA로 보존하며, 최종 입력은 신규 콘텐츠5개와 생성 캐시1개를 명시한다. 미확인 파일을 무시하는 광범위 예외는 넣지 않는다. 관련 generated-cache-owner-review.json·unexpected-addition-review.json을 기록하고 최종 전체 대조를 재개한다.
