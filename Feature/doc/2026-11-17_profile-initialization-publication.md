# 선택 프로필 possession 초기화 독립 게시 결과

2026-11-17 · Sprint#13-1-profile-initialization. **독립 Editor 빌드44.35초·회귀1/1 성공, Error0·Warning15, 독립 리뷰 완료**다.

## 변경

- `Source/Rogue10m/Character/Rogue10mCharacter.cpp`: 기존 main 원문에 profile subsystem include1행과 possession 초기화11행만 추가했다. PlayerState가 연결된 뒤 선택 프로필 이름·외형을 적용하고 progression 부착 전 초기화한다. 선택 프로필이 없으면 기존 경로를 유지한다.
- `Source/Rogue10m/Tests/Rogue10mProfileInitializationTestCharacter.h`: Blueprint·mesh 없는 transient native Pawn fixture다. production PossessedBy를 override하지 않는다.
- `Source/Rogue10m/Tests/Rogue10mProfileInitializationTests.cpp`: 실제 virtual Possess/BeginPlay·이름·GUID·교체·거부와 memory-only 저장 격리를 검사하는 기존 회귀1개다.

원래 누적 작업 트리의 Character 전체를 가져오지 않았다. 적용 전 publication Character의 기존 blob이 준비 기준 main `88d0bef2930262673a0a270146de9ee106a0ab5d`와 같음을 확인했고, 세 파일 적용 후 준비 패킷 SHA256과 일치했다. fixture2개는 원래 검증본 그대로다. Character.h·전투/카메라·NPC·지형·Build.cs·Config·Content는 본 패킷의 수정 대상이 아니다.

## 사전 검토

선택 프로필 조회·적용, native PlayerState identity, progression Attach/Detach·GUID, QA 슬롯 switch와 first-person camera getter는 모두 기존 main의 public API다. 모듈·프로젝트 파일 변경은 필요하지 않다.

테스트의 platform-file overlay는 GameInstance 초기화 전에 설치된다. 새 QA 슬롯·현재 profile GUID의 progression 슬롯만 메모리에 허용하고, 알 수 없는 SaveGames 경로는 실패로 거부한다. 기존 subsystem의 SaveGame 호출은 동기 API이며 정상 serialization을 메모리에서 실제 실행한다. world/subsystem 종료도 overlay가 살아 있는 동안 실행하고 teardown 후 원래 command line·platform file을 복원한다. 기존 Game/PIE가 있는 프로세스와 비unattended 실행은 거부한다. 운영 저장 파일을 테스트 입력으로 사용하지 않는다.

## 검증 요구와 실행 결과

main 기반 정규 Editor/UHT 빌드와 `Rogue10m.CharacterProfile.PossessionInitialization` 회귀1개, 실제 지정 출력 로그 검사, 생성 파일·diff 검사·독립 리뷰를 통과했다. 새 경고15개는 빈 QA 저장 첫 읽기1개와 fixture의 무기/스킬 미설정14개다. 이전 누적 작업 트리의 Warning13은 당시 이력이며 현재 결과로 대신하지 않는다.

범위는 초기 possession의 선택 프로필 적용이다. 전체 메뉴의 물리 입력·실제 허브 여행·성장/전투·지형·패키지 실행 인수를 포함하지 않는다. 최종 빌드·회귀·게시 결과를 아래에 추가한다.

### Sprint#13-1 — 선택 프로필 초기화

선택한 프로필 이름·GUID가 possession 이후 적용되도록 Character의 include1개와11행만 분리했다. fixture2개와 함께 main기준 정규UHT/Editor빌드44.35초·회귀1/1·Error/Fatal/ensure0을 확인했다. 빈 QA 저장 첫읽기와 무기 미설정 경고15개를 보존한다. 정확슬롯 메모리 저장격리로 사용자 세이브는 수정하지 않는다. 관련: Feature/doc/2026-11-17_profile-initialization-publication.md. 상태: 검증완료.
