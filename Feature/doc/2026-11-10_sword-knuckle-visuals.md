# 대검·너클 무기 표시와 모션 검증

## 범위와 현재 상태

작업 브랜치: Sprint#9-29-humanoid-boss-npc. 대검과 너클의 프로젝트 소유 메시를 추가하고, 대검이 기존 비무장 동작을 재사용하던 문제를 별도 Mixamo 동작으로 교체한다. 보스·NPC 원본 및 기존 FistAuthored 너클 애니메이션은 보존한다. 커밋·푸시는 수행하지 않는다.

사용자의 최신 지시에 따라 1인칭 신규 개발과 검수는 중단하고 3인칭에서 손 부착·준비·피해·회복을 확인한다. 뉴비 비무장 시작, 무기별 최대 10개 스킬과 6개 등록, Ctrl 아이덴티티는 별도 스킬·카메라 작업과 조율한다. 본 기능은 무기를 자동 장착하거나 초기 직업을 바꾸지 않는다.

## 구현

- Rogue10mWeaponVisualComponent: Tick 없이 실제 렌더되는 몸의 hand_r/hand_l에 무기를 붙인다. 대검 한 자루, 너클 양손, 비무장에서는 숨긴다. 메시 충돌은 끄고 기존 CombatComponent의 피해 판정을 유지한다.
- PrepareWeaponVisuals20261110.py: 프로젝트에서 직접 만든 저폴리 대검·너클과 금속·가죽 재질을 UE OBJ importer로 생성한다. 원본 OBJ/MTL과 생성 수치를 함께 보관한다.
- PrepareGreatSwordAnimations20261110.py: 로컬에 확보된 Mixamo Great Sword 후보 중 준비·걷기·달리기와 공격 5개를 별도 Source 폴더에 수입한다. Mixamo→Manny→기존 HumanMale 스켈레톤의 두 단계 리타깃 후보를 만든다.
- 기존 방향별 다리 보행을 유지하고 검을 쥔 팔 자세만 새 파생 동작으로 합친다. 기존 비무장 보행과 원본 Fab 자산은 덮어쓰지 않는다.
- FighterCombatStanceAnimInstance: 무기별 준비 자세와 보행 자산을 캐시하고 기존 그래프의 노출 핀으로 선택한다. 검을 들지 않은 상태의 비무장 전투 자세는 유지한다.
- Rogue10mWeaponAnimationAuthoring: 단일 준비 Sequence/보행 BlendSpace 핀과 전용 대검 몽타주 segment만 UE API로 연결한다. 컴파일과 바인딩을 확인한 후 Python caller가 저장한다.

## 검증 계획과 증거 구분

1. Editor target 전체 빌드. 2026-11-10 B2 빌드 성공. 첫 빌드의 UE5.8 TObjectPtr 테스트 반환형, 두 번째 빌드의 테스트 멤버 이름 충돌을 각각 수정했다.
2. 자동 Attachment test: 실제 손 bone, 메시 로드, 양손/한손/비무장 전환, collision 비활성, 컴포넌트 재사용을 검사한다.
3. AnimPose 원시 샘플과 손·검 궤적을 보관하고, 양손 사이 축으로 정적 그립 후보를 계산한다. 수치 검사만으로 시각 품질을 합격 처리하지 않는다.
4. 전용 3인칭 런타임 fixture에서 실제 bound-skill press/release, 수락된 몽타주, 대상 Health 감소, 준비/최초 피해/회복 캡처를 함께 기록한다. 합성 콜백 검증이며 물리 키·동시 클릭 검증과 구분한다.
5. 실제 스크린샷을 확인하고 그립·자세·피해 타이밍을 조정한 뒤에만 활성화 완료를 기록한다.

## 출처와 배포

- 무기 모델·재질: 프로젝트에서 새로 제작, 외부 모델이나 텍스처 없음.
- 대검 모션: 사용자 요청으로 확보한 Mixamo Great Sword 다운로드 폴더. 원본 FBX는 Downloads에 보존하고 수입한 소스 자산에 파일명·SHA256을 기록한다. 원시 FBX 재배포를 하지 않는다.
- 너클 모션: 기존 프로젝트의 FistAuthored 전용 5개 동작을 유지한다.

아직 실행하지 않은 검사는 완료로 표시하지 않는다. 최종 수치와 스크린샷, 누락 사항은 후속 결과에 덧붙인다.

## 후속 검토 및 수정

- C3 공통 native 빌드 성공. D 빌드에는 검수 전용 프로필 이름 길이 수정, 실제 렌더된 손 이동과 준비 자세의 발 높이 검사, 스크린샷 처리 완료 대기, 새 몽타주 길이 갱신을 추가했다. D 검증은 아직 실행 전이다.
- 초기 수입은 같은 프로세스에서만 성공했다. cold reload 로그를 확인해 생성된 SK_MixamoXBot_Skeleton.uasset가 저장되지 않은 것을 발견했다. 외부 FBX를 바꾸지 않고 수입 메시의 reference bones로 누락된 작업 전용 skeleton을 복원하고 mesh와 skeleton을 각각 저장하는 보완을 준비했다. 새 프로세스 재로드 확인 전에는 해결 완료로 기록하지 않는다.
- 선택한 앞의 대검 모션 두 개는 전방 검 궤적이 불충분해 대체 slash 두 개를 추가 조사한다. 선택되지 않은 후보는 생산 DataAsset에 연결하지 않는다. 선택 후보가 실패해도 기존 소스 8개와 완료된 메시 검증은 독립적으로 진행하도록 분리했다.
- 기존 비무장 다리 보행 위에 검 팔 자세를 합친 파생본은 준비 자세 변경 때마다 다시 갱신한다. HumanMale 샘플은 실제 게임에서 쓰는 기존 IKR_Manny_To_Hu_M 리타깃 정책과 동일하게 만들어 런타임과 다른 별도 정렬의 그립을 잘못 승인하지 않도록 했다.
- 초기 ForceRootLock 후보의 수치 기록은 RawForceLockCandidateA 폴더에 보존했다. head-foot 간격과 root 이동만으로는 전신이 지면 아래로 이동한 오류를 찾을 수 없으므로 실제 캡슐 바닥 대비 발 위치와 렌더된 포즈를 별도로 확인한다.
- 스크린샷은 요청한 순간 바로 저장되지 않는다. 준비·회복 캡처가 처리되기 전에 공격, 다음 무기 전환, 종료가 일어나지 않도록 했다. 최초 피해 직후 캡처는 요청/처리 시각을 각각 기록하며 정확히 같은 프레임이라고 부르지 않는다.

## ResumeSword1 실제 적용 상태
- 2026-11-10 UTC: 누락된 작업 복제본 BS 1개와 Montage 5개를 Editor API로 저장했다. 모든 원본·스킬 참조는 유지했다.
- 준비/이동 그래프는 임의의 SequencePlayer를 고르지 않고 FighterCombatStanceAlpha→TwoWayBlend.B 실제 연결로 식별한다. Walk / Run의 정확한 BlendSpace player만 연결했다.
- 실제 원본 그리드는 27 sample 좌표, 17 unique clip이다. 대검 A_Armed_17종으로 전 sample을 교체하고 Common AnimBP와 BS를 저장했다. Blueprint compile, section/notify/skeleton preflight와 10개 전체 skill snapshot 불변 통과.
- SwordResumeB: 기존 무기 스킬10개 전부 route/accept/complete, 34개 캡처 요청/처리 및 기존432개 save 보존 통과. 아직 새 대검 공격의 시각/접촉 승인은 아니다.
- PreviewAligned_61da49fe: 실제 pose refresh 후21개 PNG, component/sequence 양손·머리 오차를 함께 기록. 기준 자세로 찍힌 이전 D/E 후보는 승인 근거에서 제외했다.
- 왼손 그립 드리프트는 bone-finalized delegate에서 두 palm을 이용하는 3P presentation 보정으로 구현했다. 원본 roll Y 계약 유지, 비정상 좌표 static fallback, 외부 transform/부착 소유권 존중. Tick/피해/충돌/슬롯/1P 신규 개발 없음.
- 기본 공격 후보의 정면 궤적 문제는 남아 있다. AltSlash04/05 새 작업 후보는 import/retarget 저장됐으나 정지 reference pose가 검출되어 미승인·미연결 상태다. import 성공과 모션 성공을 구분한다.
- 이동8방향 및 무기 전환 회귀, 양손 보정 실제 게임 화면 검수는 이어서 진행한다. 물리 입력·일반 필드 완주를 수행했다고 주장하지 않는다.


## ResumeSword1 실제 공격 활성화 및 통합 회귀 후속

- 대검 5개 편집 Montage와 접촉 지연을 실제 스킬에 적용했다. 원본 FBX/애니메이션과 기존 밸런스/슬롯/VFX 필드는 보존했다.
- SwordAuthoredA 실제 게임 렌더에서 기존 너클/대검 10개 공격과 PNG 34장을 검증했다. 대검 5개의 최초 HP 감소 관측에서 실제 타깃 캡슐과 칼날 선분 거리 조건을 통과했다. 실패 0, 기존 저장 432개 해시 보존이다.
- 34장 모두 직접 확인했다. 대검의 공격과 무장 자세 복귀, 너클의 주먹 공격과 검 잔존 없음은 보인다. 후방 시점·블러·지연 캡처 때문에 정확한 접촉 프레임, 손가락 표면, 연속 동작까지 승인한 것은 아니다. 기존 두 스킬의 시전/적중 VFX는 사진에서 뚜렷하지 않아 시각 완료로 처리하지 않았다.
- 공유 콘텐츠에서 CatalogSharedA 소비/정책/성장 6개 검사가 통과했다. 20개 신기술 콘텐츠 및 카탈로그 활성화 완료를 뜻하지 않는다.
- WeaponIntegratedA는 10/11, BossTargetA는 1/5였다. Staff fixture의 ASC 초기화 누락과 Boss fixture의 abstract 캐릭터 생성 실패를 확인하여 검사 준비 코드만 수정했다. 실패 기록을 보존했고 새 빌드/새 라벨 재검증 전까지 통과로 계산하지 않는다.
- 드롭 자동 재획득 4파일 패치를 백업 후 통합했다. 파티클 30개는 아직 실제 생성 전이며 검증 준비 도구만 작성했다.
- 근거: SwordAuthoredA/rendered-review.json, catalog-consumption-20261110/CatalogSharedA, weapon-presentation/WeaponIntegratedA, tmp/work-boss-ai-target-loss-20261110/Engine_BossTargetA.


## 통합 회귀 및 파티클 전체 실제 화면 검토 마무리

- 무기11, 타깃 생명주기5, 카탈로그 소비/성장6, 드롭 전체8 및 Write26/Read9, 자연AI112개 검사 결과를 통합했다. 기존432개 저장 해시를 현재 디스크와 재대조해 변경·소실0을 확인했다.
- SkillParticlesReviewG에서 기존30후보 전부 실제 age/입자1·Color/Size/NormalizedAge와 PNG완료를 확인하고30장을 직접 보았다. 보호56개 해시 동일, 소유 임시 NiagaraActor30개 정리, 오류/ensure0이다.
- 기본 효과 형태는 보이지만 일부 시전은 몸/손에 겹치고 전반 대비가 낮다. 최종 게임 가시성/스킬 연결 승인은 아니다. 별도 MIC/NS gain8 프로토타입1개는 더 명확한 금색 시전 flash로 확인했으며 기존30개에는 확대하지 않았다.
- 장판 첫 사용/cooked 패키지 가시성, 실제 스킬 scale/dilation/부착/타격 연계, 연속 ribbon, 지팡이 모델 지정, 신규20 기술 콘텐츠는 후속 범위다. 1인칭은 계속 보류했다.
- 소유 PID55388은22:28:07 UTC 정상 종료 로그를 남겼고22:30:28 UTC UE/빌드/셰이더0을 확인했다. 엔진 종료 뒤 launcher의 PowerShell false/$false 오타가 있었으나 실제 작업은 반복하지 않고 캡처·종료로그·프로세스 상태를 독립 확인해 근거를 보완했다.
- 최종 근거: Feature/doc/evidence/sword-knuckle-20261110/particle-candidates/final-integration-summary.json 및 SkillParticlesReviewG/visual-review.json.
[Notion 요약 후보] 대검 편집 모션과 통합 기능 회귀를 검증했고, 너클·대검 파티클30후보 전부 실제 렌더를 검토했다. 최종 게임 연결 전 대비/위치/실제 전투 조건 보완이 남아 있으며 원본·저장·마법 자산은 보존했다.
