# 소유자 팔 시점: 검증 체크포인트

중앙 몸통 가림은 제거했고 누락된 Orc 여성 팔 메시 생성을 복구했다. 그러나 극하향/벽 접촉의 팔 절단면과 중립 손 프레이밍은 시각 검증에서 미완료다. 제품 기본값은 OFF를 유지한다. 이 기록은 시점 기능 전체 완료 또는 일반 필드 완주를 뜻하지 않는다.

- 프로젝트 D:\Project\Rogue10m, Sprint#9-14-owner-first-person. 시작 HEAD 9cdf86220775270a2a5f371b0418a0ca65460ff1. 최종 로컬 커밋은 CHECKPOINT.json 참조. push/병합 없음.
- 재개 때 기존 종료 문서/스냅샷은 없음을 확인했다. 이전905파일 무변경을 다시 확인하고, 현재925파일(세이브259)을 별도 물리 백업/SHA256 검증했다.
- UE5.8.3 CL58210709 / MSVC14.44 / SDK10.0.22621. 빌드A29.98초, 최종B11.25초 PASS.
- 팔 파생6개 완성. Or_F13842→3322 triangles,172bones,1982rendervertices,non_arm0. 원본25·기존팔5해시 불변, 별도 프로세스6개재로드 PASS.
- 자동회귀30개 PASS. Weighted4552/0,Original4664/0 실제D3D11Offscreen 각각20PNG. NPC F39/0 및3PNG. 포기메뉴 Normal89/0,SaveFailure97/0은 NullRHI 프로그램 UMG 검사다.
- 총43원본PNG 디코딩/SHA256 PASS. 선택한 핵심 이미지의 실제 시각 검토에서는 몸통 중앙가림 제거를 확인했으나 시각 게이트는 PARTIAL_FAIL이다. 전체 모션/모든 외형의 시각 통과가 아니다.
- 현 런너는 QA용 정상 너클 입장·autosave 분리 뒤 해당 pawn만 owner view를 활성화하며 종료시 OFF로 복원한다. CDO/프로필 설정은 저장하지 않는다. NPC·포기 회귀는 제품 기본 OFF 경로 검사다.
- 기존공격/카메라/콜리전/히트시계/자원규칙은 변경하지 않았다. 세 번의 동일프레임 OFF/ON으로 소스·body local pose, 카메라, montage instance/rate/clock,damage/use/CD불변을 확인했다. V inspection은 전체body/head복구와proxy숨김을 검사했다.
- 13_owned_greatsword는 메시가 없는 transient QA아이템의 인벤/트랙 전환 검사다. 실제 대검 외형 장착 검증으로 해석하지 않는다. 맨손 시작과 이후획득너클외형 계획은 유지한다.

## 재현

빌드 후 Scripts/VerifyOwnerView.ps1 -Label <fresh> -Profile Weighted -Rendered -Offscreen (또는 Original). 새label과격리QA프로필만사용한다. Scripts/VerifyNpcInteraction.ps1도새label로실행한다. 사용자세이브를 QA 슬롯으로지정하지않는다. 일반플레이에서새시점기본ON은아직하지않는다.

## 실패 원인과 미실행

기존 DeleteTriangles assert는 전체 polygon 삭제로 수정해 Or_F 생성이 성공했다. 실제 원본 empty polygon은0이므로 '원본에 빈polygon존재' 가설은 폐기한다. 엔진 lazy-index 삭제순서로 빈polygon이 남는 경로는 코드 분석상 후보이며 당시 index 상태를 계측해 완전 입증한 것은 아니다. 이전 실패 manifest는 status=creating 그대로 보존했다.

팔은 body의 동일 leader pose/world basis를 사용한다. 중립 손의 view depth가 약5cm/-28cm이고 아래쪽 약70cm여서 프레임 밖이다. 극하향/벽에서는 upperarm 절단면이 카메라 가까이 투영되는 문제가 남았다. 대시 검은 영역은 팔 형상과 월드몸체 그림자를 아직 분리하지 못했다. 단순 카메라 후퇴·본체 포즈변형·눈대중 팔 전진을 제품에 적용하지 않았다.

물리키보드/마우스입력, 일반필드완주, six-appearance gameplay, packaging/cook, 전체모션 clipping-free는 미실행이다. 기존 Humanoid dependency/portal collision 경고는 원본을 변경하지 않고 남겼다. 성능 향상률을 측정하거나 주장하지 않는다.

## 다음 재개 첫 단계

기존6개메시와실패캡처를보존한다. 격리QA에서월드몸체그림자와실제CPUskinned 정점근평면투영을분리측정하고, body/source/camera/hit계약을 유지하는 소유자 표현의 포즈·프레이밍 후보를 검증한다. 시각수용전에는제품기본OFF를유지한다.

../../DevLog/20261031_OwnerFirstPerson_Review/AnimationRecord.md에준비·수락·최초피해관측·회복사진과정확경로/시간/손궤적을기록했다. 기존애니메이션은편집하지않았다. 영상은촬영하지않았으며사진은실제게임뷰포트다.

증거: DevLog/20261031_OwnerFirstPerson_Review/
