# 검토 및 실행 경계

- 기존 strict/B/B2와 이전 ZIP을 그대로 두고 새 schema/owner release/출력 상태를 만들었다. 해시·manifest·고정 UAT 인수·사후 검증의 기존 순수 함수는 바이트 동일 helper로 재사용한다. strict quit gate를 수정하거나 통과로 만들지 않는다.
- 출력 manifest의 exclusive 생성마다 물리 경로·owner 범위·입력/보호 저장과의 분리를 다시 확인하도록 독립 리뷰의 capture scope 지적을 반영했다.
- 가상 후보 통합 반례는 source/WP 외부 패키지/보호 저장 변화, 다른 native root, startup 검토 누락, project write 권한, 기존 Game snapshot 변경, stale Game/archive mismatch와 phase별 같은 plan hash를 검사한다.
- native43 raw parser는 정확한43 ID·source/config/DLL·engine/project/selector·기기 PID·경고6사례12개·실제 로그를 묶는다. 순차 완료·마지막 완료→queue43→유일한 canonical exit 순서와 모순 exit를 검사한다. test/shutdown red는 startup review로 허용하지 않는다.
- 실제 체크포인트 C 자료를 읽기 검사했으며 Source/Config366 및 DLL180D…76D가 일치했다. native report의43 Success와 전체 로그 Error0는 다른 사실이다. 시작 Error15는 현재 NOT_APPROVED이며 정확한 원인·영향에 대한 검토가 없으면 실제 smoke preflight도 차단된다. 원인에 대한 지역화 가설은 승인 근거로 사용하지 않았다.

일반 소유자 PowerShell release는 정확한 현재 PID/start ticks/exe/SHA와 기간·scope·project build outputs 권한을 요구한다. lease는 해당 run의 중복 실행을 막는 advisory 경계이며 전역 엔진 사용권이나 native mutex를 대체하지 않는다. 자식 PID 관측·종료/kill·GUI·게임 실행 기능은 없다.

UAT 실행 경로는 오프라인 AST·UNSEALED 차단만 검사했다. 실제 UAT/Game/cook/IoStore/archive는 이 Work에서 실행하지 않았다. 로그 형식·도구·SDK 또는 compile/cook 문제가 발생하면 실패와 출력을 보존한다. UAT 오류나 source 변경을 artifact 성공으로 승격하는 경로는 없다.

내부 artifact가 만들어져도 UI7/시각/물리플레이/저장 routing/포털 검증은 UNVERIFIED다. 현1800m 미축소·NPC 미배치·기본맵 미전환 상태는 계획/성공/실패 결과에도 남는다. 실제 UI gate와 이전254/이번 보관 native43을 새 실행 개수로 합치지 않는다.
