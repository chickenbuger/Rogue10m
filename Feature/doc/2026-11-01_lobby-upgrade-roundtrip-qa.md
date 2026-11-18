# 로비 강화 구매·저장·다음 판 UI 검증

Sprint#9-22, 기반3a306c8c, D:\Project\Rogue10m. 이미 사망 정산을 완료한 H/I QA 기록 중 정확히 검토한 I 저장 2개를 새 namespace에 바이트 그대로 복제한 뒤 정상 메뉴와 게임 UI로 검사했다. 사용자 저장과 기존 QA 원본은 변경하지 않았다.

B는 실제 NPC F 대화→Power 카드→강화 버튼→디스크 저장 확인→정상 필드 진입→설정 메뉴 포기 확인→정산→마을 귀환까지 완료했다. 프로그램 Controller.InputKey/Character.DoAim/실제 보이는 UMG delegate를 이용했으며 물리 입력은 하지 않았다. 구매는 실제 로비 재화6→5, Power0→1, 공격력13→15였고 다음 판과 포기 후에도 유지됐다. 런 Gold/Crystals와 Discovery를 별도로 검사했다.

검사27회는 반복 관측을 포함한9개 항목이다. 실패0, OSexit0, startup identity 확인, Error없음, preservation verified. 실행 전344개 저장은 모두 해시가 유지됐고 B의 새 QA2개만 추가됐다. 사진4개를 직접 열어 확인하고1280×720 디코딩/SHA256을 검사했다. 필드·마을 사진에 셰이더 준비/체커보드 재질이 보여 화면 품질의 완료로 해석하지 않는다.

A도 같은 UI 경로27/0을 기록했으나 런처가 시작 직후 비어 있는 Process.Path를 읽어 uncertain을 남겼다. 실제 소유 PID/명령행과 정상 엔진 종료, 원본342개 해시 보존을 별도 reconciliation으로 대조했다. OS 종료코드는 관측하지 못했으므로0으로 만들지 않았으며 원래 uncertain 기록도 보존했다. 이후 런처는 PID를 먼저 기록하고 최대5초 동안 원래 Process 객체의 경로/시각을 관찰하며, 미확인 신원은 kill하거나 recorded 성공으로 바꾸지 않는다.

Source-Proof는 PS7 JSON의 DateTime(Utc) 또는 명시 offset 문자열을 UTC로 비교한다. H/I의 정확한2파일/프로필/버전/해시/종료/보존 계약은 유지했다. PS7 실제 H/I 읽기 전용 검사 통과. Windows PowerShell5.1 추가 호환 검사는 AuthorizationManager 실행 거부로 미실행이며 정책 변경·우회·재시도를 하지 않았다.

코드: Tests/Rogue10mLobbyUpgradeRoundTripRuntimeTest.cpp, Scripts/VerifyLobbyUpgradeRoundTrip.ps1. 새CPP는 -gather 포함22.29초 실제 컴파일 통과. 제품 가격·능력치·보상·저장 스키마를 바꾸지 않았다. 직접 SaveNow/Upgrade/Abandon/Travel이나 stat setters를 쓰지 않고 정상 UI로 처리했다. Source byte identity는 UE 실행 전 복제 시점에만 보장하며 복제본은 정상 게임 저장으로 변한다.

근거: DevLog/LobbyUpgradeRoundTripQA/LobbyRun1101A 및 B, DevLog/20261101_LobbyUpgradeRoundTrip_Review/README.md. raw 개인 저장 목록·GUID/프로세스 메타데이터와 대형 미디어는 Git에서 제외하고 로컬에 보존한다. 보스 승리·20분 시간초과·강제 종료·저장 실패 주입·패키징·수동 플레이는 이번 검증에 포함하지 않았다. push/병합 없음.
