# 보유 격투 애니메이션의 Manny 적용 준비

작업 브랜치: `Sprint#5-6-close-combat-animation-preparation`. 프로젝트: `D:\Project\Rogue10m\Rogue10m.uproject`, UE 5.8, remote `https://github.com/chickenbuger/Rogue10m.git`, HEAD `b7ab6b6`.

보유 Close Combat의 실제 로컬 클립을 기본 Manny로 리타기팅한 프리뷰 11종을 준비했다. 게임용 DA·몽타주에는 연결하지 않았다. 후보 선정용 수치 검사와 원본 보존 검증을 완료했으며 화면 재생·실제 접촉·정면 타깃 판정은 미검증이다.

## 확인한 현재 상태

기존 브랜치는 `Sprint#5-5-editor-ai-assistant-error`였다. 이미 많은 미커밋 작업이 있으므로 모두 유지한 채 새 작업 브랜치를 만들었다. 중복 체크아웃이나 프로젝트는 생성하지 않았다.

9월 28일의 실제 root 사용, 외형 추가 FBIK 비활성, FootIK→DefaultSlot 순서, V 관찰과 전신 그림자를 유지한다. 9월 29일 FistAuthored에는 별도 맨손 훅·너클 어퍼컷이 이미 적용돼 있다. 현재 해당 두 시퀀스의 머리·골반 회전 변화는 정적 클립 검사에서 0도다. 원래 `MM_Attack_03`은 머리 174.84도·골반 179.10도로 다르다. 새 클립은 이를 무조건 교체하는 작업이 아니라 동작 다양화 후보다.

`BasicBrawler` 폴더의 기존 `RightUppercut`·`ChargedRightUppercut` DA는 여전히 공통 `AM_Punch_03`을 참조한다. 이름만으로 실제 어퍼컷이라고 판단하지 않는다. 이번 DA 감사는 BasicBrawler 6개와 Knuckle 5개를 대상으로 했으며 전체 스킬의 실제 발동 검증을 의미하지 않는다.

저장소와 상위 경로의 AGENTS를 확인했다. 프로젝트 및 부모의 `.agents/skills`는 없었다. Codex 메모리 디렉터리는 비어 있고 읽기 전용 memory DB snapshot의 stage1_outputs는 0건이라 관련 memory_summary를 확보하지 못했다. 최신 Feature 문서와 로컬 코드·에셋을 근거로 진행했다.

## 에셋 출처와 추가 위치

실제 보유 캐시: `C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\CloseCom2addb0ced842V1\data\Content\CloseCombatAnimSet`.

좌훅·우어퍼컷·로킥·직선 킥, 가드 시작/유지/피격, 앞/뒤/좌/우 회피 파일을 확인했다. 원본은 UE4 마네킹 메시·스켈레톤을 사용한다. `Source Files.rar` 안에도 같은 이름의 원본 FBX가 존재한다. 새 외부 다운로드와 업로드는 하지 않았다.

- `Content/CloseCombatAnimSet/`: 필요한 애니메이션·UE4 메시·스켈레톤·재질 의존성 33패키지만 동일 바이트로 복사했다. 원본 패키지를 수정하거나 재저장하지 않았다.
- `Content/Rogue10m/Animation/Preview/CloseCombat/`: Manny 시퀀스 11개와 IK Rig 2개·Retargeter 1개를 Unreal Editor API로 생성했다.
- Rolls and Dodges, MC Sample, Mudra Samples, Death Animations MoCap, GASP retargeted to Manny 패키지는 조사한 프로젝트·VaultCache에서 식별하지 못했다. 보유 목록과 로컬 설치 여부는 구분한다.

새 리타기터는 실제 root 체인을 명시하고 SourceRoot/TargetRoot를 root로 설정한다. Run IK Rig는 비활성이다. UE4/Manny 기준 자세를 정렬하고 FK 동작을 베이크했다. 새 프리뷰만 `EnableRootMotion=false`, `ForceRootLock=true`, `AnimFirstFrame`으로 설정해 제자리 재생을 준비했다. raw root 트랙은 유지하며 RM 방향 수치도 별도로 기록한다. 판정·이동 시스템에 연결하지 않았다.

## 공격 후보

수치는 60Hz의 프리뷰 클립 포즈에서 첫 자세 대비 최대 회전이다. 머리 카메라의 실제 회전이나 프레임당 회전이 아니다. 검토 시점은 손/발의 골반 기준 +Y 도달 peak이며 실제 표적 접촉이나 확정 HitStartDelay가 아니다.

| 프리뷰 이름 `A_CC_*_Manny` | 길이(s) | 골반 변화(도) | 머리 변화(도) | 접촉 검토 cue(s / 60Hz frame) |
| --- | ---: | ---: | ---: | ---: |
| LeftHook | 1.300 | 27.75 | 34.90 | 0.517 / 31 |
| RightUppercut | 1.133 | 19.38 | 36.08 | 0.450 / 27 |
| LowRightThaiKick | 1.633 | 89.84 | 54.94 | 0.667 / 40 |
| StraightKick | 2.467 | 177.50 | 175.40 | 0.600 / 36 |

좌훅과 우어퍼컷을 우선 화면 검토 후보로 선정한다. 좌훅은 왼손 동작이므로 기존 오른훅 DA에 그대로 대입하지 않는다. 기존 FistAuthored보다 머리 움직임이 크므로 실제 머리 카메라와 고정 조준 판정 확인이 필요하다.

로킥은 실제 클립이 있으나 골반 회전이 약90도라 조건부 후보다. 직선 킥은 복귀를 포함해 약177도 회전하므로 작은 전신회전의 단순 앞차기 후보로 보류한다. 기존 Martelo도 골반 변화 약120.88도라 단순 앞차기로 대체하지 않는다. Mixamo의 낮은 킥·앞차기 후보는 현재 로컬 원본이 없고 이번 작업에서 다운로드·재생 검증하지 않았다.

[공격 포즈 도표](evidence/close-combat-preparation-20261002/attack-pose-traces.png)는 UE 샘플에서 만든 과학적 투영이다. 메시·피부·카메라 화면·실제 표적은 포함하지 않는다.

## 가드와 실제 방향 회피

`BlockIntro` 0.300초, `BlockLoop` 1.467초, `BlockHit` 0.300초를 준비했다. 가드 전환·유지·피격용 원본이며 전용 패리라고 확정하지 않는다. 가드 상태나 무적·방어 판정을 새로 구현하지 않았다.

앞·뒤 회피는 0.967초, 좌·우는 2.800초다. 원본 RM root는 +Y 375.42cm / -Y 374.58cm / +X 528.59cm / -X 530.01cm로 실제 방향이 다르다. Manny 베이크의 raw 이동은 체격 차이에 따라 약0.991배이며 제자리 프리뷰의 root 이동은 11종 모두 0cm다. 실제 플레이 이동 거리나 Dodge 속도로 적용한 수치가 아니다.

[회피 포즈 도표](evidence/close-combat-preparation-20261002/evade-pose-traces.png)에서 앞·뒤는 몸을 뒤집는 덤블링 성격으로 추정된다. 렌더된 실제 동작을 확인한 결론은 아니다. 짧은 방향별 구르기로 승인하지 않는다. 좌·우도 전체2.8초·약5.3m의 동작이므로 짧은 스텝 구간을 추가 검토해야 한다. 8방향 구르기를 완성했다고 기록하지 않는다.

기존 `AM_Dodge_Roll`은 실제로 `MM_Dash` 하나를 재생한다. 이를 방향별 구르기 팩으로 오인하지 않는다. Rolls and Dodges 로컬 설치와 스켈레톤·실제 클립 확인이 다음 구르기 준비에 필요하다.

## 타격·카메라 계약과 다음 적용 기준

로컬 CombatComponent는 판정 직전 `UpdateCameraFromAppearance()`를 호출하고 카메라 위치·forward·rotation으로 표적을 찾는다. AppearanceCamera는 머리 회전과 컨트롤러 조준을 결합한다. 현재 native CDO는 맨손/너클 머리 회전 기여0.15, 최대8도, 초기 pitch -20도다. 이 설계를 변경하지 않았다. 새 훅/킥의 머리 움직임이 판정 방향에 영향을 줄 수 있으나 이번 작업에서 버그를 재현한 것은 아니다.

첫 타격 실시간은 `HitStartDelaySeconds / max(0.01, AttackSpeedMultiplier * AnimationPlayRate)`다. 후속 간격은 `max(0.01, HitInterval / AttackSpeedMultiplier)`로 AnimationPlayRate가 적용되지 않는다. segment/montage 재생률1 기준으로 후속 접촉의 몽타주 간격은 보통 `HitInterval * AnimationPlayRate`다. 현재 너클6연타는 H=0.18, I=0.075, P=1.24여서 몽타주 시각이 0.180/0.273/0.366/0.459/0.552/0.645초다. 이번에는 값을 변경하지 않았다.

새 프리뷰의 cue를 기존 DA에 자동 대입하지 않는다. 클립을 몽타주에 연결하거나 자를 때 segment 시작·재생률에서 montage clock으로 변환한 실제 접촉 프레임을 확인한 다음 첫 delay를 맞춘다. 연타 재생률 변경은 전체 contact 배열과 타이머 pulse 배열을 다시 비교한다.

다음 통합에서는 마우스를 고정한 정면 타깃에서 카메라·debug hit volume·실제 피해·ExecutionId와 접촉 프레임을 함께 기록해야 한다. 좌/우회피 방향, 원래 Dodge 이동 정책과 root 이동의 중복, 가드→공격 전환, V 왕복도 검증한다. 이 준비 단계에는 새 클립의 게임 연결·타깃 피해·debug volume 화면 검증이 없다.

## 실행 검증과 한계

- 설치된 UE5.8 `UnrealEditor-Cmd.exe`에서 새 에셋 생성과 저장, 이후 읽기 전용 감사 실행 성공(exit0).
- Manny 11클립·983개 raw 포즈, 유한수와 스켈레톤·길이 일치 확인. 본 길이비 0.999978~1.000000. root lock 적용 포즈의 root 이동0cm.
- 서로 다른 기준 자세의 절대 회전 오프셋과 동작 오차를 분리했다. head/pelvis 첫 자세 대비 회전 delta의 Source→Manny 최대 차이0.006084도. UE4/Manny 절대 자세 차이 최대 약15.88도는 기록하되 동작 증폭으로 판정하지 않는다.
- 기존11개 DA의 전후 필드·몽타주 segment 계약 일치. 초기5,630파일과 VaultCache121파일의 SHA256 보존 확인. SprintChangeLog에는 기존 바이트를 보존하고 추가분만 append.
- Python AST, diff 공백, Harness 생성 경로 검사 수행. C++·설정·기존 게임 에셋 변경이 없어 전체 빌드는 수행하지 않았다. 과거9월28~30일 런타임 PASS는 기존 이력이며 이번 실행 결과와 구분한다.
- Computer Use SKILL.md를 확인했지만 현재 node_repl/화면 제어 도구가 없다. 프로세스 확인을 화면 확인으로 보고하지 않는다. 최종 도표는 샘플 데이터에서 만든 그림이다.

초기 생성 중 자동 Root 체인 누락, FName 대소문자 비교, 구조체 문자열의 메모리 주소 비교 문제를 발견해 새 준비 스크립트에서 수정했다. 초기 실패 로그와 최종 성공 로그를 모두 보존한다. raw root 이동이 남아 있는 것도 발견해 이번에 생성한 Preview에만 root lock을 설정했다. 게임 코드와 카메라의 오류를 고친 작업이 아니다.

## 재현과 산출물

`Scripts/Editor/PrepareCloseCombatPreview.py` 기본 실행은 읽기 전용이다. `-RogueCloseCombatPrepare`는 비어 있는 Preview에만 생성하며 기존 프리뷰가 있으면 중단한다. 이번 신규 에셋의 root lock 갱신은 `-RogueCloseCombatLockPreview`로만 허용하고 baseline에 있던 파일은 거부한다.

```powershell
& 'D:\Program Files\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Project\Rogue10m\Rogue10m.uproject' -run=pythonscript '-script=D:\Project\Rogue10m\Scripts\Editor\PrepareCloseCombatPreview.py' -unattended -nop4 -nosplash -nullrhi -nosound
```

`AuditCloseCombatReadiness.py`는 현재 FistAuthored, Boxing, Martelo, MM_Attack_03, MM_Dash와 카메라 native CDO를 읽기 전용 비교한다. `AnalyzeCloseCombatPreview.py --evidence <evidence 경로> --output <출력 경로> --draw`는 포즈 구조를 검증하고 도표를 만든다. 도표 생성은 기존 .venv의 Pillow를 사용하며 새 패키지를 설치하지 않았다.

주요 증거: `editor-audit.json`, `editor-summary.json`, `readiness-comparison.json`, `readiness-summary.json`, `pose-validation.json`, `contact-review-cues.csv`, `host-inventory.json`, `baseline-files.json`, `preservation.json`, `ue-preview-locked.log`.

남은 차단: 실제 짧은8방향 구르기 소스, 작은 회전의 앞차기와 전용 패리 확인. 남은 QA: 후보의 메시 재생, 1인칭 가독성·머리 카메라·고정 조준 표적 피해, 몽타주 블렌딩, 스텝 구간, 체형별 외형 리타깃, 가드 상태·방어 판정. 결제·새 약관·캐릭터 업로드·삭제·커밋·push는 수행하지 않았다.


## 후속 실제 스킬 연결 — Sprint#5-7

격투가 현재 스킬 요청에 따라 유운수·금강쇄·무극패왕권에 별도 Combat/FighterSkills 파생 시퀀스와 몽타주를 실제 연결했다. 기존 9개 전용 동작 및 본 문서의 원본/프리뷰/루트 정책은 보존했다. 적합한 연결은 공격 12개이며 태산압정은 내려찍기 소스 부족으로 기존 연결 유지, 금강불괴는 상시 패시브다. 세 DA의 AttackMontage/HitStartDelaySeconds만 변경했다. 판정 지연은 각각 0.05/0.273/0.283333초다.

전체 매핑과 검증 한계는 [격투가 스킬별 연결 결과](2026-10-02_fighter-skill-animation-binding.md) 참조. Editor 필드/에셋 검사와 기존9개 실제 타깃·카메라 회귀는 PASS. 13개 키 실행은 확인했지만 유운수의 늦은 몽타주 활성 1항목은 미통과다. 셰이더 준비 제한으로 실제 렌더 및 새3개 개별 접촉은 미검증이다.


후속 진단에서 유운수의 기존 미통과는 정상 블렌드 아웃 후 늦게 관측한 검사 문제로 확인됐다. 에셋 변경 없이 실제 발동 중 관측 검사로13개 모두 통과. 새3개는 각각 실제135cm 정면타깃 피해1회·판정시간·몽타주진행·기존카메라상한 검증 통과. 승룡권은 기존 Knuckle_Uppercut 유지이며 Close Combat 어퍼컷은 적용하지 않았다. 실제 렌더/스킨 접촉과 태산압정 소스 부족은 남아 있다. 자세한 근거는 스킬별 연결 결과 문서의 후속 진단 항목 참조.
