# Walking.fbx 기반 플레이어 보행 손바닥 보정

- 작업 브랜치: `Sprint#5-22-walking-palm-reference`
- 요청: 제공한 `C:/Users/PC/Downloads/Walking.fbx`의 손바닥 방향을 특히 참고해 현재 캐릭터 걷기를 수정한다.
- 대상: 현재 플레이어의 맨손/너클 공통 보행. NPC 모델, HUD, 카메라 설정 및 전투 클립은 별도 작업으로 유지한다.

## Scope Gate / Architect

실제 플레이어의 숨겨진 Manny 소스는 `ABP_Common_Unarmed`의 중첩 `Walk / Run` 상태에서 `BS_Idle_Walk_Run`을 재생한다. 이 BlendSpace는 방향 8개씩 Walk(300 cm/s), Jog(600 cm/s), Idle 중복 표본을 포함한 27표본/17클립이다. 기본 이동 속도 600을 고려해 Walk만 고치지 않고 16개 이동 클립을 함께 검토한다.

원본 FBX는 mesh 없는 Mixamo 애니메이션 골격(65 bones, 30 fps, 약 0.967초)이며 루트가 약 162 cm 이동한다. 보행 전체를 덮어쓰지 않는다. MCP 손가락 평면, 엄지 위치 및 실제 손가락 굽힘으로 확인한 손바닥 방향을 참고한다. 두 손은 몸 안쪽/뒤쪽을 향한다. 현재 Manny의 손바닥도 안쪽이지만 뒤쪽 기울기가 다르다. 서로 다른 bone-local 좌표를 직접 복사하지 않는다.

Manny는 source +Y 전방, -X 오른쪽, +Z 위쪽이며 실제 source component yaw -90으로 actor 좌표에 연결된다. 양손 손바닥 normal은 `(middle_01-hand) × (index_01-pinky_01)`에 좌 -1 / 우 +1의 해부학적 부호를 적용한다. 이 부호는 현재 동작마다 자동 뒤집지 않고 엄지/손가락 굽힘과 기준자세로 독립 검증한다.

## Ultrawork Packets

| Packet | 목표 / 수정 영역 | 완료 조건 / 검증 | 롤백 경계 |
| --- | --- | --- | --- |
| ULW-1 Reference | Blender headless에서 FBX의 손가락/손목 및 현재 UE 보행 읽기 | 원본 SHA 유지, semantic palm axes/시각 증거, 실제 BlendSpace 입력 확인 | 입력 파일 무수정, 새 script/evidence만 생성 |
| ULW-2 Author | 새 Locomotion/PalmReference 경로의 이동 클립/BS 생성, 기존 플레이어 노드의 asset ref만 변경 | 손목 roll은 손목→중지 축을 유지, 다리/root/상완/전완/손가락 local keys와 타이밍/커브/notify 유지; Blueprint compile 성공 | 기존 ABP 패키지 checkpoint와 원래 BS 참조 보관, 새 경로만 분리 |
| ULW-3 Builder/QA | 독립 Editor 전용 native palm fixture 및 wrapper | 실제 CharacterMovement·source/외형 evaluated finger geometry·여섯 외형·속도/전환, 시각 검토; 관련 build 및 기존 이동 공격/전방·카메라 회귀 통과 | 테스트용 transient actor/프로필, 별도 QA save slot, 기존 테스트 무수정 |
| ULW-4 Review/Docs | 독립 solver/asset/runtime 검토 및 결과 기록 | 범위/원본 보존·손목 seam 확인, paired doc·한국어 DevLog·SprintChangeLog 갱신 | 문서 append; 커밋/푸시 없음 |

## 구현 원칙

- 원래 animation 패키지는 변경하지 않고 파생 클립을 만든다. 원본 손가락 굽힘과 손가락 길이를 유지한다.
- 원하는 reference palm normal을 현재 손목→중지 축의 수직 평면에 투영해 signed twist만 적용한다. 임의 shortest-arc 회전으로 손목을 꺾지 않는다.
- source frame rate/프레임 수를 그대로 유지하며 `hand_l/r` 회전만 바꾼다. 모든 위치·스케일, 손가락 로컬 키, 보조 hand IK의 원본 키를 유지한다.
- 원래 BlendSpace 표본좌표, 인덱스, rate scale, smoothing, marker/notify/loop 설정을 그대로 복제한다.
- Slot 이후 보정이나 전역 retarget pose 수정은 하지 않는다. 공격·가드·점프·회피 상태의 원래 경로를 보존한다.
- raw pose 채널 비교와 실제 visible retarget 손바닥 검사를 분리한다. 전자만으로 렌더 품질을 주장하지 않는다.
- 구현 전 전체 Content SHA 및 기존 ABP checkpoint를 확보했다. 최종 허용 범위 밖 패키지는 동일 SHA여야 한다.

## 계획된 검증 명령

`Scripts/RunEditorRemote.py`를 통한 읽기/author/audit, `Scripts/BuildEditor.ps1 -NoHotReload`, `Scripts/VerifyWalkPalm.ps1`, `Scripts/VerifyMovingAttack.ps1 -Phase runtime`, `Scripts/VerifyCameraForwardAttacks.ps1 -Phase runtime`, `Scripts/CheckGeneratedChanges.ps1`.

세부 solver target/fixture 범위는 원본과 실제 evaluated 외형의 측정값으로 확정한다. 완료 결과는 paired 개발 문서에 기록한다.

## 실행 중 확인한 범위 조정

첫 보정에서는 보조 hand IK의 원본 component 위치를 맞추기 위해 부모 회전과 자식 위치를 함께 조정했다. raw 키 검사에서는 위치가 유지됐지만 실제 리타깃 외형의 `ik_hand_l`은 최대 33.25 cm 이동했다. 소스와 외형의 IK 기준 좌표 및 translation retarget 설정이 달라 raw 위치 보정만으로 실행 결과를 보장할 수 없었다. 해당 결과는 실패 증거로 남기고, 보조 IK의 원본 키를 전부 복원한 손목 회전 전용 보정으로 범위를 줄였다. 최종 판정은 새 실행 증거의 원본 IK 유지 여부까지 확인한다.
