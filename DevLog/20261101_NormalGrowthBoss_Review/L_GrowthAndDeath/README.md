# 대검 성장 검증과 보스 사망 기록

2026-11-01 UTC. 작업 경로 D:\Project\Rogue10m, Sprint#9-23-normal-growth-boss-qa, 실행 소스의 기준 commit c9cca558 및 CPP SHA A23D08BD. 현재 소스는 이 기능 묶음의 미커밋 QA 코드다.

실제 렌더링된 별도 QA 프로필에서 엔진 입력 드라이버가 정상 메뉴 → NPC F 대화/보급품 → 필드 → 대검 획득/인벤토리 우클릭 장착 → 자연 LV3/대검 M3 → 마나 입문 1P 학습 → 보스전 → 사망 정산 +13 저장 → 마을 복귀를 수행했다. 사람의 물리 입력이나 보스 승리 증거가 아니다. 같은 판 종료 후 다음 필드 진입은 아직 확인하지 않았다.

재현: Scripts/VerifyBasicRunPlay.ps1에서 새로운 QA Label과 -PreparedBossStrategy -OrbitBossStrategy -GreatSwordGrowth -DodgeObserve -AnimationEvidence -GearUI -Visible 사용. 전역 UI 제어를 먼저 조율한다. 기존 Label 재사용 및 사용자 세이브 초기화 금지.

사진 5장은 원본 PNG를 편집 없이 복사했고 1280x720 디코딩과 SHA를 확인했다. 장착/마나 학습/보스 도착/사망 정산/마을 복귀 순서다. 마지막 마을 사진의 shader/texture 준비 및 sky 경고와 하단 몸체 가림은 해결된 것으로 처리하지 않는다. 보스 도착은 전투 승리를 뜻하지 않는다.

자동 검사: Editor 실제 컴파일 12.64초 성공, 스크립트 구문 검사 성공, 결과 판독기 26건 및 메모리 옵션 검사 11건 통과. 위 37건은 텍스트/옵션 검사다. 게임 종료 코드 0도 승리 판정이 아니다. 기존 저장 350개 및 보존 에셋 31개/맵은 불변이고 QA 세이브 2개만 추가됐다.
