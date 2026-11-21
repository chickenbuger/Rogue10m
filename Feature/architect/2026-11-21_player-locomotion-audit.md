# 3인칭 플레이어의 전방 달리기 착지

작업은 Alt 회피 개선 커밋 `2666013d`에서 시작했다. StartHub의 기본 HumanMale에서 대기, W 이동, Shift 질주, Space 이륙·하강·착지를 검수했다. 기본 이동은 620cm/s, 질주는 920cm/s였다. 이번 적용 대상은 평지에서 검수한 맨손 3인칭 첫 점프의 전방 착지다.

## 원본과 실패한 전환

`ABP_Common_Unarmed`의 `Main States`는 Manny `MM_Jump`, `MM_Fall_Loop`, `MM_Land`를 재생한다. 원래 `To Land → Land`는 접지에서 0.10초로 전환한다. `Land → Locomotion`은 움직일 때 0.50초 `ShouldMove`, 정지 시 0.40초 시퀀스 기반 자동 전환을 적용한다. 원본 `MM_Land`는 0.867초의 정지 착지로, Fab 보유 목록 및 로컬 원본 검토에서 찾은 Combat Magic 데모·Polyart 변형도 같은 방식으로 양발을 낮게 고정했다. 로컬 Close Combat Animset의 `NinjaJumpLand`과 `ThirdPersonJump_End`에는 접촉 자세가 있지만 Jog로 이어지는 교대 지지는 없다. Fab 계정의 최신 보유 목록을 새로 열거나 팩을 설치하지 않았다.

0.20초와 0.34초로 전환 시간만 줄인 두 시험은 발을 끄는 시간을 줄였으나 무릎과 발의 한 프레임 변화가 커져 제외했다. `Main States` 아래에 `Inertialization`은 없고 `CR_Mannequin_FootIK`가 접지 시 활성화된다. 현재 외형 `ABP_Retarget_Hu_M`이 그 결과를 사용한다.

## Blender 파생 동작과 지상 보폭 인계

UE에서 실제 Manny `MM_Fall_Loop`, `MM_Land`, `MF_Unarmed_Jog_Fwd_PalmReference`를 내보내 Blender의 같은 Armature에서 비교했다. 60Hz 실측에서 마지막 공중, 첫 접지의 Fall Loop는 각각 0.350000초와 0.366667초였다. 28프레임 Action은 이 자세에서 시작해 왼발로 충격을 받은 뒤 전방 Jog 오른발로 인계한다. Manny 양다리 길이와 인플레이스 루트를 사용한다. `L`은 0.133333초, `R`은 0.416667초다. 오른발 목표는 전방 Jog의 두 번째 `R` 0.851702초로, Blender 원본 기준 52.10212프레임이다.

`ABP_Common_Unarmed`에 `Forward Run Land`를 추가하고 기존 `To Falling` 별칭에 이 상태를 포함한다. 접지 직전 첫 점프였으며, 활성 3인칭·맨손·전방 3° 이내·600–640cm/s·이동 입력이 유효할 때 새 상태를 선택한다. 질주, 몽타주 및 회피 조건에서는 원래 경로가 적용된다. 상태 내부의 시퀀스가 `Locomotion` 리더로 `L/R` 구간을 보낸다. 두 번째 `R` 직전 0.10초부터 `Walk / Run`과 블렌딩한다. 이 착지에서만 `Walk / Run`이 초기화될 때 정규화된 Jog `0.767617 / 1.766667`를 받는다. 다른 상태 초기화의 `StartPosition`은 0이며 기존 `bResetPlayTimeWhenBlendSpaceChanges` 설정은 유지한다.

## 검수와 되돌리기

`VerifyThirdPersonGameplay.ps1 -ForwardLanding -NullRHI -Label <새 라벨>`로 기본 620cm/s, 추가 610cm/s, 정지와 D+Space를 같은 고정 60Hz로 비교한다. 원래 그래프에서는 `-ExpectStockLanding`을 함께 지정한다. 각 접지 뒤 숨김 Manny와 표시 외형의 본, 지면 높이, 실제 Land/Fall 시점, Jog 마커를 기록한다. 67–300ms의 근접 발볼 이동을 절반 이상 줄이고, 시퀀스가 끝난 뒤 같은 시점의 원본 Jog와 무릎 각을 대조한다. `-NullRHI`를 빼면 WARP에서 두 측면 장면을 남긴다.

롤백 범위는 `ABP_Common_Unarmed`, 추가 `A_CommonForwardRunLanding_L`, Fighter AnimInstance의 첫 착지 조건 및 전용 검사다.
