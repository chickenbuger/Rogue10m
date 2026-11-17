# 활성 무기 VFX 가독성 개선 계획

## 범위와 선택 근거
이미 연결된 너클 Primary03(HookFlurry) 및 대검 Charged(Heavy) 두 스킬의 Cast/Impact 네 효과만 다룬다. 기존 프로젝트 소유 WeaponFantasy20261110의 M_ 및 NS_를 재사용한다. 단일 CPU emitter·단일 sprite·finite burst 구조는 유지하고, 원본 수정 대신 새로운 MI/NS 8개를 ActiveReadability/<fresh label>에 만든다. 원본의 절차적 UV 마스크와 프로젝트 로컬 IronShell 계통을 재사용하므로 외부 다운로드·라이선스·구매가 필요 없다. 현재 Fab 계정 소유권을 재검증한 작업으로 표현하지 않는다.

## 가설과 변경 계약
기존 독립 Gain8 프로토타입은 선형 RGB를 8배로 만든 결과이지 지각 밝기 8배를 입증한 것이 아니다. 이번 작업도 그 차이를 지킨다.

- MI: 각 원본 M_를 부모로 연결하고 CueTint의 RGB만 8배, alpha는 상속값 그대로.
- NS: 각 활성 원본을 새로 복사하고 renderer Material만 새 MI로 변경. 수명·크기·로컬 좌표계·입자 수·속도·모듈 입력은 동결.
- 두 DA: Cast/Impact 참조 및 CastEffectScale/ImpactEffectScale 네 필드만 변경.
- 너클 scale: .20 / .30. 대검 scale: .25 / .36.
- dilation, emission duration, charge 참조/크기, 모든 FP override, montage, hit delay, damage, resources, input, unlock, 카메라와 생산 C++는 동결.
- 기본 scale은 FP 배율에도 적용되지만 현재 FP scale 수치 상한(.0601/.1801)을 넘지 않는지 확인한다. FP 실제 화면 검수 완료로 표현하지 않는다.

## 구현과 가드
AuthorActiveWeaponReadability20261110.py는 공식 MCP live schema와 프로젝트 skills를 먼저 읽고, 새 label에서만 변경한다. 출력 manifest에는 mutation 단계별 기록, 실제 tint, compile/stack 결과, 원본/readback, 59개 보호 패키지 해시를 보관한다. 원본30 후보, Magic8, 독립 gain8 prototype2, 원본/재사용 NS·M_, 기존10 DA를 보호하며 activation은 하지 않는다.

ActivateActiveWeaponReadability20261110.py는 기존 activation.json 및 별도 대검 motion evidence를 기존 reconcile_reviewed_motion으로 합쳐 67개 반영 필드를 비교한다. 두 DA의 디스크 백업과 before snapshot을 먼저 남기고, 양쪽 live readback이 네 허용 필드만 달라졌는지 확인한 뒤 두 DA만 저장한다. 63개 필드는 각 DA마다 동결한다. cold validate와 명시적 rollback을 제공한다. 반복 실행과 모호한 저장 재시도는 금지한다. rollback은 기록된 before/after와 현재 상태가 일치할 때만 에디터 API로 원래 네 필드를 복원하며 새 파생 파일은 삭제하지 않는다.

## 실제 게임 검증
Rogue10mWeaponVisualRuntimeTest.cpp 한 테스트 파일만 변경한다. 기존 10종·34 PNG 및 strict sword capsule proxy 기준을 유지한다. 두 활성 스킬의 추가 Cast/Impact 캡처 네 장은 정확한 자산 포인터·현재 world·입력 이후 생성된 개별 NiagaraComponent를 확인하고, 최초 관측 후 2프레임 이상 및 controller GetAge()의 실제 simulated age > 0 이후 요청한다. DesiredAge를 실제 나이로 사용하지 않는다. vfx-components.csv와 캡처 완료/파일 크기를 확인하고 실제 PNG를 따로 읽는다. 수치 gate는 입자 픽셀의 가독성 승인이나 성능 검증이 아니다.

## ULW 완료/롤백 경계
1. 파생 제작: 8개 저장 및 compile/stack 0오류, 59개 원본 보존.
2. 제한 적용: 두 DA 네 필드만 변경, motion reconciliation 및 나머지 동결.
3. 빌드/회귀: 루트 독점 lease에서 빌드 후 fresh label strict34 게임 검증.
4. 시각 판정: 실제 acceptance/impact PNG, actual age와 frame evidence 확인. 과도한 flash/가림 또는 약함은 실패로 기록하고 기존 baseline으로 rollback 가능.

후속 수명·dilation·입자 구조·위치 조정은 이번 범위에 포함하지 않으며 결과를 보고 별도 결정한다. 원본30과 Magic 후보는 활성화하지 않는다.

## 발견된 3인칭 연결 결함의 수정 범위
실제 Niagara 관측에서 3인칭 카메라인데 cast 크기가 FP배율(.060/.056)이고 비활성 FP 메시를 참조했다. GetEffectAttachMesh에서 실제 3인칭 게임 모드를 먼저 확인해 렌더 GetMesh를 반환하도록 분기만 추가했다. 기존 appearance/FP fallback은 유지한다. 공통 함수이므로 두 스킬만의 데이터 변경과 달리 다른3P효과도 올바른 본체/원래크기를 사용하게 된다. 검증은 기존10스킬34사진의 비교, 개별cast부모/scale, 장착·전환11개 회귀까지 수행했다. 시전 픽셀의 최종 시인성은 별도 미승인이다.
