# 선택 프로필 possession 초기화 독립 게시 계획

2026-11-17 · Sprint#13-1-profile-initialization. 남은 기능을 검증한 뒤 각각 commit·push하라는 사용자 지시에 따라, 초기 스폰의 표시 이름과 선택 프로필 연결 수정을 별도 기능으로 분리한다.

## Scope Gate

정상 초기 진입이 선택 프로필의 Pawn class만 결정한 뒤 PlayerState 이름을 적용하지 않아 기본 이름이 남던 문제를 고친다. `ARogue10mCharacter::PossessedBy`에서 `Super::PossessedBy`가 PlayerState를 연결한 직후 선택 프로필을 적용한다. progression 부착과 기존 능력·표현·loadout 초기화 순서는 이후 그대로 진행한다.

대상은 Character.cpp의 include1행과 초기화11행, 기존 native fixture h/cpp, 본 계획·결과 문서다. Character.h·전투·카메라·NPC·지형·Build.cs·Config·Content 변경은 가져오지 않는다. 준비한 Character.cpp는 main `88d0bef2930262673a0a270146de9ee106a0ab5d` 원문을 사용했고, 적용 직전 publication worktree의 Character.cpp가 그 원문과 일치함을 확인했다. 소스는 `tmp/profile-publication-plan/`에서 세 파일만 옮겼다.

## Ultrawork Packets

| 패킷 | 목표·영역 | 완료 조건·검증 | 복구 경계 |
| --- | --- | --- | --- |
| P1 Architect/Implementer | main 기반 Character의 최소 초기화와 fixture만 분리 | 정확12행 추가, fixture2개는 원래 검증본과 SHA 일치, 기존 API 의존성 확인 | Character 최소 패치와 신규 fixture2개 |
| P2 Builder | 정규 Editor/UHT와 실제 native 회귀 | `BuildEditor.ps1`, `Rogue10m.CharacterProfile.PossessionInitialization` 1개 전부 성공, 실제 `-AbsLog`의 신규 Error/Fatal/ensure 조사 | 소유 빌드·headless PID·새 증거 경로 |
| P3 Reviewer | 생명주기·선택 없음·저장 격리·teardown 검토 | 운영 SaveGame 접근0, 원래 명령행·platform file 복원, generated/path·diff 통과 | 본 패킷 소스와 fixture만 수정 |
| P4 Doc Writer | 결과·DevLog·Sprint 기록, 기능 commit·push | 작업 브랜치와 develop→test→main에 일반 게시 후 직접 원격 SHA 일치 확인 | 이 기능·후속 완료 문서 |

## 기존 회귀와 격리

기존 회귀1개는 프로필을 선택하지 않은 native Pawn의 기본 identity, 정상 프로필 생성 뒤 실제 virtual Possess에 의한 이름 적용, PlayerState 소유, BeginPlay의 progression GUID, 다른 프로필로 교체, 잘못된 프로필의 기존 거부를 확인한다. 초기화 함수를 테스트에서 직접 대신 호출하지 않는다.

fixture는 isolated unattended game-thread에서만 실행하며 기존 Game/PIE의 GameInstance가 있으면 거부한다. 정확한 임시 슬롯과 새 profile GUID의 progression 슬롯만 memory platform-file overlay에 허용한다. 다른 SaveGames 경로는 실패로 막고, 정상 SaveGame serialization도 메모리에서만 수행한다. cleanup은 world와 subsystem 종료를 포함하고 원래 platform file·명령행 복원까지 확인한다. 운영 저장을 읽거나 복사하지 않는다.

## 경계

이 범위는 native possession/BeginPlay 회귀다. 물리 입력·전체 메뉴 UI·허브 포털·지형 보행·게임 완주·패키지 실행을 인증하지 않는다. 11/16 전체 누적 작업 트리의 기존 성공은 원문 이력으로 보존하며, 이번 독립 main 기반 빌드·회귀 결과가 있어야 게시한다.
