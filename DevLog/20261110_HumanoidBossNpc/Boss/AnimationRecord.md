# 인간형 보스 애니메이션 기록

## 범위와 현재 상태
- 작업 사본: D:\Project\Rogue10m, Sprint#9-29-humanoid-boss-npc, UE 5.8.4.
- 보스는 기존 최종 보스 Azathor의 능력치·보상·패턴 판정·예고·맵을 유지하고 외형과 연출만 파생 자산으로 교체한다.
- 2026-11-10 최신 요청에 따라 1인칭 신규 개발·검수는 멈추고 3인칭 플레이만 검증한다.
- 현재 이 문서의 프리뷰는 실제 에디터 캡처다. 자산 활성화 및 18개 런타임 결과는 별도 측정 뒤 아래에 추가한다.

## 사용 자산과 출처
- 메시: /Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody.
- 원본은 Fab 보유 Polyart 캐릭터, Close Combat Animset, Combat Magic Animations 및 UE Manny 애니메이션을 사용한다. 원본은 수정하지 않는다.
- Idle NinjaIdle, Hook NinjaLeftHook, Hit NinjaBlockHit를 프로젝트 보스 스켈레톤으로 실제 리타깃했다.
- Wave AS_DualEnergyBlas, Zone AS_GroundManaShot을 같은 스켈레톤으로 실제 리타깃했다.
- Walk MF_Unarmed_Walk_Fwd, Death MM_Death_Back_01을 같은 스켈레톤으로 실제 리타깃했다.
- 파생 자산 폴더: /Game/Rogue10m/Characters/Boss/AzathorHuman20261110.
- 원본 SHA-256, 정확한 경로, 클립 길이 및 포즈: Feature/doc/evidence/humanoid-boss-npc-20261110/boss-retarget.json과 A_HumanBoss_*-poses.json.

## 연출과 타이밍 계약
- BT/BB가 대상 선정·추격·공격 결정을 담당한다. 네이티브 UAnimInstance는 Idle/Walk 블렌드와 DefaultSlot을 제공한다.
- 공격·피격·사망은 AnimMontage로 재생한다. 런타임에는 몽타주를 복제해 공유 자산 설정을 바꾸지 않는다.
- 기존 authoritative windup에서 피해를 판정한다. 소스 접촉 프레임이 해당 시점에 오도록 재생 속도를 정하고, 피해 이벤트에서 접촉 위치를 다시 맞춘 후 남은 클립을 기존 recovery 길이에 맞춘다.
- Hook 접촉: 0.500000초. 왼손 전방 궤적이 30fps 표본의 최대값에 도달하는 프레임이다.
- Wave 접촉: 2.233333초. 오른손 전방 뻗기의 최대 구간이다.
- Zone 접촉: 3.166667초. 지면 방향 시전 동작의 전방 뻗기 구간이다.
- 위 값은 작성한 소스 접촉 시점이다. 게임에서 실제 관찰한 피해 시각·몽타주 위치는 런타임 증거와 구분한다.
- root motion은 액터 이동을 지배하지 않는다. 공격 잠금 위치·예고 위치·판정 범위는 기존 C++ 계약을 유지한다.

## 실제 프리뷰 검토
- PreviewA: 실패. Python Rotator 위치 인자의 순서 때문에 프리뷰가 -90도 pitch로 누워 있었다.
- PreviewB: 회전은 교정했으나 아직 포즈 갱신 전 A-pose가 캡처됐다. 애니메이션 합격 근거로 쓰지 않는다.
- PreviewC: pitch/yaw/roll 명시, AlwaysTickPoseAndRefreshBones, editor animation Tick, 임시 background throttle 해제 후 포즈가 갱신됐다.
- PreviewC의 Idle/Hook/Wave/Zone 정면·사선 8장을 실제로 검토했다. 서 있는 격투 대기, 왼손 훅, 양손 전방 시전, 낮은 지면 시전이 각각 구분된다. 프리뷰 액터와 일시적 성능 설정은 종료 시 제거·복원했으며 맵은 저장하지 않았다.
- 프리뷰는 전투 중 피해/회피 성공의 증거가 아니다. 3인칭 실제 게임 캡처를 최종 판단 근거로 사용한다.

## AAA 레퍼런스 적용 범위
- God of War Baldur 첫 보스: 공격 전 준비 동작과 접촉·회복을 서로 읽을 수 있도록 나눈다.
  https://blog.playstation.com/2018/08/16/fighting-a-god-behind-the-scenes-of-god-of-wars-first-boss-battle/
- Ghost of Tsushima 전투: 전방 방향과 타격 실루엣이 예고와 맞아야 한다.
  https://blog.playstation.com/2020/06/23/ghost-of-tsushima-mastering-the-katana/
- 해당 게임 자산·애니메이션을 복제하지 않았다. 이번 파생 애니메이션을 AAA 품질로 인증하거나 전체 게임이 같은 품질이라고 주장하지 않는다.

## 검증 구분
- 자동화: 접촉/분노/회복 시간 계산과 legacy opt-in 안전성.
- 프로그램 입력 실제 렌더: 새 3인칭 보스 fixture의 정지/이동/Alt 회피 18개 케이스, BT/BB 구동, 손·발 궤적, 피해 시각, 예고·접촉·회복 캡처 예정.
- 물리 키보드/마우스 입력과 일반 필드 전체 플레이는 아직 수행하지 않았다.
- 피격/사망 클립의 최종 게임 화면 합격은 아직 별도 미검증이다.

## D2 검증 체크포인트
- HumanoidUnit_D2A에서 ContactTiming/ProfileTransitions2개가 경고0·오류0으로 통과했다. 일반 및 null Blueprint 클래스의 native→legacy/null 복원, 데칼 복원, 원본 RateScale2 불변·복제본1, 시퀀스 대체 몽타주 재생을 검사했다.
- ContactTiming은0.825000048초의 실제 float 계산을 출력한다. 이전 실패는 영어 literal0.825와 기본1e-8 허용오차의1ULP 차이였고, 현재1e-6 허용오차는 프레임 타이밍을 바꾸지 않는다.
- B2 피해 전 몽타주 진행의 최대 계산 오차는0.000002745초였다.18개 케이스에서 실제 손 움직임이5cm 이상이었다. 강제 접촉 정렬 뒤의 극소 오차와 구분한다.
- 사망 보완은 A_HumanBoss_Death_Right / AM_HumanBoss_Death_Right로 연결됐다. 실제 리타깃의 머리-발 높이는70.02~156.82cm이며 원본 SHA는 boss-death-adaptation.json에 보존했다. 최종 게임 수명주기 캡처 전까지 시각 합격은 보류한다.
- 별도 TP_D1은 실제3인칭/13마우스 케이스786검사 실패0, 기존 저장390개 보존이었다. 기본 주먹 실제 자산의 뉴비LMB1회와 무기 스킬 거부를 포함한다. 물리 입력·저장 재시작은 아니다.


## E2 최종 3인칭 판독

- 실행 binary: E2. SHA256 75C22003ED3D14222C9454C2A748180409290FF9F4AA0C91BC42A63AB3B79AE1.
- 18전투 + 피격/복귀/사망 생애주기, 451검사 실패0.
- 실제 3인칭 프로그램 InputKey, 2603프레임, PNG22장을 각각 원본으로 열어 검토했다.
- 기존394개 저장파일 SHA256 모두 보존, 신규 QA 저장2개만 생성했다. 엔진 Error/ensure/fatal0.
- 회복 전 montage phase 오차 최대 0.000002745028초, 접촉 전환점 위치오차0.0000003초, 보스 actor 위치변화 최대1.85cm. 모든 케이스 손 움직임5cm 초과.
- 첫 Niagara를 QA 시작 전에 컴파일하고 준비를 기다려 이전 첫 타격의 Preparing Niagara Systems 문구가 사라졌다. 게임플레이 windup·damage·cooldown은 바꾸지 않았다.
- 세 패턴의 일반/분노 ready·resolution·recovery를 구분할 수 있고 캐릭터 전체에 바닥 데칼이 칠해지지 않는다. 피격·복귀 및 짧은 사망 굽힘은220cm 구도에서 확인했다.


제한: 기존 범위판정의 물리접촉·지속시체·전종 자연AI·전체런 인증은 아니다. 최종 PNG별 SHA와 판독 메모는 Human3P_E1/visual-review.json을 따른다.
