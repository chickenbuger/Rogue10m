# 보스 처치부터 마을 복귀까지

## Scope Gate

- 작업 브랜치: `Sprint#5-39-boss-run-completion`. main 병합 전 생성한 작업이므로 Sprint5를 유지한다.
- 공유 원본에서 스킬트리와 체크포인트 작업이 진행 중이므로 `tmp/boss-run-completion-worktree`의 별도 Git 작업 폴더에서 구현·빌드한다. 처음에는 Content를 읽기용 연결로 사용했으나 Write/Read 사이 공유 에셋 변경을 실제 감지했다. 이후 연결을 해제하고 체크포인트 `207c595`의 Content 전체를 별도로 체크아웃해 고정한다. 원본 콘텐츠와 바이너리는 편집하거나 저장하지 않는다.
- 범위: 최종 보스의 실제 사망 → 승리 판정 → 기존 XP/골드 정산과 진행 저장 → 결과 UI → StartHub 복귀. 일반 사망/시간 초과 패배도 같은 종료 UI로 연결한다.
- 기존 보스 처치 XP/골드를 사용한다. 새로운 클리어 보너스·아이템·SaveGame 스키마·스킬트리 경제·원본 에셋은 추가/변경하지 않는다.
- 콘텐츠 선택: 현재 OpenRunMap의 아자토르와 기존 보상 Data Asset, StartHub를 그대로 사용한다. 새 애니메이션/VFX/외부 팩 반입 없음.

## Ultrawork Packets

|패킷|목표와 수정 위치|완료 조건|검증|되돌릴 경계|
|---|---|---|---|---|
|R1 Architect/Implementer|GameState 종료 가드, GameMode 소유 RunCompletionComponent, Monster/Character 사망 연결|Running에서 최초 종료만 허용, 테스트 보스 제외, 사망 후 중복 보상/판정 없음|Editor 빌드 및 실제 런타임 경계 검사|새 컴포넌트와 관련 C++ 연결|
|R2 Implementer|네이티브 결과 UMG와 Controller 입력 연결|승리/패배·보상·저장 상태, 저장 실패 재시도, 저장 성공 후 옵션 없는 StartHub 이동|렌더된 게임 화면·실제 버튼/키 입력|새 위젯과 Controller 연결|
|R3 Builder/Reviewer|실제 맵/보스/프로필의 격리 QA|포탈 출발·보스 사망·정산·중복 방지·오류 재시도·귀환·별도 프로세스 복원|전용 런타임 검증 스크립트, 기존 마을/저장 회귀, 독립 코드 리뷰|전용 QA 코드/스크립트/증거|
|R4 Doc Writer/Exit Gate|결과·Sprint·한국어 DevLog 및 통합|범위/실제 검증/한계 기록, 원격 재확인 후 작업→develop→test→main 통합·푸시|CheckGeneratedChanges, scoped diff, 빌드/검증 결과|기능 커밋 단위|

## 계약

- GameState는 Running에서만 terminal 상태로 전환한다. 타임아웃·사망·보스 처치가 겹치면 먼저 확정한 결과를 보존한다.
- 최종 보스는 MonsterRank::FinalBoss이며 CombatTestInstance가 아니어야 한다. 단일 플레이 권한·유효한 실제 플레이어·살아있는 Pawn과 활성 런을 확인한다.
- 일반 몬스터 사망의 기존 보상을 먼저 한 번 지급하고 최종 보스 사망을 알린다. UI 재시도/귀환은 SaveNow만 실행하며 보상 코드를 재호출하지 않는다.
- 저장 실패/선택 프로필 불일치/유효하지 않은 복원에서는 귀환하지 않는다. 결과 화면에 오류와 재시도를 표시한다.
- 결과 화면을 입력 차단 창으로 취급하고 terminal 이후 피해를 차단한다. 마을은 StartRun 옵션을 넘기지 않고 연다.
- 실제 QA는 별도 QA 프로필·진행·키 설정 슬롯과 별도 작업 폴더의 Saved를 사용해 생산 저장을 보호한다.

## 통합 주의

동시 작업의 변경과 기존 미커밋 파일을 덮어쓰지 않는다. 체크포인트가 main에 반영되면 그 커밋 위에서 이 기능의 순수 변경만 적용·검증한다. 다른 기능의 검증 상태를 본 기능의 완료로 해석하지 않는다.
