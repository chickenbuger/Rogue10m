# Rogue10m 기본 플레이 개발 기록

2026-11-01 실제 UE 게임 캡처 6장. **프로그램 자동 입력 QA 기록**이며 사람의 수동 플레이나 영상 기록이 아니다. 원본 PNG의 프레이밍·게임 UI·셰이더 경고를 그대로 보존했다.

| 순서 | 원본 파일 | 확인한 장면과 검증 범위 |
| --- | --- | --- |
| 1 | [대검 장착](Originals/01_greatsword_equipped.png) | 필드 고정 대검을 실제 overlap으로 획득한 뒤 인벤토리의 우클릭 경로로 장착. 실제 장착 아이템과 GreatSword 기본 공격 binding을 QA가 확인했다. |
| 2 | [자연 성장](Originals/02_natural_growth_conditions.png) | 일반 몬스터 전투·처치 XP로 LV3/대검 숙련 M3에 도달한 상태. 공용 성장 2P와 학습 전 상태를 보여 준다. |
| 3 | [마나 입문 학습](Originals/03_mana_entry_learned.png) | 기존 `I.Mana.Entry`를 스킬트리 UI에서 1P로 학습해 공용 성장 2→1P, 실제 투자 0→1P와 마나 활성 상태를 확인했다. 독립 MageRank 2나 신규 복합 검술 구현을 뜻하지 않는다. |
| 4 | [보스 접근](Originals/04_boss_approach.png) | 정상 도보 경로로 실제 배치된 최종 보스에게 접근. 이 사진은 전투 시작 장면이며 승리 증거는 다음 결과 화면과 실행 기록에 있다. |
| 5 | [승리 정산](Originals/05_victory_settlement.png) | 실제 보스 처치 뒤 토벌 성공·저장 완료 UI. 보스 보상 XP +3,000/골드 +300, 이번 판 로비 포인트 +24와 마을 귀환 버튼을 표시한다. |
| 6 | [마을 귀환](Originals/06_returned_hub.png) | 결과 UI로 돌아온 LV1/너클 상태. 선택한 마나 입문 초기화와 공개 정보 유지·귀환을 QA가 확인했다. 다음 판 전체 완주나 모든 legacy 기술·슬롯 초기화를 검증한 결과는 아니다. |

`I.Mana.Entry`는 현재 공용 성장 1P로 학습하는 기존 기능이다. 사진의 LV3/M3는 이번 자동 플레이의 성장 상태이며, 마나 입문의 필수 숙련 조건으로 해석하지 않는다.

## 빌드와 실행 근거

- 엔진: **UE 5.8.3-58210709 / Development**, 엔진 branch `++UE5+Release-5.8`.
- 프로젝트 실행 branch: `Sprint#9-23-normal-growth-boss-qa`.
- 실행 당시 HEAD: `37aa005615b601c398e661a6943b2efb63cdd8de`, 아래 실행 소스는 당시 WIP였다.
- 실행 CPP SHA256: `49f6a0894e5bd4e2a46f7158553e994e1059880d29edab76fd89f382d64e4d15`.
- 실행 header SHA256: `164db30109c3e04e5e54655209661a28111d57b1acbf1d679f6f8fdd402be605`.
- 이후 같은 실행 소스와 결과를 보존한 로컬 커밋: `e5ec170389cf5af52eee1d61b402c6b364f22037`.

원시 세션에는 프로젝트 Git 정보가 기록되지 않았다. 위 프로젝트 정보는 별도의 검증 체크포인트 `BASIC_RUN_M_VERIFIED_1101.json`과 부모의 로컬 확인을 근거로 보완했다. 엔진 branch와 프로젝트 branch를 구분한다.

QA 입력은 Controller InputKey/DoAim, 실제 Slate/UMG 상호작용 경로를 사용했다. 실행 기록은 `returned_victory`, `full_boss_completion=1`, `no_gameplay_fixtures=1`, `physical_input=not_performed`다. K 실행은 시간초과, L 실행은 사망으로 끝났으며 이 성공 장면에 섞지 않았다.

## 원본 무결성

모두 1280×720 RGBA PNG이며 6장 전체 디코딩과 복사본의 바이트 동일성을 확인했다. 최종 시각 검수 상태: `PASS_Root_view_image_original_6_images_2026-11-01` — 개인 계정·프로필/세이브 목록이 포함되지 않은 후보들이다.

| 파일 | SHA256 |
| --- | --- |
| `01_greatsword_equipped.png` | `db51d355b6828a4d98994625f52aae7bbd9e67404f8c935bcb66b5df712352d5` |
| `02_natural_growth_conditions.png` | `269012531c31c7b578e55138559913a85476d6afd74ed400a5017ac9be6b113e` |
| `03_mana_entry_learned.png` | `719e22e40754c4294ddd41f0db505e8acae0b5e13adb8480e532db3a476499b2` |
| `04_boss_approach.png` | `1adb10873275fdf69408826b640503137f0a02261991fb944c87045dad578a9e` |
| `05_victory_settlement.png` | `ad4e0299d9823f718cc8c7693e72d734ba5d8740a9323988d93840953450be96` |
| `06_returned_hub.png` | `b9fabbaa44d62af4cf2cd26bb87a782adca8bcbd87c8411290d84eb0eb98c0f5` |

개발 빌드의 팔/몸 프레이밍, 재질·셰이더 경고와 UI 과제는 남아 있다. 이 기록은 시각 품질 완성, 새 애니메이션 제작, 물리 입력 검증, 다음 판 전체 완료를 주장하지 않는다. 이미지 편집·재생성이나 영상 합성은 수행하지 않았다.
