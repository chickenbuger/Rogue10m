# 아린 자연스러운 대기 자세 검증 기록

- 작업: D:/Project/Rogue10m, Sprint#9-29-humanoid-boss-npc. 2026-11-10, UE 5.8.4.
- 목적: 기존 아린 외형과 대화/보급 동작을 보존하면서 팔을 덜 벌리고, 작은 호흡과 어깨/손 움직임을 추가한다.
- 참고 원칙: PlayStation의 The Last of Us Part II 애니메이션 인터뷰(https://blog.playstation.com/?p=334953)를 설계 참고로 사용했다. 해당 게임 애니메이션이나 데이터를 복제하지 않았다.

## 출처와 실제 경로

- Mixamo `Breathing Idle`, X Bot With Skin, FBX Binary, 30fps, keyframe reduction none.
- 원본 SHA256: `0fd99763f82a37175eb3a45a174af647f552c71017eca14e70b8a2ebf28bd8f2`.
- 실제 리타깃: Mixamo → `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple` → 기존 `/Game/Rogue10m/Characters/Arin/RefinedV3/SK_Arin`.
- 소스, IK Rig, Retargeter, Manny 중간 결과, 실패한 Arin 원시 결과는 `/Game/Rogue10m/Characters/Arin/Standing20261110` 아래에 격리했다.
- 최종 Sequence: `/Game/Rogue10m/Characters/Arin/Standing20261110/A_Arin_BreathingIdle`.
- 활성화: `/Game/Rogue10m/World/StartVillage/Tutorial/DA_StartVillageGuide`의 GuideIdleAnimation만 변경했다. 기존 Walk/Talk, 캐릭터 메시, 보급품, 문구, 상호작용 수치는 변경하지 않았다. activation.json에 보호 필드를 기록했다.
- 원본 ArinIdle, RefinedV3 메시/스켈레톤/재질, Blender 원본은 변경하지 않았다. 별도 Montage는 필요하지 않은 반복 대기 애니메이션이다.

## 실패 후보와 보정

자동 리타깃 원시 결과는 정상 체형으로 판단하지 않았다. 기존 아린의 상위 `Arin_Rig` 100배 단위 스케일이 사라져 머리-발 간격이 약 1.3cm가 되고 발이 약 16.7cm 이동했다. retarget-report.json, scale-diagnostic.json 및 원시 Sequence를 보존했다.

최종 파생본은 기존 정상 대기의 상위 스케일/로컬 길이/루트/골반/다리를 보존하고 Manny를 거쳐 아린에 리타깃한 상체 회전을 제한적으로 혼합한다. 척추/머리 35~40%, 쇄골/손 70%, 팔 85%를 적용했다. 전신 모션 전체를 그대로 사용했다고 주장하지 않는다. 마지막 0.8초를 첫 포즈로 부드럽게 연결했다. 전체 길이는 9.933333초, 30fps, 299개 평가 샘플이다.

## 수치 검증

- 전체 구간 루트/골반/양발의 월드 기준 이동 범위: 0cm.
- 시작/마지막 샘플의 검사한 모든 본 위치 차: 0cm.
- 머리 이동 범위 최대 약 1.55cm, 손 이동 범위 최대 약 7.52cm. 기존보다 팔을 덜 벌린 비대칭적인 자연스러운 상체 자세다.
- 모든 검사 좌표 유한값, 기존 아린 스켈레톤 일치.
- adapted-report.json과 A_Arin_BreathingIdle-poses.json에 전체 경로/범위/샘플 기록.

## 실제 렌더 검증

`ViewportA/manifest.json`과 PNG 12장: 실제 StartHub의 SV_TutorialGuide_Arin, 공식 Unreal MCP CaptureViewport 사용. 정면/사선에서 기존 0초, 새 idle 0/2.5/5초, talk 0.8초, idle 복귀 포즈를 캡처했다. 각 캡처의 실제 Sequence 재생 위치와 손/발/머리/루트 월드좌표를 함께 기록했다. 정면과 사선 검토에서 체형 붕괴, 팔 꺾임, 몸통 관통, 발 미끄러짐은 보이지 않았다. 보행/전투용 발 IK를 새로 구현한 것은 아니다. 에디터 가이드 선/아이콘이 일부 캡처에 보이며 게임 HUD 캡처라고 부르지 않는다.

캡처는 동작별 정지 포즈 검토다. 정지 프레임만으로 실시간 전환 블렌딩을 검증했다고 주장하지 않는다. 공격 애니메이션이 아니므로 contact/첫 피해 프레임은 해당하지 않는다. 카메라와 검토용 component 상태를 복원했으며 지도는 저장하지 않았다.

## 실제 PIE 상태 검증

runtime.json: 10개 검사 모두 통과. 실제 설정된 아린 → 새 대기 → 네이티브 BeginDialogue → 기존 Talk → CloseDialogue → 새 대기 복귀를 검사했다. 루트와 양발 위치도 복귀 전후 0.05cm 이내다. 격리된 QA 저장 슬롯으로 실행했으며 보급품 지급, 인벤토리 변경, 수동 키 입력이나 일반 필드 완주는 수행하지 않았다. 수동 F키 전체 회귀는 별도 기존 검사 영역이다. PIE는 검증 후 종료했다.

## 실행 도구와 범위

- PrepareArinStanding20261110.py: 실제 FBX import와 두 단계 리타깃.
- AuditArinStandingScale.py: 원시 후보의 스케일 진단.
- AdaptArinStanding20261110.py: 원본 단위/접지 보존 및 상체 제한 혼합.
- ReviewArinStanding20261110.py, CaptureArinStanding20261110.py: 실제 에디터 캡처 및 상태 복원.
- ActivateArinStanding20261110.py: idle 참조만 저장.
- ValidateArinStanding20261110.py: 실제 PIE 대화 상태 검증.

C++ 런타임 코드는 변경하지 않았으며 보스 작업의 합동 빌드는 부모 작업에서 진행한다. Mixamo 원본 staging과 X Bot/실패 후보는 공개 저장소 배포 범위와 라이선스를 별도 검토해야 하며 자동으로 전체 추가하면 안 된다.
