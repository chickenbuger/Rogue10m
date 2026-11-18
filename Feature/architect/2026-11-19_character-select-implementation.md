# 캐릭터 선택창 시안 실제 적용

- 작업: `Sprint#9-35-character-select-ui`, 기준 `084865cd`.
- 요청: 승인된 `SourceArt/UI/CharacterLobby/2026-11-19/character-select-v1.png` 시안을 실제 게임에 적용한다.
- 범위: 기존 `WBP_CharacterLobby`의 화면 구성, C++ 선택 상태/장식/빈 슬롯 입력, 기존 공용 종료 버튼과의 정렬, 관련 검사·검증 기록. 캐릭터 모델·카탈로그·세이브 형식·전투·맵은 유지한다.
- 구현 원칙: 시안 이미지를 화면 전체에 붙이지 않고, 배경/실제 프리뷰/실제 프로필 문구/입력 위젯을 분리해 작동하는 UI로 구성한다. 기존 배경과 카탈로그를 재사용하고 금속 테두리·장식은 UMG/Slate로 표현한다. 새 외부 아트 반입·구매는 없다.

## ULW 작업 패킷

### L1 — 화면 배치

- 목표: 기존 성채 테라스와 3슬롯을 유지하며 남색·금색 헤더/이름표/주요 버튼/빈 슬롯을 시안에 맞춘다.
- 수정: `Scripts/Editor/ConfigureCharacterSelectionStage.py`, 대응 Validator, `/Game/Widget/Lobby/WBP_CharacterLobby`.
- 완료: 기존 생성 화면 바인딩과 투명 프리뷰 보존, 실제 자산 저장·검사.
- 검증: 명령줄 Editor Python에서 선택창 Validator와 외형/흐름 검사.
- 롤백: 선택창 단일 자산의 사전 물리 사본과 기존 authoring script.

### L2 — 런타임 상태

- 목표: 저장된 이름·종족/성별, 금색 선택 강조·선택됨 표시, 빈 슬롯 생성, 액션 호버/비활성 상태를 실제 데이터와 연결한다.
- 수정: `Rogue10mCharacterLobbyWidget`, 필요 시 공용 QuitMenu의 로비용 표현.
- 완료: 선택·생성 취소·삭제 확인·접속 경로가 기존 계약을 유지한다. Tick 추가 없이 이벤트 갱신한다.
- 검증: Editor 타깃 정규 빌드와 렌더된 로비 QA, 실제 화면 검토.
- 롤백: 해당 C++ 변경만 되돌린다. 저장 API/형식은 바꾸지 않는다.

### L3 — 검증과 게시

- 목표: 실제 세이브를 보존하면서 UI 상태와 게임 진입을 확인하고 기능별 체크포인트를 게시한다.
- 수정: 전용 opt-in Editor QA driver, 최종 증거·결과 문서·DevLog·Sprint 이력.
- 완료: 신선한 QA 전용 세 저장 이름을 사전 검증, 실사용 세이브 전후 SHA 비교, 대표 화면 검토, 신규 Output Log 오류 조사 완료.
- 검증: `Scripts/BuildEditor.ps1`, 개별 자산 Validator, 전용 렌더 QA, `Scripts/CheckGeneratedChanges.ps1`, 범위 diff 검사.
- 롤백: QA namespace는 실사용 저장과 분리한다. 기존 WIP·다른 프로세스·미저장 맵을 건드리지 않는다.

## 실행/보존 경계

시작 시 별도 맵 복구 Python commandlet만 관찰됐고 공식 MCP 8001 연결은 거절됐다. 캐시나 미연결 상태를 실제 에디터로 판단하지 않는다. 위젯 저작은 별도 명령줄 Editor에서 실행하며, 시각 검증은 별도 렌더 게임 세션에서 한다. 다른 프로세스를 종료하거나 활성 맵을 저장하지 않는다.

각 Editor/빌드/검증은 실제 `-AbsLog`와 줄/UTC 경계를 사용한다. 기존 `Content/AI/Monster/BT_Monster.uasset`, `DevLog/20261118.txt`와 미추적 타 작업을 보존한다. 바이너리 직접 편집과 전체 dirty 패키지 저장은 금지하고 선택창 자산만 저장한다. 맵 제작 게이트에 따라 작업 브랜치만 게시하며 develop/test/main 승격은 보류한다.
