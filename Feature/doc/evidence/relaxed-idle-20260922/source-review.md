# Relaxed Idle / 권사 연결 — 독립 소스 검토 진행 기록

- 날짜: 2026-09-22
- 역할: 기획 1 이후 독립 검증 1. 런타임 소스 수정 없음.
- 최종 판정: 독립 소스 및 전용 런타임 검증 PASS. 최종513프레임 실패0. 손목 움직임의 화면상 자연스러움은 검증2의 시각 결과를 별도로 따른다. 초기 실패는 아래 이력으로 보존한다.
- 기준: `tmp/relaxed-idle/before-runtime/Rogue10mFistAnimInstance.h/.cpp` 대비 변경.

## 통과한 구조적 확인

새 설정은 UPROPERTY로 노출하고 PreUpdate에서 유한값/범위 검사 후 proxy POD에 복사한다. Ready 상태 시간은 Update에서 진행하며 Evaluate 중에는 UObject 상태를 직접 읽지 않는다. GC 소유 구조와 기존 LoadedIdle/Fist UPROPERTY 수명은 유지한다. Relaxed는 전신 맨손에서만 활성화되고 너클/팔전용은 기존 가드를 사용한다.

손가락을 원래 Idle에서 주먹 포즈로 섞고, FullBodyAim 어깨 보정과 발 지지를 Ready에 연동해 Idle에서 팔이 시선 때문에 올라가는 것을 방지한다. 머리 snapshot은 기존과 같이 전신/보행 후·마우스 조준 및 렌더러 머리 숨김 전에 읽는다. Gameplay attack snapshot·HitFraction·피해 경로는 바꾸지 않았다.

부모 순으로 정렬한 CS IK 결과는 엔진 `BonePose.h`의 LocalBlendCSBoneTransforms에서 local로 변환한 뒤 blend된다. 엔진 구현을 직접 확인했으며 함수 자체는 자식 변환 캐시/정규화를 처리한다. 기존 TwoBoneIK stretch=false와 팔 reach94%는 유지한다. 실제 limb length와 손목 연속성은 새 실행으로 확인해야 한다.

## 발견 및 수정한 문제 1 — 재입력의 이중 가중치

최초 구현은 이미 ReadyWeight로 섞인 ActualBasicHandTargets를 새 공격 시작 타깃에 넣은 뒤, 결과 IK를 다시 Idle과 ReadyWeight로 섞었다. Ready가 중간값일 때 공격 재입력 직후 팔을 다시 아래로 당길 수 있다. 실제 최종 위치와 미적용 IK 타깃 공간을 혼합한 것이 원인이다.

root와 합의하여 CaptureHands는 기존 CurrentBasicHandTargets(Ready를 적용하기 전 CS IK 목표)를 사용하도록 되돌리고 ActualBasicHandTargets 및 후반 저장 루프를 제거했다. 읽은 수정본 SHA256은 CPP `306C45724FB12C158335D206D6430FB02CB3156BC63FCFA9C1C29708175081C6`, H `EA6C750CC4CC9680CD11DDD9629B9D387AC6274410B401DE1F5722520CD79A19`다. 이 수정은 첫 구현 실행 이후이므로 아래 초기 결과를 수정본 통과로 해석하지 않는다.

## 발견 문제 2 — 최초 올림 손 이동 과다

초기 `tmp/relaxed-idle/reentry-first.log`는510프레임,실패2였다. 최대 손 이동38.682cm/프레임으로 기존20cm 제한을 넘었다. 실패는 재입력 구간이 아닌 처음 팔을 올리는47→48프레임이다.

- Ready .3762→.6836
- 왼손 camera-local: (4.789,-25.868,-68.968)→(20.759,-16.986,-34.875)cm
- 차징303→304도29.767cm, Ready .6836→.9259

0.16초의 smoothstep Ready가 중간에 약0.3씩 진행되고, 팔 전체 localrotation blend가 큰 손 원호를 만들어 속도가 커진다. 단순 Actual 타깃 제거만으로 이 별도 문제가 해결됐다고 단정할 수 없다. 첫 타격 전에 Ready가1이어야 한다는 기존 계약도 함께 유지해야 한다.

수정 후보는 Idle 손/팔꿈치 위치와 최종 전투 IK 목표를 같은 CS 공간에서 한 번 보간하고, fullweight IK로 길이를 유지하며 손목 orientation만 별도로 보간하는 방식이다. 개발자가 실제 경로와 팔꿈치 안정성을 확인한 뒤 결정한다. 또는 현재 경로/시간 조정으로20cm와 첫 HitFraction 둘 다 충족하는지 실측한다. 단순 테스트 한도 상향은 해결로 취급하지 않는다.

## 발견 문제 3 — 무기 복귀 Ready 검사 시점

초기 실행에서 weapon_switch_leaked_ready_state가 함께 실패했다. 테스트는 Staff→Unarmed 직후 같은 프레임에 Getter를 읽는다. 실제 새 proxy PreUpdate/Update/Evaluate가 완료되기 전 값인지, 다음 평가에도 전투 상태가 남는 진짜 문제인지 구분해야 한다. 초기화 수명주기를 확인하고 실제 애니메이션 평가 후 Ready0을 검증해야 한다.

## 테스트 검토와 후속 확인

기존 BasicBrawler 시험은 bEnableRelaxedIdle=false로 기존 가드/공격 공간 계약을 유지하고, 새 TestBrawlerIdleTransition은 기본 true 상태의 네 공격·가드 유지·Idle 복귀·중간 재입력을 다룬다. 기존 피해1회·입력 순번·자원/취소 테스트 완화는 없다. 새 전용 시험에서 실제 피해 프레임 Ready>.99를 확인하도록 권고했다. 손목 quaternion 프레임각도 로그도 권고했다. 기존 게임 snapshot 시간과 피해시각 일치만으로 실제 손이 도달했다고 주장하지 않는다.

재개 시 수정된 단일 blend 경로·초기화·팔 길이/손목/머리 순서를 다시 검토하고, 최종 빌드로 최초올림/차징/내림중재입력 로그 및 독립 영상 검증을 결합한다. Mixamo 실시간 참조는 ACL 제한으로 확인하지 못했으며 기존 Boxing.fbx 분석을 참고했다는 한계를 최종 결과에도 유지한다.

## CS/IK 재설계 재검토

전체 팔의 Ready localrotation blend를 제거했다. Idle에서 공격을 시작하거나 내림 중 재입력하면 실제 도달 제한을 거친 마지막 CS 손 끝점을 출발값으로 삼고, 기존 HitFraction까지의 전체 시간에 걸쳐 목표점으로 이동한다. 경로 함수는 선형75%+smoothstep25%로 초반과 중앙에 이동량이 과도하게 몰리지 않게 한다. 차징 진입은0.20초 경로이며, 일반 Idle 복귀는 Idle 손 위치와 가드 위치를 Ready로 한 번 섞은 목표를 따라간다.

이 구조에서 CurrentBasicHandTargets가 최종 IK 손 위치를 저장하는 것은 이전 문제와 다르다. 이번에는 그 위치 뒤에 전체 팔 Ready를 다시 적용하지 않으므로 이중 가중치가 없다. Ready에 따라 Idle 팔꿈치와 전투 elbow pole을 보간하고, IK 결과는 fullweight로 적용한다. 손목은 Idle quaternion→해부학적 주먹 quaternion을 Slerp하고, 전완 회내도 SolvedForearmRotation→회내 결과만 Slerp한다. parent 순서 정렬·normalize·CS→local 안전 변환은 유지한다.

팔 reach 상한은 Relaxed에서 Idle1.0→전투0.94로 연속 변화한다. 이는 Idle 원본 팔을 강제로6% 구부리지 않기 위한 변화이며 SolveTwoBoneIK stretch=false는 그대로다. 전투 Ready1에서는 기존94% 한도를 사용한다. 다리 경로는 기존 support weight에 Ready를 곱하는 변경뿐이며 stretch=false를 유지한다. Relaxed=false인 legacy/Knuckle은 기존 reach94%, charge0.16, 손 타깃/방향 경로로 돌아간다.

초기 손 초기화는 Idle 또는 기존 Guard를 명시 선택한다. 무기/사망 등 basic/relaxed 활성 상태 전환에서 bBasicHandsInitialized와 Ready 진행 상태를 초기화한다. 새 bAttackRaisedFromIdle은 매 AcceptedSerial에서 다시 산출되므로 다음 공격에 과거 상태가 재사용되지 않는다. UObject 수명/worker 접근 및 머리 snapshot 순서에는 새 문제가 없다.

정적 검토 범위에서 재설계의 미해결 차단 문제는 찾지 못했다. 손 이동20cm와 팔 길이0.15cm 이내, 피해 시 Ready>.99, 실제 손목 회전의 자연스러움은 런타임/영상 결과를 확인해야 한다.

## 전용 테스트 보완 확인

피해가 발생한 프레임에 Ready>.99 조건을 추가했고, 상완/하완 길이 변화0.15cm 미만을 검사한다. 손목 quaternion 변화량과 재입력 전후6프레임의 손 이동/각도를 기록한다. 기존 손20cm 한도는 유지했다. Staff 전환 후 다음 프레임에 Unarmed로 복귀하고 +3프레임에서 실제 새 애니메이션 Ready0을 확인해 이전 즉시 getter 샘플 문제와 구분한다. 기존 BasicBrawler 검사는 Relaxed 비활성 상태의 공격 궤적 회귀를 담당하며, 기본 true 상태는 새 전용 검사로 별도 확인한다.

재검토 파일 SHA256:

- FistAnim CPP: `C932B30EAC1736ED4D9F5AEBEC4338166F941DC7DD084D42DB8FE11827560C4D`
- FistAnim H: `EA6C750CC4CC9680CD11DDD9629B9D387AC6274410B401DE1F5722520CD79A19`
- Idle transition test: `8E6D6114C7AA5D2941158DFBCF6E03AED612C74F8CF56CF851693FF3A520F189`

## 2차 실행 진단과 시각 콤보 준비 게이트

CS/IK 재설계 실행 `tmp/relaxed-idle/reentry.log`는513프레임에서 손 이동 한 건만 실패했다. 최대32.093cm,피해5회,피격시간오차0.02735초,팔 길이오차0,무기 복귀Ready0을 확인했다. 최초 구현의 전체팔 rotation blend로 인한38.682cm 문제와 다른 경로다. 통합 담당의 프레임 진단에서49번 프레임 오른손만 급히 올라오며,48번에 수락한 다음 잽 버퍼의 시각 준비가 아직 올리고 있던 반대손 목표를 NextLoaded로 덮는 것이 원인임을 확인했다. 단독 차징 올림은약15.7cm로 감소했다.

현재 `bPreparingJab`에 `(!bAttackRaisedFromIdle || Progress >= Peak)` 조건을 추가했다. 일반 Idle에서 시작한 첫 타의 Peak 전에는 반대손의 CS 올림 경로를 유지하고, Peak 이후에만 다음 잽 준비를 합성한다. 그 시점에는 손이 가드에 올라와 다음 Loaded와의 차이가 작다. 이미 전투 자세에서 시작한 공격은 기존 준비 경로를 그대로 사용한다. 게임플레이 버퍼·AcceptedSerial·HitFraction·피해시간은 수정하지 않았으며 시각 준비만 잠시 보류한다. 정적 소스 검토 PASS.

손목 최대 프레임각도는2차에서67.867도였으므로 손 위치만 개선됐다고 손목 자연스러움까지 통과한 것으로 간주하지 않는다. 새 WRIST_FAST 로그는35도 이상을 관찰용으로 기록하며 임의 실패 기준은 아니다. 최종 실행의 해당 프레임과 실제 화면을 검증2가 확인해야 한다.

최신 검토 SHA256:

- FistAnim CPP: `F1D59663C9EB1FAA3B614F88220DA494811DACD75B129EFE6D86139EE2EC8339`
- Idle transition test: `7E62B71C03101B5DB29FF9DCC64346E81BA75D7B941C573EF4FEBE69C34DA32D`

## 최종 실행 검증과 fixture 검토

최신 `tmp/relaxed-idle/build-final.log`는 Succeeded,11.69초다. `reentry-final.log`를 직접 확인했으며 BRAWLER_IDLE_TRANSITION_PASSED,513프레임,실패0이다.

- 최대 손 프레임 이동16.937cm로 기존20cm 한도 통과.
- 상완/하완 길이 오차0.000000cm로0.15cm 한도 통과.
- 피해5회,최대 피격시간오차0.02735초로30fps 한 프레임 이내. 피해 시 Ready>.99 검사 통과.
- 내림 중 재입력407~413프레임에서 손 이동 최대7.1610cm,손목 회전 최대23.5999도.
- 무기 복귀 후513프레임 Ready0.00000 확인.
- 전체 손목 최대45.259도는 최초 올림49프레임이며, 관찰 로그상48프레임35.075도 및 차징304/305프레임42.122/37.486도도 있다. 이 수치는 실패 조건을 완화한 것이 아니라 시각 검토 위치를 정하는 근거다.

fixture 최종 변경도 확인했다. lookdown 옵션은 Idle 검사구간20~34와 종료 전9프레임만65도 아래보기·UI제외로 촬영한다. 공격 구간은0도 정면+UI로 유지한다. 소스 포즈와 공격 타이밍은 바꾸지 않는다. 의도한 ControlRotation 변경은 카메라의 상대 회전에도 정상 반영되므로 전체 relative transform 비교 대신 카메라 mount 위치/scale 불변과 기대 ControlRotation을 각각 검사한다. 표현 코드가 CameraComponent를 이동시키거나 입력을 바꿨는지는 계속 보호한다. nocapture는 RequestScreenshot만 건너뛰며 포즈·피해·상태 검사를 생략하지 않는다.

최종 검토 해시:

- FistAnim CPP: `F1D59663C9EB1FAA3B614F88220DA494811DACD75B129EFE6D86139EE2EC8339`
- FistAnim H: `EA6C750CC4CC9680CD11DDD9629B9D387AC6274410B401DE1F5722520CD79A19`
- Idle transition test: `45E4B6A0E81E53A13B256B1F459A1C5EF473102912406954F7ADE17A3CC17C76`
