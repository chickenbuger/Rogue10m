# V키 3인칭 전신 확인 토글

작성: 2026-09-28 / 브랜치: Sprint#4-40-third-person-inspection / 역할: 기획1

## 목표와 범위

플레이 중 V키를 누르면 현재 캐릭터의 전신과 공격 동작을 뒤쪽 3인칭 시점에서 확인하고, 다시 누르면 기존 Appearance 머리 카메라로 돌아간다. 기존 애니메이션 재생 상태·입력·이동·공격 판정은 전환 중에도 그대로 유지한다. 이번 변경은 관찰 시점 하나를 추가하는 작은 기능이며 자유 궤도 카메라, 포토 모드, 전투 밸런스 변경은 포함하지 않는다.

## 구현 결정

- `AppearanceCameraComponent`에 기본 false인 3인칭 확인 상태를 두고 V 입력으로 토글한다. 거리는 300cm, 관찰 pivot 높이 오프셋은 최종 0cm(캡슐 중심)를 기본값으로 노출한다. 첫 촬영의 +60cm 구도에서 하체가 HUD에 가려져 몸 중앙을 관찰 중심으로 조정했다.
- 실제 `FirstPersonCameraComponent`는 계속 Appearance head를 따라 계산한다. 3인칭 상태에서는 `Character::CalcCamera`의 최종 `OutPOV`만 관찰 카메라로 덮어쓴다. 따라서 공격 trace의 머리 카메라 위치·방향은 전환 전과 같다.
- 관찰 위치는 캐릭터 뒤쪽에서 몸이 보이는 위치로 두며, 최종 방향은 설정한 전신 관찰 중심을 바라보도록 계산한다. 초기 아래 30도 시선 때문에 3인칭 카메라가 과도하게 바닥/하늘로 이동하지 않도록 중심·높이·방향 정의를 분리한다.
- 캐릭터와 목표 관찰 위치 사이를 sphere sweep(ECC_Camera)하여 벽을 통과하지 않게 한다. 자기 자신은 제외하며 hit 위치에서 여유 거리를 유지한다. 충돌로 가까워진 경우 NaN/반전이 없도록 보정한다.
- 3인칭에서는 이번 1인칭 모드가 가린 head를 다시 표시하고, Hair/Facial 등 외형 파츠의 OwnerNoSee를 일시 해제한다. 복귀·비활성화·종료 때 기존 상태를 복원한다. 원래 가려져 있던 다른 본이나 파츠는 임의로 표시하지 않는다.
- FirstPersonMesh는 계속 숨김·pause·tick off 상태다. Appearance body와 Source/Retarget ABP는 변경하지 않는다.
- PlayerController 기존 BindKey 패턴으로 V Pressed를 연결한다. 로컬 플레이 중이고 유효한 현재 Pawn과 Appearance camera가 있을 때만 처리한다. 인벤토리·설정 등 차단 UI가 열렸거나 사망 상태이면 전환하지 않는다.
- 새 Pawn은 기본 1인칭으로 시작한다. viewtarget 변경·unpossess·종료로 Appearance 모드가 해제되면 외형 파츠 복원을 보장한다. 필요 이상의 state 유지나 PlayerState 저장은 하지 않는다.

## Ultrawork Packets 및 역할

| Packet / 역할 | 목표·수정 영역 | 완료 조건 | 검증 | 되돌림 경계 |
| --- | --- | --- | --- | --- |
| P1 개발1 | AppearanceCameraComponent의 상태·관찰 POV·벽 sweep·head/part visibility | 뒤쪽 전신 보기와 1인칭 복귀가 가능하고 가림 상태를 정확히 복원 | Editor 빌드, 런타임 위치·가시성·충돌 확인 | Component 변경 |
| P2 개발2 | PlayerController V 바인딩/차단 UI 검사, Character CalcCamera 최종 POV 연결 | 실제 V 입력으로 전환하며 기존 head CameraComponent는 변경하지 않음 | 입력→토글/복귀, UI 차단 상태 테스트 | PC/Character 변경 |
| P3 검증1 | 독립 소스 리뷰 | 수명주기·조준 불변·FP 비활성·상태 복원·UObject 참조 확인 | 기준 복사본 diff + 코드 읽기 | 증거 문서 |
| P4 검증2·root | 실제 1↔3인칭·공격·벽·가시성 검증, 빌드·기록 | 실행 증거와 한계 명시, 요청 동작 완료 | 관련 런타임 테스트, 영상/캡처, CheckGeneratedChanges 및 diff 검사 | 검증·문서 산출물 |

사용자 지정 기획1 / 개발2 / 검증2 역할을 유지하며 기획 완료 후 같은 담당을 독립 검증1로 전환한다. 동시 작업 파일 소유권을 분리하고 기존 Appearance/Boxing 변경을 덮어쓰지 않는다.

## 확인할 회귀와 완료 기준

1. 게임 시작은 1인칭이다. V를 두 번 누르면 1인칭→3인칭→1인칭이 된다.
2. 3인칭에서 실제 Appearance 몸통·양팔·양다리·머리와 설정된 머리카락/얼굴 파츠가 보이고, FP Mesh는 나타나지 않는다.
3. 애니메이션 중 전환해도 몽타주 시간·콤보·입력 상태가 초기화되지 않는다. 실제 머리 카메라와 공격 trace 기준은 유지된다.
4. 벽 앞에서 카메라가 벽 안으로 들어가지 않고, 충돌이 없으면 설정 거리로 돌아온다.
5. 차단 UI가 열린 동안 V는 무시된다. 복귀 후 마우스 조작과 기존 기본 시선은 보존된다.
6. 1인칭 복귀 시 head/appearance 파츠 가림 정책이 되돌아오고, 전신/공격/이동은 기존과 같다.
7. 일반 실행 테스트에 포함하지 못한 사망·외형 교체·재빙의·네트워크 경로는 소스 검토와 실행 검증을 구분해서 기록한다.
8. BuildEditor, 관련 실행 검증, CheckGeneratedChanges, diff 검사 후 결과 문서를 작성한다. 에셋 변경은 이 기능에 필요하지 않다.

## 문서와 종료

결과: `Feature/doc/2026-09-28_third-person-inspection.md`.
한국어 일지 및 Notion 요약 후보: `DevLog/20260928.txt` append.
Sprint 기록: `Docs/SprintChangeLog.md` 업데이트.
커밋·푸시는 사용자 승인 없이 수행하지 않는다.

## 종료 기록

구현 및 지정 범위 검증 완료. 최종 빌드12.43초, 실제V입력 기반280프레임 실패0. 3인칭중 공격재생/왕복/가림/UI/벽충돌 검증과 시각확인 완료. 정확한 공격중토글순간 및 생명주기/네트워크 별도실행 범위는 결과문서에서 구분했다. 최종기본높이는0cm이다.
