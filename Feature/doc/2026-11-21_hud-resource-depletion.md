# 전투 HUD의 자원 소모 표시

체력, 스태미나, 마나, 스킬로 소모되는 아이덴티티 게이지가 한 번에 최대치의 10% 이상 감소하면 표시 끝과 숫자가 약 0.4초 동안 감속하며 실제 값에 수렴한다. 남은 자원 다음부터 움직이는 끝까지 같은 색을 조금 어둡게 칠하고, 목표 위치에 가까운 부분을 가장 진하게 표시한다.

연속 소모는 현재 보이는 끝에서 이어진다. 보이는 끝 아래까지 회복하면 남아 있는 밝은 구간만 늘고, 끝에 도달할 만큼 회복하면 현재 값을 즉시 표시한다. 처음 표시할 때와 캐릭터·최대치·자원 종류가 바뀔 때는 새 값으로 초기화한다. 구현 범위는 현재 플레이에 쓰는 전투 HUD A다.

## 시각 초안

기존 StartHub 1280×720 캡처를 참고 이미지로 주고 내장 ImageGen으로 스태미나 83 → 61 → 38의 세 상태를 생성했다. HUD는 기존 텍스처의 채움 영역을 나눠 같은 효과를 렌더링한다. [ImageGen 초안](evidence/hud-resource-depletion-20261121/imagegen-draft.png)

<details>
<summary>ImageGen 프롬프트</summary>

```text
Use case: ui-mockup. Asset type: a single polished landscape game-HUD motion storyboard for Rogue10m, a third-person action RPG. Image 1 is a reference only for the existing upper-left resource UI: compact dark beveled bronze-gunmetal frames, green health, golden amber stamina, charcoal unfilled track, white Korean UI numerals. Show a close-up design sheet with three aligned frames stacked vertically on a quiet graphite backdrop. Each frame repeats a single identical horizontal stamina gauge shape and width, readable at a glance. Row labels and numbers exactly: "소모 전" with "83 / 100", "감소 중" with "61 / 100", "완료" with "38 / 100". All gauges fill left to right. In row 1, golden amber fills from zero to 83%; rest is charcoal. In row 2, bright golden amber fills from zero to the final target at 38%, and from that target at 38% to the moving edge at 61% use a darker shade of the SAME amber: subtly deepest immediately at the target boundary on the left, lighter toward the moving right edge, always darker than the remaining gold. From 61% to 100% is visibly charcoal. In row 3, only the first 38% is golden, the rest charcoal. Keep the moving part contiguous with the healthy fill, no gap. Use precise clean typography and uncluttered composition, no decorative art or arrows, no additional labels, no watermarks. Match the game screenshot's restrained metal frame and amber hue.
```

</details>

## 실행 기록

첫 격리 실행은 StartHub 로드 전에 UE의 플랫폼 SDK 조회가 멈춰 종료했다. 이 실행에서도 기존 저장 4개의 SHA-256이 일치했다. 후속 검증에서는 프로세스의 `UE_SKIP_UBT_SDK_SETUP=1` 설정으로 해당 부팅 단계를 건너뛰었다.

최종 후보는 `46b4c62a`에 자원 소모 변경을 합친 소스다. UE 5.8 Editor Development 빌드를 통과했다. `Scripts/VerifyCombatHudA.ps1 -Label HudDepletionIntegrated01`로 1672×941과 1280×720에서 실제 StartHub를 실행해 75개 검사 중 75개를 통과했다. 프로그램 입력과 테스트 프로필의 GAS 변경으로 큰 소모, 연속 소모, 회복, 마나 전환, 작은 감소, 최종 수렴을 확인했다. 기존 저장 4개는 SHA-256이 일치했다. 부팅 시 기록된 오류 58줄은 기존 UE Python 초기화 트레이스와 모두 일치했고, StartHub HUD 검증 구간의 신규 오류는 0건이었다. [검사 결과](evidence/hud-resource-depletion-20261121/runtime-contract.txt)

약 0.1초 시점의 측정에서 스태미나 표시 끝은 90%에서 65.5%까지 줄었고 목표는 35%였다. 체력은 80%에서 62.2%까지 줄었고 목표는 40%였다. 대검 아이덴티티는 100%에서 55.4%까지 줄었고 목표는 0%였다. PNG 저장 프레임의 스태미나 픽셀은 같은 높이에서 남은 구간의 RGB (194, 139, 39), 목표 경계 직후의 (158, 113, 31), 이동 중인 끝에 가까운 구간의 (177, 127, 35)로 측정됐다. [실제 게임 화면](../../DevLog/20261121_HudResourceDepletion/depletion-720p.png)
