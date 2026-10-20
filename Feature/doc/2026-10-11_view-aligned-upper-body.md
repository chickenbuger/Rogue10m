# 플레이어 시선을 따라가는 상체 애니메이션

- 시작일: 2026-10-11 / 최종 검증일: 2026-10-13
- 브랜치: `Sprint#5-29-view-aligned-upper-body`
- 상태: C++ 구현·최종 Editor 빌드·집중 통합·원본/부정 대조·실제 렌더·이동/가드/카메라/전투 회귀·독립 검토·기존 Editor 복원 확인 완료.

## 적용 결과

현재 플레이어의 상체와 양팔이 위아래 시선을 따라 움직인다. idle·전투 가드·제자리 공격·이동 공격의 최종 포즈에 같은 조준 보정을 적용한다. 몸은 기존 마우스 yaw를 따라 회전하고, 몸과 컨트롤 시선 사이에 남은 yaw도 제한된 범위에서 상체에 반영한다. 별도 First Person Mesh를 다시 사용하지 않고 현재 Animation Source Mesh → Appearance Mesh 리타깃 경로를 유지한다.

원래 보행·DefaultSlot·이동 공격 합성 그래프를 평가한 뒤 C++ native proxy에서 상체를 보정한다. Animation Blueprint 그래프와 원본 시퀀스·몽타주를 다시 제작하지 않는다. 하체는 원래 평가 결과를 유지하고, 양팔의 손목·손가락 로컬 자세와 렌더 본의 로컬 위치·스케일도 보존한다. 손 IK helper는 조준 후 FK 손의 최종 위치·회전과 맞춘다.

목·머리의 원래 component 회전을 복원해 상체 조준이 카메라 회전에 다시 더해지지 않도록 했다. 머리 위치는 실제 몸의 움직임을 따른다. 따라서 카메라가 완전히 고정되는 기능은 아니며, 극단적인 조준이나 원래 전신 회피에서는 머리 위치가 움직인다. 기존 머리 카메라의 초기 pitch -20도, 눈 오프셋 `(8,0,-3)cm`, 권사 회전 안정화 15%·최대 8도, V키 3인칭 검사 시점은 유지한다.

## 구현과 조정값

`URogue10mFighterCombatStanceAnimInstance`가 `FRogue10mViewAlignedUpperBodyProxy`를 생성한다. proxy의 game-thread `PreUpdate`는 컨트롤 조준, 캐릭터/메시 회전과 조정값을 값 타입으로 복사한다. parallel 평가에서는 UObject·월드·컨트롤러에 접근하지 않는다.

실제 main root에서만 `EvaluateAnimationNode_WithRoot`로 기존 그래프를 한 번 평가하고 결과를 보정한다. 연결 root는 기존 평가에 맡긴다. actor 축을 실제 source mesh component 좌표로 변환하므로 Manny의 +Y 전방과 수입 복싱 포즈의 root 회전을 플레이어 조준 축으로 혼동하지 않는다. 전체 조준 quaternion 하나를 나누어 적용하므로 pitch/yaw가 함께 바뀌어도 분배 회전이 누적된다.

| 설정 | 기본값 | 용도 |
|---|---:|---|
| `bEnableViewAlignedUpperBody` | true | 현재 Common source의 조준 보정 사용 |
| `UpperBodyAimMaxPitch` | 75도 | 위아래 상체 보정 제한 |
| `UpperBodyAimMaxYaw` | 45도 | 몸과 시선 사이의 잔여 yaw 제한 |
| `UpperBodyAimTorsoWeight` | 0.25 | 척추 총 25%, 양쪽 upperarm에 각각 나머지 75% 적용 |
| `UpperBodyAimInterpolationSpeed` | 18/s | 급격한 시선 변화를 지수 보간으로 연결 |

기본 척추 분배는 `spine_01/02/03`의 5%/8%/12%다. 뼈 길이를 늘이거나 손목·손가락을 추가 회전하지 않는다. compact bone 인덱스는 bone container serial이 바뀌면 갱신한다. 필요한 상체 체인이 없는 LOD는 원래 포즈를 보존한다. 손 helper가 예상과 달리 렌더 몸 체인 아래에 배치된 경우 해당 helper를 수정하지 않는다.

비활성·사망·컨트롤러 부재·차단 UI·비유한 입력에서는 보정을 생략한다. 실제 회피 몽타주가 재생·블렌드 중이면 원래 전신 회피를 우선하며 조준 보정을 즉시 해제한다. 회피가 끝나면 현재 시선으로 다시 보간한다. 점프 상태 자체는 보정을 중단하지 않으며 하체와 root 이동은 원래 그래프가 담당한다.

## 집중 검증 결과

검증은 현재 인간 남성 `SK_Hu_M_FullBody` 외형과 실제 생산 플레이어의 source→Appearance 경로를 사용했다. LOD 1·30FPS 고정 단계에서 같은 소유자·컨트롤러·바닥·이동 입력을 읽는 독립 대조 메시를 만들었다. 대조는 같은 원래 그래프에 조준 보정만 비활성화하며, 실제 Combat이 수락한 몽타주를 함께 평가한다. 대조 몽타주의 notify가 생산 전투에 중복 간섭하지 않게 억제했다.

| 검사 | 현재 확인한 결과 |
|---|---|
| 최종 Editor 빌드 | `build-editor6-final.log`: 성공, 8.23초. 일회 복구 도구 제거 후 빌드 |
| 원본 대조 `aim-baseline03` | 21 case·1,575 frame·최종 459,385 검사·실패 0 |
| 기능 활성 `aim-runtime05` | 21 case·1,575 frame·최종 460,488 검사·실패 0 |
| 기능 비활성 부정 대조 `aim-negative06` | 1,575 frame 완료, 예상된 조준 관련 실패 2,456개. 포즈 보존 검사 실패 0 |
| 실제 렌더 `aim-render07 -Movie` | 21 case·1,575 frame·최종 460,488 검사·실패 0. PNG 1,575장 |
| 기존 이동 공격 | 1,100 frame·최종 258,066 검사·실패 0. neutral aim에서 원래 상·하체 포즈 검사 유지 |
| 기존 전투 가드 | 640 frame·실패 0. 기본 -20도 조준 유지 |
| 기존 머리 카메라/전방 공격 | 1,572 frame·최종 4,978 검사·13종 공격/15 case·체력 21펄스·실패 0 |
| 기존 맨손 전투 | 840 frame·실패 0. 실제 9종 스킬 입력·이동·점프·실제 피해 대상 검사 |
| 콘텐츠/저장 보존 | Content 6,359개의 추가·삭제·크기/mtime 변경 0, Common AnimBP SHA 동일. 생산 저장 3개 원본 SHA 동일 |
| Editor 복원 | 기존 OpenRunMap, PIE 종료, dirty 콘텐츠/맵 0. Common AnimBP의 native aim=true·75/45도·척추25%·보간18 확인 |

21 case는 중립 idle, 위아래 30/60도 조준, 0/90/180/270도 world heading, 왼손→오른손 잽, 짧은 우클릭 스트레이트·긴 우클릭 훅, 이동 중 맨손·너클 공격, V키 검사, ±85도 입력의 ±75도 제한, 잔여 yaw ±30도와 ±60도 입력의 ±45도 제한, 60도 상향 가드 중 실제 회피를 포함한다. 공격은 생산 Combat handler를 사용하고 실제 스킬 사용 횟수로 각 입력 수락과 좌우 잽 두 공격을 확인한다. V는 PlayerController의 실제 키 입력 경로를 거친다. 모든 물리 마우스 이벤트와 멀티플레이 입력까지 재현한 시험은 아니다.

`metrics.txt`는 마지막 네 개 증거 파일 저장 검사 직전에 만들어져 runtime 460,484 / baseline 459,381 검사를 기록한다. 최종 엔진 로그의 460,488 / 459,385를 종료 판정에 사용했다.

| `aim-runtime05`의 독립 포즈 측정 | 최대 오차/변화 |
|---|---:|
| Source 하체 local 회전 | 0.000002958도 |
| Source render local translation | 0cm |
| Source 머리 component 회전 | 0.000005663도 |
| 실제 Appearance 하체 local 회전 | 0.000003415도 |
| 실제 Appearance 머리 component 회전 | 0.000002415도 |
| 실제 Appearance 손가락 local 회전 | 0.000002415도 |
| 실제 Appearance 팔·다리 길이 | 0.000036886% |
| 원본 대조 머리 움직임으로 계산한 카메라 회전 | 0.000002415도 |
| 최종 조준 upperarm의 세계 시선 방향 | 0.000005123도 |
| 조준 중 FK·IK 손 위치 | 0.000011401cm |
| 조준이 더한 Source 머리 위치 변화 | 14.594203191cm |

세계 시선 검사는 보정한 upperarm과 같은 시점의 원본 upperarm 사이의 회전 차이를 실제 actor forward에 적용해 계산한다. 이 오차가 작다는 것은 입력 방향에 맞게 상체 회전이 적용됐다는 뜻이며 모든 기술의 손끝 타격 궤도가 정면 직선이라는 뜻은 아니다. 훅·어퍼컷 등의 원래 상대 궤도는 유지된다.

부정 대조는 보정을 강제로 끈 상태에서 정상 기능의 기대값을 검사했다. 실패 종류는 pitch/제한 1,087개, 잔여 yaw/제한 252개, 팔 세계 방향 1,097개, 외형 손 변화 20개였다. 예상된 기능 부재를 검출한 것이며 실제 활성 실행의 실패로 집계하지 않는다.

### 실제 화면과 실행영상

1인칭 상향 잽·하향 훅과 V키 상향/하향 시점 4장, 고각 회피의 연속 16장을 독립 검토했다. 관측 표본에서 팔·다리 체인이 끊기거나 역으로 꺾인 새로운 변형은 찾지 못했다. 빠른 공격의 motion blur 때문에 손가락 skin의 세부 미관까지 보증하지 않는다. 60도 상향 V검사 중 회피는 카메라가 몸에 가까워져 메시가 잘리는 기존 검사 시점의 한계가 관측됐다. 수치상의 원본 포즈 보존과 화면 클리핑을 구분했다.

- [맨손·너클 공격과 V시점 20초 발췌](D:/Project/Rogue10m/tmp/view-aligned-upper-body/view-aligned-upper-body-attacks.mp4): 원래 30FPS 속도의 실제 캡처 600프레임.
- [21상황 전체 검사영상 52.5초](D:/Project/Rogue10m/tmp/view-aligned-upper-body/view-aligned-upper-body-preview.mp4): 1280×720 H.264, 실제 1,575프레임. 고각·회피의 화면 한계도 포함한다.
- [동영상 메타데이터](evidence/view-aligned-upper-body-20261011/video-metadata.json), [독립 최종 QA](evidence/view-aligned-upper-body-20261011/qa-review.md), [독립 최종 소스 검토](evidence/view-aligned-upper-body-20261011/final-source-review.md).

## 실패 분석과 세이브 복원

첫 통합 실행 `aim-runtime02`는 21 case·1,575 frame을 끝냈지만 1,286개 실패가 있었다. 이동 대조의 하체·로컬 위치와 중립 포즈까지 불일치해 비교 대상의 상태·시간·업데이트 순서를 다시 조사했다. 새 컴포넌트를 기존 프레임 중간에 등록하며 고정 시간 단계를 바꾸는 초기 상태와 이동 업데이트 순서를 맞추고, 양쪽 원본 그래프를 warmup 후 동시에 시작했다. 단순히 허용 오차를 늘리지 않았으며 별도 원본 대조 실행이 실패 0으로 통과한 뒤 활성 실행을 다시 확인했다. 초기 실패 증거는 보존했다.

초기 QA의 진행 슬롯 접두사에 하이픈이 들어가 유효한 테스트 슬롯으로 인식되지 않았다. 잘못된 접두사가 생산 진행 슬롯으로 돌아가 QA 공격 사용 횟수와 해금 항목이 기록되는 문제가 발생했다. 캐릭터 프로필과 스킬 키 파일은 원래 해시를 유지했다. wrapper는 현재 진행 접두사를 영문·숫자·밑줄로 구성하고 독립 QA 슬롯을 지정한다.

변경된 진행 파일에서 원래 진행 상태를 재구성한 뒤 작업 시작 전 **전체 파일 SHA256와 정확히 일치한 바이트 후보**를 확인했다. Unreal `SaveDataToSlot`으로 생산 슬롯의 원래 바이트를 복원했고, `RESULT=VIEW_AIM_EXACT_SAVE_RESTORE_PASSED bytes=138536`과 실제 생산 파일 해시를 확인했다. 본 결과 문서를 작성할 때에도 같은 원본 해시를 다시 확인했다.

```text
대상: Rogue10mProgression_F896E7AD4CDCF783285888A597914739
SHA256: B39F2FCD80C499C50928C39082714FF3920B82A419F775C8FC02E192718DE35D
길이: 138,536 bytes
```

이는 원래 파일 전체 해시가 일치하는 복원이다. 자세한 과정과 대상은 [생산 세이브 복원 분석](evidence/view-aligned-upper-body-20261011/production-save-recovery.md) 및 [실제 Unreal 복원 로그](evidence/view-aligned-upper-body-20261011/production-save-restore.log)에 남겼다. 복구용 임시 C++ helper는 Source에서 제거했고 최종 빌드도 완료했다.

## 현재 한계

현재 외형·LOD 1에서 source와 Appearance의 팔·다리·머리 회전 및 길이와 실제 렌더 표본을 확인했다. 여섯 외형 전부, LOD 전환, Shipping/Cooked 빌드와 멀티플레이는 별도 시험이 필요하다. 비활성·사망·차단 UI·컨트롤러 부재·비정상 입력 보호는 코드로 검토했으며 실제 실행에서 모두 별도 case로 검증한 것은 아니다. 수치상 뼈 길이와 상대 자세 보존은 모든 frame의 세밀한 skin 변형 미관을 보증하지 않는다.

회피는 전신 원본 포즈를 유지하려고 조준을 즉시 해제한다. 고각 가드에서 회피가 시작된 13번 frame의 Appearance 머리 component 위치 step은 활성 15.310111333cm / 원본 대조 11.789469809cm로 **추가 3.520641524cm**가 관측됐다. 이후 회피 본연의 최대 component step은 양쪽 모두 20.298217528cm였다. 전체 world head step 119.806698579cm는 회피 actor 이동까지 포함하므로 새 조준이 더한 흔들림만으로 해석하지 않는다. 회피 진입의 즉시 해제는 눈에 띄는 위치 전환을 만들 수 있어 완전히 부드러운 전신 회피라고 단정하지 않는다.

조준 입력의 평활화는 회전 입력을 연결한다. 기본 척추 가중치가 25%여도 고각 시선에서 머리 위치는 원본보다 최대 약14.59cm 이동한다. 카메라 회전에 pitch가 중복되는 문제와 이 실제 위치 이동은 구별한다. 현재 독립 카메라 회전 오차는 매우 작지만, 머리 위치까지 고정한 상태는 아니다.

## 변경 파일과 재현

- 런타임: `Source/Rogue10m/Character/Rogue10mViewAlignedUpperBodyProxy.h/.cpp`, `Rogue10mFighterCombatStanceAnimInstance.h/.cpp`의 이번 증분.
- 집중 QA: `Source/Rogue10m/Tests/Rogue10mViewAlignedUpperBodyTest.cpp`, `Scripts/VerifyViewAlignedUpperBody.ps1`.
- 기존 포즈 계약: `Rogue10mMovingAttackRuntimeTest.cpp`, `Scripts/VerifyMovingAttack.ps1`의 neutral aim 선택. 기존 raw montage의 상·하체 검사 자체는 유지한다.
- QA 격리: 관련 회귀 wrapper의 독립 진행 접두사 설정 증분.
- 문서: [설계](../architect/2026-10-11_view-aligned-upper-body.md), [런타임 정적 리뷰](evidence/view-aligned-upper-body-20261011/runtime-review.md), [QA 리뷰](evidence/view-aligned-upper-body-20261011/qa-review.md), 이 결과 문서와 최종 개발/Sprint 기록.

최종 보존 범위는 시작 상태 Source/Config/Scripts 451개 중 변경 6개·추가 4개·삭제 0개다. 모든 Content의 SHA 대조라고 확대하지 않으며 전체 6,359개는 크기와 mtime, Common AnimBP는 SHA를 비교했다. `CheckGeneratedChanges.ps1`와 범위 내 `git diff --check`를 통과했다. 전체 작업 트리의 기존 바이너리 자산 변경 경고는 이 작업에서 편집한 자산을 의미하지 않는다. [Editor 재로드 증거](evidence/view-aligned-upper-body-20261011/editor-restored-state.json)에 새 native 설정과 clean 상태를 기록했다.

```powershell
.\Scripts\BuildEditor.ps1 -NoHotReload
.\Scripts\VerifyViewAlignedUpperBody.ps1 -Phase baseline -Label fresh-baseline
.\Scripts\VerifyViewAlignedUpperBody.ps1 -Phase runtime -Label fresh-runtime
.\Scripts\VerifyViewAlignedUpperBody.ps1 -Phase negative -Label fresh-negative
.\Scripts\VerifyViewAlignedUpperBody.ps1 -Phase render -Movie -Label fresh-render
.\Scripts\VerifyMovingAttack.ps1 -Phase runtime -NeutralAim -Label fresh-moving-neutral
```

핵심 증거: [활성 통합 로그](evidence/view-aligned-upper-body-20261011/aim-runtime05.log), [원본 대조 로그](evidence/view-aligned-upper-body-20261011/aim-baseline03.log), [비활성 부정 대조 로그](evidence/view-aligned-upper-body-20261011/aim-negative06.log), [실제 렌더](evidence/view-aligned-upper-body-20261011/aim-render07.log), [최종 Editor 빌드](evidence/view-aligned-upper-body-20261011/build-editor6-final.log), [보존 검사](evidence/view-aligned-upper-body-20261011/preservation-after.json). 자세한 frame/외형 metric은 `tmp/view-aligned-upper-body/aim-runtime05/`에 저장했다. 기존 미커밋 작업을 유지하며 이 작업의 커밋·푸시는 하지 않았다.
