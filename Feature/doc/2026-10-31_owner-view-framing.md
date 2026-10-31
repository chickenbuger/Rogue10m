# 소유자 팔 프레이밍 보정 결과

극하향·벽근접에서재현되던상부팔절단면돌출과중립손프레이밍을소유자표현복사본에서보정하고실제렌더로확인했다. 공격/회복때양손이중앙에겹친첫후보도보존한뒤목표범위를조절해다시검증했다. **제품기본은OFF유지**다. 일반플레이나모든모션의완성판으로표현하지않는다.

프로젝트 D:\Project\Rogue10m, branch Sprint#9-15-owner-view-framing. 시작HEAD7958b62148a339334d26c587509ac1b3e50c9c26. 새로컬커밋/SHA는CHECKPOINT.json참조. 원격push/병합은없다.

## 변경과 가역 경계

- OwnerArms: 기존leader-pose fallback유지. 별도UPoseableMesh에본체의평가된localpose를복사하고upperarm/lowerarm좌우4회전만IK보정한다. 모든local위치·스케일과손/손가락/나머지회전은보존한다. 독립AnimInstance·Notify·Tick·충돌·게임플레이시계는추가하지않았다.
- AppearanceCamera: 실제View계산뒤표현보정만호출한다. 카메라퇴각이나본체/source pose변형을하지않았다. 팔getter는실제활성USkinnedMeshComponent를반환한다.
- 안정중립기준캡처,동일몸체OFF/ON캐시유지,외형변경초기화,포즈캐시와독립인재질동기화를넣었다. 유효하지않은데이터는legacy표현으로fallback한다.
- 튜닝초안: 어깨앵커(5,0,-40)cm,ready손(45,±22,-24)cm,authored변위배율.65. goal범위앞25..90/각side12..42/높이-38..-14cm. 이는화면표현목표이며실제게임플레이공격거리나모든정점범위를뜻하지않는다.
- 새OwnerFramingQA/runner는팔뷰제외·몸체그림자OFF·실제CPU스킨정점/삼각형투영을분리한다. 기존OwnerViewQA는원본/보정손궤적을별도기록하고4회전외의원본pose·camera·clock·hit/use/CD불변을검사한다.

## 검증

UE5.8.3 CL58210709 / MSVC14.44 / SDK10.0.22621. 최종BuildG34.07초PASS. 자동회귀30개PASS(AttackResources4/Cooldown5/WeaponSwitch8/Requirements4/Hybrid3/CurrencyBaseline6).

BaselineB와최종CandidateB각15사진,진단검사252506/0. 최종WeightedB103802/0·OriginalB106518/0 각20사진. 이큰수는반복프레임/본/정점검사수이며독립테스트케이스수가아니다. 최종70PNG전수디코딩/1280x720/SHA256확인. 중간CandidateA15+FullF40사진55장은원본경로와해시로별도보존했다.

중립손이몸체원래위치의카메라아래약70cm에서,소유자표현에서앞약45cm·아래약24cm로보인다. 본체/world손은변하지않는다. 재현된돌출은그림자OFF에도남고팔뷰제외시사라졌으며보정사진에서제거됐다. 후보에도near5cm교차와UV/material seam 경계가남는다. 모든clip교차가0이라거나100%가림없음을주장하지않는다.

공격자료는기존DA_Attack_Knuckle_Primary01→AM_Knuckle_Jab→A_Knuckle_Jab이다. 원본Montage/Sequence와기존회피DataAsset은수정하지않았다. AnimationRecord.md/AnimationPhotos.md에준비·수락·최초피해관측·회복의실제프레임/시각·궤적을기록했다. 최초피해관측은정확한서브프레임접촉사진이아니다.

## 보존·한계·다음 단계

시작세이브270개와1,127파일을독립백업·SHA256검증했다. 원본25패키지와팔6개를이번묶음에서변경하지않았다. QA세이브만새namespace에생성하고이전실패로그/사진을삭제하지않았다. 최종hash결과는VERIFICATION.json을참조한다.

물리키보드/마우스입력·일반필드완주·패키징·six-appearance실제플레이·실제대검/너클무기메시socket부착·일시정지중재질교체는완전검증하지않았다. 13_owned_greatsword는메시없는QA인벤/트랙전환검사다. 모션블러와첫공격큰frameDT가관측돼60fps무정지/성능향상률보장으로해석하지않는다. 기존engine toolset Python/NTFS/XGE관련환경경고를시스템설정변경으로우회하지않았다.

다음은실제입력·일반필드에서human맨손동작을확인하고,실제장착무기및나머지외형을검증하는것이다. 현재요청대로기본OFF를유지한다. 테스트는새label로 Scripts/VerifyOwnerFraming.ps1 -Mode Candidate -Rendered -Offscreen 또는 Scripts/VerifyOwnerView.ps1 -Profile Weighted -Framed -Rendered -Offscreen을사용한다. 사용자세이브를QA슬롯으로지정하지않는다.

사진·수치: DevLog/20261031_OwnerViewFraming_Review/
