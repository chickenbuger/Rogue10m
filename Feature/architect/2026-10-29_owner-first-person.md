# 소유자 1인칭 팔·몸 분리

기준9cdf8622, Sprint#9-14-owner-first-person. 세이브259개와 기존 미추적 증거를 합친905파일을 독립 보존·SHA256 검증했다. 이전 실패 카메라 후보와 사진을 보존한다. 원격 push/병합, 신규 구매, 실제 세이브 삭제·초기화, 맵 변경은 하지 않는다.

## ULW 묶음

1. 읽기 전용 자산 감사: 기존 팔 제작기,6종 appearance body/skeleton/LOD/재질과 현재 실제 source/body 경계를 확인한다.
2. 원본 보존 팔 파생: 기존 CloneMeshDescription 필터를 재사용해 새 OwnerArms 경로에만 팔 geometry를 만든다. 모든 골격/refpose/재질/LOD 검증을 유지하고 원본·맵은 저장하지 않는다.
3. 표시 전용 런타임: 원래 Hero body의 최종 pose를 공유하는 transient OnlyOwnerSee 팔 component. 원래 body/source의 AnimClass·pose·몽타주·Notify·root motion·공격 clock·히트·충돌은 유지한다. 카메라 원점은 건드리지 않는다. body는 OwnerNoSee만 제어해 다른 시점/그림자를 보존하며 inspection/사망/외형 변경/종료는 복원한다. 기존 FirstPersonMesh는 재활성화하지 않는다.
4. 실제 검증: Original/Weighted의 하향 정지·달리기·E 회피·벽/공격 조합에서 원본 pose 불변, proxy/source 손 위치, 실제 명중·쿨다운, inspection/그림자 경계를 검사하고 D3D11 실제 사진으로 가림 판정한다. 프로그램 입력과 물리 플레이를 구분한다.

완료 조건은 수치 PASS와 실제 시야 가림 해소가 함께 충족되는 것이다. 원본 body를 숨긴다고 전신 skeleton/spine을 축소하거나, 카메라를 몸 밖으로 밀거나, 몽타주를 새 proxy에서 재생하지 않는다. 프록시 재질은 실제 body override를 복사하여 피부색을 유지한다. 현재 너클은 맨손 외형이며 새 무기 모델은 생성·구매하지 않는다. 향후 무기 부착을 위한 표시 전용 mesh 접근을 두되 게임플레이 getter를 바꾸지 않는다.

관련 코드/에셋을 빌드·검토하고 Feature/doc 결과, DevLog/20261029.txt, SprintChangeLog를 기록한다. 실패·실험 사진은 성공 증거로 바꾸지 않는다. API/뷰 변화에 맞게 기존 QA의 가시성 계약을 명시적으로 확장하며 수치 허용값을 느슨하게 바꾸지 않는다.
