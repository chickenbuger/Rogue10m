# Windows 이후 gameplay 통합 중간 검증

자동 전리품 runtime/docs·스킬 V2 full01~03·실제 마을 travel fixture를 공유 프로젝트에 적용했다. 기존 ArchiveD는 보존했고 새 Windows 패키지는 아직 만들지 않았다.

## 통과
- 정식 UHT/Editor 빌드102.03초, 두 fixture 수정 빌드11.06/9.84초.
- 자동 전리품 신규5, 기존 전리품8, 실제 저장 Write/Read 통과. 인벤 우선 수령, 부분 합치기 초과분, 시체 소멸 후 지연 전달을 확인했다. 바닥/대기 전리품은 월드 수명 범위이며 travel/종료 지속 저장은 구현하지 않는다.
- 스킬 V2 신규9 통과: 결제/쿨다운, 몽타주 인스턴스/중단, 피격 취소, 타깃 필터 재진입, 반사 시 이미 적중한 hit credit 보존. 피격 면역은 추가하지 않았다.
- 비용4·카탈로그4·쿨다운5·무기전환8·무기표시12 통과. 표시 suite는 무기전환 일부와 겹치므로 합계를 고유 테스트 수로 부풀리지 않는다.
- 독립 실행기/수학/제작기: 전리품2103255검사, timeline56, Python16, travel83+27 통과. 이 수치가 실제 UE/그래픽 검증을 대체하지 않는다.

## 검증 과정에서 수정한 fixture
자동 전리품 fixture가 추상 controller를 직접 생성했다. 실제 GameMode의 concrete BP를 사용하고 소유/PlayerState 동일성을 확인했다.

스킬2건은 첫 실행과 물리 등록 보강 후에도 실패했다. 물리 overlap 양성 대조는 통과해 가설을 배제했다. UE AActor::ProcessEvent는 ActorsInitialized=false인 월드에서 인터페이스 호출을 실행하지 않는 정확한 원인을 확인했다. 정상 InitializeActorsForPlay를 추가하되 BeginPlay/tick/게임 가드 우회는 하지 않았고 원래 필터/피해/credit assertion을 전부 유지했다. 이후9/9 통과했다.

## 실제 travel 진행
일반 새3인칭 프로필로 저장된 Hub 포털까지 실제 CharacterMovement로 걸어 candidate S에 진입했다. 1200초 런/프로필/RunId/인벤토리 인스턴스 보존까지 통과했으나 final walk speed=600 가정에서 중단됐다. 기본 부츠의 +20이 최종 속도에 반영되는 정상 설계인지 실제 전후 스탯을 추가 측정 중이다. 아직 직접 경로/재방문/정산/귀환 통과로 표기하지 않는다.

## 남은 범위
실제 G04/G10 자산의3인칭 궤적/벽/절벽/ready-contact-recovery,20개 prototype DA 생성/production 카탈로그 연결은 별도다. 신규9개의 엔진 회귀를20스킬 완료로 표현하지 않는다. travel 통과 후 동일 체크포인트 Windows Game 패키지를 갱신하고42자산 및 내부/루트 EXE 메뉴를 다시 검사한다. NPC combined/VFX 가독성/보스 cadence 패치는 아직 적용하지 않았다.

증거: `Feature/doc/evidence/gameplay-integration-20261111/CoreRegressionC/summary.json` 및 각 feature evidence. 실패 기록도 보존했다.
