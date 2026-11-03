# 동일 프로필 다음 판·포기 왕복 검증

기준은 cc6ca298 / UE5.8 / Sprint#9-26-next-run-validation이다. 기존 #9-24의 미커밋 NextRunConnection QA와 기존 세이브360개를 task/next-run-1103에 보존했다. 기존 WIP/증거/사용자 저장을 지우거나 초기화하지 않는다.

## ULW packet — 실제 연속 검증

- 목표: 정상 메뉴/캐릭터/마을/필드/자연 성장/보스 승리/저장/마을 귀환 이후, 같은 프로필의 새 RunId와 다음 필드의 동일 RunId, 너클·LV1·경험치/숙련/학습/특정 대검 초기화, 최초 Discovery와 로비 재화 보존을 실제로 관찰한다.
- 범위: 기존 Scripts/VerifyBasicRunPlay.ps1 및 Tests/Rogue10mBasicRunPlaytest.cpp의 기본OFF NextRunConnection. 실패 시 증거에 근거한 최소 QA 또는 제품 수정만 수행한다.
- 실행: 새 Label BasicRun1103O, Visible, PreparedBossStrategy/OrbitBossStrategy/GearUI/GreatSwordGrowth/AnimationEvidence/DodgeObserve/NextRunConnection, MaxSeconds1800. 녹화는 OFF이며 창 공유/브라우저 녹화/전역 OS 입력은 사용하지 않는다. 엔진 안의 프로그램 입력과 실제 Slate 경로를 사람 물리 입력과 구분한다.
- 예산: 기존 첫 승리 M은1111초. 전체1800초 고정 deadline 안에서 첫 승리 후240초 이상이 남아야 후속을 시작한다. 시간/위치/HP/경험치/자원/AI/승리를 주입하거나 deadline을 연장하지 않는다.
- 메뉴: 다음 필드에서 실제 설정→포기 버튼→확인 취소 초점 관찰→확정→결과 저장/재시도→마을 귀환으로 이어진다. 현재 드라이버가 취소 초점만 관찰할 경우 실제 취소 클릭까지 검증한 것으로 확대하지 않는다.
- 보존: 새 QA namespace3종만 허용하고 모든 이전 .sav 해시를 대조한다. 원본 에셋/맵/성장·경제·보스 밸런스는 수정하지 않는다. 첫 승리 결과와 후속 실패/완료 flag를 분리한다.
- 완료: 명확한 동등 프로필/RunId/slot reset/Discovery/재화/타이머/포기저장/귀환 근거와 원본 저장 보존. 기존 Ctrl/Q/E/R4액티브 프리셋 완료를 주장하지 않는다.
- 검증: 현재 CPP와 실제 Editor DLL은 직전47.69초 빌드에 포함됐고 이후 제품/QA 소스 변경은 없다. 변경이 필요하면 실제 build 후 관련 회귀를 수행한다. 기존 safe-menu142+제품30 검사는 직전 기능 결과와 구분한다.
- 되돌림 경계: 이번 packet의 신규 QA 코드/문서/증거만. 이전 WIP/생산 에셋/세이브 복원·삭제·강제 Git 작업은 금지한다.

독립 리뷰는 next-run driver/summary의 false PASS 위험과 제품 reset/Discovery/장착 출처를 나누어 수행한다. 실행 중인 다른 Unreal 프로세스가 없음을 먼저 확인하며, 이후 프로세스 정리가 필요하면 이번 실행기의 정확한 PID/시작 시각/경로만 사용한다.

리뷰 보완: 취소 초점 관찰과 실제 취소 실행을 분리한다. 포기 확인 창을 실제 취소한 뒤 런/재화/Discovery 불변을 검사하고 다시 확인한다. 포기 후 최종 Hub의4슬롯도 별도 안정화 상태로 최초와 대조한다. 활성 버튼만 QA delegate를 호출하며, parser는 취소·최종슬롯 flag를 완료 의존성으로 요구한다. 제품 수치/정산 규칙 변경은 없다.


BasicRun1103O는 실제 LV3/대검M3·마나1P 학습 후 보스 이동 중 wall389.908초에 evidence_write_failed로 중단됐다. 보스 승리/다음 판은 미실행이며 wrapper exit1, engine exit0이다. 기존360저장 SHA는 모두 유지되고 QA2개만 추가해362개다. 독점 append와 관찰 읽기의 Windows 공유 충돌을 별도 파일에서 재현했고, 읽기 공유 append100회는 누락 없이 통과했다. 당시 로그는 정확한 파일/Win32 코드를 남기지 않아 원인을 과장하지 않는다. QA append에 FILEWRITE_AllowRead와 파일명/플랫폼 오류 기록을 추가하고, 관찰기는 FileShare.ReadWrite 스냅샷을 메모리에서만 파싱한다. 원래 O 소스·실패 증거는 보존하며 새 P에서 다시 검증한다.


최종 결과: BasicRun1103P에서 실제 첫 승리와 동일 프로필 다음 판/취소/포기/저장귀환을 완료했다. 상세 Feature/doc/2026-11-03_next-run-validation.md.
