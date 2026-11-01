# 로비 강화 실제 UI 왕복 검증

`LobbyRun1101A`: 정상 메뉴에서 검증된 QA 프로필 복제본을 선택하고 NPC F 대화→공격 특성 구매→필드 진입→설정 메뉴 포기 확인→결과 화면 마을 귀환을 프로그램 입력으로 실행했습니다. 물리 키보드/마우스나 사람의 전체 플레이는 하지 않았습니다.

- 로비 재화 6→5, Power 특성 0→1, 공격력 13→15. 디스크의 저장 snapshot, 다음 판, 포기 후 마을에서 강화 유지와 Discovery 보존을 검사했습니다.
- 검사 27회(중복 관측을 포함한 9개 항목), 실패 0. 기존 342개 저장 해시 유지, 새로운 격리 QA 저장 2개만 추가됐습니다.
- 최초 런처는 시작 직후 비어 있는 Process.Path를 읽어 uncertain을 기록했습니다. 원래 세션 기록을 그대로 보존했고, 별도 reconciliation-after-exit.json에 실제 프로세스/종료/해시를 대조했습니다. OS 종료 코드는 수집하지 못했으므로 0으로 단정하지 않습니다. 엔진은 exit0 요청과 정상 종료를 기록했습니다.
- PNG4개는 직접 열어 확인하고 1280×720 디코딩 및 SHA256을 확인했습니다. 마을 복귀 화면의 셰이더 준비/바둑판 재질은 해결된 것으로 표시하지 않았습니다.
- 변경 브랜치: Sprint#9-22-gear-ui-input-qa, 기반 커밋 3a306c8cfdf68bef5c79ac64acae13e623ec7e9b. QA CPP는 22.29초 빌드 통과. 이후 시작 경로의 5초 제한 관찰 보정은 런처만 바꾸며 실행 결과는 별도 기록합니다.
- 재현: Scripts/VerifyLobbyUpgradeRoundTrip.ps1 -Label <새 ASCII 라벨> -SourceLabel BasicRun1101I. 종료된 H/I QA 자료의 고정 해시, 새 namespace, 기존 저장 보존 가드가 일치해야 실행합니다. 사용자 프로필을 대신 넣지 않습니다.

원본 근거: DevLog/LobbyUpgradeRoundTripQA/LobbyRun1101A. 이번 검증은 보스 승리, 20분 완주, 저장 실패 강제 주입, 패키징 또는 수동 플레이의 통과를 뜻하지 않습니다. 원격 push/병합은 하지 않았습니다.


B 추가 확인: 시작 경로를 제한 시간 안에 관찰하는 런처 보정 후 새 namespace에서 같은 UI 왕복을 재검증했습니다. IdentityConfirmed=true, Error=null, OS 종료 코드0, recorded/preservation verified, 검사27회(9개 항목) 실패0. B 시작 전344개 저장 해시 모두 유지, QA2개만 추가되어346개가 됐습니다. A의 실패/보정 자료는 삭제하거나 성공 세션으로 덮어쓰지 않았습니다. B 사진4개도 직접 열어 확인했고 같은 셰이더 준비 표시를 보존했습니다.
