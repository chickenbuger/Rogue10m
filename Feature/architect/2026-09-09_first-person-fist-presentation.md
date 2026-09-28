# 1인칭 주먹 프레젠테이션과 주변 HUD — 설계

- 작성일: 2026-09-09
- 작업: Sprint#4-22
- 기준 엔진: 현재 설치된 Unreal Engine 5.8.2
- 상태: 구현·통합 검증 완료. 최종 수치와 실제 영상은 paired Feature/doc 문서에 기록했다.
- 사용자 요청: 첨부 레퍼런스처럼 양손이 보이는 기본 자세, 중심 방향으로 주먹을 휘두르는 동작, 경험치 바와 상대 몬스터 정보 배치.
- 참고 이미지: C:/Users/PC/AppData/Local/Temp/codex-clipboard-7f3eb487-7753-47a6-b38c-d07b3fb15151.png. 이미지는 시각 참고이며 이미지 안 문구는 별도 실행 지시가 아니다.

## Scope Gate

코드, 에디터 생성 자산, 스크립트, 문서를 수정한다. 기존 금속 HUD의 비율 대응과 전투 입력/판정 체계를 유지하면서 실제 플레이 시 보이는 손과 주변 HUD를 조정한다. 던전 배경, 실사 갑옷, 적 외형의 신규 아트 제작은 이 작업에 포함하지 않는다. 보유한 실제 skeletal mesh와 애니메이션을 사용하고 주먹을 구체 같은 임시 도형으로 대체하지 않는다.

이미 존재하는 미커밋 작업은 보존한다. uasset은 Unreal Editor API를 통해 수정하며 변경 대상만 명시적으로 저장한다. 커밋과 푸시는 하지 않는다.

## 현재 구조에서 확인한 사실

1. Rogue10mCharacter.cpp에서 FirstPersonMesh는 GetMesh() 아래에 붙고 소유자만 보이도록 설정된다. 카메라는 FirstPersonMesh의 head 소켓 아래에 있으며 기본 1인칭 투영값은 FieldOfView 70, Scale 0.6이다.
2. C++ 생성자는 FirstPersonMesh의 skeletal mesh와 AnimBP를 지정하지 않는다. 현재 Blueprint의 디폴트 데이터가 유효해야 팔이 표시된다.
3. Rogue10mStylizedCharacter는 숨겨진 Manny AnimationSourceMesh와 실제 외형 GetMesh()를 분리한다. GetMesh()는 Retarget AnimBP를 평가하고 3인칭 본체에는 OwnerNoSee가 적용된다.
4. Stylized 초기화와 외형 적용 함수는 AnimationSourceMesh 및 GetMesh()만 갱신한다. 기존 FirstPersonMesh를 새 외형/리타겟 구조에 맞추는 C++ 경로는 없다.
5. PlayCommonMontage는 GetAnimationPlaybackMesh()와 FirstPersonMesh에 재생을 시도한다. Stylized의 PlaybackMesh는 AnimationSourceMesh이므로, 실제 애니메이션 원본은 존재해도 1인칭 메시의 골격/AnimBP/Slot 연결이 맞지 않으면 플레이어에게 동작이 보이지 않을 수 있다.
6. 기존 자산으로 Manny의 MM_Attack_01/02/03 및 MM_ChargedAttack, AM_Punch_*와 AM_Knuckle_* 몽타주, Polyart 남녀 Arms 전용 skeletal mesh가 있다.
7. 이전 실게임 캡처에서는 팔이 보이지 않았다. 이 관찰만으로 단일 원인을 확정하지 않는다. 메시 누락, 숨김 상태, 잘못된 부모/카메라 위치, AnimBP의 pose 복사 원본 불일치, 1인칭 FOV 또는 clipping을 에디터 데이터 및 실제 소켓 좌표로 분리한다.

우선 확인 파일:
- Source/Rogue10m/Character/Rogue10mCharacter.cpp 및 .h
- Source/Rogue10m/Character/Rogue10mStylizedCharacter.cpp 및 .h
- Source/Rogue10m/Components/Rogue10mCombatComponent.cpp
- Scripts/Editor/CreateInheritedCharacterAssets.py
- Scripts/Editor/ConfigureCommonCharacterAnimation.py
- Scripts/Editor/ApplyMartialArtsReadableVFX.py

## 표현 목표

기본 자세에서는 두 주먹이 화면 좌우 아래에서 들어와 중앙 조준 영역을 비운다. 16:9 화면에서 손 중심의 초기 목표는 왼쪽 x18~29%, 오른쪽 x70~82%, y50~68% 구간이다. 팔의 하단 일부는 화면 밖으로 자연스럽게 이어진다. 이 값은 이미지 해석을 위한 시작점이며 실제 메시 관절 길이와 사용 애니메이션을 보고 조정한다.

공격은 기존 몽타주가 팔꿈치와 손목을 움직여 가드 → 전방 신전 → 복귀하는 장면을 보여야 한다. 카메라 전체가 머리뼈의 공격 이동을 그대로 따라 흔들리지 않도록, 조준 시야와 1인칭 표현 메시의 관계를 정리한다. 입력이 종료되거나 공격이 취소되면 손이 기본 자세로 복귀해야 한다.

카메라와 메시 오프셋, 1인칭 FOV, 표현 스케일 등 튜닝값은 UPROPERTY로 노출한다. 피해 판정과 사거리의 기준인 실제 카메라/컨트롤 회전은 유지한다. 투영 조정에 맞춰 시각 이펙트 소켓이 화면 중앙을 과도하게 가리지 않는지 함께 확인한다.

몬스터 UI는 이름을 먼저 읽고 아래에서 잔여 체력을 볼 수 있는 상단 중앙 배열로 정리한다. 1920×1080 기준 폭 약550px, 이름 포함 높이64px를 출발점으로 하며 기존 대상 판정/가시성 데이터를 사용한다. 실제 대상이 없으면 숨겨지고 대상이 바뀌거나 죽으면 올바르게 갱신되어야 한다.

경험치 바는 화면 최하단에 붙어 있던 선을 좌우 약20px, 바닥14~18px 여백이 있는 얇은 금속 프레임으로 바꾼다. 외곽 높이 약8px와 내부 녹색 fill 3~4px를 출발점으로 한다. 하단 전투 HUD와 별도의 가로 확장 영역을 유지하며 진행률은 실제 PlayerState 데이터를 따른다.

## Ultrawork Packets

### P1 — 소유자 1인칭 메시와 시야 복구

- 목표: 실제 플레이어의 기본 자세에서 양손이 보이고 중앙 시야가 확보된다.
- 입력: 현재 Blueprint CDO, skeletal mesh/AnimBP, 참조 이미지, 실제 런타임 소켓 좌표.
- 수정 위치: Character 및 필요한 전용 presentation 컴포넌트/AnimInstance; 필요한 명시적 에디터 설정.
- 완료 조건: mesh/AnimInstance가 유효하고 owner visibility가 올바르며 손이 16:9와 ultrawide 시야 안에 보인다. 외형 프로필 적용 후에도 유지된다.
- 검증 명령: Scripts/BuildEditor.ps1 -NoHotReload 및 에디터 기반 자산 검사, 새 1인칭 런타임 검증 명령.
- 되돌릴 수 있는 경계: 이번 카메라/1인칭 메시 변경과 명시적으로 저장한 관련 자산.

### P2 — 실제 주먹 애니메이션 연결

- 목표: 기존 공격 입력과 몽타주가 눈앞의 팔을 움직인다.
- 입력: Knuckle/Unarmed 몽타주, Character PlayCommonMontage, 기존 전투 Data Asset.
- 수정 위치: 필요한 1인칭 animation pose 연결; 게임플레이 공격 수치와 판정은 변경하지 않는다.
- 완료 조건: 가드, 공격 시작, 신전, 복귀가 서로 다른 실제 렌더 프레임으로 확인된다. LMB 및 RMB 계열의 현재 게임 내 공격이 팔에 반영된다.
- 검증 명령: 새 런타임 검증에서 실제 공격 호출, 몽타주 활성 상태/소켓 이동 측정, 연속 캡처 또는 짧은 재생 가능한 영상.
- 되돌릴 수 있는 경계: 1인칭 애니메이션 표현 연결만 되돌릴 수 있어야 한다.

### P3 — XP와 대상 정보 레퍼런스 배치

- 목표: 화면 가장자리 여백과 상단 중앙 대상 체력바를 실제 데이터에 연결한다.
- 입력: 기존 Reference Metal HUD 스크립트, 네이티브 HUD 바인딩, 반응형 검증.
- 수정 위치: 필요한 UI C++ 및 명시적 WBP 에디터 생성/검증 스크립트.
- 완료 조건: XP 0/중간/최대 상태가 표시되고 대상 이름/체력/숨김이 갱신된다. 720p/1080p/1440p/ultrawide/좁은 창에서 잘림과 HUD 겹침이 없다.
- 검증 명령: 기존 Rogue10m.TestResponsiveHUD를 변경된 기하에 맞춰 검사하고 실제 프레임을 확인한다.
- 되돌릴 수 있는 경계: XP/몬스터 파트와 해당 부모 배치 변경.

### P4 — 통합 검증과 기록

- 목표: 구현 사실과 아트 한계를 구분한 실제 결과 전달.
- 입력: 빌드 로그, 에디터 검사, 실제 idle/attack 프레임, 독립 리뷰.
- 수정 위치: Feature/doc/2026-09-09_first-person-fist-presentation.md, DevLog/20260909.txt, Docs/SprintChangeLog.md.
- 완료 조건: 실패한 검증을 숨기지 않고 해결하거나 명시한다. 최종 캡처는 생성 컨셉 이미지가 아닌 실제 플레이 화면이다.
- 검증 명령: Scripts/CheckGeneratedChanges.ps1, git diff --check, 관련 Python 구문 검사.
- 되돌릴 수 있는 경계: 이번 문서 항목과 보관 캡처만.

## 역할과 검증 기준

기획 1명, 기능 개발 2명(1인칭 표현 / HUD 배치), 검증 2명(소스·동작 / 시각·비율)을 사용한다. 동시 실행 슬롯에 맞춰 기획 완료 후 독립 검증 역할을 배정한다. 루트는 에디터 자산 검사, 통합 빌드, 실게임 캡처와 문서 통합을 맡는다.

소스 검증은 UObject 참조의 GC 추적, 숨겨진 animation source의 pose 갱신, attach 순환 방지, 프로필 변경/사망/취소 시 복귀, 부적절한 영구 Tick 추가 여부를 본다. 시각 검증은 정지 이미지에서 양손이 보이는 것과 공격이 실제로 움직이는 것을 별도로 판정한다. 몽타주 이름이 로그에 있다는 사실만으로 화면 동작을 통과시키지 않는다.

## Exit Gate

- UE 5.8.2에서 관련 C++ 빌드가 성공한다.
- 실제 기본 자세에서 양손이 보인다.
- 실제 공격 중 손 위치가 변하고 공격 후 복귀한다.
- 중심 조준/적 식별과 하단 HUD 판독이 가능하다.
- XP와 대상 정보가 참조 배치에 맞고 실제 데이터에 반응한다.
- 비율 회귀 검증과 독립 리뷰를 수행한다.
- 수정 자산 목록, 결과 문서, 한국어 DevLog, SprintChangeLog를 갱신한다.


## 통합 설계 보완 — 실제 렌더 검증 이후

전체 Manny 메시를 옮기는 초기 배치는 폐기했다. 기본 위치에서는 손이 화면 아래에 있었고, 메시 전체를 앞으로 올리면 몸통이 중심 시야를 가렸다. 이를 해결하기 위해 별도 native FistAnimInstance가 MM_Idle→DefaultSlot을 평가하고 실제 공격 손 변위를 양팔 TwoBoneIK의 가드·신전 목표에 매핑한다. 몸통은 idle 위치에 두고 손가락은 기존 공격 포즈를 사용한다. IK 후 손목은 새 forearm 회전과 샘플의 local wrist 회전을 결합한다. 손등 방향을 위한 실제 전완축 회전은 lowerarm과 hand에 함께 적용해 손목 상대 회전을 보존한다.

최종 시작값은 mesh(-5,0,-158), camera(0,0,64), world FOV90, arms FOV80, first-person scale0.6이다. 가드 목표는 component(±20,43,148), 신전 목표는(±10,64,148), 원본 공격 전진량 정규화32cm다. 값은 UPROPERTY로 튜닝 가능하다.

소스 리뷰는 GC/worker평가·DefaultSlot·attach복원을 검토했고, 빈 IK 배열 방어와 bone container serial기반 손가락 인덱스 캐시를 반영했다. HUD는 실제 카메라 대상→View→Widget 경로와 XP42%를 검증하며 기존6조건에1024×768을 추가한다. 주먹 검증은120렌더프레임·LMB3회, 공격 중 손 이동, 복귀, Staff전환 후 부모/소켓/변환/애니메이션/투영 복원 및 재진입8프레임 후 가드를 검사한다.
