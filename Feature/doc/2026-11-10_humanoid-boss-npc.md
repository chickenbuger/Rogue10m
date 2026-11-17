# 인간형 보스·NPC 애니메이션 및 출력 오류 점검

## 현재 상태
2026-11-10, Sprint#9-29-humanoid-boss-npc. 인간형 보스 라이브 적용과 NPC 대기 자세 개선을 완료했다. 공유 E2 빌드와 실제 3인칭 Human3P_E1의18전투·피격/사망451검사가 실패0이고22개 원본 PNG를 개별 검토했다. 물리접촉·지속시체·전체런·AAA 최종품질을 보증하는 결과는 아니다. 공개 커밋·푸시는 하지 않았다.

## 적용 내용
- 기존 최종 보스 Azathor의 메시를 보유 Polyart Garret로 교체했다. 능력치·보상·경고·판정 형상·맵은 유지했다.
- /Game/AI/Monster/BT_Monster 및 BB_Monster를 실제 구동하고 TargetActor/HasTarget 갱신을 확인했다.
- 네이티브 Rogue10mHumanoidAnimInstance가 Idle/Walk 블렌드와 DefaultSlot을 제공한다. 공격·피격·사망은 저장형 몽타주를 사용한다.
- 프로젝트 파생 경로는 /Game/Rogue10m/Characters/Boss/AzathorHuman20261110이다. 라이브 데이터는 기존 DA_OW_AbyssOverlordAzathor가 새 DA_HumanBoss_AttackProfile을 가리킨다.
- 공격 원본의 접촉 시점은 Hook0.5초, Wave2.233333초, Zone3.166667초다. 재생 속도로 기존 windup에 맞추고 authoritative 피해 시점에서 회복 단계로 전환한다. 원본 몽타주는 복제 후 사용한다.
- 독립 리뷰에서 발견한 빈 legacy/null 프로필 전환 시 그래프 복원 누락과 비표준 RateScale 영향을 보완했다. Blueprint 클래스가 원래 null인 경우까지 회귀검사에 포함했다.
- 기존 사망 Back 후보는 보존했다. 보유 MM_Death_Right_01을 추가 리타깃한 짧은 측면 무너짐/소멸 연출을 연결했다. 지속되는 바닥 시체 연출은 아니다.
- 아린은 기존 담당이 적용한 Mixamo 호흡 대기 파생본을 유지했다. 루트/양발299표본 이동0cm, 대화→대기10/10은 NPC 별도 기록의 결과다.

## 실제 검증과 발견한 문제
- B2 Human3P_A1: 실제3인칭18/18케이스,431검사 실패0,저장386개 SHA 보존,빨간 Error0. 일반/분노의3패턴×정지/이동/Alt회피를 검사했다.
- 해당2594프레임의 좌표가 모두 유한했고 모든 케이스에서 손의 실제 움직임이5cm 이상이었다. 몽타주 피해 전 진행과 작성한 phase 계산의 최대 오차는0.000002745초였다.
- 피해 순간 위치 오차0.0000003초는 BeginRecovery의 명시적 접촉 정렬 뒤 값이다. 이 값만으로 화면의 연속성이나 타격 자세 정확도를 주장하지 않는다.
- 실제 PNG에서 평평한 카메라의 보스 가림, 몸에 투사되는 바닥 데칼, 준비 중 셰이더를 발견했다. B2 시각 합격은 보류했다.
- C3 카메라 표본에서는 -12도/어깨85cm로 중앙 시야가 비었고 셰이더 대기 문구가 사라졌다. 카메라 거리331.25→벽 앞140.78→331.25cm를 확인했다. 당시 fixture의 임의200cm 회피 기준이 실제248.46cm 이동을 거절해 마우스 케이스는 시작되지 않았다. D에서 설정값 기반 검사로 교정했다.
- D 빌드는29.95초에 성공했다. 최종 D 단위검사와 실제 화면/마우스/보스 수명주기는 별도 결과로 추가한다.

## 빨간 출력 로그
- 한국어 UE5.8.4 시작 smoke test15개는 현지화 결과를 영어 고정 문자열과 비교하는 엔진 테스트 결함이었다. QA 프로세스에만 en/en-US를 사용했다. 한국어 설정·엔진 원본·오류 심각도를 바꾸지 않았으며 한국어 엔진 자체 결함은 남아 있다.
- -game에서 Editor/Niagara 도구의 Python init이 Editor 전용 AgentSkill을 참조하는 별도 오류가 있었다. C++ 게임 QA에만 공식 -DisablePython을 사용했다. 에디터/MCP·자산 제작 Python은 유지했다.
- Python 캡슐 getter·Rotator 위치 인자·AlphaBlend 타입 오류는 지원되는 UE5.8 API로 교정 후 재실행했다. 실패 로그와 프리뷰 후보를 보존했다.
- 상세 출처: 2026-11-10_output-log-diagnosis.md.

## 증거와 복원
- 애니메이션 기록: DevLog/20261110_HumanoidBossNpc/Boss/AnimationRecord.md.
- 원본·파생 경로와 SHA: Feature/doc/evidence/humanoid-boss-npc-20261110/boss-retarget.json, boss-death-adaptation.json.
- 최초 복원 정보: boss-activation-original.json. 기존 Beholder 메시/2.4배 스케일·원래 Readability 프로필을 기록했다. 재실행 시 이 증거와 개선된 사망 후보를 보존한다.
- B2 결과: Feature/doc/evidence/boss-readability-20261103/Human3P_A1, 실제 PNG는 DevLog/20261103_BossReadability_Human3P_A1. 기존 fixture 경로의 날짜를 사용하므로 실제 실행은 label과 로그 시각으로 구분한다.
- NPC 기록: DevLog/20261110_HumanoidBossNpc/NPC/AnimationRecord.md 및 runtime.json.
- 빌드: Feature/doc/evidence/humanoid-boss-npc-20261110/build-*.log. 원래 미검증 인계 상태부터 실패 원인·성공 재빌드를 남겼다.

## 범위와 한계
- 최신 요청에 따라1인칭 신규 개발·검수는 하지 않고 기존 코드를 보존했다.
- 프로그램 입력과 전용 QA 바닥/보스 결과다. 물리 키보드/마우스, 일반 필드 전체 플레이, 사망 중 공격 인터럽트 전체 조합, 지속되는 시체 자세는 검증하지 않았다.
- AAA 자료는 준비/접촉/회복 분리와 실루엣 가독성에 참고했다. 해당 게임 자산을 복제하거나 이 결과를 AAA 품질로 인증하지 않는다.
- Fab 원본은 보존했다. Mixamo raw/source staging·실패 후보를 공개 일괄 추가하지 않는다.

## D/D2 추가 검증
- D의 SkillLoadout2개 및 HybridSkillTree3개는 각각 경고0·오류0으로 통과했다. HumanoidUnit_D2A의 ContactTiming/ProfileTransitions2개도 경고0·오류0으로 통과했다. 정확한 실행 binary와 report를 나누어 보존한다.
- TP_D1은3인칭/마우스13/13,786검사 실패0,663 POV 표본,PNG5장,기존 저장390개 보존,빨간 출력0이었다. 실제 뉴비 기본 주먹 자산의 LMB 사용1회와 QER/RMB/Ctrl 무기 기능 거부를 포함했다. 이 fixture는 실제 보스 접촉 화면을 인증하지 않는다.
- D2 빌드는11.59초에 성공했다. 최종 보스22장 및 수명주기 렌더 검수는 후속 결과를 아래에 기록한다.


## E2 최종 실행과 시각 판독
- E2 빌드14.08초 성공. Human3P_E1은451검사 실패0,18전투+피격/복귀/사망,2603프레임·PNG22장이며 기존394개 저장의 SHA를 모두 보존했다. 엔진 Error/ensure/fatal0.
- 각 PNG를 원본으로 열었다. 일반/분노 세 패턴의 준비·해결·회복을 구분할 수 있고 첫 Niagara 준비문구와 전신 데칼 칠하기는 보이지 않는다. 피격/사망 사진은 모든 공격 종료 뒤220cm에 재배치해 판독했다. 공격 중 순간이동은 없다.
- 수치상 최대 회복 전 phase 오차0.000002745초, 접촉 전환점0.0000003초, 보스 actor 이동1.85cm, 케이스별 손 움직임5cm 초과다. 접촉 snap 수치를 물리접촉이나 자연스러운 연속성 보장으로 바꾸어 해석하지 않는다.
- 사망은 짧은 측면 굽힘/소멸이고 완전히 누운 지속시체는 아니다. 기본공격은 기존 shape/range 판정을 유지하므로 외곽 거리에서 주먹이 실제 몸에 닿는다고 주장하지 않는다.
- 최종 근거: Feature/doc/evidence/humanoid-boss-npc-20261110/Human3P_E1/{README.md,launcher.json,animation-analysis.json,visual-review.json,engine.log}.
- 전체몬스터 자연 추적/내비게이션용 VerifyHumanoidMonsterAI20261110.ps1은 준비했지만 이번에는 실행하지 않았다. 보스의 실제 BT 실행/BB TargetActor·HasTarget 확인과 수동 패턴 fixture를 전종 자연AI 검증으로 표현하지 않는다.

## Unreal MCP 읽기 전용 확인
- 엔진5.8.4 CL58902052에 Epic Unreal MCP1.0 Experimental descriptor와 모듈이 존재한다. 프로젝트에서 ModelContextProtocol/MCPClientToolset와 Editor·AI·GAS·UMG·AllToolsets가 활성화돼 있다.
- 초기 실행 중 에디터에서는 toolset 목록과 IsPIERunning=false 실제 응답을 받았다. 최종 설치 확인 시127.0.0.1:8001/mcp 전송 실패였으므로 현재 활성 연결로 부르지 않는다. 설치·활성화·현재 응답을 구분했다.
- 빌드 방해를 피하고자 확인을 위해 에디터를 켜거나 설정을 바꾸지 않았다. mcp-read-only-status.json에 상세를 보존했다.


## 회수 후 자연 AI·파티클 실제 검증 후속

- BossNaturalB 자연 AI/공격/정리 회귀는 112개 성공, 실패0, 기존 저장 보존이다. 실제 wave1.6초/zone1.9초 windup을 관측했다. 고정 검사 시점을 제거한 QA 수정의 재검증이며 AI/밸런스를 바꾼 결과가 아니다.
- 관련 PNG8장을 직접 확인했다. 파동의 분홍 효과는 보이나 장판은 첫 Niagara 컴파일 이전에 캡처를 요청해 작은 점과 Preparing Niagara Systems가 남았다. 장판 시각·첫 사용·패키지 검증 완료로 승인하지 않는다.
- 파티클 최소 ProbeA는 running/paused/restored 캡처 차이와 실제 Age 키·입자1을 실증했다. 이를 적용한 PreviewC는30개 모두 실제 데이터 age/입자1 검증 및 PNG30장 확보, 보호44개 해시 보존에 성공했다.
- C의 대표 사진에서 카메라와 target cylinder 가림 때문에 효과를 판독하기 어려워 시각 승인하지 않았다. 색/재질/자산은 건드리지 않고 반대 카메라·가림 제거의 대표6개 DiagnosticD로 검토를 좁혔다.
