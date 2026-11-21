# Rogue10m Content 기능별 정리

언리얼 콘텐츠 브라우저에서 공유 컬렉션 **Rogue10m Content**를 펼치면 기능별 목록 17개를 바로 열 수 있다. 동적 컬렉션이 지정된 경로를 검색하므로 같은 기능 폴더에 새로 추가한 에셋도 표시된다.

| 컬렉션 | 찾아볼 수 있는 에셋 |
| --- | --- |
| 01 Maps | `StartHub`, `OpenRunMap` |
| 02 World and Props | 지형, 동굴, 길, 마을 제작물과 공용 프롭 |
| 03 Characters and NPCs | 플레이어, 커스터마이징, 아린과 NPC |
| 04 Animation and Rigs | 프로젝트 애니메이션, 몽타주, IK·리타깃 |
| 05 Monsters and AI | 몬스터 Blueprint, 프로필, AI, 보스 |
| 06 Combat and Skills | 공격·회피 스킬, 캐릭터 스킬 프로필, 스킬트리 |
| 07 Items and Equipment | 무기, 아이템 데이터·아이콘, 보상 상자, 전리품 가방 |
| 08 UI Widgets | HUD, 메뉴, 로비, 인벤토리와 개별 위젯 |
| 09 UI Art and HUD | HUD 그래픽, 로비 배경, UI 재질과 스킬 아이콘 |
| 10 Input | Enhanced Input 액션·컨텍스트, 터치 입력 |
| 11 Visual Effects | 게임 VFX와 포탈 원본 팩 |
| 12 Audio | UI 효과음 팩 |
| 13–16 Art and Animation Packs | 캐릭터·애니메이션·몬스터·환경 원본 팩 |
| 17 Data and Tuning | 여러 기능에 걸친 Data Asset 모아보기 |

월드 파티션의 `__ExternalActors__`와 `__ExternalObjects__`는 소유 맵에서 편집한다. UI PNG 37개, UI 오디오 원본 WAV 38개, HUD 생성 프롬프트 1개는 각 기능 폴더에서 파일 탐색기로 열 수 있다. Data Asset 폴더의 `.gitkeep` 10개는 현재 디렉터리 안내용이다.

## 실제 경로 변경

| 에셋 | 새 패키지 경로 | 수량 |
| --- | --- | ---: |
| 시작 장비와 물약 아이콘 | `/Game/Rogue10m/Items/Textures/Starter` | 6 |
| 캐릭터 로비 배경 | `/Game/Rogue10m/UI/Textures/CharacterLobby` | 1 |
| 전리품 가방과 던전 보상 상자 | `/Game/Rogue10m/World/Loot`, `/Game/Rogue10m/World/Rewards` | 2 |

Unreal AssetTools로 이동하고 아이템 Data Asset 7개와 로비·메인 메뉴 Widget Blueprint 2개의 참조를 다시 저장했다. 경로를 사용하던 생성·검증 스크립트 8개도 갱신했다. SaveGame은 아이템과 공격 스킬의 패키지 경로를 저장하므로 해당 에셋은 기존 패키지 경로에서 로드된다. 제작에 재사용하는 원본 팩도 원래 패키지 경로에서 로드된다. 두 경우 모두 공유 컬렉션에서 기능별로 검색할 수 있다.

## 레거시 제거

| 삭제 대상 | 수량 | 근거 |
| --- | ---: | --- |
| ThirdPerson 외부 액터·객체 패키지 | 47 | 소유 템플릿 맵이 이미 제거됨 |
| 슈터 템플릿의 권총·소총·유탄 발사기 | 27 | 유지되는 게임 에셋이나 코드에서 사용 경로가 없음 |
| Canvas HUD Blueprint | 1 | 실제 HUD는 `WBP_Rogue10mMainHUD`를 사용 |
| 아린 Blender·RefinedV2 이전 외형 | 43 | StartHub의 가이드가 RefinedV3와 Standing 애니메이션을 사용 |
| 합계 | 118 | 모든 대상에서 유지 에셋의 들어오는 레지스트리 참조 0건 |

삭제한 경로와 기준 해시는 [삭제 목록](evidence/content-organization-20261121/removals.csv)에 있다. 이전 아린 외형 제작·검증이나 `ReviewReferenceArinLOD.py`의 `baseline` 비교를 재현할 때는 정리 전 Git 리비전 `e0d05a92`를 사용한다. 소스 FBX·BLEND와 텍스처는 `Art/NPC/Arin`의 제작 자료에 있다.

초기 Unreal 삭제 검사에서 Canvas HUD의 파일이 남은 것을 확인했다. 두 번째 단일 에셋 삭제 시도에서도 파일 해시가 같았다. 기준 감사에서 참조가 없음을 다시 확인한 뒤 파일을 독립 복제본의 복구 폴더로 옮기고 새 프로세스에서 확인했다.

## 검증

- 분리한 현재 작업 폴더에서 Unreal 레지스트리 캐시를 건너뛰고 패키지 6,380개를 읽었다. 삭제되거나 이동한 이전 경로를 가리키는 패키지는 0개였다. 이동한 에셋 9개와 해당 에셋을 참조하는 9개를 새 프로세스에서 로드했다.
- Unreal 공유 컬렉션 관리자에서 컬렉션 18개를 읽었다. 외부 액터·객체를 제외한 패키지 경로 4,409개를 기능 분류에 대조했다. 각 경로가 기본 기능 분류 하나에 들어갔으며 Data and Tuning은 추가 조회용이다.
- 월드 파티션 초기화부터 `StartHub`, `OpenRunMap`을 각각 열었다. 두 맵에서 Map Check 오류와 경고가 각각 0건이었다. 결과는 [콜드 검증 기록](evidence/content-organization-20261121/verification.json)에 있다.
- 로그 검사기는 맵을 열 때 생성되는 `LoadErrors: New Page: Loading map` 안내 두 줄을 오류 건수에 포함했다. 해당 줄은 메시지 탭 생성 기록이며 각 맵의 Map Check 결과는 오류 0건이다.
- 게시용 체크아웃에서 UE 5.8 Editor Development 빌드와 콜드 검증을 별도로 실행했다. 첫 빌드는 Windows 페이지 파일 한도로 MSVC PCH 컴파일에서 실패했고, 동시 작업을 2개로 줄인 재시도는 성공했다. 통합 콘텐츠의 패키지 6,444개와 컬렉션 분류 대상 4,425개를 확인했다. [통합 검증 기록](evidence/content-organization-20261121/integration-verification.json)
