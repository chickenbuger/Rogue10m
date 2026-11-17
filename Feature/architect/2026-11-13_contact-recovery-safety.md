# 접촉·회복 타이밍 안전 통합 패킷

공유 프로젝트는 모든 파일/Git/UE·MCP·UBT·Cook·게임을 읽기만 하거나 실행하지 않는다. 수정은 새 물리 사본 `tmp/SkillTiming20261113`의 미커밋 `Sprint#9-33-contact-recovery-safety`에 한정한다. 현재339개 Source/Config를 안정 복사하고 미통합 G08 V1 코드3패치를 사본에만 적용했다. 공유의 Controller.cpp/h·MainMenuWidget.cpp 최신 변경은 baseline으로 유지하며 패치 대상에서 제외한다. `.agents/skills`는 현재 없다. 최신 사용자 AGENTS 지시와 Lazy overlay/AnimationRecordingGuide를 적용한다.

첫 체크포인트: 기존 source/code4개 패치 apply-check0, 이전 DevLog/Sprint patch만 drift로 실패. 기존 core3 baseline은 공유와 동일하고 신규3 source는 미통합이다. 12개 원본 animation SHA/크기/mtime는 이전 기록과 동일하다. 원본 DA/Skeleton의 과거 해시는 없어 과거 동일성을 주장하지 않는다.

| 패킷 | 목표·변경 | 완료/검증 | 되돌림 |
|---|---|---|---|
| R1 | 기존 Timeline/Combat에 callback arm generation·expected ordinal, editable recovery end, accepted effective rate | standalone 실제 kernel·arm/recovery 회귀, UE 회귀 작성·소스 검토 | recovery0/V1 호환, 새 arm/회복 branch 제거 |
| R2 | 기존 G08 author command recovery 인수/새 필드/명시 blendout 정책·새 후보경로 | reflected property/parse/native API 정적 검토, 엔진 실행은 소유자 대기 | 새13일 후보 namespace·기존 파일 불변 |
| R3 | full/delta·최신 해시·12일 이후 로그 추가문·증거 구분 | shared read-only apply-check·문서·별도 한국어 DevLog/Sprint | 전달 파일만 |

회복 필드는 `AuthoredRecoveryEndTime`: 몽타주1x 절대 종료시각,0은 V1 접촉-only 동작을 보존한다. 양수는 마지막 접촉 이상/몽타주 길이 이하/승인 배속 환산30초 이하. 접촉·회복 필드는 EditAnywhere/BlueprintReadOnly로 DA에서 직접 편집 가능하다. 양수 회복은 최종 접촉 뒤 map/monitor를 유지해 새 공격을 막고, deadline 또는 피격/외부 중단/수명 종료에서 소유한 action lock을 해제한다. 비용·공유쿨다운·이미 적용된 명중 credit은 보존한다. 면역은 추가하지 않는다.

명시 contact mode는 montage intrinsic RateScale을 승인 instance rate와 곱해 시간축에 반영하고, 실행 중 둘이 바뀌면 취소한다. 지원되는 natural blend-out은 autoBlendOut=false 또는 trigger0이다. 새 제작 몽타주는 autoBlendOut=true/trigger0을 명시해 마지막 contact가 기본 .25초 선행 blend-out으로 지워지지 않게 한다. 위치/section seek·mesh/actor custom time dilation의 최종 지원은 이번 작은 패킷 밖이며 실제 UE/3P 검수 대상으로 남긴다.

TimerManager가 정상적으로 콜백을 중복 전달한다고 주장하지 않는다. arm generation/expected ordinal은 수동 replay·늦은 delegate를 명시적으로 무효화하는 방어다. cancel/restart는 기존 execution ID와 arm 두 경계로 확인한다. 실제 montage playback·접촉/회복 캡처와 숫자 통과를 구분하고, 과거155통과·이번 새 검사·작성만 한 UE 검사를 각각 기록한다. 공유 DevLog/Sprint는 쓰지 않고 추가문만 전달한다.
