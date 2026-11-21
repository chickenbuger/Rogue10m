# 아린 대화 시작과 종료 자세 개선

실제 StartHub의 아린이 대화에 들어가거나 대기로 돌아갈 때 직전 자세에서 다음 클립으로 0.18초 동안 블렌드된다. 동작이 끝나기 전에 다시 대화를 열면 그 순간의 자세에서 이어간다. 기존 Mixamo 파생 대기와 Blender에서 저작한 아린 전용 대화·보행 클립을 사용한다. [설계](../architect/2026-11-21_arin-dialogue-motion.md) · [애셋과 저작 기록](../../DevLog/20261121_ArinDialogueMotion/AnimationRecord.md)

## 실제 자세 검증

UE 5.8 Development Editor를 격리 작업 사본에서 빌드했다. Standalone `StartHub`의 아린과 격리 프로필에서 실제 대화 시작·닫기 메서드를 호출했다. 대화의 0.816667초에서 두 번 모두 손목이 시작 위치로부터 약 31cm 움직인 것을 확인했다.

| 전환 | 새 포즈 수 | 첫 손목 이동 | 가장 큰 표본 간 손목 이동 |
| --- | ---: | ---: | ---: |
| 대기 → 대화 | 15 | 0cm | 1.563512cm |
| 대화 → 대기 | 15 | 0cm | 4.429038cm |
| 재열기 직전 대화 → 대기 | 3 | 0cm | 2.128676cm |
| 3포즈 뒤 대기 → 대화 | 15 | 0cm | 3.924785cm |

표는 기준 `2011e0de`에 최종 아린 코드를 적용한 WARP 렌더의 측정값이다. 표본 사이 최대 간격은 0.016667초였고, 루트·양발 이동은 최대 0.000006cm였다. 별도 원본 비교에서 대화 0.8초와 대기 첫 자세의 왼손목 간격은 31.037cm였다. 원본 비교는 클립 두 자세의 비교이며 위 표는 실제 게임 중 연속 포즈 이동량이다. [수치 검사](evidence/arin-dialogue-motion-20261121/results-warp.txt) · [본 좌표 CSV](evidence/arin-dialogue-motion-20261121/poses-warp.csv)

첫 후보에서 Unreal의 Inertialization을 네이티브 프록시에 직접 연결하자 StartHub 시작 시 `NodeData` assertion이 발생했다. 마지막 출력 자세를 저장해 Unreal 자세 보간에 전달하는 방식으로 고쳤다. 이어진 첫 렌더링은 대화 시작 두 번째 프레임에서 손목이 11.025cm 움직였다. 뼈 목록이 다시 계산되어도 직전 자세를 메시 인덱스로 이어주도록 보완했다. 실제 렌더 재시험에서 메시 LOD가 2에서 1로 바뀌었으며 대화 시작 최대 손목 이동은 1.564cm였다. [초기 노드 오류](evidence/arin-dialogue-motion-20261121/initial-native-node-failure.txt) · [첫 렌더 실패](evidence/arin-dialogue-motion-20261121/initial-render-lod-failure.txt)

## 실제 렌더 확인

`ArinMotionWarpB`가 1280×720의 `StartHub`에서 39개 검사와 결과 저장을 통과했다. Microsoft Basic Render Driver로 전환되어 게임 시계를 60Hz로 고정해 검사했다. 하드웨어 D3D11 탐색 오류 한 건은 원본 로그 924행에 있고 그 이후 오류는 0개다. 기존 QA 저장 4개를 SHA-256으로 확인했다. 오른쪽 대화창 뒤로 아린의 자세와 양발을 확인했다. 캡처에는 해당 기준 빌드의 Alt HUD 안내가 남아 있다.

- 대화 닫기 요청과 같은 게임 프레임 57에서 손이 앞으로 나온 자세를 캡처했다. 게임 시각 0.966667초, 본 갱신 58이다. [닫기 시작](evidence/arin-dialogue-motion-20261121/01_arin_close_source.png)
- 요청 0.100000초 뒤인 게임 프레임 63에서 팔이 내려가는 중간 자세를 캡처했다. 다음 재생 클립이 진행하는 동안 발 위치가 유지됐다. [중간 자세](evidence/arin-dialogue-motion-20261121/02_arin_close_blend.png)
- 다시 연 뒤 0.25초까지 본 자세를 읽고 게임 프레임 139에서 대화창과 안내인을 캡처했다. 전체 3장은 실제 게임의 캡처이며 물리 키 입력이나 필드 완주는 수행하지 않았다. [다시 열린 대화](evidence/arin-dialogue-motion-20261121/03_arin_motion_reopened.png)

## 통합 후보 재검증

`2666013d`에 아린 변경을 적용한 통합 후보에서 Development Editor 빌드 18개 작업, `ArinMotionIntegratedA`의 `NullRHI` 자세 검사 39개와 결과 저장이 통과했다. 전체 런타임 로그 오류는 0건, 기존 테스트 저장 6개는 SHA-256이 같았다. 최대 프레임 손목 이동은 대화 시작 1.412129cm, 닫기 4.429038cm, 다시 열기 3.924785cm였으며 네 전환의 첫 손목 이동은 0cm였다. 루트·양발은 0.000006cm 이내였다. 두 기준의 활성 아린 애셋 여섯 개와 `StartHub` 맵의 Git 개체 및 실제 바이트가 같음을 확인했다. [통합 결과](evidence/arin-dialogue-motion-20261121/results-nullrhi.txt) · [통합 좌표 CSV](evidence/arin-dialogue-motion-20261121/poses-nullrhi.csv) · [검증 요약](evidence/arin-dialogue-motion-20261121/runtime-verification.txt) · [애셋 해시](evidence/arin-dialogue-motion-20261121/runtime-assets-sha256.csv) · [증거 해시](evidence/arin-dialogue-motion-20261121/evidence-sha256.csv)
