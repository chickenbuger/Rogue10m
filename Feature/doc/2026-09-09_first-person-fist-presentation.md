# 1인칭 가드·주먹 공격과 주변 HUD

- 작업: Sprint#4-22-first-person-fist-presentation
- 엔진: 설치된 Unreal Engine 5.8.2
- 상태: 구현·빌드·Editor 적용·실게임·독립 시각 검증 완료
- 요청: 첨부 레퍼런스의 양손이 보이는 기본 시야, 주먹 공격, 경험치와 상대 몬스터 정보 배치.

## 구현

Unarmed/Knuckle의 소유자 1인칭 표현을 전용 ActorComponent와 native AnimInstance로 분리했다. 카메라는 애니메이션의 head 소켓 대신 캡슐에 붙어 조준 방향을 유지한다. 기존 Manny 메시의 몸통은 카메라 뒤·아래에 두고 양팔 관절만 TwoBoneIK로 가드 위치에 올린다.

MM_Idle과 DefaultSlot의 실제 공격 몽타주를 평가하고, 몽타주에서 얻은 손의 전진량을 가드와 전방 신전 목표 사이에 매핑한다. 손가락과 손목은 보유한 MM_Attack_01의 닫힌 주먹 포즈를 사용한다. 게임플레이용 별도 메시, 피해 수치, 판정 시점, 입력 및 몽타주 자산을 변경하지 않는다.

다른 무기에서는 원래 카메라·메시 부모와 소켓, 상대 변환, AnimClass·AnimationMode, FOV·투영 스케일·가시성을 복원한다. 소유자만 적용하며 컴포넌트 Tick이나 새 상시 타이머는 없다. 포즈는 기존 skeletal evaluation에서 계산하고 뼈 이름 검색은 bone container가 바뀔 때만 수행한다.

## UI 배치

- 하단 전투 코어: 기존 1200×208, 아래56, 균등 축소 유지.
- 경험치: 좌우24·바닥16 논리 픽셀 여백, 금속 프레임 높이10, 내부 녹색 게이지4.
- 몬스터: 상단 중앙560×68, 위24. 이름 아래 뾰족한 금속 체력 프레임, 붉은 게이지532×12.
- 실제 카메라 대상 탐지와 PlayerState/Attribute 수치를 표시한다.
- 실제 행동5개와 현재 잠금 아이템4개를 유지한다.

## 변경 파일

- Source/Rogue10m/Components/Rogue10mFirstPersonPresentationComponent.h/.cpp
- Source/Rogue10m/Character/Rogue10mFistAnimInstance.h/.cpp
- Source/Rogue10m/Character/Rogue10mCharacter.h/.cpp
- Source/Rogue10m/Components/Rogue10mCombatComponent.cpp
- Source/Rogue10m/Rogue10m.Build.cs: AnimationCore 의존성
- Source/Rogue10m/UI/Widgets/Rogue10mMainHUDWidget.cpp
- Source/Rogue10m/Tests/Rogue10mFirstPersonFistRuntimeTest.cpp
- Source/Rogue10m/Tests/Rogue10mResponsiveHUDRuntimeTest.cpp
- Scripts/Editor/ApplyReferenceMetalHUD.py, ValidateReferenceMetalHUD.py, ApplyFirstPersonHUDFraming.py

Unreal Editor API로 아래 Widget Blueprint4개만 저장했다. 적용 전 전체 Widget 해시와 비교하여 다른 Widget 변화가 없음을 확인했다.

- Content/Widget/WBP_Rogue10mMainHUD.uasset
- Content/Widget/Parts/WBP_BottomHUD.uasset
- Content/Widget/Parts/ReferenceMetal/WBP_ReferenceMetalExperienceLine.uasset
- Content/Widget/Parts/ReferenceMetal/WBP_ReferenceMetalMonsterInfo.uasset

## 검증

프로젝트 파일 생성 및 UE Editor 빌드 성공. 독립 소스 리뷰에서 빈 IK 배열의 충돌 경로와 매 프레임 뼈 이름 할당을 수정하고 재검토했다. Editor 자산 검사는 RESULT=FIRST_PERSON_HUD_FRAMING_PASSED assets=4.

최종 실제 영상과 7조건 HUD 결과는 아래 검증 기록에 보관했다. 처음 시도한 전체 메시 이동은 팔이 보이지 않거나 몸통이 화면을 가려 폐기했고, 그 실패 캡처를 최종 결과로 사용하지 않는다.

## 아트 범위

현재 프로젝트가 보유한 Manny 메시와 테스트 맵을 사용한다. 첨부 그림의 실사 갑옷·폐허 배경을 신규 제작한 결과는 아니다. 게임 안에서 움직이는 포즈와 화면 배치를 구현한 작업이다.

기존 미커밋 변경은 보존했으며 커밋·푸시는 하지 않았다.


## 최종 검증 결과

- 최종 UE Editor 증분 빌드 성공:63.03초. 프로젝트 파일 생성 완료.
- RESULT=FIST_PRESENTATION_PASSED frames=120 failures=0.
- 실제 기본 공격 요청3회/몽타주 재생3회, 공격 중 몽타주54프레임, 손 이동 최대18.4cm.
- 기본 손 좌표 camera-local:(38,±20,-10). 기본 카메라/머리 애니메이션 분리 및 공격 후 복귀 확인.
- Knuckle→Staff→Knuckle에서 원래 부모·소켓·변환·AnimClass·모드·FOV·투영 스케일 복원, 재진입8프레임 후 가드 확인.
- RESULT=REFERENCE_METAL_HUD_PASSED cases=7 failures=0.
- 1280×720/1920×1080/2560×1440/3440×1440/800×900 UI125%/1080p 복귀/1024×768.
- 실제 카메라 Pawn trace→몬스터 View→Widget의 이름·체력 연결, 72/100→35/100 변화 확인.
- XP42%의 실제 Attribute→ProgressBar 연결, 좌우24/바닥16/외곽10/내부4 검증.
- 독립 검증2가 좁은 창/4:3에서 상단 이름·타이머 비겹침, XP여백, 코어·슬롯 잘림 없음 확인.
- 독립 검증1이 최종 공격/복귀 프레임에서 주먹 이동과 손목·전완 연결, 중앙 적/HUD 판독 확인.
- Python AST3개, 새 소스6개 공백, git diff --check, Harness 경로 검사 통과.

최종 가드 목표:(±20,43,148), 신전 목표:(±10,64,148), 원본 전진량 정규화32cm. 손목의 비틀림을 막기 위해 전완축 회전을 lowerarm과 hand에 함께 적용했다. 현재 프로필의 우클릭은 Data Asset 미지정으로 잠겨 있으므로 프리뷰는 실제 사용 가능한 LMB3회로 촬영했다.

### 실제 플레이 자료

- images/first-person-fist-20260909/fist-gameplay.gif:120 실제 렌더 프레임,5초,24fps 시뮬레이션.
- guard.png / punch.png / recovery.png:실제 기본자세·공격·복귀 원본1280×720.
- hud-narrow-125.png / hud-4x3.png:실제 비율 검증 캡처.
- evidence/first-person-fist-20260909/:빌드·런타임·Editor 적용 결과.

캡처의 적 정지·위치와 경험치42%는 검증용 프로세스에서만 설정했다. 원래 열려 있던 Editor는 미저장 패키지가 없음을 확인한 후 정상 종료했고, 빌드·촬영 후 같은 L_Menu로 다시 열었다.
