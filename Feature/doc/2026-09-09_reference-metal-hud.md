# Sprint#4-21 참조 이미지 기반 금속 HUD

- 요청: 첨부한 고딕 액션 RPG HUD의 느낌을 현재 게임 UI에 적용.
- 브랜치: Sprint#4-21-reference-metal-hud
- 작업 기간: 2026-09-08~2026-09-09
- 엔진: 설치 UE5.8.2, CL56702186
- 상태: 구현·빌드·Editor 적용·최종 실게임 및 독립 시각 검증 완료.

## 적용 내용

검은 금속 프레임, 좌우 체력·자원 바, 큰 중앙 원형 게이지, 아이콘 아래 키 표기를 구성했다. 기존 평면 패널을 대체하고 전투 코어를1200×208 디자인 단위로 확장했다. 하단 여백56·좌우 여백24, 엔진 DPI 상속과 DownOnly 균등 축소는 유지한다.

슬롯은64 정사각형 아이콘 영역과20 높이 키 영역으로 나누고 간격10을 사용한다. 현재 행동5개와 잠긴 아이템4개를 표시한다. 첨부 이미지의 가짜 아이템 수량·쿨다운·층수·보스 이름은 게임 데이터로 복사하지 않았다.

원형 게이지는 실제 아이덴티티 자원을12시부터 시계 방향으로 그린다. 0%·부분 충전·100% 및 자원 없음(-- 표시)에 대응하며 새 Tick을 추가하지 않았다. 일부 아이덴티티 아이콘 경로의 자산이 없는 현재 데이터에도 장식이 비지 않도록, 실제 그림이 없거나 로딩 중이면 기존 돌주먹 문장을 표시한다. 유효한 실제 그림이 준비되면 기본 장식을 숨긴다.

스킬 그림도 실제 SkillIcon을 우선 사용하고, 없는 슬롯에만 기존 StoneFist 그림5개를 표시한다. Gameplay View나 Data Asset의 스킬 정의를 바꾸지 않는 UI 대체 표시다. 잠금·쿨다운은 실제 데이터 그대로이며, 잠긴 아이템을 사용 가능한 소모품처럼 꾸미지 않았다.

상단에는 실제 대상 이름·체력, 우상단 런 타이머, 좌하단 최근 로그2줄, 우측 획득 알림3줄을 유지했다. XP는 화면 아래 전체폭4 디자인 단위다. 첨부 이미지의 던전 환경·장비·카메라·신규 보스는 이번 UI 요청 범위 밖이다.

## 아트와 렌더링

built-in image_gen으로 새 투명 금속 프레임 PNG를 생성했다. 로컬 참조 파일 첨부는 샌드박스 ACL 오류로 실패하여, 이미 확인한 이미지의 특징을 텍스트로 설명하는 신규 생성으로 진행했다. CLI/API fallback은 사용하지 않았다.

원본 출력1920×819 RGBA를 그대로 보존하고, URogue10mAtlasImage가 UPROPERTY UVMin/UVMax를 런타임 브러시에 적용한다. 좌우 날개와 중앙 장식 세 부분을 별도 배치하여 중앙 원형 장식을 비균등 확대하지 않는다. 최종 날개는 중앙 뒤로 겹쳐 접합부의 수직 절단을 가렸다.

- 원본·프로젝트 자산: Content/UI/HUD/ReferenceMetal/T_HUD_ReferenceFrame.png 및 동명.uasset
- 정확한 생성 프롬프트: Content/UI/HUD/ReferenceMetal/generation-prompt.txt
- 기존 아이콘: Content/UI/Icons/StoneFist 및 T_Identity_StoneFist
- 원본 PNG의 픽셀 편집은 하지 않았다. uasset은 UE Editor로만 생성·저장했다.

## 변경 영역

- C++6파일: BottomHUDWidget.h/.cpp, HudWidgetParts.h/.cpp, 신규 AtlasImage.h/.cpp.
- 런타임 검증: Rogue10mResponsiveHUDRuntimeTest.cpp를 현재 HUD 계약으로 갱신, 명령 Rogue10m.TestReferenceMetalHUD.
- Editor 스크립트3개: ImportReferenceMetalHUD.py, ApplyReferenceMetalHUD.py, ValidateReferenceMetalHUD.py.
- 저장 자산: 프레임 Texture2D1개, Widget Blueprint9개. 기존 Main/Bottom2개와 전용 ReferenceMetal 파트7개.
- 전용 IdentityBar 파트는 호환 부품으로 남지만 현재 Bottom 트리에는 바인딩하지 않는다. 중앙 원형 게이지와 중복되지 않는다.

기획1·개발2·검증2 역할을 분리하여 진행했다. 빌드와 Editor 자산 저장은 메인이 직렬 실행했다. 기존 Widget 해시 비교에서 이번에 수정한 기존 자산은 Main/Bottom 두 개뿐이었다.

## 검증과 수정 사항

- UE Editor C++ 빌드 및 실제 Editor 생성기 검증 통과.
- 첫 게임 실행: RESULT=REFERENCE_METAL_HUD_PASSED cases=6 failures=0.
- 1280×720·1920×1080·2560×1440·3440×1440·800×900(UI125%)·1920×1080 복귀에서 실제 geometry와 화면을 검사했다.
- 검사: 중앙정렬, 전체 코어 비율·경계, 슬롯 수/간격/중첩, 스킬 그림, 원형 정사각 비율, live 퍼센트, 마나/스태미나, XP, 타이머, 획득 수량.
- 독립 리뷰에서 발견한 선형 아이덴티티 막대와 직업명 겹침을 제거했다.
- 첫 시각 QA에서 발견한 프레임 절단면과 비어 있는 중앙 문장을 보완했다.
- UE5.8 Python SlateBrush.ImageSize의 타입 차이로 첫 적용이 실패했다. 명시적 Canvas 크기를 이미 사용하므로 불필요한 ImageSize 할당을 제거하여 성공했다.
- git diff --check 및 Harness 경로 검사 통과.

테스트는 실제 Editor 실행 파일의 -game 모드다. 테스트 프로세스에만 런 시작·무기/자원·UI배율을 설정하고 검사 후 종료한다. 패키징·모든 메뉴 조작·접근성 최소 글자 크기까지 검증한 것은 아니다.

## 실행 및 증거

1. Scripts/BuildEditor.ps1 -NoHotReload
2. UE Editor Python에서 ImportReferenceMetalHUD.main(), ApplyReferenceMetalHUD.main() 실행.
3. ValidateReferenceMetalHUD.main()은 저장된 위젯 계약을 재검사한다.
4. 별도 게임 세션에서 Rogue10m.TestReferenceMetalHUD 실행. 창 크기를 바꾸며 캡처 후 해당 게임 프로세스를 종료한다.

- 설계: Feature/architect/2026-09-08_reference-metal-hud.md
- 백업: tmp/reference-metal-hud/backup/
- 커밋·푸시는 하지 않았다.

## 최종 검증 확정

최종 빌드48.87초 성공. RESULT=REFERENCE_METAL_SETUP_PASSED 및 RESULT=REFERENCE_METAL_HUD_PASSED cases=6 failures=0 확인. 1080p·울트라와이드에서는 코어1200×208/문장156×156, 좁은 창 UI125%에서는816.6×141.5/106.2×106.2로 측정됐다. 6조건에서 문장 실제/대체 그림 전환도 정상이며 새 겹침·잘림이 없다.

독립 검증자가 최종1080p·3440×1440·800×900 캡처를 직접 확인하여 접합부·문장·막대 색상·키 표기를 PASS로 판정했다. 대상은 실제 조준 유무에 따라 나타나고 숨겨지는 것을 확인했다.

- 최종 실제 캡처6장: Feature/doc/images/reference-metal-hud-20260909/
- 최종 실행·적용 로그: Feature/doc/evidence/reference-metal-hud-20260909/
- 날개 최종 배치: (0,10,600,188), (600,10,600,188). 중앙 장식(512,0,176,202)이 접합부 위에 그려진다.
