# 근접 신규 스킬 콘텐츠 제작 패킷

2026-11-11 UTC. 작업 사본은 `tmp/SkillContent20261111`, 미커밋 브랜치는 `Sprint#9-30-skill-content-prototypes`이다. 공유 Source/Content·UE 에디터·UBT는 이번 패킷에서 변경하거나 실행하지 않았다. 아래 구현은 통합 가능한 격리 패치이며 UE 컴파일·실제 자산 제작·게임 내 완료와 구분한다.

## 실제 구현

- G04 돌진참과 G10 단공참의 **지상 캡슐 전진 → 실제 종점에서 기존 실행ID 단타** 경로를 추가했다. `bAdvanceBeforeHit=false`가 기본값이다. 전진을 켠 제작 후보만 새 경로를 쓴다.
- 승인 전에 명시 슬롯(Common/RightChargeOnly), 단일 타격·비투사체·비콤보, 유한 거리/시간, 지상 지지, 회피 중 아님, root-motion 없는 몽타주를 검사한다. 비용을 지불한 뒤 몽타주 재생이 실패하거나 동기 콜백으로 취소되면 이동/피해를 예약하지 않고 승인된 비용·사용 기록은 유지한다.
- 승인 시 공속/방향/무기를 고정한다. 타이머로 캡슐을 sweep하며 지지면을 최대20cm 간격으로 검사한다. 벽과 절벽에서는 이동을 멈추고 기존 몽타주의 접촉 시각까지 기다린다. 접촉 대기와 Walking 복구 뒤에도 바닥을 검사한다. 평지용 prototype이며 계단·경사 이동이나 좁은 틈 통과가 검증된 상태가 아니다.
- 전진 중 회피·다른 기술은 거절한다. 메뉴·교체·사망·컨트롤러 소실·외부 이동·EndPlay 등은 취소한다. MovementMode 복구와 이동 중 동기 콜백 재진입에는 generation/승인 epoch로 늦은 타격을 막는다. 새 전진 경로의 표적만 Visibility 차폐를 검사한다. 피해 소유자는 기존 Combat 실행 하나다.
- 통합 검토에서 확인한 **비용 직후 동기 취소의 쿨다운 누락**을 수정했다. TryPay 성공 바로 뒤에 공유 쿨다운을1회 확정하고 그 다음 취소 epoch를 검사한다. 취소 callback이 이미 설정한 더 긴 종료시각과 해당 표시 metadata는 보존한다. 미승인/결제 실패에는 쿨다운을 만들지 않는다.
- 전진은 승인된 primary AnimInstance와 Montage instance ID를 고정한다. 시작/10ms step/각 sweep 전후/Walking 복구 뒤 및 Combat handler 진입에서 같은 재생이 `IsActive && IsPlaying`인지 재검사한다. 외부 Stop·정상 blend-out·Pause·같은 자산 재시작·AnimInstance 교체/소실이면 다음 이동과 미발생 접촉을 취소한다. 완료 multicast의 앞선 listener가 중단하는 경계도 Combat에서 다시 검사한다. single montage delegate를 덮어쓰지 않는다. 중단 뒤 의도적 커밋은 별도 디자인 결정으로 남겼으며 이번에는 허용하지 않는다.
- 사용자의 **피격 시 우선 취소** 결정을 적용했다. `Character::TakeDamage`가 finite 양수 AppliedDamage를 확인한 뒤 HP/LastStand/피드백 전에 한 번 Combat을 중단한다. HP 자원 비용·회복·0/거절 피해와 구분하며 기존 피해 수락 조건을 바꾸지 않는다. 새로운 면역·슈퍼아머·피격 애니메이션/경직 시간은 추가하지 않았다. 면역 도입 판단은 사용자의 스킬/실플레이 테스트 뒤 결정으로 남긴다.
- 피해 epoch를 모든 기술의 비용/Play/표현/접촉 단계에서 검사한다. 이미 지불한 비용/use/공유쿨다운 종료는 보존하고 미래 콤보 쿨다운은 즉시 활성화하며, 차징·홀드·콤보/GAS 예약·남은 다단타와 이동을 정리한다. 접촉을 지역 실행 snapshot으로 처리해 반사 피격이 맵을 지워도 참조가 무효화되지 않는다. Blueprint 표적 필터 뒤와 실피해 직전에 재검사하여 취소 뒤 추가 피해를 막는다. 이미 적용된 양수 타격의 명중 credit은 보존하도록 현재 접촉의 miss 판정을 마지막 결과까지 유예한다.
- 피격 중단은 저장된 AnimInstance/instance ID만 Stop한다. Basic 입력·타이머·상태 정리는 유지하되 이 경로의 asset 기준 Montage_Stop은 생략하여 같은 자산의 교체 재생을 함께 끊지 않는다. 기존 다른 취소 호출은 기본 동작을 유지한다. 1인칭은 이번 신규 검증 범위에서 계속 보류한다.
- canonical K01–K10/G01–G10을 독립 프로젝트 DataAsset으로 만들 **실행 가능한 Editor 레시피·제작기**를 추가했다. 전체 비용/피해/타이밍/몽타주/VFX 필드를 보존하고 이름·설명·명시 슬롯·독립 콤보·G04/G10 전진 옵션만 바꾼다. 모든 후보는 `[제작중]`, `Rogue10mGameplayReady=false`, 미구현 목록을 가진다.
- 기본 실행은 검토만 한다. `apply=True`는 새 prototype20개만 생성한다. 선택 기능인2개 검수용 프로필은 production에 할당하지 않는다. 기존 프로필·성장·캐릭터·세이브는 수정하지 않는다. 검수 프로필을 캐릭터에 할당하면 legacy 비관리 기술이 자동해금될 수 있으므로 production 해금 정책을 보장하는 fixture로 간주하지 않는다.

## 콘텐츠 상태와 다음 우선순위

기존의14개 의미 매핑은 새20개 기믹 완성을 뜻하지 않는다. K01은 고정 기본공격과 겹쳐 독립 DA 분리가 필요했고, K10/G04/G05/G06/G08/G10은 새 정의가 없었다. 이번 제작기는 fallback까지 포함해18개 서로 다른 기존 패키지를 사용한다. 이 원본18개는 로컬 파일 존재·해시만 확인했으며 Fab 계정 소유·현재 런타임 로드 증명과 구분한다.

| ID / 슬롯 후보 | 재사용·이번 변경 | 모션·판정·VFX·QA 누락 |
|---|---|---|
| K01 연환권 / 공용 | Primary01 독립 복제 레시피, 기본 콤보 연결 해제 | 독립 연타 모션/타수/접촉·기본공격 분리 검수 |
| K02 붕권 / 공용 | 기존 Primary02 | 전진·경직 고유 기능, 준비/접촉/회복·VFX |
| K03 금강쇄 / 공용 | StoneFist Straight | 어퍼컷·띄움·보스 대안, VFX·접촉 검수 |
| K04 유운수 / 공용 | StoneFist Jab | 파리 창·성공/실패·반격 기능과 모션/VFX |
| K05 철산고 / 공용 | 기존 Charged 임시 재사용 | 어깨충돌·이동 모션/판정·VFX |
| K06 파진각 / 공용 | 기존 Special | 킥/지지발·부채 판정·VFX 실측 |
| K07 백열난무 / 동시 전용 | Primary03 타수/타이밍 보존 | 동시 재입력·연타 궤적/상한·VFX/3P |
| K08 무극패왕권 / 동시 전용 | ChargedShockwave의 release 공격 재사용 | 동시 입력 전환·부채 판정/지형·VFX |
| K09 태산압정 / 차징 전용 | 기존 JumpSlam | 실제 도약/착지·천장/벽·지상 차지 전환·VFX |
| K10 천붕권 / 차징 전용 | 새 독립 레시피, 기존 Charged/ShortBlow 실행 재사용 | 전용 차지포즈/집중 판정/VFX, 붕권과 구분할 실모션 |
| G01 분쇄참 / 공용 | Hybrid CrushingSlash | 고유 준비/접촉/회복과 VFX·그립 |
| G02 회전참 / 공용 | Hybrid Whirlwind | 회전폭/벽/동일표적 상한·실모션/VFX |
| G03 파쇄참 / 공용 | Hybrid GuardBreak | 방어붕괴/자세 누적 고유 기능·모션/VFX |
| G04 돌진참 / 공용 | 새 레시피와 실제 전진→종점 코드 | slide 수입/편집·전진/충돌 UE 검증·VFX |
| G05 반격참 / 공용 | 새 fallback 레시피(GuardBreak) | 방어창·성공/실패·반격 토큰 전부 미구현 |
| G06 올려베기 / 공용 | 새 fallback 레시피(Primary03) | 상승 모션·띄움/보스 대안·양손 그립 미완료 |
| G07 대지파쇄 / 동시 전용 | Hybrid EarthSplitter | jump/slam 수입·실착지/지면파·지형/VFX |
| G08 섬멸연참 / 동시 전용 | 새 fallback 레시피(Primary01) | 독립3연격·비균등 접촉·복합 편집/VFX 미구현 |
| G09 패왕참 / 차징 전용 | Hybrid Ultimate | 전용 완충포즈/고유 강화·접촉/VFX |
| G10 단공참 / 차징 전용 | 새 레시피와 차지 후 전진→종점 코드 | slide+power-up 편집·경로 관통 피해 미구현·VFX |

공용 후보6·동시 후보2·차징 후보2 배치는 **설계 제안**이다. 확정 계약은 무기당 최대10개, 최대6개 장착, Q/E/R/RMB 공용4칸과 동시1·차징1 전용이며 전용은 공용에 들어가지 못한다. 기존 카탈로그의 opt-in 검증은 정확10개를 요구한다. 이번 레시피는20개를 모두 production에 공개하지 않는다. 뉴비 맨손 시작/Unarmed→None, Ctrl 아이덴티티는 유지하며 Staff/마법사의 별도 명령 입력에는 적용하지 않는다. 1인칭은 보류한다.

우선순위는 (1) 런타임 빌드/기존 회귀와 G04·G10 벽/절벽/취소 검사, (2) 새20개 DA 생성·cold reload와 K01/K10 분리 검사, (3) G04/G10 원본 slide/power-up의3P 편집·접촉 증거, (4) G05 반격/G06 띄움/G08 복합연타 기능·모션을 별도 패킷으로 구현, (5) 성장노드/선택UI/저장ID 이관 후 production opt-in이다. 피해량·거리·시간은 최종 밸런스로 승인하지 않는다. G04의150cm/.25초, G10의250cm/.35초는 임시 실험값이다.

## 검증과 근거

- 실제 production 헤더 `Rogue10mAdvanceTimeline.h`를 독립 MSVC C++17 `/W4 /WX`로 컴파일하고10시나리오56검사를 통과했다. 충돌/바닥/애니메이션을 실행한 테스트는 아니다.
- 순수 레시피/모의 Unreal 제작기16개 테스트를 통과했다. 검토 모드 무변경,20개 독립 복제/보존, 잘못된 원본/슬롯/advance·stale reflection·외부 destination·수동 편집 거절, 기존 프로필 보존, 저장 실패 근거 보존을 검사했다. 실제 `.uasset` 생성 수는0이다.
- 독립 Reviewer가 바닥 검사 누락, 이동/모드 콜백 재진입, 외부 텔레포트, 회피 충돌, 승인 공속 변경을 찾아 수정한 뒤 추가 차단급 결함은 발견하지 못했다. 로컬 UE5.8 헤더에서 `FindFloor`, `FFindFloorResult`, `SafeMoveUpdatedComponent`, `HasRootMotion` 사용 가능성을 확인했다. UHT/UE 컴파일 통과를 뜻하지 않는다.
- 후속 통합 검토의 쿨다운/몽타주 중단 문제를 수정하고 독립 Reviewer가 최종 소스를 재검토했다. `GetMontageInstanceForID`, instance `Montage/IsActive/IsPlaying`과 weak 참조·완료 listener 경계의 API/수명 검토에서 추가 결함은 찾지 못했다. 실제 UE 회귀는 `Source/Rogue10m/Tests/Rogue10mTechniqueInterruptionTests.cpp`에 별도로 작성하며, 이 패킷에서 빌드/실행하지 않았다.
- 피격 검토에서는 지역 pulse snapshot·두 번째 표적 필터 경계·반사 타격 hit/miss 순서·Basic 우회 stop을 보완했다. `Rogue10mTechniqueInterruptionTests.cpp`의6개는 실제 자원 delegate2, native montage identity, 완료 listener 중단/Anim 교체2, 실제 TakeDamage 중단을 검사한다. 추가 반사/일반 기술 회귀는 `Rogue10mHitInterruptionTests.cpp`와 native 테스트 표적에 작성했다. 모두 **작성·정적 검토 상태로 엔진 실행0회**다. 테스트의 합성 montage state/기지급 sentinel은 실제 에셋 재생·생산 수락 검증과 분리한다.
- 근거: [검증 기록](evidence/melee-skill-content-20261111/README.md), [원본 파일 인벤토리](evidence/melee-skill-content-20261111/source-asset-inventory.json). 수치 통과와 실제3P 시각 완료는 분리한다. 새 애니메이션 편집/재생/화면 촬영은 이번 작업에서 하지 않았다.

## 통합 담당자 작업

1. 변경 native 구현9개와 새 UE 회귀3개, 제작/독립 테스트5개를 확인하고 공유 Source 소유자에게 범위를 알린 뒤 패치를 통합한다. 구현9개는 Character.cpp, Combat.cpp/h, AttackSkillData.h, AdvanceTimeline.h, TechniqueMotion.cpp/h, BasicBrawler.cpp/h다. 회귀는 TechniqueInterruptionTests.cpp, HitInterruptionTests.cpp, HitInterruptionTestTarget.h다. 기존 staff/Combat/Character 작업을 덮어쓰지 않는다. `.Build.cs` 변경은 없다. 최초 V1 런타임 패치 대신 `Delivery/InterruptionFixB`의 수정본을 사용한다. V1을 먼저 반영했다면 그 사본에 대한 delta만 적용하고 full 패치를 중복 적용하지 않는다.
2. 담당자가 `Scripts/BuildEditor.ps1`로 UHT/Editor 빌드를 실행하고 재시작한다. `Rogue10m.TechniqueMotion`의 신규 중단/피격 검사, 카탈로그6·슬롯·기존 무기/마우스·아이덴티티/맨손·공격자원·특성 회귀를 실행한다. 새 native 테스트 표적 UCLASS가 있어 UHT도 필요하다. 이번 작업이 이 빌드·QA를 실행하지 않았다.
3. Editor 단독 사용권 아래 아래 dry run을 먼저 검토한다. 매번 새 label을 사용한다. stale reflection/없는 montage/다른 destination/변경된 prototype은 덮어쓰지 않는다. save 실패는 부분 후보와 근거를 남기므로 원인 확인 없이 반복하지 않는다.

```python
import runpy
from pathlib import Path
import unreal
project = Path(unreal.Paths.project_dir())
author = runpy.run_path(str(project / 'Scripts/Editor/StageMeleeSkillPrototypes20261111.py'))
author['stage_prototypes'](apply=False, label='MeleePlan11A')
# 검토 후 담당자가 새 label로 실제 생성
author['stage_prototypes'](apply=True, label='MeleeStage11A')
# create_staged_profiles=True는 production 미할당 fixture가 필요할 때만 별도로 사용
```

생성 경로는 `/Game/Rogue10m/Skills/MeleeCatalog20261111/{Knuckle|GreatSword}/DA_Prototype_{K01..K10|G01..G10}`다. 현재 production 프로필에 연결하지 않는다. 재로드 후 실제 원본/필드 해시와10후보·6장착·전용 교차차단을 확인한다.

4. flat-ground 성공, 벽 앞 종점/벽 너머 무피해, 절벽/hitch/지지면 소실, 제자리 방향, 공속 snapshot, 회피와 양방향 충돌, 메뉴/사망/교체/EndPlay/텔레포트·몽타주 실패/취소 콜백에서 늦은 피해0을 실제 플레이로 검증한다. 비용1회·기존 실행ID당 종점 단타1회·원본 세이브 보존을 확인한다. 비용 delegate 동기 취소에서도 비용/use/공유쿨다운1회·피해0, 기존 긴 deadline 보존을 검증한다. 이동 중/종점 대기/접촉 직전 Stop·Pause·같은 Montage 재시작·AnimInstance 교체와 앞선 완료 listener 중단을 검사한다. 정상 blend-out 전에 접촉해야 하며 중단 후 재생 재개가 옛 타격을 살리지 못해야 한다.
5. `Docs/AnimationRecordingGuide.md`에 따라 새 DevLog 기능 폴더에 ready/contact-or-first-damage/recovery 캡처, 실제 montage/sequence/skeleton과 출처, authored/observed timing·손/무기 궤적을 남긴다. 프로그램 입력·물리 입력·풀런을 구분한다. VFX는 실제 scale/부착/수명·빗나감/무적·정리까지 확인한다.

피격 QA에는 전진/종점 대기/일반기술/다단타 도중 양수 피해, 비용 또는 Play 콜백 속 피해, 0/거절된 피해, HP 비용·회복, 사망과 중복 취소, 첫 타격 반사와 두 번째 표적 필터 반사를 포함한다. 이미 적용된 피해/credit1회와 이후 피해0을 분리한다. 면역 시스템 평가·Windows 패키징은 상위 담당자 범위이며 이 작업에서 에디터/UBT나 공유 소스를 사용하지 않았다.

빌드·에디터 사용권은 통합 담당자에게 있다. 공유 반영·커밋·푸시·production 에셋 연결은 수행하지 않았다. 구조/독립 테스트 완료와 게임플레이 완성 사이의 남은 차단은 **UE 컴파일/물리 검증 및 미구현 고유기능·모션/VFX**다.
