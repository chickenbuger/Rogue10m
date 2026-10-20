# 독립 코드 검토

2026-10-20. 별도 Reviewer가 격리 작업 폴더의 결과 UI와 런 종료 코드를 검토했다.

- 종료: `FinishRun`은 Running에서 최초 terminal 전환만 허용한다. 동일 맵에서 완료한 런의 재시작도 거절한다.
- 순서: 보스 기존 XP/골드 지급 → 정산·동기 저장 → 사망 delegate/소멸. 저장 재시도에는 보상 호출이 없다.
- 패배: GameMode의 사망 처리와 Character의 FinishRun 재진입은 dead/terminal 가드로 종료된다.
- 저장: attached Pawn, 선택/연결/SaveData 프로필 일치를 CanRetrySave로 확인한다. 귀환 직전 재저장에 실패하면 이동하지 않는다.
- 입력: 결과 창이 blocking/UIOnly 최우선이다. Enter/Space preview 처리로 버튼의 중복 클릭을 피하고 Escape/F6를 소비한다.
- 피해: 종료 후 플레이어와 몬스터 TakeDamage는 base delegate 호출 전에 거절한다.
- 발견/수정: 배경 Overlay 슬롯의 기본 Left/Top 정렬을 Fill/Fill로 수정했다.
- 발견/수정: 기존 LastStand fixture가 종료한 같은 맵을 재시작하던 검사를 새 terminal 계약 검증과 명시적 run-state reset 검사로 갱신했다. 새 실제 QA에서 맵 이동 후 새 런을 별도로 검증한다.

정적 검토에서 추가 차단 결함은 발견하지 못했다. 실제 빌드·게임 런타임·화면 검증은 별도 결과 파일로 확인해야 한다.

## 원본 통합 후 독립 검토

Source14개 모두 통합 기록의 결과 SHA-256과 일치했다. 정규화한13개는 격리본과 같고 Character의 차이는 기존 병행 스킬트리 include·컴포넌트 생성·숙련 능력치 코드였다. 이번 종료 피해 가드와 사망 연결을 함께 유지했으며 GameMode/Monster/Controller/결과 UI의 선언·초기화·입력 연결 누락은 없었다. 이 검토는 읽기 전용으로 수행했고 원본의 미완성 스킬트리 runtime 검증으로 확대하지 않는다.
