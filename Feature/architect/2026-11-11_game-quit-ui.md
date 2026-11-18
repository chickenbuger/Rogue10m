# 게임 종료 UI

- 목표: 메인 메뉴·캐릭터 선택·설정(Esc/O/F10)·탐험 결과에서 명시적으로 앱을 종료한다.
- 기존 메인 메뉴 QuitGame 즉시 호출을 공통 확인창으로 연결한다. 취소에 기본 초점을 두고 Esc/Backspace/게임패드 B로 취소한다.
- 런 중단·재화·발견·강화 정책은 변경하지 않는다. 정상 v5 진입 기록이 있는 미완료 탐험은 다음 StartHub 진입 때 기존 코드가 초기화한다. 입장 비용은 환불하지 않는다.
- 필드는 메뉴를 열어도 시간과 전투가 진행된다. 안전 Hub 일시정지 계약을 그대로 사용한다.
- 부착된 캐릭터는 종료 확인 시 SaveNow 성공 후에만 종료한다. 실패하면 확인창에서 재시도·취소한다. 미저장 terminal 정산은 결과 화면 재시도 경로를 안내한다.
- 월드·pawn·선택 프로필·부착 프로필·런 ID를 확인 시 고정하고 최종 제출 시 재검사한다. 런 phase 변경·패널 닫기·EndPlay는 컨텍스트를 해제한다.
- 구현: 기존 Controller에 좁은 연결부를 추가하고 새 ControllerQuit.cpp/UMG widget으로 분리한다. 자산 변경은 없다.
- 동시 작업 보호: Source 기준 별도 작업 브랜치, D:/R10/QuitUI1111A/Rogue10m 물리 사본에서 빌드/검증. 원본 에디터/빌드는 종료하지 않는다. 기존 ArchiveE는 보존한다.
- 검증: Editor/Game Win64 빌드, 실제 UMG delegate·Slate 입력을 쓰는 격리 런타임 fixture, readonly QA 저장 파일 실패 주입, 실제 프로세스 종료와 별도 재실행 복구, 오프스크린 PNG 확인, Windows 새 archive.
- 롤백 범위: 이번 공통 quit widget/Controller 연결부와 메인 메뉴 quit handler만 되돌린다. 기존 저장/런/정산 구현은 건드리지 않는다.
