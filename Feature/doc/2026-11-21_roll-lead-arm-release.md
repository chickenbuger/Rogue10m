# 대검 구르기에서 오른팔을 떼는 동작

대검을 든 채 구르면 오른손이 지지 자세에서 돌아오는 중간에 팔뚝이 한 프레임 만에 뒤집혔다. 정면, 오른쪽, 후방 모두 같은 진행도에서 재현됐다. 부모 본 기준 회전의 부호를 보정해 비교했고, 화면에 보이는 외형과 Manny 소스, 마지막 Blender 원본 및 FBX에서 같은 급회전을 확인했다.

프로젝트 소유 Manny 구르기의 오른쪽 위팔·아래팔·손 회전 키를 Blender 4.5.14에서 다시 맞췄다. 손이 바닥에 가까운 자세를 지나면서 팔꿈치 방향을 일정하게 이어 주고, 어깨가 몸을 받는 구간에서 손을 몸 쪽으로 되돌린다. 기존 Unreal 경로의 Sequence 한 개를 업데이트했다. 새 외부 클립을 들여오지 않고 기존 스켈레톤, 60Hz 25구간, 0.416667초 및 제자리 루트를 사용했다. [계획](../architect/2026-11-21_roll-lead-arm-release.md) · [Blender 반출 검사](evidence/roll-lead-arm-release-20261121/blender-fresh-reimport.json) · [Unreal 반입 검사](evidence/roll-lead-arm-release-20261121/unreal-import.json)

## StartHub에서 측정한 변화

격리된 Human Male 프로필에 초급 대검을 장착했다. C.EvadeRecovery, C.Roll, C.EvadeDistance, C.RollCooldown을 배운 뒤 실제 컨트롤러 입력 경로에 Ctrl, D+Ctrl, S+Ctrl을 보냈다. 기준은 b662c3b5, 수정본은 피격·전방 착지·이동 중 대검 전환을 포함한 a710c576 통합 기준에서 실행했다. 진행 중 22프레임을 60Hz로 측정했다. [프레임별 측정 요약](evidence/roll-lead-arm-release-20261121/release-metrics.json) · [실행 결과](evidence/roll-lead-arm-release-20261121/runtime-results.txt)

| 표시 외형에서 측정한 항목 | 이전 | 수정 |
| --- | ---: | ---: |
| 46.7→51.4% 오른팔뚝의 부모 기준 회전 | 153.26° | 18.33° |
| 56.0→60.7% 오른팔뚝 축 회전 | 106.45° | 37.84° |
| 지지 해제 구간의 한 프레임 최대 팔꿈치 굽힘 평면 변화 | 122.13° | 41.41° |
| 35~48% 구간 오른손목의 수평 이동 범위 | 10.05cm | 5.88cm |
| 대검 전체 경계 상자의 최저 바닥 여유 | 1.045cm | 1.235cm |

세 방향 모두 요청 방향으로 304.56cm 이동했다. 회피 도중 일반 공격이 수락되지 않았고 일어서면 대검의 양손 자세로 돌아왔다. 머리 본의 최저 바닥 여유는 32.28cm였고 발볼 본은 3.34cm 이상이었다. 오른쪽과 후방의 접힌 자세에서 각 무릎과 팔꿈치 방향은 정면과 0.04° 이내였다. Unreal 가져오기와 세 방향의 수치 검사 출력에서 새 오류는 0건이었다. 동일 통합 버전에서 3인칭 카메라·이동·일반 회피 검사 1,168개를 통과했고 기존 QA 저장 28개를 보존했다.

후방 구르기의 같은 카메라에서 51.4%와 60.7% 자세를 720p로 비교했다. 수정본의 렌더 기준은 96b1e21d다. 오른팔을 몸에 가깝게 접는 동안 손잡이가 오른손에 있고 칼날과 지면 그림자가 분리됐다. 회복 때 양손 그립이 돌아왔다. Microsoft Basic Render Driver 선택 전 NVIDIA 장치 검사에 DXGI 887A0004가 기록됐으며, 소프트웨어 렌더는 플레이와 예정한 사진을 완료했다. 검증 범위는 기본 Human Male 프로필과 프로그램으로 보낸 입력이다. [51.4% 수정 전](evidence/roll-lead-arm-release-20261121/baseline-back-release-entry.png) · [51.4% 수정 후](evidence/roll-lead-arm-release-20261121/back-release-entry.png) · [60.7% 수정 전](evidence/roll-lead-arm-release-20261121/baseline-back-release-exit.png) · [60.7% 수정 후](evidence/roll-lead-arm-release-20261121/back-release-exit.png) · [회복](evidence/roll-lead-arm-release-20261121/back-recovered.png)

StartHub 게임 실행에서 Rogue10m.TestHybridSkillTree를 호출한다. -RogueHybridRollQA, -RogueRollAuditDirection=Forward|Right|Back, -UseFixedTimeStep, -FPS=60을 선택하고 매번 기존 QA 접두사가 붙은 새 프로필·성장·스킬키 슬롯과 새 -RogueHybridEvidenceLabel을 사용한다. 실제 표시 본과 원본 Manny는 같은 CSV에 기록되며 원본의 본 이름에는 source. 접두사가 붙는다.
