# 월드 구성 1차 기획 결과와 후속 Landscape 구현

2026-11-16 · Sprint#9-32 · 1차 기획 완료 / 2차 native 편집·저장·cold 재로드 확인, 통행 인수 대기

사용자가 요청한 순서에 맞춰 [상세 기획서](../architect/2026-11-16_landscape-production-plan.md), [좌표 보충 데이터](../../Docs/References/WorldMap/landscape-production-20261116.json), [제작 배치도](evidence/landscape-production-20261116/landscape-plan.png)를 작성했다.

채택안은 기존21노드·29개 양방향 도로와 큰 마을3·전문초소5의 좌표를 보존하는1,012.5×562.5m Landscape다. 9×5컴포넌트·유지45/외곽211, 중앙300m, 통행폭8m/전체정지폭14m, 대지 반경·최종보스장·기준고도·검증·롤백을 구체화했다. 900m폭 후보는 동쪽 중간보스 대지가 경계를 넘어 제외했다.

`PrepareLandscapePlan20261116.py`의 설계 기하10개 항목은 모두 통과했다. 21/29·3/5·연결/양방향/무조건개방·중앙300m·대지/도로어깨경계·책23권을 확인했다. 대지의 최소 경계 여유는 최종보스장의52.5m, 도로19m 어깨 envelope의 최소 경계 여유는81.5m다. 이는 기존 절차형 원본과 계획 좌표의 검사이며 현재 저장된 지형·paint·레이어의 인증이 아니다.

1차 기획 당시 일반 sandbox의 프로세스 조회에는 Unreal Editor가 표시되지 않았고, 공식 MCP8001은 연결 거절(WinError10061)이었다. 이는 당시 제한된 조회 범위의 관찰이다. 이후 호스트 직접 조회에서 잔여 `-game` 프로세스를 확인했으므로 ‘호스트 전체에 프로세스 없음’으로 확대하지 않는다. 초기 기획 시점의 지형 편집·삭제·저장·빌드·게임 실행0 기록과 현재 상태를 구분한다. [조회 범위 정정과 정상 에디터 복구 기록](2026-11-16_editor-startup-recovery.md)을 참조한다.

초기에는 `C:/Users/PC/.codex/skills/unreal-engine-mcp-codex/SKILL.md`의 live 연결 부재 시 실행 요청 규칙에 따라 대기했다. 후속 복구에서 정상 에디터와 공식 MCP 연결을 확인한 뒤 사용자 승인 범위의 편집을 계속했다. 과거 거절된 별도 metadata clone QA·startup 승인 파일을 대신 실행하지 않는다.

기존 작업 브랜치 `Sprint#9-32-landscape-world-blockout`를 유지하고 최초 기존 WIP605개 항목을 확인했다. 1차 기획 단계에서는 기존 Source/Config/Content를 수정하지 않았다. 후속 구현에서는 독립 Crop 후보의 지형 external actor16개를 저장했다. 두 기존 카탈로그와 벤더 원본은 불변이다. 남은 인수 검증과 무관한 기존 WIP를 묶어 commit/push/승격하지 않는다.

후속 구현에서 live baseline·물리 백업, 외곽211개 native 삭제, 유지45개 데이터 대조, 후보 저장과 독립 cold 재로드를 확인했다. Undo/Redo 직후에는 composite seam 오류가 발생했고 공개 native 재합성1회 후 데이터가 복구됐다. 정적 도로 검사는 exact X 격자선의 ray miss2개로 strict 실패를 보존한다. 추가 작은 sphere·X offset ray의 실제 hit를 원래 실패의 합격으로 바꾸지 않는다. 최신 수치·실패·증거는 [Landscape 제작 결과](2026-11-16_landscape-production-result.md)에 기록했다. 중앙3회 실제보행은 정상 프로필 표시 이름 불일치로 준비 단계가 실패해 미실행이며, 성장 사람5회·15분/20분 완주·콘텐츠 전투는 후속 통합 검증이다. 과거 합성경사15.095°와 전체 삼각형12° 인수는 해결 완료로 표시하지 않는다.

독립 리뷰에서 NPC명 무건, 정상 프로필·기본장비 중앙3회/성장 사람5회 기준, crop 보존 검증과 국소 경사 보정의 구분을 확인해 반영했다. 기하 검사 실패 시 ready JSON을 쓰지 않도록 준비 도구도 보정했다. 경사 보정이 필요하면 crop 보존 검증 후 별도 패킷·변경 구역·최대 차를 기록한다.
