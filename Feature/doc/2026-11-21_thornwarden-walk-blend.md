# 가시수호자 대기·이동 전환 보정

가시수호자는 일정 간격으로 속도를 확인한 뒤 대기와 걷기 클립을 각각 처음부터 재생했다. OpenRunMap에서 사용하는 Cactus 대기의 여덟 자세와 보행 첫 자세를 비교했을 때 오른손 끝의 최대 차이는 77.32cm였다. 걷기 루프의 처음과 끝 자세는 일치했다. [에셋 경로와 측정값](evidence/thornwarden-walk-blend-20261121/asset-binding-and-poses.json) · [설계](../architect/2026-11-21_thornwarden-walk-blend.md)

가시수호자 프로필에 기존 보행 그래프를 연결해 같은 두 클립을 속도에 따라 보간한다. 이동·정지 사이 포즈가 천천히 바뀐다. 공격, 피격, 사망은 기존 개별 클립 경로를 사용한다. 사용한 에셋은 보유 Monster For Survival Game의 `Cactus_IdleNormal_ANIM`과 `Cactus_WalkFWD_ANIM`이다. 프로필을 Unreal Editor에서 저장했다.

StartHub에서 실제 가시수호자 데이터로 복제 개체를 소환해 같은 입력으로 250cm/s 이동과 감속을 실행했다. 60Hz로 읽은 오른손 끝의 최대 프레임 이동은 76.839cm에서 9.544cm로 줄었다. 기존 구성을 임시 프로필에서 재현했다. 기본 공격은 예전 클립과 준비 시간을 사용했고 공격이 끝나면 보행 블렌드로 돌아왔다. 10개 검사와 UE 5.8 Editor 빌드가 통과했다. 새 실행 로그 오류는 0건이었다. 이 비교는 프로그램으로 이동시키며 본 좌표를 읽은 검사다. [초기 검증](evidence/thornwarden-walk-blend-20261121/initial-results.txt) · [본 위치 표본](evidence/thornwarden-walk-blend-20261121/initial-pose-samples.csv)
