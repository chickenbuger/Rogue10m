# Blender 제작과 검수

Manny `MM_Idle` 골격을 기준으로 Blender 4.5.14에서 기존 절차적 구르기를 재현했다. 오른손, 오른쪽 어깨, 발로 지지가 넘어가도록 Manny의 어깨 구르기를 새로 키프레임했다. 로컬 VaultCache에 있는 Flexible Combat System - Basic `Roll`은 지지 순서를 검토하는 데 사용했다. Fab 계정의 현재 라이브러리 상태는 조회하지 않았다. [Fab 제품](https://www.fab.com/listings/40d402ad-0b0d-42c5-ab14-fffe458f6308) · [Fab 약관](https://www.fab.com/eula?lang=en)

같은 Manny 골격과 평면에서 기존 머리 프록시는 바닥 아래 23.979cm까지 들어갔다. 최종 프록시는 해당 구간에서 바닥 위 22.125cm를 확보했다. 250cm 직선 이동을 합성한 36–48% 네 표본의 오른 손목 XY 위치 차이는 0.000031cm였다. 확인한 전체 신체 표본의 바닥 편차는 최소 −0.4cm였다. [옆면과 사선 비교](v4/roll-profile-comparison.png) · [후보 검토 기록](v4/iteration_review.json)

재가져온 FBX의 본 계층 89개와 60Hz 표본 26개를 확인했다. 편집본·FBX 간 추적 본의 최대 위치 차이는 0.000479cm, Blender·Unreal 간 차이는 0.000498cm였다. Unreal 저장본의 루트 이동·회전 변화는 0이다. 초기에 GPU Workbench가 중단돼 CPU 렌더로 같은 카메라를 확인했다. Unreal SDK 확인 대기에서 멈춘 별도 가져오기는 `UE_SKIP_UBT_SDK_SETUP=1`을 해당 프로세스에 적용해 성공했다. [Blender 재가져오기](v4/fresh-blender-reimport.json) · [Unreal 저장 결과](v4/production-import-report.json)

편집 가능한 `MannyShoulderRoll.blend`와 FBX는 로컬 `tmp/AltMotionBlender20261121/v4/`에 있다. 제작 스크립트는 같은 로컬 작업 폴더의 `MannyIdle.fbx`를 읽고, `ROGUE_SHOULDER_ROLL_LABEL`에 지정한 새 폴더로 결과를 쓴다. [제작 스크립트](author_manny_roll.py)
