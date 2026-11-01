# 정상 성장 후 보스 승리와 마을 귀환

2026-11-01 08:34:13–08:53:04 UTC, D:\Project\Rogue10m, Sprint#9-23-normal-growth-boss-qa. 실행 당시 HEAD37aa0056 및 미커밋 CPP49F6A089/header164DB301을 실제 컴파일12.66초로 빌드했다.

BasicRun1101M은 정상 메뉴 → NPC F 보급품 → 필드 → 실제 대검 획득/우클릭 장착 → 자연 LV3/대검M3 → 마나 입문1P 구매/활성 → 실제 보스 처치 → 승리 결과 저장 → 마을 귀환을 프로그램 입력으로 수행했다. HP/XP/위치/시간/보스수치/AI/승리를 강제로 바꾸지 않았다. 사람의 직접 완주나 난이도 공정성 인증은 아니다.

스킬 화면은 실제 마나 입문과 잔여1P를, 승리 화면은 경험치3000/골드300 보스 보상 및 로비포인트+24/저장완료를 표시한다. 세번째 사진은 귀환이다. 사진3장 모두 원본1280x720 PNG를 편집 없이 복사하고 디코딩/SHA/직접 시각 검토를 확인했다. 귀환 사진의 shader/texture 준비/sky 경고는 남아 있다.

기존 저장352개 SHA 불변, 새QA2개만 추가돼354개다. 보존 에셋31개와 맵도 불변이다. 이번 확인의 초기화 범위는 선택한 런 값/학습/특정검GUID/Discovery이며 모든 legacy기술이나 인벤토리 전도메인을 검증했다고 확대하지 않는다. 귀환 후 스킬아이콘 차이는 소스 검토 중이다. 같은프로필 다음field진입은 M에서 하지 않았다.

재현은 Scripts/VerifyBasicRunPlay.ps1에서 새Label과 -PreparedBossStrategy -OrbitBossStrategy -GreatSwordGrowth -DodgeObserve -AnimationEvidence -GearUI -Visible 사용. 기존Label 재사용/사용자세이브 초기화 금지, 전역UI단독제어 인계를 먼저 확인한다. 실패한 L은 별도폴더와37aa0056에 보존했다.
