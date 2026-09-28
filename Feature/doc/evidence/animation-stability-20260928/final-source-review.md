# 애니메이션 안정성 검증 2 — 최종 코드 정적 리뷰

검사일: 2026-09-28. 제품 코드·에셋 읽기 전용 검토.

## 검토 대상과 판정

- `Rogue10mAppearanceBoxingAuthoring.cpp`: 오른잽 하체 보존, 중앙 시작 자세 보정, 가슴 component-space 운동 유지, 최종 손 자세에서 IK goals 재구성.
- `PrepareAppearanceBoxing.py`: 저장 전 원본 왼잽·오른잽 하체·손 IK·팔 길이·전방 타점 검사.
- `ConfigureBrawlerSourceFootIKOrder.py`: 기존 locomotion FootIK를 DefaultSlot 이전으로 이동.

**이 세 변경에서 현재 적용을 막을 정적 코드 결함은 발견하지 않았다.** 아래 회귀 조건은 실제 런타임 확인이 필요하다.

## 미러 및 베이크

`spine_01` 이하 상체만 미러하고 root/pelvis/하지를 보존한다. 보존된 pelvis의 회전이 미러 상체에 다시 더해지는 문제를 해결하기 위해, 초기 guard 기준으로 정렬한 mirrored chest component-space rotation을 목표로 삼아 경계 본의 local 회전을 다시 계산한다. 부모 역회전과 component-space 목표 회전의 곱 순서는 맞다.

`SyncHandIK`는 최종 실제 손 CS를 목표로 사용한다. `ik_hand_gun`을 오른손에 맞춘 뒤 그 자식 손 IK를 갱신하므로 원본 손 IK를 유지해 미러 팔과 달라지던 우려가 해소됐다. 주어진 Manny 계층의 부모 우선 순회와 일치한다. 필수 본 누락 시 중단하고 유한값/정규 quaternion 검사를 유지한다.

`boxing-assets.json`의 저장 전 검사는 121개 키에서 하체 위치·회전 오차 0, 시작 머리 오차 0, 오른잽 손 IK 위치 오차 약 0.0000971 cm / 회전 오차 약 0.0001216°, 팔 segment 길이 오차 약 0.0004444 cm를 기록했다. 오른손 전방 변위는 약 34.14 cm이며 공격 타점의 전방 위치는 52.37 cm로 peak 52.46 cm에 가깝다. 이는 해당 베이크의 제약 검증 결과이며 런타임 블렌딩 품질까지 보증하지 않는다.

왼잽 주요 본 위치와 원본의 타임워프 비교는 최대 약 0.0000971 cm다. 이 검사는 위치 위주이므로 손발 회전 보존은 추가 runtime/source clip 비교가 필요하다.

## 그래프 순서 변경

기존 `locomotion → DefaultSlot → FootIK → Output`은 공격 포즈에 locomotion 발/골반 보정을 다시 적용할 수 있었다. `locomotion → FootIK → DefaultSlot → Output`은 full-blend 공격에서 원본 공격 포즈를 우선하며, 몽타주가 없을 때에는 기존 locomotion FootIK를 유지한다.

편집 스크립트는 특정 Slot/ControlRig/Root와 기존 연결을 확인하고 세 연결만 교체한다. 예상 그래프 edge 집합, 노드 수와 설정 불변, compile 결과를 확인한 뒤 저장한다. 저장 전 실패에는 원래 연결을 복원하며 원본 에셋 백업 hash를 남긴다. `source-ik-order.json`에 compile/save 성공이 남아 있다.

회귀 조건:

1. `DefaultSlot` 설정 `bAlwaysUpdateSourcePose=False`를 유지하므로 full-blend 동안 하위 locomotion/FootIK 갱신이 멈출 수 있다. 공격 종료→idle 또는 이동/점프 복귀에서 튀는지 확인해야 한다.
2. 순서 변경은 이 공통 ABP의 DefaultSlot을 사용하는 모든 montage에 적용된다. 권사 공격 외 dodge/다른 무기 전환의 기본 경로가 정상인지 확인해야 한다.
3. full-blend 공격 도중 지형에 맞추는 발 접지 IK가 적용되지 않는 것은 원본 발 움직임 보존에 따른 명시적 절충이다. 경사면 공격의 접지 개선은 별도 조건부 IK로 다룰 수 있다.
4. 짧은 hook blend-in은 원본 회전이 뭉개지는 시간을 줄일 수 있지만 전환 속도가 증가한다. 급격한 각도 변화와 실제 영상에서 전환을 함께 확인해야 한다.

## 최종 통합 검증 요구

Source와 Appearance 양 단계에서 전체 네 공격과 idle 복귀를 기록한다. 관절 연결 local 길이비 1.0, 발 rotation의 단계간 차이, full-blend 프레임의 clip→Source 발/골반 보존, V 전환 및 1인칭 head camera 유지 여부를 비교한다. Capture PASS와 모션 품질 판정은 별개다.
