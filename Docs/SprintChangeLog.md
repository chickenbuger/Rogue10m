# Sprint 변경 이력

### Sprint#29-4 — 아자토르 보행과 기본 훅 연결

- **목표** 걸음 뒤 기본 훅을 시작할 때 보이는 발 움직임을 완화한다.
- **변경** 기존 훅의 시작과 직전 보행 자세를 0.22초에 걸쳐 섞는다.
- **검증** 첫 표시 프레임 최대 본 이동 24.90→3.45cm, 첫 피해 60프레임과 회복 143프레임. 취소 후 피해가 없고 다시 시작할 수 있음을 확인했다.
- **상태** 격리 Editor 빌드, 보스 18건과 배치 몬스터 12건, 720p 준비·공격·회복 검토 완료. [계획](../Feature/architect/2026-11-21_azathor-walk-hook-handoff.md) · [결과](../Feature/doc/2026-11-21_azathor-walk-hook-handoff.md)

### Sprint#29-3 — 오크 남성 대지파쇄 양손 자세

- **목표** 오크 남성의 대지파쇄에서 보조 손이 자루를 따라가고 검이 정면 아래로 내려가게 한다.
- **변경** 프로젝트 오크 리타깃을 검증한 네이티브 부모에 연결했다. 기존 장비 전환과 구르기 동작에서 사용하는 값을 유지했다.
- **검증** 동일한 첫 피해 시점에 양손 49.65→20.01cm, 정면 110cm 표적 캡슐 교차, 바닥 여유 +8.94cm. 일반·빠른 벽 방향 전환과 늦은 D 프로그램 입력을 확인했다.
- **상태** 격리 Editor 빌드, 60Hz 프로그램 입력과 720p 준비·타격·회복 검증. [설계](../Feature/architect/2026-11-21_orcmale-earthsplitter-qa.md) · [결과](../Feature/doc/2026-11-21_orcmale-earthsplitter.md)

### Sprint#29-2 — 드워프 대지파쇄의 양손 그립

- **목표** 드워프 남녀가 움직이지 않을 때 대지파쇄를 양손으로 낮게 내려친다.
- **변경** Blender 대검 궤적을 표시 외형의 두 팔 길이에 맞춰 적용했다. 벽 가까운 남성 외형은 회복 때 검이 지나가는 공간에 따라 기본 자세를 고른다.
- **검증** 60Hz에서 실제 Q와 우클릭 48건 통과. 두 외형의 첫 피해와 원본 몽타주 시간은 동일했고, 20.01cm 손 간격과 8.94cm 최소 바닥 여유를 기록했다.
- **상태** 격리 Editor 빌드, 정지·옆걸음·벽 방향 전환 수치 및 720p 두 외형 검토 완료. [설계](../Feature/architect/2026-11-21_dwarf-earthsplitter-grip.md) · [결과](../Feature/doc/2026-11-21_dwarf-earthsplitter-grip.md)

### Sprint#29-1 — 철산고의 어깨 타격

- **목표** 짧은 주먹 모션을 사용한 철산고의 타격 자세를 맞춘다.
- **변경** Blender에서 팔을 접고 앞발로 디디며 어깨를 넣는 전용 모션을 제작했다.
- **검증** 실제 우클릭 첫 피해는 수락 후 0.233333초로 동일하다. 표시 오른어깨의 양손 대비 정면 차이는 −48.69cm에서 +6.14cm가 됐다.
- **상태** 격리 Editor 빌드, 기존·수정 프로그램 입력 및 720p 검토 완료. [설계](../Feature/architect/2026-11-21_knuckle-shoulder-motion.md) · [결과](../Feature/doc/2026-11-21_knuckle-shoulder-motion.md)

### Sprint#26-2 — 대검 대지파쇄의 낮은 내려베기

- **목표** 실제 대지파쇄 차징과 첫 피해에 맞춰 화면에서 검을 낮은 정면으로 내리게 한다.
- **변경** 보유 대검 포즈를 Blender에서 두 쌍으로 편집했다. 평평한 바닥의 인간 남성·여성과 오크 여성에 적용했다. 이동이나 벽 방향 전환 시 기본 동작으로 이어간다.
- **검증** 60Hz 프로그램 우클릭에서 첫 피해 수락 프레임과 원본 관측 0.010953초를 기록했다. 세 적용 외형의 검 최소 바닥 여유는 +11.43·24.11·26.08cm였다. 드워프 남녀와 오크 남성은 기본 동작으로 같은 입력을 완료했다.
- **상태** Editor 빌드, 대검 범위 안·밖 검사, 늦은 D와 벽 방향 전환, 인간 남녀의 720p 화면 확인 완료. [계획](../Feature/architect/2026-11-21_earthsplitter-motion.md) · [결과](../Feature/doc/2026-11-21_greatsword-earthsplitter-motion.md)

### Sprint#27-4 — 파진각의 낮은 전방 발차기

- **목표** 실제 전수 무공 파진각의 올려치기 자세를 낮은 전방 발차기로 바꾼다.
- **변경** 기존 Manny 가드에서 양손 가드와 왼발 지지를 유지하는 오른발 궤적을 Blender로 제작했다. 새 시퀀스와 몽타주를 파진각에 연결했다.
- **검증** 프로그램 Q에서 같은 첫 피해 74프레임과 정지 캡슐을 기록했다. 표시 발볼에서 훈련 대상 캡슐까지 간격은 +133.36cm에서 −3.53cm였다.
- **상태** Editor 빌드, 실제 프로그램 Q의 720p 화면, 기존 근접 전투 14건 확인 완료. [설계](../Feature/architect/2026-11-21_breaking-kick-motion.md) · [검증](../Feature/doc/2026-11-21_breaking-kick-motion.md)

### Sprint#28-2 — 패왕참의 양손 베기와 가드 복귀

- **목표** 차징을 놓았을 때 검에서 멀어지는 보조 손을 수정한다.
- **변경** 프로젝트 대검 베기를 Blender에서 양손 동작으로 편집하고 기존 패왕참의 발동에 연결했다.
- **검증** 손바닥 간격 최대 94.87→30.15cm. 발동 첫 프레임 검 방향 변화 2.485→0.225°. 동일하게 즉시 단발 피해, 0cm 캡슐 이동.
- **상태** 60Hz 우클릭 전후 2건, 차징 회귀 4건, 대검·너클 공격 14건, 720p 준비·검 접촉·회복을 검증했다. [계획](../Feature/architect/2026-11-21_greatsword-capstone-release-motion.md) · [결과](../Feature/doc/2026-11-21_greatsword-capstone-release-motion.md)

### Sprint#28-1 — 왼쪽 대검 구르기 검증

- **목표** 기존 오른쪽·후방 기록을 보완해 여섯 실제 외형의 왼쪽 구르기를 확인한다.
- **변경** 공용 구르기 런타임 검사에 실제 A 입력과 왼쪽 방향 검증을 연결했다.
- **검증** 60Hz에서 외형별 22개 자세를 기록했다. 여섯 외형 각각 304.56cm, 방향 내적 1.0, 검의 최저 바닥 여유 +0.511cm였다.
- **상태** 격리 Editor 빌드와 StartHub 프로그램 입력 확인 완료. [결과](../Feature/doc/evidence/alt-dodge-motion-20261121/left-sword-roll-20261121.txt)

### Sprint#26-3 — 대검 분쇄참의 양손 내려베기

- **목표** 분쇄참에서 크게 벌어지는 양손과 튀는 오른손목을 보완한다.
- **변경** 기존 대검 동작을 Blender에서 양손으로 들어 내리는 자세와 왼발 디딤으로 편집해 분쇄참에 연결했다.
- **검증** 최대 손바닥 간격 94.88→28.47cm, 오른손목 한 프레임 이동 22.72→10.39cm. 110·205cm 정면 표적에서 검이 캡슐을 지나고 입력 수락 틱에 첫 피해가 들어갔다.
- **상태** Blender 독립 재반입, Editor 빌드, 60Hz 대검 검사 및 720p 실제 화면 완료. [계획](../Feature/architect/2026-11-21_greatsword-crushing-slash-motion.md) · [결과](../Feature/doc/2026-11-21_greatsword-crushing-slash-motion.md)

### Sprint#27-1 — 카메라 모드별 회피 검사

- **목표** 3인칭 기본 시점과 기존 전신 1인칭의 하향 제한 및 회피 입력을 각각 검증한다.
- **변경** LookDash에서 기본 3인칭을 확인한 다음 분리한 검사 캐릭터에서 전신 1인칭 프로필을 평가한다. 검사 스크립트의 콘솔 명령을 단일 항목으로 전달한다.
- **검증** 3인칭 −70°, 전신 1인칭 −85°, Original 936개와 Weighted 940개 통과. 실제 검사 스크립트 재실행에서 Weighted 934개와 기존 저장 54개 보존을 확인했다.
- **상태** Editor 빌드와 StartHub 프로그램 입력 확인 완료. [결과](../Feature/doc/2026-11-21_alt-dodge-motion.md)

### Sprint#26-1 — 대검 파쇄참의 정면 사선 타격

- **목표** 파쇄참에서 검의 동작이 피해를 주는 정면 공간에 닿게 한다.
- **변경** 프로젝트의 대검 동작을 Blender로 편집해 가까이 든 검을 내려치고 가드로 돌아오는 전용 자세를 파쇄참에 연결했다.
- **검증** 정면 85·110·140cm에서 표시 검 궤적이 표적 캡슐을 지나며, 각 1회 피해와 입력 수락 프레임이 기존과 일치했다. 바깥 표적은 피해가 없었다.
- **상태** Editor 빌드, 60Hz 동작 및 720p 실제 화면 확인 완료. [계획](../Feature/architect/2026-11-21_greatsword-guard-break-motion.md) · [결과](../Feature/doc/2026-11-21_greatsword-guard-break-motion.md)

### Sprint#25-4 — 이동 중 대검 보행 전환

- **목표** 걸으면서 대검을 장착하고 해제할 때 표시 손목과 발이 한 프레임에 튀는 문제를 줄인다.
- **변경** 기존 Manny 보행을 계속 재생하고 포즈 차이를 상체 0.22초, 하체 0.16초 동안 줄인다. 마지막 자세는 새 루트 본 아래로 환산한다.
- **검증** 드워프 남녀와 인간 남성의 이동·정지 장비 전환을 대조했다. 드워프 남녀의 첫 해제 이동은 27.15cm와 23.92cm에서 각각 0cm로 줄었다.
- **상태** 격리 Editor 빌드와 720p 해제·보행 복귀, 드워프 구르기, 3인칭 프로그램 마우스 13개 사례를 검증했다. [설계](../Feature/architect/2026-11-21_moving-sword-locomotion-handoff.md) · [결과](../Feature/doc/2026-11-21_moving-sword-locomotion-handoff.md)

### Sprint#25-2 — 대검 회전참의 전신 스윙

- **목표** 뒤쪽까지 피해를 주는 회전참에서 캐릭터의 몸과 검이 함께 크게 도는 자세를 보여 준다.
- **변경** 기존 대검 동작을 Blender에서 편집해 회전참 전용 클립과 몽타주를 만들었다. 정지 루트와 준비·회복 자세를 적용했다.
- **검증** 두 60Hz 실행이 분쇄참, 회전참, 파쇄참, 대지파쇄의 범위 안팎 8건을 각각 통과했다. 회전참 표시 어깨 방향 15.42→409.37°, 캡슐 이동 0cm, 뒤쪽 표적 피해 1회와 수락 시점이 일치했다.
- **상태** Editor 빌드, Blender 재반입, 60Hz 프로그램 입력 및 720p 후방 스윙 확인 완료. [계획](../Feature/architect/2026-11-21_greatsword-technique-motion.md) · [결과](../Feature/doc/2026-11-21_greatsword-technique-motion.md)

### Sprint#25-3 — 몬스터 기본 공격·피격 자세 연결

- **목표** 필드 몬스터가 보행에서 기본 공격으로 넘어가거나 피해를 받고 돌아올 때 자세가 튀는 문제를 줄인다.
- **변경** 지상 보행을 섞는 다섯 프로필에서 기존 공격·피격 시퀀스를 0.10초 슬롯 전환으로 재생한다. 기존 피해·회복 타이머로 슬롯을 시작하고 종료한다.
- **검증** 여섯 종의 대기·보행 12개 사례를 비교했다. 가시수호자의 보행 중 첫 공격 본 이동은 60.37→8.85cm였으며 피해 17프레임과 회복 41프레임이 일치했다.
- **상태** [설계](../Feature/architect/2026-11-21_monster-combat-transitions.md) · [검증](../Feature/doc/2026-11-21_monster-combat-transitions.md)

### Sprint#25-1 — 플레이어 지상 패배 자세

- **목표** 지상 치명 피해 뒤 패배 화면 가장자리에서 플레이어가 다시 서는 자세를 보완한다.
- **변경** Manny 원본을 Blender에서 제자리 쓰러짐으로 편집하고 마지막 표시 전 원본 자세를 0.22초 섞는다. 결과는 실제 피해 다음 틱에 열린다.
- **검증** OpenRunMap 대기·달리기·공중 대조 137개와 1280×720 패배·귀환 66개를 통과했다. 달리기의 첫 머리 회전은 16.39→0.28°였다.
- **상태** 격리 Editor 빌드와 수치·실제 화면 검증 완료. [설계](../Feature/architect/2026-11-21_player-grounded-defeat.md) · [결과](../Feature/doc/2026-11-21_player-grounded-defeat.md)

### Sprint#24-4 — 외형별 대검 구르기 바닥 여유

- **목표** 여섯 외형에서 검이 튀거나 바닥을 통과하는 구르기 진입을 고친다.
- **변경** 직전 검 방향을 기준으로 칼을 옮기고, 유효한 바닥에 맞춰 두 드워프의 오른팔 지지를 보정했다.
- **검증** 여섯 실제 외형의 후방 구르기에서 검 경계 상자 바닥 여유 +0.63cm 이상, 진입 최대 회전 33.96°를 확인했다. 단차, 드워프 착지와 정지 장착 회귀를 통과했다.
- **상태** 격리 Editor 빌드와 드워프 남성 720p 진입·손 지지 비교 완료. 기존 드워프 이동 중 해제 급변은 별도 기록했다. [설계](../Feature/architect/2026-11-21_avatar-motion-coverage.md) · [검증](../Feature/doc/2026-11-21_avatar-motion-coverage.md)

### Sprint#24-3 — 필드 달리기 착지 오른발 회복

- **목표** 달리기 착지에서 오른발이 앞으로 나올 때 발목과 종아리의 급격한 회전을 줄인다.
- **변경** 기존 Blender 원본에서 오른쪽 다리 네 트랙을 보정해 Manny 파생 착지 시퀀스에 반영했다. 두 발의 동기 마커와 재생 길이를 보존했다.
- **검증** 실제 동굴 오르막에서 표시 외형의 오른 발목 최대 프레임 회전이 40.13→11.73°로 줄었다. 실제 필드 네 코스, 평지 착지 9,640개 조건과 Blender FBX 재가져오기를 검증했다.
- **상태** 격리 Editor 빌드와 프로그램 이동·점프 검증을 완료했다. [설계](../Feature/architect/2026-11-21_field-ground-motion.md) · [결과](../Feature/doc/2026-11-21_field-ground-motion.md)

### Sprint#24-2 — 태산압정 공중 자세

- **목표** 실제 배운 공중 무공 태산압정에서 내려치는 손과 낙하·착지의 다리 동작을 읽을 수 있게 한다.
- **변경** 기존 Manny 원본을 Blender에서 상체 가산 클립으로 편집해 태산압정에만 연결했다.
- **검증** 한 번과 두 번의 Space 뒤 Q로 실제 몽타주와 같은 프레임의 첫 피해를 확인했다. 게임 이동과 몽타주 시계의 총 205프레임이 기준에 일치했다.
- **상태** Editor 빌드, 수치 실행 및 720p 공중 내려치기 확인 완료. [계획](../Feature/architect/2026-11-21_active-skill-motion.md) · [검증](../Feature/doc/2026-11-21_active-skill-motion.md)



### Sprint#24-1 — 필드 몬스터 사망 자세 전환

- **목표** 대기·보행·공격 준비에서 치명 피해를 받을 때 표시 자세의 급변을 줄인다.
- **변경** 다섯 몬스터의 바로 전 표시 자세를 사망 클립의 처음 0.15초에 섞는다.
- **검증** 여섯 실제 프로필의 18개 사망 구간에서 클립 재생 및 제거 시점이 일치했다. 가시수호자의 보행 뒤 첫 프레임은 77.77→2.44cm였다.
- **상태** UE Editor 빌드, 60Hz 검사 189개와 720p 전후 비교 32개 확인. [설계](../Feature/architect/2026-11-21_monster-death-motion.md) · [결과](../Feature/doc/2026-11-21_monster-death-motion.md)

### Sprint#23-3 — 일반 몬스터 보행 전환

- **목표** 초록·붉은 점액체와 철갑 등껍질의 걸음 시작 및 정지 자세의 튐을 줄인다.
- **변경** 세 종의 기존 대기와 이동 시퀀스를 지상 보행 그래프에서 섞도록 실제 프로필을 저장했다.
- **검증** 60Hz 정수리·뿔의 보행 시작 최대 이동이 10.64→1.29cm와 3.48→0.97cm였다. 지속 보행의 몸 높이·등껍질 움직임과 한 번의 기본 공격 피해·피격 복귀를 대조했다.
- **상태** Editor 빌드, 세 transient A/B와 실제 저장본의 독립 재실행 확인. [설계](../Feature/architect/2026-11-21_ordinary-monster-gait.md) · [결과](../Feature/doc/2026-11-21_ordinary-monster-gait.md)

### Sprint#23-1 — 대검 구르기 오른팔 지지 해제

- **목표** 대검 구르기에서 오른팔이 지지를 끝내고 몸 쪽으로 돌아올 때의 급회전을 줄인다.
- **변경** Blender에서 기존 Manny 구르기의 오른쪽 위팔·아래팔·손 키를 조정하고 동일 Sequence를 다시 반입했다.
- **검증** 60Hz 정면·오른쪽·후방의 표시 외형에서 최대 지지 해제 팔뚝 회전이 153.26→32.76°로 줄었다. 세 방향에서 각각 304.56cm 이동했고 대검 경계가 바닥 위 1.235cm 이상이었다.
- **상태** Editor 빌드와 세 방향 입력 검사, 720p 후방 비교 화면을 확인했다. [계획](../Feature/architect/2026-11-21_roll-lead-arm-release.md) · [검증](../Feature/doc/2026-11-21_roll-lead-arm-release.md)



### Sprint#23-2 — 이동 중 대검 해제 첫 자세

- **목표** 걷기 시작하며 대검을 해제할 때 첫 그래프 평가의 손·발 이동을 줄인다.
- **변경** 첫 평가에 이전 전체 자세를 전달하고, 시간이 흐르면 움직이는 다리를 현재 걸음에 더 빠르게 반영한다.
- **검증** 136.53cm/s에서 해제 호출 안의 본 이동 29.25cm/36.04°가 0으로 줄었다. 새 표시 프레임에서 손은 1.28cm, 발은 3.31cm 움직였다.
- **상태** 통합 빌드와 인간 남성 실제 W·인벤토리 경로, 정지 장비 전환 회귀를 확인했다. [설계](../Feature/architect/2026-11-21_moving-weapon-pose.md) · [결과](../Feature/doc/2026-11-21_moving-weapon-pose.md)

### Sprint#22-1 — 대검 장착·해제 자세 전환

- **목표** 인벤토리 대검 장착 및 해제 직후 손과 발의 튐을 줄인다.
- **변경** 이전 Manny 가드를 먼저 내리고, 인간 남성은 두 기존 리타깃 그래프를 마지막 표시 포즈에서 0.18초 동안 전환한다.
- **검증** 여섯 프로필의 해제 첫 표시에서 손과 발의 차이가 최대 1.4cm로 줄었다. 인간 남성 장착 첫 표시도 16.27cm에서 0.40cm로 줄었다.
- **상태** UE Editor 빌드, 실제 인벤토리 경로 6건, 720p 4장, 전투 이동 회귀 6,497개 확인. [설계](../Feature/architect/2026-11-21_weapon-equip-pose-audit.md) · [검증](../Feature/doc/2026-11-21_weapon-equip-pose-audit.md)

### Sprint#21-2 — 플레이어 피격 자세

- **목표** 3인칭 캐릭터가 실제 비치명적 피해를 받을 때 짧은 상체 반응을 보여 준다.
- **변경** 보유 Manny 전후 피격 원본을 Blender에서 0.25초 척추 가산 클립 두 개로 편집했다. 실제 체력 감소와 기존 공격 중단 뒤에 재생한다.
- **검증** StartHub의 실제 전후 점 피해와 프로그램 마우스 공격, 피해 거부와 연속 피격을 확인했다. 60Hz에서 5프레임 흉부 회전 5.67°와 5.86°, 발 추가 이동 0.050cm 이내를 기록했다.
- **상태** 대검 장착 통합 Editor 빌드, 피격 2,048개, 3인칭 카메라·머리·팔 1,630개, 이단 점프 2,976개 통과. 장착 통합 전 별도 720p 대기·피격 화면 확인. [설계](../Feature/architect/2026-11-21_player-hit-pose-audit.md) · [검증](../Feature/doc/2026-11-21_player-hit-pose-audit.md)

### Sprint#21-1 — 두 번째 점프 하체 동작

- **목표** 실제 수직 속도가 다시 올라가는 두 번째 Space 입력에서 짧은 다리 동작이 보이게 한다.
- **변경** 기존 Manny 점프와 낙하를 Blender에서 비교해 60Hz 0.25초 공중 다리 시퀀스를 만들고 기존 낙하 상태 위에 합쳤다.
- **검증** StartHub 수치 검사 2,976개와 소프트웨어 렌더 1,748개를 통과했다. 표시 메시의 왼발 차이는 최대 19.54cm였고 점프 속도와 착지 프레임은 같았다.
- **상태** Editor 빌드와 실제 입력 기반 수치·720p 사진 두 장 확인. [설계](../Feature/architect/2026-11-21_double-jump-motion-audit.md) · [결과](../Feature/doc/2026-11-21_double-jump-motion-audit.md)

### Sprint#20-2 — 패왕참 차징 준비 자세

- **목표** 대검 패왕참의 우클릭 유지에 준비 자세를 보여 주고 발동 첫 장면으로 자연스럽게 연결한다.
- **변경** 기존 Manny 대검 포즈를 Blender에서 편집해 120Hz 상체 가산 클립을 만들었다. 표시 메시 기준으로 가중치를 0.40에 맞췄다.
- **검증** 발동 첫 표시의 양손목은 1cm 미만 움직였고, 기존과 보정본 모두 몽타주 0초에 첫 피해가 적용됐다. 조기 해제, 취소, 카메라 ±65°를 확인했다.
- **상태** UE Editor 빌드, 패왕참 유지·발동 사례 4건, 기존 근접 회귀 14건, 720p 동일 자세 단계 2장 검증을 완료했다. [설계](../Feature/architect/2026-11-21_capstone-charge-brace.md) · [검증](../Feature/doc/2026-11-21_capstone-charge-brace.md)

### Sprint#20-1 — 스포어링 대기·보행 전환

- **목표** 스포어링이 걸음을 시작하거나 멈출 때 치마와 머리 자세가 튀는 현상을 줄인다.
- **변경** 스포어링의 기존 대기·걷기 클립을 활성 프로필의 공용 지상 블렌딩으로 연결했다.
- **검증** StartHub 60Hz에서 최대 230cm/s 및 138.15cm 이동이 일치했다. 치마 본 시작 구간 최대 변화가 27.91cm에서 3.96cm로 줄었고 첫 피해와 회복 시점이 일치했다.
- **상태** UE Editor 빌드, 임시·저장본 각각 17개와 가시수호자 회귀 10개 검증 완료. [설계](../Feature/architect/2026-11-21_sporeling-walk-transition.md) · [측정 기록](../Feature/doc/2026-11-21_sporeling-walk-transition.md)

### Sprint#19-2 — 가시수호자 대기·이동 전환

- **목표** 걸음을 시작하거나 멈출 때 가시수호자의 팔 자세가 튀는 현상을 줄인다.
- **변경** Cactus의 기존 대기·걷기 클립을 보행 그래프로 보간했다.
- **검증** 250cm/s 이동·감속의 오른손 끝 최대 프레임 이동이 76.84cm에서 9.54cm로 줄었다. 기존 공격 클립·준비 시간과 보행 복귀를 확인했다.
- **상태** 빌드 및 StartHub 수치 검사 10개 통과. [설계](../Feature/architect/2026-11-21_thornwarden-walk-blend.md) · [검증 기록](../Feature/doc/2026-11-21_thornwarden-walk-blend.md)

### Sprint#19-1 — 스킬트리 잠금과 미발견

- **목표** 아직 배우지 못한 능력과 현재 잠겨 있지만 이미 확인한 능력을 스킬트리에서 구분한다.
- **변경** 공통·너클·검에 황동 자물쇠와 미발견 물음표를 적용하고, 첫 습득 기록을 캐릭터 프로필에 보존한다.
- **검증** 프로그램 입력 342개, 프로필 재실행 111개, 720p·1080p 화면을 검증했다. 최신 후보의 Editor 빌드와 자동화 13개도 통과했다.
- **상태** 구현과 게임 검증 완료. [설계](../Feature/architect/2026-11-21_skill-tree-discovery-locks.md) · [검증과 화면](../Feature/doc/2026-11-21_skill-tree-discovery-locks.md)

### Sprint#18-3 — 아자토르 보행 접촉

- **목표** 아자토르의 이동 중 지지 발이 미끄러지는 정도를 줄인다.
- **변경** 활성 Garret 보행 클립의 발 궤적을 측정해 실제 수평 속도와 재생률을 연결했다.
- **검증** 240cm/s 보행에서 지지 발의 미끄러짐 속도 중앙값이 59.446cm/s에서 2.184cm/s로 줄었다. UE 5.8 통합 빌드, 보행 41개와 전투 18개 시나리오의 401개 검사가 통과했다.
- **상태** 보행 수치와 보스 전투 회귀 검수 완료. [설계](../Feature/architect/2026-11-21_azathor-walk-contact.md) · [결과](../Feature/doc/2026-11-21_azathor-walk-contact.md)

### Sprint#18-2 — 대검 가드와 지상 이동 전환

- **목표** 대검을 들고 이동을 시작하는 첫 프레임의 흉부와 손 자세 튐을 줄인다.
- **변경** 3인칭 대검의 일반 지상 이동에서 기존 가드를 최대 0.18초로 보간한다. 차지·몽타주·공격 회복·회피·공중 동작은 기존 선택 조건을 사용한다.
- **검증** UE 5.8 Editor 빌드와 `4cba9894` 통합 기준의 StartHub 포즈 검사 6,497개, 기본 3인칭 회귀 1,568개·마우스 13건, `a24f522e` 기준의 소프트웨어 렌더 검사 994개를 통과했다. 첫 일곱 프레임의 흉부 최대 변화는 27.45°에서 3.44°, 골반 상대 손 최대 이동은 19.43cm에서 2.72cm로 줄었다.
- **상태** 독립 작업 사본에서 구현 및 실행 확인. [작업 계획](../Feature/architect/2026-11-21_combat-animation-continuity.md) · [측정 결과](../Feature/doc/2026-11-21_combat-animation-continuity.md)

### Sprint#18-1 — 전방 달리기 착지

- **목표** 맨손 3인칭 전방 착지에서 발을 끄는 동작을 줄이고 다음 달리기 보폭으로 연결한다.
- **변경** 기존 Manny 낙하·착지·Jog를 Blender에서 편집해 0.45초 인플레이스 클립을 만들고 두 번째 오른발 Jog 마커로 인계했다.
- **검증** StartHub 60Hz 통합 검수 9,640개를 통과했다. 620/610cm/s 표시 외형에서 접지 부근 발볼 이동은 각각 77.1%/77.2% 줄었고, 정지와 옆 방향은 기존 `MM_Land`였다.
- **상태** 통합 UE Editor 빌드 및 720p WARP 2,396개 검사와 측면 두 장 확인. [설계](../Feature/architect/2026-11-21_player-locomotion-audit.md) · [검증](../Feature/doc/2026-11-21_player-locomotion-audit.md)

### Sprint#17-5 — 아린 대화 자세 전환

- StartHub 아린의 대화 시작과 종료에 기존 Idle·Talk 자세가 부드럽게 이어지도록 연결했다. 실제 안내인에서 전환 전후 손목·루트·발 위치를 기록하는 검사를 추가했다.
- 통합 기준 `c81da27c`의 UE 5.8 Editor 빌드와 StartHub 60Hz 수치 검사 39개를 통과했다. 소프트웨어 렌더에서 LOD 변경과 대화 전환을 확인했다. 닫기의 최대 프레임 손목 이동은 4.43cm, 다시 열기는 3.92cm였다. [작업 계획](../Feature/architect/2026-11-21_arin-dialogue-motion.md) · [검증 기록](../Feature/doc/2026-11-21_arin-dialogue-motion.md)

### Sprint#17-4 — 너클·대검 공격 VFX

- **목표** 손과 검날의 공격 흐름, 실제 공격 범위와 명중점을 기본 3인칭에서 읽을 수 있게 한다.
- **변경** ImageGen 초안을 바탕으로 금색 너클 잔상과 청록색 대검 리본을 제작했다. 바닥 윤곽은 현재 판정에 맞추고, 피해가 확정된 대상 표면에는 작은 섬광을 표시한다.
- **검증** UE 5.8 Editor 빌드, 범위·타격점 자동화 2건, 720p 실제 전투 14개 케이스와 46장 캡처를 통과했다. 두 최상위 기술의 범위 밖 공격은 피해와 적중 섬광 0회였다. 최신 키·콘텐츠 통합본의 NullRHI 실제 전투 14개 케이스도 통과했다.
- **상태** 구현과 통합 검증 완료. [설계](../Feature/architect/2026-11-21_melee-animation-vfx.md) · [검증과 화면](../Feature/doc/2026-11-21_melee-animation-vfx.md)

### Sprint#17-3 — Content 기능별 정리와 레거시 제거

- **목표** 현재 프로젝트의 에셋을 기능별로 탐색하고 사용이 끝난 콘텐츠를 제거한다.
- **변경** 공유 동적 컬렉션 17개, 에셋 9개 경로 정리, 레거시 패키지 118개 삭제.
- **검증** 독립 Unreal 프로세스에서 이전 경로 참조 0개, 이동·참조 에셋 18개 로드, StartHub·OpenRunMap Map Check 오류·경고 0건을 확인했다.
- **상태** 원본 Content 적용 및 검수 완료. [설계](../Feature/architect/2026-11-21_content-organization.md) · [콘텐츠 인덱스와 검증](../Feature/doc/2026-11-21_content-organization.md)

### Sprint#17-1 — 전투 키 변경

- **목표** 대시를 왼쪽 Ctrl로 옮기고 V로 무기 아이덴티티를 발동한다.
- **변경** 너클 고유기와 외형 확인 카메라를 각각 V와 C에 연결하고 A안 HUD 및 조작 안내를 갱신했다.
- **검증** UE 5.8 빌드와 StartHub 1672×941·1280×720의 72개 검사를 통과했다. 키 안내가 해당 프레임에 들어가고 고정 슬롯 좌표가 일치했다.
- **상태** 적용 및 검증 완료. [설계](../Feature/architect/2026-11-21_combat-key-remap.md) · [검수와 캡처](../Feature/doc/2026-11-21_combat-key-remap.md)

### Sprint#16-3 — 너클·검 스킬트리 UI와 탐험 성장

- **목표** 공통 화면의 시각 구성을 너클과 검 스킬트리에 적용하고 내공·투지·마나는 레벨과 영약으로 성장시킨다.
- **변경** 숯색·황동 패널과 무기 메달 16종을 적용했다. 기존 아이덴티티 화면을 제거했고 수련 경로, 검의 분기 투자, 장착 무기 우선 탭을 유지했다.
- **검증** 전투 키 변경과 에셋 정리 통합 후 실제 1080p·720p 화면, 런타임 326개, 저장 재실행 106개와 자동화 13개를 통과했다. 하드웨어 그래픽 어댑터 상태로 인해 최신 화면은 Direct3D 11 소프트웨어 렌더링에서 확인했다.
- **상태** 통합 검증 완료. [설계](../Feature/architect/2026-11-21_weapon-skill-tree-identity-growth.md) · [결과와 캡처](../Feature/doc/2026-11-21_weapon-skill-tree-identity-growth.md)

### Sprint#17-2 — 전투 HUD 자원 소모 애니메이션

- **목표** 큰 폭으로 줄어든 자원과 남은 자원을 한눈에 구분한다.
- **변경** 전투 HUD A의 체력·스태미나·마나·소모형 아이덴티티에서 최대치의 10% 이상 감소하면 게이지 끝과 숫자를 약 0.4초 동안 보간한다. 소모 구간의 목표값 쪽을 조금 더 진하게 표시한다.
- **검증** UE 5.8 통합 후보 빌드와 StartHub 검사 75개를 통과했다. 720p 실제 캡처에서 소모 구간의 색상 차이를 확인했다.
- **상태** 적용 및 검증 완료. [설계](../Feature/architect/2026-11-21_hud-resource-depletion.md) · [검사 결과](../Feature/doc/2026-11-21_hud-resource-depletion.md)

### Sprint#16-3 — 기본 회피와 어깨 구르기

- **목표** 뒤나 옆으로 이동할 때 앞걸음이 재생되는 문제와 배운 구르기의 머리·무기 바닥 관통을 수정한다.
- **변경** 전방 스텝의 적용 방향을 제한했다. 구르기는 Blender로 제작한 Manny 시퀀스를 사용하고 준비와 복귀를 블렌딩한다. 대검은 접힌 자세에서 오른손 바깥쪽으로 향한다.
- **검증** 최신 Ctrl 바인딩을 포함한 UE 5.8 빌드, StartHub 검사 1,604개와 마우스 13건, 전수 구르기 수치·720p 렌더를 통과했다. 대검 경계의 최소 바닥 여유는 1.05cm였다.
- **상태** 전방 구르기와 전후·우측 기본 회피 검수 완료. [설계](../Feature/architect/2026-11-21_animation-motion-audit.md) · [검수와 캡처](../Feature/doc/2026-11-21_alt-dodge-motion.md)

### Sprint#16-2 — 3인칭 팔의 상하 시선 추종 제거

- 3인칭 팔이 카메라의 높낮이를 따라 움직이는 보정을 제거했다. 가드·대검 준비 자세와 카메라 조준을 함께 확인했다.
- UE 5.8 빌드와 StartHub 수치 검사 1,497개를 통과했다. ±65°에서 본 위치 차이는 0cm, 회전 차이는 최대 0.000002°였다. [설계](../Feature/architect/2026-11-21_arm-look-pitch.md) · [결과](../Feature/doc/2026-11-21_arm-look-pitch.md)

### Sprint#16-1 — A안 HUD 글자와 아이덴티티 정리

- **목표** 공격 슬롯에서 넘치는 키 안내와 원형 계기판의 사각형 흔적을 없앤다.
- **변경** HUD 글자를 기준 해상도에서 2px 축소하고 슬롯 키를 내부 폭에 맞췄다. 자원 없는 무기는 퍼센트를 생략하며 `Ctrl` 안내는 너클 질풍투기에 연결된다.
- **검증** UE 5.8 빌드·임포트, 실제 StartHub의 63개 검사, 기본 카메라 맨손 캡처, 대검과 너클 및 픽셀 비교를 통과했다.
- **상태** 적용 및 검수 완료. [설계](../Feature/architect/2026-11-21_hud-label-polish.md) · [실행 검수](../Feature/doc/2026-11-21_hud-label-polish.md)

### Sprint#15-1 — 승인 A안 전투 HUD

- **목표** 승인 A안 좌표와 그림을 StartHub 및 전투 HUD에 반영한다.
- **변경** 좌측 상단 자원과 하단 회피·스킬·대검을 새 네이티브 위젯에 표시한다. 승인 PNG에서 추출한 RGBA 아틀라스를 사용하고 실시간 입력, 자원, 잠금을 연결한다.
- **검증** UE 5.8 Editor 빌드와 최신 통합 베이스 빌드를 통과했다. 실제 StartHub 2종 해상도 56개 검사·실패 0, 고정 요소 좌표 오차 0px와 불투명 픽셀 56,956개 일치를 확인했다. 기존 UE Python 초기화 오류 58줄과 HUD 검증 구간 오류 0줄을 구분해 기록했다.
- **상태** 구현과 런타임 검증 완료. [설계](../Feature/architect/2026-11-21_combat-hud-a.md) · [검수와 캡처](../Feature/doc/2026-11-21_combat-hud-a.md)

### Sprint#14-4 — 인벤토리 시안 실제 적용

- 승인된 고딕 프레임과 10×10 격자, 아이템 점유 영역과 투구 비교 카드, 골드·무게를 실제 `I` 창에 반영했다.
- 캐릭터 교체 시 선택된 캐릭터의 가방과 장비창을 다시 연결한다. 드래그 위치 표시와 우클릭 장착·해제·소비가 실제 게임에서 동작한다.
- UE 5.8 Editor 빌드, 위젯 재컴파일, 1080p·720p 각 79개, 저장 복원 8개를 통과했다. 두 해상도의 가로·세로 격자선을 PNG에서 확인했고 세 게임 로그의 오류는 0건이다.
- 구현 및 검증 완료. [기획](../Feature/architect/2026-11-20_inventory-concept-implementation.md) · [결과](../Feature/doc/2026-11-20_inventory-concept-implementation.md)

### Sprint#10-2 — 캐릭터 생성 피부색 지정

- 생성 화면에서 견본 8개와 색상·채도·명도, 6자리 HEX로 피부색을 선택하고 종족 기본값으로 돌아갈 수 있다.
- 외형 6종에서 얼굴·몸·1인칭 팔에 같은 색을 적용한다. 기존 피부 인덱스와 얼굴·문신 옵션을 읽고, 사용자가 지정한 색을 프로필에 저장한다.
- 에디터 빌드와 재질 재조회, 실제 게임 341개 검사 두 차례를 통과했다. 이전 QA 저장 두 프로필을 읽었고, 최종 게임 로그 오류는 0건이다.
- 작업 브랜치와 맵 마감 이후 통합본의 검증을 완료했다. 관련 문서는 Feature/architect/2026-11-19_character-skin-color.md와 Feature/doc/2026-11-19_character-skin-color.md에 있다.

### Sprint#10-1 — 캐릭터 머리 상하 회전과 이동 자세

- 목표는 카메라를 위아래로 움직여도 캐릭터의 머리와 목이 따라 기울지 않도록 하는 것이다.
- 머리·목은 수평 시선과 클립의 자세를 따른다. 맨손 대기의 과한 팔 움직임을 줄이고, Blender에서 대기·걷기·달리기 클립을 비교해 3인칭 이동 자세를 보정했다.
- UE 5.8 빌드와 StartHub 3인칭 검사 1,007개를 통과했다. ±65° 입력에서 머리·목·손의 회전 차이는 측정 정밀도에서 0°였다. 마우스 전투 입력 13개를 통과했고 게임 로그 오류는 0건이다.
- 피부색 기능과 병합한 뒤 통합 빌드와 StartHub 검사 1,005개를 통과했다. [설계](../Feature/architect/2026-11-19_head-pitch-animation.md) · [결과](../Feature/doc/2026-11-19_head-pitch-animation.md)

### Sprint#9-36 — 맵 1차 마감과 배포 브랜치 통합

- `StartHub`와 `OpenRunMap` 복구, 캐릭터 선택창과 정리 커밋을 기존 배포 이력에 합쳤다.
- UE 5.8 Editor 빌드 성공. 포탈 17항목, 동굴 48시야, 승리와 귀환 77항목, 저장 재로드 17항목, 캐릭터 선택 238항목을 통과했다.
- 맵 복구 커밋에서 진행한 연속 플레이는 동굴 이동 중 카메라 4,457개 표본에서 가림 0건을 기록했다. 보스전은 시간 초과로 귀환했다.
- 통합 QA 완료. `develop`, `test`, `main`에 같은 검증본을 배포한다. [계획](../Feature/architect/2026-11-19_map-phase-one-publication.md) · [마감 결과](../Feature/doc/2026-11-19_map-phase-one-publication.md)

### Sprint#9-32 — OpenRunMap의 지형·동굴 복구

- 목표는 기존 `OpenRunMap`에 보관 지형과 지하 훈련고·작업함을 반영하고 지상 이동과 런을 다시 확인하는 것이다.
- Landscape 편집 레이어로 높이 177,014개를 복구했다. 중앙로와 마을 세 곳, 보스 바닥 연결을 정리하고 두 동굴과 도로·마을 데칼 39개를 같은 World Partition에 저장했다.
- UE 5.8 Editor 빌드 성공. 재로드 지형 45지점, 동굴 카메라 48시야, 시작 마을 포탈 17항목, 승리와 귀환 77항목, 저장 재로드 17항목을 통과했다.
- 상태는 QA 완료이며 맵 제작 작업 브랜치에 적용한다. 테스트 전투는 자동화가 위치를 준비했다.
- [설계](../Feature/architect/2026-11-19_openrun-recovered-terrain-caves.md) · [개발 결과](../Feature/doc/2026-11-19_openrun-recovered-terrain-caves.md)

### Sprint#9-35 — 캐릭터 선택창 외형 호환성 복구

- 목표는 캐릭터의 회색 재질과 구형 Humanoid 참조 오류를 제거하는 것이다.
- 몸체와 1인칭 팔에 종족별 재질을 적용했고, 드워프 얼굴과 디버그 눈·장식 슬롯을 수정했다. 구형 참조가 있던 스켈레톤 6개는 원본 복구본을 남기고 기존 경로에서 다시 저장했다. 셰이더 준비 후 선택창 프리뷰가 갱신된다.
- 새 에디터에서 6종 외형·리타깃 연결과 문신 8개를 확인했다. 빌드와 실제 게임 검사 238개를 통과했고, 게임 로그 오류 0건과 기존 세이브 499개 보존을 확인했다.
- 작업 브랜치에 게시한다. 관련 기록은 Feature/architect/2026-11-19_character-lobby-asset-compatibility.md, Feature/doc/2026-11-19_character-lobby-asset-compatibility.md, DevLog/20261119.txt에 있다.

### Sprint#9-35 — 캐릭터 선택창 시안 실제 적용

- 목표: 승인된 이미지 시안을 실제 선택창 배치·선택 상태·입력으로 적용한다.
- 변경: 기존 WBP를 비파괴 갱신하는 도구, 남색·금색 테마·선택 배지·빈 슬롯 버튼, 창 좌표에 맞는 장식, 화면비/대기 포즈 프리뷰 보완, 전용 QA와 대표 사진.
- 검증: 최종 Editor 빌드6작업/27.93초 성공, 자산4검사/Editor 오류0, 실제1920×1080 UI238검사/실패0, 별도 OS 마우스 선택·접속 확인. 기존493세이브와 보호6입력 보존.
- 상태: UI 구현·시각/기능 검증 완료. 기존 vendor Humanoid 참조5오류로 전체 런타임 로그 QA는 보류한다. 작업 브랜치 체크포인트만 게시하며 맵 제작 완료 전 develop/test/main 승격은 보류한다.
- 관련: Feature/architect/2026-11-19_character-select-implementation.md, Feature/doc/2026-11-19_character-select-implementation.md, DevLog/20261119_CharacterSelect/character-selection-final.png, DevLog/20261119.txt.

### Sprint#9-34 — 캐릭터 선택창 ImageGen 시안

- 목표: 기존 3슬롯 프로필 선택 구조와 성채 테라스 방향을 유지한 수정 시안을 제작한다.
- 변경: 내장 이미지 생성 시안 1장·최종 프롬프트·짝 기획/결과 문서·최종 검수 기록. 선택 PNG 하나만 ignore 예외를 등록했다.
- 검증: PNG 디코딩·1672×941 치수·SHA256 확인, 별도 검토자를 포함한 한글 문구/3슬롯/전신/선택 강조 검수 통과. 런타임 변경이 없어 UE 빌드/PIE는 실행하지 않았다.
- 상태: 디자인 시안 완료, 실제 선택창 적용 전. 작업 브랜치 `Sprint#9-34-character-select-concept`에 한정 게시하며 맵 제작 게이트에 따라 develop/test/main 승격은 보류한다.
- 관련: Feature/architect/2026-11-19_character-select-concept.md, Feature/doc/2026-11-19_character-select-concept.md, SourceArt/UI/CharacterLobby/2026-11-19/character-select-v1.prompt.txt, DevLog/20261119.txt.

### Sprint#9-32 — 정상 에디터 복구·Landscape 외곽 선택 완료

- 결과:정상후보맵·공식MCP·PIE정지·dirty0확인.256지형/충돌·16proxy·1layer,유지179488표본의height/paint/layer기준확보.
- 보존:후보1423파일/18272864B물리백업·SHA확인,보호3947파일/44924472B해시기록.기존맵·Source/Config변경0.
- 추가검사:전역reflection미지원1156건실패는보존.후보18package전체7종referencer외부0·1361actor/1986component의PCG/식생/spline/water/brush0확인.독립리뷰에따른범위검증이며전역안전인증아님.
- 상태:정식Manage/Select로외곽211선택/유지45제외·dirty불변·실제PNG저장.앱내삭제직전확인규칙에따라사용자확인대기.실제삭제/저장/UndoRedo/재로드/통행검증미완료.
- 근거:Feature/doc/2026-11-16_editor-startup-recovery.md;Feature/doc/evidence/landscape-production-20261116/scoped-crop-review.md;live-before/native-selection.png.

### Sprint#9-32 — 에디터 실행 실패 점검·Live Coding 충돌 복구

- 목표: 사용자 VS 활성화 작업을 보존하며 정상 에디터를 열고 Landscape 제작을 재개한다.
- 원인: 최신게임은-game/창닫힘종료요청, 잔류PID21224가LiveCoding잠금유지. VS Editor빌드는그잠금으로실패했다. 프로젝트파일생성은성공이며Python오류를최초종료원인으로단정하지않았다.
- 조치: 호스트직접읽기·원문로그보존·정확한잔여게임만정리. 사용자VS종료0. 정규Editor빌드110actions/169.49초/exit0. 정상에디터실행기추가후-game없이후보맵PID11088실행.
- 상태: 에디터로딩/live확인·후보물리백업진행. 실제Landscape삭제·저장·통행검증은아직0. 일반sandbox의프로세스없음관찰은호스트전체부재로해석하지않도록정정했다.
- 문서: Feature/doc/2026-11-16_editor-startup-recovery.md; Feature/doc/evidence/landscape-production-20261116/editor-recovery/.

### Sprint#9-32 — 월드 구성 1차 기획과 Landscape 제작 세부안

- 목표: 기존 구성을 정확한 지형 치수·좌표·동선·검증 기준으로 정리해 먼저 공개하고 Landscape 구현으로 이어간다.
- 변경: 상세 기획·좌표 보충 JSON·제작 배치도·재생성 도구. 기존21노드/29도로·3큰마을/5초소·중앙300m를 유지하는9×5컴포넌트/1,012.5×562.5m안을 기록했다. 두 기존카탈로그와 Source/Config/Content는 이번 작업에서 변경하지 않았다.
- 검증: 설계 기하10항목 통과. 모든 대지·도로어깨가 경계 안이며 최소여유52.5m/81.5m. 독립리뷰에서 무건 이름·3회직행/성장5회·crop불변과경사보정분리·실패시ready저장가드를 보정했다. GeneratedChanges와 범위diff검사 통과.
- 상태: 1차 기획 완료, 2차 live편집 대기. Editor없음/MCP8001연결거절을 확인하고 사용자에게 에디터실행을 요청했다. 실제crop/저장/빌드/게임/commit/push0, 기존WIP605항목보존. 과거15.095°경사와실제통행·시간검증은미해결이다.
- 문서: Feature/architect/2026-11-16_landscape-production-plan.md; Feature/doc/2026-11-16_landscape-production-plan.md; Docs/References/WorldMap/landscape-production-20261116.json.

### Sprint#9-32 — Hub 입구 포함 Windows 내부 아티팩트 완료

- 결과: SDK0·정규 BCR0/332.93초·사후0, 맵6+DA·2252exports/158셀·새HubActor6/기존포털3 identity 확인. 새 EXE/PDB의 row48/Info32/Version16·consumer48를 새 원문으로 제한 검토.
- 산출물: tmp/InternalWindows20261114HubEntryA_UNVERIFIED/Windows,50파일/1,957,709,382B, SHA B9C2ED1B46F045C2CA80A2D6844EED5C1AA023B1AE9F47F2010311EB8A80A60B.
- 보존/검증: 입력10472·Source/Config368·Editor6·저장2루트·E5/B/D두사본·C6쌍·기록14핀 동일. AST7/7·PS0·diff0·Generated0. 기존 WIP/경고 보존, commit/push0.
- 미완: 새패키지실행/여행/시계/NPC/F/저장종료·재시작/시각/전체런. 별도승인차단 유지. 새사용자지시에따라 실제맵축소/배치로이어가며 이 아티팩트는축소전1800m버전으로보존.
- 근거: Feature/doc/2026-11-14_remaining-development-and-hub-windows.md; Feature/doc/evidence/hub-entry-windows20261114A/final-result.json.

### Sprint#9-32 — 개발 재연결·Hub 입구 포함 Windows 내부 패킷 준비

- 목표: Source의 새 Hub 포털1+표시Actor5를 별도 Windows 출력에 반영하고 새 게임 실행 없이 정적 콘텐츠를 검증한다.
- 변경: 남은 개발 여섯 묶음과 이번 ULW 계획, 새 입력/보존 확인 및 실제 owner-session 도구. Source/Config/Content 변경은 없다.
- 검증/상태: 재연결 파일 읽기 및 UE/빌드/게임0 확인. 전체 지문 재대조 진행 중이며 새 Build/Cook/Archive는 아직 미완료다. 기존 D 메뉴 실행과 새 패키지 실행을 구분한다.
- 문서: Feature/architect/2026-11-14_hub-entry-windows-artifact.md; Feature/doc/2026-11-14_remaining-development-and-hub-windows.md.

이 문서는 Rogue10m의 Sprint별 주요 변경 사항과 검증 결과를 요약한다.
세부 설계와 구현 결과는 Feature 문서, 일자별 작업 과정은 DevLog를 참조한다.

## 기록 규칙

각 작업은 다음 형식으로 누적한다.

- 작업 식별자: Sprint#스프린트번호-작업번호
- 브랜치: Sprint#스프린트번호-작업번호-간단한-영문-설명
- Sprint 번호: `test`가 `main`에 성공적으로 병합되어 현재 Sprint가 종료된 시점에만 증가
- 작업 번호: 같은 Sprint 안에서 독립 작업이 시작될 때 증가하며, 다음 Sprint 시작 시 1로 초기화
- 상태: 계획 / 개발 중 / 빌드 완료 / QA 완료 / main 반영
- 목표: 작업이 해결하려는 문제
- 주요 변경: 플레이어가 체감하거나 구조적으로 중요한 변경
- 검증: 빌드, 테스트 또는 확인한 체크리스트
- 관련 문서: Feature/architect 및 Feature/doc 문서

작은 수정이 같은 Sprint 작업 범위에 포함되면 기존 항목에 추가한다.
별도 기능이나 독립적인 롤백 경계를 가진 작업은 다음 작업번호를 사용한다.

---

## Sprint 0 — 플레이어블 프로토타입 기반

### 주요 목표

- 10분 로그라이크 전투의 기본 플레이 루프 구성
- 전투, 장비, 스킬, HUD, 몬스터와 런 진입 구조 확보
- UE 5.8 프로젝트 Harness와 문서 흐름 도입

### 주요 변경

- 시작 허브와 전투 맵 이동 흐름
- 기본 몬스터와 플레이어 전투
- GAS 기반 Attribute 및 스킬 구조
- 공격 Data Asset과 콤보/쿨다운
- 장비, 인벤토리 초기 구조
- MainHUD와 Widget Part 브리지
- 다단 히트, 피해 숫자, 플레이어 피격 피드백
- Lazy Codex Harness와 Feature/DevLog 문서 체계

### 상태

- main 반영

### 관련 문서

- Feature/doc/2026-06-23_prototype-run-flow.md
- Feature/doc/2026-06-29_equipment-damage-feedback.md
- Feature/doc/2026-07-02_gas-skill-system.md
- Feature/doc/2026-07-11_multihit-damageable-targets.md
- Feature/doc/2026-07-12_attack-patterns-and-hit-modes.md
- Feature/doc/2026-07-12_hud-refresh-optimization.md

---

## Sprint 1 — 조작, 성장, 인벤토리, 몬스터 데이터 확장

### Sprint#1-1 — sprint-postprocess-stamina

- 상태: 빌드 완료, 통합 대기
- 목표: 달리기와 회피 조작 및 플레이어 전투 피드백 개선
- 주요 변경:
  - Shift 달리기와 스테미나 소비
  - Post Process 기반 달리기 효과
  - 방향성 Dodge와 입력 잠금
  - Space 점프, E 구르기 입력 재배치
  - 공중 구르기 차단
  - 피격 효과 지속 시간 0.5초 설정
- 검증:
  - Rogue10mEditor 빌드
  - 이동 상태와 입력 충돌 코드 검토
- 관련 문서:
  - Feature/doc/2026-07-12_sprint-postprocess-stamina.md
  - Feature/doc/2026-07-12_directional-dodge.md
  - Feature/doc/2026-07-13_jump-roll-input-remap.md

### Sprint#1-2 — progression-skill-slots

- 상태: 빌드 완료, 통합 대기
- 목표: 레벨/경험치 표시와 스킬 슬롯 상태 표현 개선
- 주요 변경:
  - 경험치 Hover 상세 정보
  - 현재 경험치 / 필요 경험치 소수점 3자리 표시
  - 정수 레벨 표시
  - WBP_Progression을 LevelExperience UI로 정리
  - 좌/우 공격, 차징, Space 스킬 슬롯 배치
  - 해금 및 콤보 진행 상태에 따른 슬롯 비활성화
- 검증:
  - Rogue10mEditor 빌드
  - Widget Blueprint 변수명 확인
- 관련 문서:
  - Feature/doc/2026-07-12_experience-hover-level.md
  - Feature/doc/2026-07-12_level-experience-skill-slots.md

### Sprint#1-3 — grid-inventory-windows

- 상태: 빌드 완료, 통합 대기
- 목표: 확장 가능한 NxM 인벤토리와 메뉴 창 기반 마련
- 주요 변경:
  - 기본 10x10 인벤토리
  - NxM 아이템 배치 및 충돌 검사
  - 가방 아이템 기반 추가 컨테이너
  - 아이템 Data Asset과 월드 드롭 Mesh
  - 인벤토리, 장비, 스킬트리 UserWidget 부모 클래스
  - 메뉴 창 토글과 입력 모드 처리
- 검증:
  - Rogue10mEditor 빌드
  - 그리드 배치/이동/제거 API 검토
- 관련 문서:
  - Feature/doc/2026-07-13_grid-inventory-and-windows.md
  - Docs/GridInventoryAndMenuWindowsGuide.md

### Sprint#1-4 — monster-data-regen-ui-cleanup

- 상태: 빌드 완료, 통합 대기
- 목표: 몬스터 데이터화, 공용 자원 회복, HUD 단순화
- 주요 변경:
  - Monster Data Asset 타입 추가
  - 이름, 레벨, 경험치, Vitals, Mesh, AnimBP, AI/공격 설정 데이터화
  - 플레이어/몬스터 공용 초당 자원 회복 컴포넌트
  - 기본 체력/스테미나/마나 회복량 각각 초당 0.1
  - MonsterInfo를 LV N : 이름 + HP 형태로 변경
  - MainWidget MiniMap 임시 비활성화
  - 아이템 획득 피드를 이미지 | 수량으로 변경
  - 현재 프로젝트 구조 Mermaid 문서 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges 통과
  - git diff --check 통과
- 관련 문서:
  - Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md
  - Docs/CurrentProjectArchitecture.md

---

## 다음 Sprint 작성 템플릿

### Sprint#N-M — short-description

- 상태: 계획
- 목표:
- 주요 변경:
  -
- 검증:
  -
- 관련 문서:
  - Feature/architect/YYYY-MM-DD_feature-name.md
  - Feature/doc/YYYY-MM-DD_feature-name.md
### Sprint#1-5 — skill-tree-drag-drop

- 상태: 빌드 완료, 에디터 에셋 설정 대기
- 목표: 하드코딩 스킬 경로 제거와 스킬트리 기반 Loadout 구성
- 주요 변경:
  - 무기별 Weapon Skill Profile Data Asset
  - 기본 1m Dodge Skill Data Asset
  - 무기 변경 시 기본 회피 자동 바인딩
  - 스킬트리 해금 스킬 Drag 시작
  - 좌/우 클릭 및 차징 슬롯 Drop 장착
  - E 회피 슬롯 자동 표시 및 Drop 차단
  - 기존 숫자키 소비 아이템 슬롯 유지
- 검증:
  - UnrealHeaderTool 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - Tick 추가 없음
- 관련 문서:
  - Feature/architect/2026-07-13_skill-tree-drag-drop-loadout.md
  - Feature/doc/2026-07-13_skill-tree-drag-drop-loadout.md
  - Docs/SkillTreeLoadoutGuide.md
### Sprint#1-6 - equipment-p-key

- 상태: 구현 및 Editor 빌드 완료
- 목표: 장비창 입력을 P로 변경하고 메뉴 Widget 및 회피 Data Asset 설정 위치 명확화
- 주요 변경:
  - 장비창 토글 키 B에서 P로 변경
  - 메뉴 창 가이드 및 입력 의사 코드 갱신
  - InventoryWindowWidgetClass 및 SkillTreeWindowWidgetClass 설정 절차 정리
  - 무기별 Weapon Skill Profile의 DefaultDodgeSkill 참조 경로 정리
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges Harness 경로 검사 통과
- 관련 문서:
  - Feature/architect/2026-07-14_equipment-p-key-and-editor-setup.md
  - Feature/doc/2026-07-14_equipment-p-key-and-editor-setup.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
  - Docs/SkillTreeLoadoutGuide.md
### Sprint#1-7 - character-assets-menu-widgets

- 상태: C++ 빌드 완료, Editor 에셋 생성 대기
- 목표: 캐릭터 기본 전투 Data Asset과 필수 메뉴 UserWidget 에셋 구성
- 주요 변경:
  - 맨손 기본 회피 Data Asset 생성 정의(100cm / 0.16초 / 쿨타임 0.45초 / 스테미나 10)
  - 기존 맨손 공격 6종을 묶는 Weapon Skill Profile 생성 및 Combat Component 연결 자동화
  - 인벤토리 10x10 그리드/용량 표시 골격
  - 장비 슬롯 컨테이너 골격
  - 스킬 트리 엔트리/목록/Drag & Drop 안내 골격
  - 메뉴 WBP 생성과 PlayerController 클래스 연결 자동화
- 검증:
  - Unreal Python `py_compile` 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - 열린 Editor Remote Execution 활성화 후 `.uasset` 생성 검증 예정
- 관련 문서:
  - Feature/architect/2026-07-14_character-assets-and-menu-widgets.md
  - Feature/doc/2026-07-14_character-assets-and-menu-widgets.md
  - Scripts/Editor/CreateCharacterAssetsAndMenuWidgets.py
#### Sprint#1-7 추가 문서 - AI 개발 세팅 참조

- Unreal Python, Remote Execution, AI 플러그인 설정 정리
- Editor 스크립트 실행 및 외부 연결 예시 추가
- Hot Reload, 에셋 저장, 동시 Editor 실행 관련 안전 규칙 추가
- 관련 문서: `Ai 개발 세팅 참조/README.md`
### Sprint#1-8 - plugin-cpp-cleanup

- 상태: 구현 및 Editor 빌드 완료
- 목표: 사용하지 않는 Editor 플러그인 자동 활성화와 C++ 설정·모듈 의존성 정리
- 주요 변경:
  - ModelingToolsEditorMode, StateTree, GameplayStateTree 프로젝트 자동 활성화 제거
  - StateTreeModule, GameplayStateTreeModule 의존성 제거
  - Main HUD 비활성 미니맵 바인딩·클래스 설정·갱신 함수 제거
  - Build.cs 빈 설정과 샘플 주석 제거
  - Python __pycache__ 및 bytecode Git 무시 규칙 추가
  - 사용 중인 GAS, AI, UMG/Slate, AI Assistant/MCP 플러그인 유지
- 검증:
  - Unreal 프로젝트 파일 생성 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - CheckGeneratedChanges 및 git diff --check 통과
- 관련 문서:
  - Feature/architect/2026-07-14_plugin-cpp-cleanup.md
  - Feature/doc/2026-07-14_plugin-cpp-cleanup.md
### Sprint#1-9 - menu-window-widget-setup

- 상태: 구현, Editor 에셋 설정 및 빌드 완료
- 목표: InventoryWindowWidget과 SkillTreeWindowWidget 누락 경고 해결 및 실제 WBP 연결
- 주요 변경:
  - WBP_InventoryWindow, WBP_EquipmentWindow, WBP_SkillTreeEntry, WBP_SkillTreeWindow 검증·컴파일
  - BP_FirstPersonPlayerController에 세 Menu Window Widget Class 지정
  - SkillTreeWindow에 WBP_SkillTreeEntry 지정
  - 명시 Class 누락 시 기본 WBP를 불러오는 Soft Class fallback 추가
  - 메뉴 WBP 생성·복구용 Editor Python 스크립트 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor CDO ClassProperty 재조회 성공
  - 저장 후 dirty package 0건
  - CheckGeneratedChanges 및 git diff --check 통과
- 관련 문서:
  - Feature/architect/2026-07-14_menu-window-widget-setup.md
  - Feature/doc/2026-07-14_menu-window-widget-setup.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
### Sprint#1-10 - content-asset-naming

- 상태: 구현, Editor 에셋 참조 정리 및 빌드 완료
- 목표: Widget, DataAsset, FirstPerson 콘텐츠의 UE 5.8 명명 규칙 위반 정리
- 주요 변경:
  - `UW_Rogue10mMainWidget` → `WBP_Rogue10mMainHUD`
  - `ABP_FP_Copy` → `ABP_FirstPerson`
  - `CtrlRig_FPWarp` → `Rig_FirstPersonWarp`
  - Main HUD C++ Soft Class 및 유지보수 스크립트·가이드 경로 갱신
  - 재실행 가능한 Editor Python 이름 변경·구 소스 정리 절차 추가
  - DataAsset의 `DA_` 접두사와 World Partition `Lvl_FirstPerson`은 검토 후 유지
- 검증:
  - Widget 21개, DataAsset 10개, FirstPerson 7개 전수 감사 결과 위반 0건
  - 구 경로 3개 부재, 신규 경로 3개 존재
  - Character/Anim Blueprint/Control Rig 및 PlayerController/Main HUD 참조 확인
  - Object Redirector 0건, dirty package 0건
  - Rogue10mEditor Win64 Development 빌드 성공
- 관련 문서:
  - Feature/architect/2026-07-14_content-asset-naming.md
  - Feature/doc/2026-07-14_content-asset-naming.md
  - Docs/WidgetBlueprintHUDGuide.md
### Sprint#1-11 - stone-fist-identity

- 상태: 데이터 구성, Editor 자산 연결 및 C++ 빌드 완료
- 목표: 권 아이덴티티의 기본 전투·회피·2단 점프를 데이터 주도 구조로 설계하고 플레이어 기본 프로필로 연결
- 주요 변경:
  - `T_Identity_StoneFist` 대표 이미지와 권 스킬 아이콘 6종 반영
  - 잽 → 스트레이트 2타 콤보, 우클릭 차징 권압, 점프 내려찍기 Data Asset 생성
  - 일반 우클릭과 좌클릭 차징 미바인딩
  - 권보 Dodge Data Asset과 `Knuckle` Skill Profile 생성
  - CharacterData 기본 무기 타입 및 Profile 최대 점프 횟수 추가
  - BP_FirstPersonCharacter CombatComponent에 CharacterData/권 프로필 연결
  - Data Asset과 GAS의 책임 경계 및 후속 전용 Ability 마이그레이션 정의
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor Remote Execution 자산 로드·속성·바인딩·콤보 참조 확인
  - 공중 회피 차단 및 `MaxJumpCount=2` 코드 경로 확인
- 관련 문서:
  - Feature/architect/2026-07-14_stone-fist-identity.md
  - Feature/doc/2026-07-14_stone-fist-identity.md
### Sprint#1-12 - menu-widget-layout

- 상태: 구현, Editor 자산 재컴파일 및 빌드 완료
- 목표: Parts UI 스타일을 기준으로 Inventory, Equipment, SkillTree 메뉴를 실제 확장 가능한 레이아웃으로 재구성
- 주요 변경:
  - 공통 Canvas/Frame/Content 메뉴 계층과 다크 패널/금색 강조 스타일
  - Inventory 10x10 UniformGrid, NxM ItemCanvas, BagTab, Capacity 영역
  - Equipment 캐릭터 프리뷰, 7개 장비 슬롯, 능력치 영역
  - SkillTreeEntry 아이콘/이름/설명/잠금 상태 카드
  - SkillTree ScrollBox/WrapBox, 필터, 스킬 포인트 영역
  - WBP 4종과 PlayerController/SkillTreeEntry 클래스 참조 재컴파일·저장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Editor Remote Execution 클래스 재로드와 참조 확인
  - Tick 추가 없음
- 관련 문서:
  - Feature/architect/2026-07-14_menu-widget-layout.md
  - Feature/doc/2026-07-14_menu-widget-layout.md

### Sprint#1-13 - menu-designer-layout

- 상태: 구현, Editor 자산 직접 배치 및 빌드 완료
- 목표: 메뉴 UI를 C++ Slate fallback이 아닌 Widget Blueprint Designer 소유 구조로 전환
- 주요 변경:
  - Inventory Canvas/Frame/Grid/NxM ItemCanvas/하단 돈·무게 직접 배치
  - GridSize X×Y에 맞춘 WBP_InventoryCell UserWidget 자동 생성
  - BagTab/Capacity/Hint 제거 및 Item Data Asset UnitWeight 기반 총 무게 계산
  - Equipment 프리뷰/장비 슬롯 7종/능력치 영역 직접 배치
  - SkillTreeEntry 아이콘/이름/설명/잠금 상태 UserWidget 구성
  - SkillTreeWindow 필터/ScrollBox/WrapBox/스킬 포인트 영역 직접 배치
  - Overlay 제거, Root Canvas 형제 배치와 ZOrder 사용
  - 네이티브 `RebuildWidget()` 제거 및 필수 `BindWidget` 계약 적용
  - Editor Python 필수 이름·Overlay 0개 검증 추가
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 메뉴 WBP 4종과 WBP_InventoryCell Compile/Save 성공
  - InventoryCellWidgetClass 연결 및 제거 대상 위젯 0개 확인
  - 필수 위젯 누락 0개, Overlay 0개
- 관련 문서:
  - Feature/architect/2026-07-15_menu-designer-layout.md
  - Feature/doc/2026-07-15_menu-designer-layout.md
  - Docs/GridInventoryAndMenuWindowsGuide.md

### Sprint#1-13 - NxM Inventory Cell/Item/BagTab 상호작용

- 목표: 인벤토리 좌표 셀, NxM 아이템, 가방 탭을 분리된 UserWidget으로 구현하고 회전·충돌 프리뷰를 제공한다.
- 주요 변경: 회전 상태 저장, 회전 footprint 기반 경계/AABB 충돌, Canvas 좌표 스냅, 잡은 셀 오프셋, R키 90도 회전, 녹색/적색 프리뷰, 가방 탭 전환.
- 자산: `WBP_InventoryCell`, `WBP_InventoryItem`, `WBP_BagTab`, `WBP_InventoryWindow`.
- 검증: Rogue10mEditor Development 빌드 성공, 열린 Editor WBP 컴파일/저장 및 클래스 할당 확인.
- 상태: 구현 및 로컬 검증 완료. PIE에서 아이템 Data Asset별 NxM 이동/회전 체감 QA 필요.
- 관련 문서: `Feature/architect/2026-07-15_menu-designer-layout.md`, `Feature/doc/2026-07-15_menu-designer-layout.md`, `Docs/GridInventoryAndMenuWindowsGuide.md`.
### Sprint#1-13 - menu-widget-contract-and-folders

- 상태: 구현 및 로컬 검증 완료
- 목표: BagTab 바인딩 오류와 UserInterfaceSettings EditCondition 오류를 해소하고 메뉴 자산 구조 및 Inventory Cell 시인성을 정리한다.
- 주요 변경:
  - `UI_BagSizeText` C++ 필수 바인딩 및 BagTab GridSize 인자 제거
  - `WBP_InventoryCell`에 0.5 패딩 기반 어두운 외곽선 적용
  - 메뉴 자산을 `Inventory`, `Equipment`, `SkillTree` 기능별 폴더로 이동
  - PlayerController SoftClassPath, 내부 WidgetClass, Editor 자동화 경로 갱신
  - 이동 후 누락된 `SkillTreeEntryWidgetClass` 재지정
  - UserInterfaceSettings Font DPI 기본값 명시 및 전체 Editor 모듈 재빌드
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - UE5.8 commandlet 메뉴 WBP 7종 컴파일 및 내부 클래스 참조 검사 성공
  - `UI_BagSizeText`, `LogEditCondition`, `bUseCustomFontDPI` 오류 재발 없음
- 관련 문서:
  - `Feature/architect/2026-07-15_menu-designer-layout.md`
  - `Feature/doc/2026-07-15_menu-designer-layout.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`
### Sprint#1-13 - inventory-grid-visual-balance

- 상태: 구현 및 로컬 WBP 검증 완료, PIE 시각 재확인 필요
- 목표: Inventory Cell과 창 내부 구획의 시각적 균형을 마비노기/Diablo II식 NxM Grid 기준으로 개선한다.
- 주요 변경:
  - 44×44 Cell에 1px 다크 경계와 차콜 Fill 적용
  - 448×448 `UI_InventoryGridFrame` 추가
  - Grid/ItemCanvas 중심 `(0, 16)` 통일
  - BagTab 136×36, 14pt 및 어두운 배경 적용
  - Title/Tab/Grid/BottomInfo 중첩 제거와 대칭 여백 확보
- 검증:
  - UE5.8 메뉴 WBP 7종 commandlet 컴파일 성공
  - `UI_InventoryGridFrame` 필수 Designer 위젯 확인
  - Inventory/SkillTree 내부 WidgetClass 참조 확인
  - Python 오류 및 Overlay 0개
- 관련 문서:
  - `Feature/architect/2026-07-15_menu-designer-layout.md`
  - `Feature/doc/2026-07-15_menu-designer-layout.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`

### Sprint#1-13 - prototype-inventory-items

- 상태: 구현, Editor Data Asset 생성 및 C++ 빌드 검증 완료
- 목표: NxM 인벤토리 배치와 회전 QA에 사용할 1x1, 2x3, 4x3 프로토타입 아이템 준비
- 주요 변경:
  - 프로토타입 Item Data Asset 3종 생성
  - 아이콘 미지정 시 사용하는 InventoryTint 속성 추가
  - 빈 기본 인벤토리에 시작 아이템을 한 번만 자동 배치
  - bAddPrototypeStartingItems, PrototypeStartingItems로 에디터 조정 가능
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - Unreal Python 에셋 생성 및 InventorySize 재검증 성공
- 관련 문서:
  - Feature/architect/2026-07-15_prototype-inventory-items.md
  - Feature/doc/2026-07-15_prototype-inventory-items.md

### Sprint#1-14 - project-warning-cleanup

- 상태: 프로젝트 측 경고 수정 및 독립 commandlet 검증 완료
- 목표: 삭제된 몬스터 컴포넌트 직렬화 경고와 GameplayCue 전체 콘텐츠 검색 경고를 제거하고 엔진 자체 경고를 분리한다.
- 주요 변경:
  - `BP_BaseMonster`와 배치 External Actor에서 삭제된 `Rogue10mVitalsComponent` 참조 제거
  - 현재 GAS Ability System 및 Vital Regeneration 컴포넌트 유지 확인
  - `GameplayCueNotifyPaths=/Game/GameplayCues` 설정 추가
  - `ValidateWarningFixes.py` 회귀 검증 추가
  - `r.MotionVectorSimulation`은 UE 5.8 엔진 측 Render Thread Safe 플래그 문제로 판정하고 프로젝트 렌더 설정은 유지
- 검증:
  - 구형 Vitals 문자열 참조 0건
  - 새 UE5.8 commandlet 검증 성공, 0 errors / 0 warnings
- 관련 문서:
  - `Feature/architect/2026-07-16_project-warning-cleanup.md`
  - `Feature/doc/2026-07-16_project-warning-cleanup.md`
  - `DevLog/20260716.txt`

### Sprint#1-14 - inventory-item-footprint

- 상태: 구현, C++ 빌드, WBP 재구성 및 독립 commandlet 검증 완료. PIE 시각 QA 필요.
- 목표: 단일 10×10 인벤토리에서 1×1·2×3·4×3 아이템의 셀 점유 크기와 아이콘 원본 종횡비를 정확히 유지한다.
- 주요 변경:
  - Grid Entry와 DragDrop Operation의 회전 상태 및 R키 회전 처리 제거
  - Item Data Asset의 원본 `InventorySize`만 배치·충돌 footprint로 사용
  - Designer SizeBox 기본값을 44×44로 명시하고 Canvas 슬롯과 런타임 `UI_InventoryItemSize`를 `Width×44`, `Height×44`로 동기화
  - Preview Border를 전체 footprint에 Fill하고 아이콘은 4px inset `ScaleBox(ScaleToFit)`로 분리
  - `SetBrushFromTexture(Icon, true)`로 텍스처 실제 크기를 반영해 세로형·가로형 아이콘 종횡비 보존
  - 아이콘 있는 아이템은 크기 문자열을 숨기고 비장비 스택 수량만 우측 상단 표시
  - 아이콘 없는 프로토타입은 `InventoryTint`와 `W×H` fallback 표시
  - Inventory Window의 BagTab 컨테이너와 클래스 참조를 제거하고 컨테이너 0으로 고정
  - 제거된 탭 공간에 맞춰 Grid Frame/Grid/ItemCanvas 중심을 `(0,-12)`로 통일
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - `WBP_InventoryItem`, `WBP_InventoryWindow` 재구성·컴파일·저장 성공
  - 새 commandlet에서 메뉴 WBP 7종 컴파일 및 클래스 참조 검증 성공
  - 최종 `WBP_InventoryItem`에 IconScale 포함, `WBP_InventoryWindow`에 BagTabContainer 없음
  - 최신 독립 검증 로그의 Blueprint/Python 오류 0건
  - 런타임 회전 코드 검색 0건
- 관련 문서:
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`
  - `Docs/GridInventoryAndMenuWindowsGuide.md`

### Sprint#1-14 - inventory-item-icon-scale

- 상태: 구현 및 C++ 빌드 검증 완료
- 목표: Item Data Asset마다 인벤토리 아이콘의 표시 크기를 원본 비율을 유지한 채 조절한다.
- 주요 변경:
  - `InventoryIconScale` Data Asset 속성 추가, 기본값 `1.0`, 범위 `0.1~2.0`
  - `ScaleToFit` 이후 중앙 Pivot 기준 균일 Render Scale 적용
  - 확대 아이콘이 인접 셀을 침범하지 않도록 footprint 경계 클리핑
  - 점유 크기·배치·충돌은 기존 `InventorySize` 기준 유지
- 검증: Rogue10mEditor Win64 Development 빌드 성공
- 관련 문서:
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`

### Sprint#1-14 - starter-item-icons

- 상태: 아이콘 생성, Data Asset/Texture Import, C++ 빌드, WBP 재구성 및 독립 commandlet 검증 완료. PIE 시각 QA 필요.
- 목표: 포션과 기본 장비를 실제 이미지로 제작해 M×N 인벤토리와 장비창에서 데이터 기반으로 표시한다.
- 주요 변경:
  - 다크 판타지 스타일 스타터 아이콘 6종 제작 및 투명 배경 처리
  - 1칸당 256px 기준으로 1×1, 1×3, 2×2, 2×3 원본 캔버스 구성
  - UI Texture 6개와 Item Data Asset 6개 생성
  - Data Asset 기반 `FRogue10mItemStack` 변환과 시작 아이템 배치
  - 포션 시작 수량 5개 및 장비 5종 프로토타입 자동 장착
  - `OnEquipmentChanged` Delegate와 장비창 슬롯 아이콘 7개 추가
  - 장비 슬롯에도 `ScaleBox(ScaleToFit)`와 `InventoryIconScale` 적용
- 검증:
  - PNG 투명 모서리와 M×N 해상도 검사 성공
  - Unreal Python Import 시 Texture/Data Asset 크기·참조 검증 성공
  - Rogue10mEditor Win64 Development 빌드 성공
  - UE5.8 commandlet 메뉴 WBP 전체 컴파일 성공, 0 errors / 0 warnings
  - 열린 Editor에서 Data Asset 6개, PrototypeStartingItems 9개, 장비 아이콘 7개와 누락 0개 확인
- 관련 문서:
  - `Feature/architect/2026-07-15_starter-item-icons.md`
  - `Feature/doc/2026-07-15_starter-item-icons.md`
  - `Feature/architect/2026-07-15_inventory-item-footprint.md`
  - `Feature/doc/2026-07-15_inventory-item-footprint.md`

### Sprint#1-14 - inventory-item-tint-layer-order

- 상태: 수정 및 Editor/commandlet 검증 완료
- 목표: InventoryTint Border가 실제 아이콘과 수량을 덮지 않도록 렌더 계층을 고정한다.
- 주요 변경:
  - `UI_InventoryItemPreviewBorder` GridSlot Layer 0
  - `UI_InventoryItemIconScale` GridSlot Layer 1
  - `UI_InventoryItemQuantityText` GridSlot Layer 2
  - `ValidateMenuWidgetAssets.py`에 Layer 회귀 검사 추가
- 검증:
  - 열린 Editor에서 Layer 0/1/2 확인 및 WBP 컴파일·저장 성공
  - UE5.8 commandlet 전체 메뉴 WBP 컴파일 성공, 0 errors / 0 warnings
- 관련 문서:
  - `Feature/doc/2026-07-15_starter-item-icons.md`
  - `DevLog/20260716.txt`

### Sprint#1-14 - inventory-item-icon-runtime-paint

- 상태: 구현, C++ 빌드, WBP 자동 검증, PIE 시각 검증 완료
- 목표: Texture가 연결됐지만 Tint만 보이던 NxM 인벤토리 아이콘을 런타임에 정상 표시한다.
- 주요 변경:
  - WBP_InventoryItem에서 Inventory Icon ScaleBox를 제거하고 Tint Border의 Image 콘텐츠로 직접 배치
  - Texture 종횡비와 NxM footprint를 이용해 44px 셀 기준 Fit 크기 계산
  - 생성 전에는 적용되지 않는 SetDesiredSizeOverride() 대신 FSlateBrush::SetImageSize()로 Brush에 크기 저장
  - 아이콘이 있는 일반 상태 Tint Alpha를 최대 0.18로 제한
  - 스타터 Texture Import를 TC_DEFAULT, TEXTUREGROUP_UI, TMGS_FROM_TEXTURE_GROUP으로 통일
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - ValidateMenuWidgetAssets.py 전체 통과
  - PIE UI 포함 캡처에서 포션, 장검, 투구, 갑옷, 부츠, 반지 아이콘 정상 표시
- 관련 문서:
  - Feature/architect/2026-07-15_starter-item-icons.md
  - Feature/doc/2026-07-15_starter-item-icons.md
  - Docs/GridInventoryAndMenuWindowsGuide.md
  - DevLog/20260716.txt

# Sprint#2-1 - 장비창 캐릭터 프리뷰 및 7부위 레이아웃

- 목표: 장비창에서 현재 플레이어 전신 외형과 7부위 장착 상태를 적절한 비율로 함께 확인한다.
- 주요 변경:
  - SceneCapture 전용 프리뷰 액터, transient 512×768 Render Target, Leader Pose 기반 메시 복제, 장비창 수명주기 연결
  - 장비 데이터와 UMG 바인딩을 무기·투구·갑옷·장갑·신발·반지·목걸이 7부위로 통일
  - 캐릭터 프리뷰 중심 CanvasPanel 직접 배치와 부위별 슬롯 크기 차등 적용
  - ScaleToFit 및 최대 1.0 아이콘 배율로 Texture 종횡비와 슬롯 경계 보존
  - WBP_EquipmentWindow 전용 재생성 및 CanvasSlot 좌표·크기 회귀 검사 추가
- 검증:
  - UE Editor 빌드 및 전체 메뉴 Widget Blueprint 컴파일 성공
  - 7개 슬롯 위치·크기 자동 검사 성공
  - PIE 장비 데이터 7개, 장비창 표시, 프리뷰 액터 1개 확인
  - 스타터 장비 5종 Texture Brush 연결 및 빈 슬롯 아이콘 숨김 확인
- 상태: 구현 및 런타임 검증 완료
- 관련 문서: `Feature/architect/2026-07-16_equipment-character-preview.md`, `Feature/doc/2026-07-16_equipment-character-preview.md`
# Sprint#2-1 - 인벤토리·장비창 동시 표시와 드래그 UI

- 목표: 인벤토리와 장비창을 동시에 표시하고 각 창을 타이틀 Drag로 이동하며, 장비창을 좌측 스탯·중앙 캐릭터 프리뷰·우측 장비 슬롯 구조로 정리한다.
- 주요 변경:
  - 인벤토리·장비창 전용 표시 그룹으로 두 창의 상호 배타 정책 제거
  - 두 전체화면 UserWidget·Canvas를 SelfHitTestInvisible로 설정해 실제 창 자식만 입력을 받고 투명 영역은 다른 창으로 입력 통과
  - 공통 `UI_WindowRoot`, `UI_WindowDragHandle`과 Canvas 이동·Viewport 경계 Clamp 추가
  - 인벤토리와 장비창에 44px 타이틀 Drag 영역 적용
  - 장비창을 980x620으로 확장하고 좌측 200x500 스탯, 중앙 340x500 프리뷰, 우측 350x500 7슬롯 Canvas 배치
  - 공격력·방어력·최대 체력·치명타 확률·공격 속도·이동 속도 Text 자리표시자 추가
  - 6개 스탯 Text를 C++ `BindWidgetOptional`로 노출해 향후 실제 수치 연결 준비
  - 슬롯을 `투구 | 목걸이`, `갑옷 | 장갑 | 무기`, `신발 | 반지` 3행으로 재정렬
  - 슬롯 크기를 갑옷 100x150, 무기 100x200, 나머지 5부위 100x100으로 통일
  - 각 장비 슬롯을 Frame > Canvas Layer > ScaleBox/Image + LocationText 계층으로 구성
  - Texture 원본 Brush 크기 복사를 끄고 긴 변 64px 정규화 및 사방 7px Stretch Anchor 적용
  - 무기·투구·갑옷·장갑·반지·신발·목걸이 위치 텍스트를 우측 상단 8px, Layer 1로 고정
  - 빈 장비 슬롯은 아이콘만 숨기고 위치 텍스트 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 활성 Editor에서 두 WBP 재구성·컴파일·저장 성공
  - 7개 슬롯 계층·좌표·크기·폰트·우측 상단 Anchor·ZOrder 자동 검사 성공
  - PIE에서 인벤토리·장비창 동시 True, 양쪽 UserWidget·화면 Canvas 입력 투과와 실제 자식 Visible 확인
  - PIE에서 투구 64×64, 갑옷 42.67×64, 무기 21.33×64 정규화 Desired Size 확인
  - 새 UE5.8 commandlet 전체 메뉴 WBP 컴파일 성공, 0 errors / 0 warnings
  - PIE 마우스 상호작용은 Remote Execution 중단으로 수동 QA 필요
- 상태: 구현 및 정적·자산 검증 완료, PIE 수동 상호작용 QA 필요
- 관련 문서: `Feature/architect/2026-07-17_draggable-inventory-equipment-windows.md`, `Feature/doc/2026-07-17_draggable-inventory-equipment-windows.md`, `DevLog/20260717.txt`

# Sprint#2-1 - 장비 → 인벤토리 Drag & Drop

- 목표: 장비창의 장착 아이템을 Data Asset의 MxN 크기를 유지한 채 인벤토리의 원하는 빈 위치로 옮긴다.
- 주요 변경:
  - `Equipment` Item Drag Source와 장비 부위·Item Data·수량·클릭 셀 오프셋 Payload 추가
  - 장비창 7개 슬롯 Frame의 Drag 시작 입력 연결
  - Inventory Window DragOver/Drop의 Grid Item·Equipment Item 공통 처리
  - `TryUnequipItemToGrid()`의 위치 재검증, Grid Entry 생성, 장비 슬롯 초기화 원자적 처리
  - 성공 시 `OnInventoryGridChanged`, `OnEquipmentChanged` 동시 갱신
  - 주무기 해제 시 `Unarmed` 전환
  - 자동 장착 시작 장비의 Grid 중복 제거
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint compile/구조 검증 성공
  - commandlet 0 errors / 0 warnings
  - `git diff --check` 통과
  - 실제 마우스 Drop은 PIE 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 컴파일 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_equipment-to-inventory-drag-drop.md`, `Feature/doc/2026-07-18_equipment-to-inventory-drag-drop.md`, `DevLog/20260718.txt`

# Sprint#2-1 - 메뉴 창 호출 순서 ZOrder 스택

- 목표: 인벤토리와 장비창이 겹칠 때 가장 늦게 호출하거나 클릭한 창을 최상단에 표시
- 주요 변경:
  - `MenuWindowStack`과 `BringMenuWindowToFront()`로 메뉴 창 호출 순서 관리
  - 스택 순서에 따라 ZOrder `50 + Index`를 제한된 범위에서 재배치
  - 공통 Preview Mouse 좌클릭으로 가려진 창 승격
  - 직접 `SetWindowOpen(true)` 호출 경로도 동일하게 승격
  - 현재 최상단 열린 창을 Game and UI 입력 Focus 대상으로 선택
  - 표시 상태가 변한 창만 갱신해 인벤토리·장비창 동시 표시 순서 보존
  - Viewport에 없는 메뉴 창만 `AddToPlayerScreen()`으로 최초 등록
  - 이미 등록된 메뉴 창은 `UGameViewportSubsystem`의 기존 Widget Slot ZOrder만 갱신
  - 아이템 이동·창 클릭 시 메뉴 위젯 중복 화면 등록 경고 제거
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint commandlet 컴파일 성공
  - `ValidateMenuWidgetAssets.py` 0 errors / 0 warnings
  - 중복 등록 방지 수정 후 Rogue10mEditor Win64 Development 재빌드 성공
  - PIE 실제 겹침·클릭·Drag & Drop 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_menu-window-z-order-stacking.md`, `Feature/doc/2026-07-18_menu-window-z-order-stacking.md`, `DevLog/20260718.txt`

# Sprint#2-2 - 인벤토리 아이템 Hover 툴팁

- 목표: 인벤토리 아이템 Hover 시 이름·아이템 정보·무게를 별도 UserWidget으로 표시
- 주요 변경:
  - `URogue10mInventoryItemTooltipWidget` C++ 부모와 `WBP_InventoryItemTooltip` 추가
  - `DisplayName`, `Description`, `UnitWeight` 기반 표시
  - 중첩 수량은 개당 무게와 총 무게를 함께 표시
  - Inventory Item의 `SetToolTip()` 연결 및 Drag Preview Tooltip 제거
  - `TSoftClassPtr` native 기본 경로로 활성 Editor의 기존 WBP 파일 잠금 회피
  - Menu Designer 빌더·좁은 Tooltip 생성 스크립트·전체 Validator 확장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_InventoryItemTooltip 생성·저장 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 필수 Widget 3개 및 native Tooltip Soft Class 기본값 검증 성공
  - PIE 실제 Hover·화면 경계·Drag 전환 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 생성 및 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_inventory-item-hover-tooltip.md`, `Feature/doc/2026-07-18_inventory-item-hover-tooltip.md`, `DevLog/20260718.txt`

# Sprint#2-3 - 인벤토리 아이템 우클릭 사용·장착

- 목표: 소비 아이템은 우클릭으로 사용하고 장비 아이템은 우클릭으로 즉시 장착·교체한다.
- 주요 변경:
  - Item Data Asset에 소비 효과용 `RestoreHealth` 설정 추가
  - 체력 회복이 실제 적용될 때만 소비 아이템 수량 1 감소
  - 빈 장비 부위 즉시 장착 및 동일 부위 기존 장비 교체
  - 기존 장비의 MxN Grid 공간을 변경 전에 확보하는 원자적 교체
  - 새 장비 원래 위치 우선, 이후 전체 컨테이너 빈 위치 검색
  - 공간 부족 시 인벤토리·장비 상태 변경 취소
  - 인벤토리·장비 Delegate 및 전투 로그 결과 갱신
  - 좌클릭 Drag & Drop과 Hover 툴팁 입력 경로 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 스타터 Item Asset Python 스크립트 문법 검사 성공
  - `git diff --check` 통과
  - PIE 실제 우클릭 사용·장착·교체 수동 QA 필요
- 상태: 구현·C++ 빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_inventory-item-right-click-actions.md`, `Feature/doc/2026-07-18_inventory-item-right-click-actions.md`, `DevLog/20260718.txt`

# Sprint#2-4 - 장비 Hover 스탯 및 장착 장비 비교 Tooltip

- 목표: 인벤토리 장비 Hover 시 장착 증가량과 동일 부위 현재 장착 장비의 교체 차이를 함께 표시한다.
- 주요 변경:
  - `FRogue10mEquipmentStatModifiers` 6종 장비 증가량 데이터 추가
  - 부위별 현재 장착 Item Data 조회 API 추가
  - Hover 장비 아이콘·이름·설명·무게·장착 증가량 카드 구성
  - 동일 부위 장착 장비 아이콘·이름·설명·현재 증가량 오른쪽 카드 구성
  - `Hover - 현재 장착` 변화량의 증가·감소·동일 색상 비교
  - 한쪽에만 존재하는 스탯을 포함한 합집합 비교
  - 일반 300px·비교 620px Tooltip 동적 폭과 비교 패널 `Collapsed` 처리
  - 스타터 장비 5종 샘플 스탯 Data Asset 저장
  - Tooltip 계층·폭·가시성·장비 스탯 값 Validator 확장
  - 등급별 불투명 아이템 이름 색상 API 및 Tooltip 적용
  - Hover 대상·현재 장착 비교 대상·장착 해제 메뉴 이름 색상 통일
  - `장착 시 증가` 제목 런타임 및 레이아웃 생성 단계에서 숨김
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_InventoryItemTooltip 재구성·컴파일·저장 성공
  - 전체 메뉴 Widget Blueprint commandlet 0 errors / 0 warnings
  - 스타터 장비 5종 스탯 값 검증 성공
  - Python 문법 및 `git diff --check` 통과
  - 등급 이름 색상 보완 후 Editor UHT/C++ 빌드 및 전체 메뉴 commandlet 재검증 성공
- 상태: 구현·C++ 빌드·WBP/Data Asset 정적 검증 완료, PIE Hover 비교 수동 QA 대기
- 관련 문서: `Feature/architect/2026-07-18_equipment-tooltip-stat-comparison.md`, `Feature/doc/2026-07-18_equipment-tooltip-stat-comparison.md`, `DevLog/20260718.txt`
- 보완 문서: `Feature/architect/2026-07-18_equipment-tooltip-rarity-name.md`, `Feature/doc/2026-07-18_equipment-tooltip-rarity-name.md`

# Sprint#2-5 - ???? ??? ?? ??

- ??: ???? ???? MxN ?? ?? ??????????????? ??? ?? ??? ????.
- ?? ??:
  - ERogue10mItemRarity? ??? 5??? ?? ????? ??
  - Item Data Asset ??? Inventory Background Color ?? ?? ??
  - WBP_InventoryItem? MxN ??? ??? UI_InventoryItemRarityBackground Border ??
  - ?? ?? 0????/Drag Preview 1??? 2 ??? ??
  - ??? Preview Tint? ?? ???? ?? ??? ?? ??
  - Tooltip Soft Class? Widget Blueprint ???? ????? ??
  - ??? 6?? ???????????? ?? ?? ??
  - ?? WBP/Data Asset ?? ????? 5????????? Validator ??
- ??:
  - Rogue10mEditor Win64 Development ?? ??
  - ?? Unreal Editor?? WBP_InventoryItem ?????????? ??
  - ?? ?? Widget Blueprint ?? ??
  - 5?? RGBA ??? ?? ?? ??
  - ??? 6? ?? ?? ?? ??
  - Unreal Editor Python ?? ?? ??
- ??: ???C++ ???WBP/Data Asset ?? ?? ??, PIE ?? ? ???? ?? QA ??
- ?? ??: Feature/architect/2026-07-18_inventory-item-rarity-backgrounds.md, Feature/doc/2026-07-18_inventory-item-rarity-backgrounds.md, DevLog/20260718.txt

# Sprint#2-6 - 장착 장비 Hover 및 장착 해제 메뉴

- 목표: 장비창의 장착 아이템에 Hover 상세 정보와 우클릭 장착 해제 UserWidget을 제공한다.
- 주요 변경:
  - 7개 장비 슬롯 Frame에 기존 장비 상세 Tooltip 연결
  - 이름·설명·무게·6종 장착 증가량 표시
  - 빈 슬롯 Tooltip 제거 및 장비 변경 시 이벤트 기반 갱신
  - URogue10mEquipmentSlotActionWidget과 WBP_EquipmentSlotAction 추가
  - 아이템명·장착 해제 버튼·공간 부족 결과 문구 구성
  - 우클릭 커서 위치와 Viewport 경계 보정
  - 장비창 닫기·장비 변경·좌클릭·성공 시 메뉴 자동 정리
  - 모든 Inventory Container의 MxN 첫 빈 공간 장착 해제 API 추가
  - 공간 부족 시 장비·인벤토리 상태 원상 유지
  - 기존 장비 좌클릭 Drag & Drop 경로 유지
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - WBP_EquipmentSlotAction 생성·컴파일·저장 성공
  - Action 결과 기본 Collapsed 및 필수 계층 검증 성공
  - 장비창 Tooltip/Action Soft Class Reference 검증 성공
  - 첫 빈 Grid 장착 해제 API 노출 검증 성공
  - 새 Unreal commandlet 프로세스 0 errors / 0 warnings
  - Editor Python AST와 git diff --check 통과
- 상태: 구현·C++ 빌드·WBP/API 정적 검증 완료, PIE Hover·우클릭·공간 부족 수동 QA 대기
- 관련 문서: Feature/architect/2026-07-18_equipped-item-hover-unequip-menu.md, Feature/doc/2026-07-18_equipped-item-hover-unequip-menu.md, DevLog/20260718.txt

# Sprint#2-7 - 인벤토리·장비 UI 비율 및 Drag & Drop 보정

- 목표: MxN 아이콘 비율과 Tooltip 크기를 정규화하고 인벤토리→장비 슬롯 Drop 장착 및 커서 중심 장비 Payload를 제공한다.
- 주요 변경:
  - 인벤토리 아이콘 footprint 84% Fit 및 원본 종횡비 유지
  - Texture Match Size 비활성화와 스케일 0.75~1.0 제한
  - 스타터 아이템 6종 InventoryIconScale 1.0 정규화
  - Tooltip 기본 280px·비교 580px·아이콘 52×52px
  - 장비·소비·장착 상태별 우클릭 동작 안내
  - 장비창 GridInventory DragOver·Drop·DragLeave 경로 추가
  - 부위 일치 검사와 호환·비호환 슬롯 Preview
  - 기존 TryEquipGridItem 기반 원자적 장착·교체
  - 장비 Drag Visual CenterCenter Pivot 적용
  - UE 5.8 UMG의 0.15초 Drag Decorator 보간과 전체 화면 Drag Source Geometry를 좌측 상단 비행 원인으로 특정
  - 장비 슬롯 Canvas의 MouseDown Hit-Test 경로에 투명 Drag Source 프록시 7개 사전 배치
  - MouseDown 시 동일 프록시를 Payload MxN 크기의 커서 중심 Layout으로 이동
  - Drop·Drag Cancel·클릭 종료·창 닫기 홈 Layout 복원 및 장비 갱신·Destruct 제거 수명주기 연결
  - 기존 CenterCenter Pivot과 MxN Payload 크기 유지
  - Drop·이탈·창·장비 수명주기 Preview 정리
  - Tooltip WBP 빌더와 전체 메뉴 Validator 확장
- 검증:
  - Rogue10mEditor Win64 Development 빌드 성공
  - 장비 Drag Source 프록시 추가 후 UE 5.8 UHT 통과
  - 수정된 Rogue10mEditor Win64 Development 재빌드 성공
  - 새 UnrealEditor-Cmd 프로세스에서 전체 메뉴 Widget Validator 통과
  - git diff --check 및 CheckGeneratedChanges 통과
  - WBP_InventoryItemTooltip 재생성·컴파일·저장 성공
  - 전체 메뉴 Widget Blueprint 명령형 검증 성공
  - Tooltip 폭·안내 문구 기본 가시성 검증 성공
  - 스타터 아이템 6종 InventoryIconScale 1.0 검증 성공
  - Python 문법 및 git diff --check 통과
- 상태: 구현·빌드·WBP/Data Asset 정적 검증 완료, 에디터 재시작 후 PIE 수동 상호작용 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-18_inventory-equipment-ui-polish.md`
  - `Feature/doc/2026-07-18_inventory-equipment-ui-polish.md`
  - `DevLog/20260718.txt`

## 2026-07-22 보완 - 장비 장착 해제 컨텍스트 메뉴

- 목표: 장착 장비 우클릭 시 Tooltip 중첩을 제거하고 메뉴 위치 및 외부 클릭 닫기 동작을 개선한다.
- 주요 변경:
  - 활성 Hover Tooltip 즉시 닫기 및 메뉴 종료 시 슬롯 Tooltip 복원
  - 커서 오른쪽 12px 메뉴 배치와 Viewport 경계 보정
  - 전체 화면 투명 dismiss 버튼으로 메뉴 외부 첫 클릭 소비 및 닫기
  - Action WBP 생성 스크립트와 Validator 필수 바인딩 갱신
- 검증: Rogue10mEditor 빌드 성공, 전체 메뉴 Widget Validator 0 errors / 0 warnings, Python 문법 및 diff 검사 통과
- 상태: 구현·빌드·WBP 정적 검증 완료, PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-22_equipment-context-menu-dismiss.md`, `Feature/doc/2026-07-22_equipment-context-menu-dismiss.md`, `DevLog/20260722.txt`

## 2026-07-23 보완 - 메뉴 Python wrapper 정리

- 목표: 메뉴 Widget 자동화의 중복 진입점을 제거하고 통합 빌더·Validator 중심으로 유지보수 경로를 단순화한다.
- 주요 변경:
  - `BuildMenuDesignerLayouts.py`로 대체된 부분 생성·재생성 wrapper 6개 삭제
  - 메뉴 생성은 `BuildMenuDesignerLayouts.py`, 회귀 검증은 `ValidateMenuWidgetAssets.py`로 통합
  - Data Asset·콘텐츠 이동·경고 검증·원격 실행용 재사용 스크립트는 유지
- 검증: 남은 Python 전체 문법 검사, 삭제 파일 참조 검색, `git diff --check`, `CheckGeneratedChanges.ps1`
- 상태: 스크립트 정리 및 정적 검증 완료
- 관련 문서: `DevLog/20260723.txt`

## 2026-07-23 보완 - HUD Quick Slot USTRUCT 초기화

- 목표: Editor Automation에서 반복된 `FRogue10mHudQuickSlotView::InputSlot` 미초기화 오류를 제거한다.
- 주요 변경: `InputSlot` 기본값을 `ERogue10mAttackInputSlot::Primary`로 지정
- 원인 분리:
  - `r.MotionVectorSimulation`: UE 5.8 엔진 CVar 플래그 문제로 프로젝트 변경 없음
  - 단독 `Condition failed` 15건: UE 5.8 엔진 테스트 로그로 프로젝트 오류와 무관
- 검증: Rogue10mEditor 빌드 성공, `UObject.Class AttemptToFindUninitializedScriptStructMembers` Automation 테스트 성공, 관련 LogClass 오류 0건
- 상태: 수정 및 자동 검증 완료
- 관련 문서: `DevLog/20260723.txt`

## 2026-07-23 보완 - 시작 인벤토리 추가 갑옷

- 목표: 시작 장비 자동 장착을 유지하면서 인벤토리에 가죽 갑옷 한 개를 추가로 제공한다.
- 주요 변경: `DA_Item_LeatherArmor` 시작 참조 추가, 동일 장비 부위의 후속 자동 장착 방지
- 검증: Rogue10mEditor 빌드 성공, `CheckGeneratedChanges.ps1` 및 `git diff --check` 통과
- 상태: 구현 및 자동 검증 완료, 새 PIE 세션 수동 확인 대기
- 관련 문서: `Feature/architect/2026-07-23_starter-inventory-extra-armor.md`, `Feature/doc/2026-07-23_starter-inventory-extra-armor.md`, `DevLog/20260723.txt`

## 2026-07-23 보완 - 에픽 수호자 투구

- 목표: 에픽 등급 Head 장비 Data Asset을 제작하고 시작 인벤토리에 추가한다.
- 주요 변경: `DA_Item_GuardianHelmet` 생성, 방어력 +10·최대 체력 +20 적용, 시작 아이템 참조 및 자동 검증 목록 추가
- 에셋 정책: 기존 철제 투구 아이콘 재사용, 불필요한 기존 스타터 에셋 재저장 변경 제거
- 검증: Rogue10mEditor 빌드 성공, UnrealEditor-Cmd 생성 검증 및 전체 메뉴·아이템 Validator 통과, 생성 파일·diff 검사 통과
- 상태: 구현 및 자동 검증 완료, 새 PIE 세션 수동 확인 대기
- 관련 문서: `Feature/architect/2026-07-23_epic-guardian-helmet.md`, `Feature/doc/2026-07-23_epic-guardian-helmet.md`, `DevLog/20260723.txt`

# Sprint#2-8 - 캐릭터 기본 스탯 및 장비창 표시

- 목표: 캐릭터 기본 스탯과 장비 보너스의 소유 구조를 확립하고 최종값을 전투·생존·이동 및 장비창에 일관되게 반영한다.
- 주요 변경:
  - Character Data Asset에 체력·자원·공격력·방어력·치명타·공격 속도·이동 기본값 정의
  - 기본·장비·최종값을 공유하는 Character Stat 구조체 추가
  - 장착 장비 전체의 6종 보너스 합산 API 추가
  - 장비 변경 성공 경로를 단일 스탯 갱신 진입점으로 통합
  - 공격력의 스킬 피해 반영과 방어력의 정액 피해 감소 적용
  - 최대 체력 변경 시 현재 체력 비율 보존
  - 이동 속도와 질주 속도를 CharacterMovement에 반영
  - 장비창 6종 스탯을 `최종 (기본 + 장비)` 형식으로 표시
  - 기본 Character Data 전용 설정 모드와 Validator 추가
- 검증:
  - Rogue10mEditor Win64 Development 전체 빌드 성공
  - 리뷰 보완 후 Rogue10mCharacter 증분 재빌드 성공
  - UnrealEditor-Cmd에서 기본 스탯 10종과 장비창 바인딩 6종 검증 성공
  - 기존 스타터 장비 6종 능력치 회귀 검증 성공
  - `UObject.Class AttemptToFindUninitializedScriptStructMembers` Automation 테스트 성공
  - Python 문법, `CheckGeneratedChanges.ps1`, `git diff --check` 검사
- 상태: 구현·빌드·에셋 정적 검증 완료, 새 PIE 세션 장착·해제·피해·이동 수동 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-23_character-base-stats-equipment-window.md`
  - `Feature/doc/2026-07-23_character-base-stats-equipment-window.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260723.txt`

## 2026-07-25 보완 - 장비창 Scene Capture 중복 갱신 제거

- 목표: 실시간 캐릭터 프리뷰는 유지하면서 자동·수동 Scene Capture의 중복 렌더 경고를 제거한다.
- 원인: `bCaptureEveryFrame=true` 상태에서 `SetPreviewActive()`와 장비 변경 경로가 수동 `CaptureScene()`도 호출
- 주요 변경: 활성 프리뷰는 자동 캡처만 사용하고 수동 캡처는 `bCaptureEveryFrame=false`일 때만 허용
- 검증: Rogue10mEditor Win64 Development 빌드 성공, 수동 캡처 보호 조건, 생성물·diff 검사
- 상태: 코드 및 빌드 검증 완료, 에디터 재시작 후 PIE 경고 재발 여부 수동 확인 대기
- 관련 문서: `Feature/doc/2026-07-16_equipment-character-preview.md`, `DevLog/20260725.txt`

## 2026-07-25 보완 - 장비창 Preview Character 좌클릭 회전

- 목표: 장비창 프리뷰에서 좌클릭 가로 드래그로 캐릭터를 좌우 회전한다.
- 주요 변경: 메시 전용 Pivot, 프리뷰 영역 판정, 마우스 캡처 기반 드래그, 0.35°/px 감도 설정, 캡처 손실 정리
- 입력 호환: 타이틀 바 창 이동, 장비 슬롯 Drag & Drop, 우클릭 장착 해제 경로 유지
- 렌더 호환: 실시간 Scene Capture 유지, 카메라·조명 고정, 자동·수동 중복 캡처 보호 유지
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, 생성물·diff 검사
- 상태: 구현·빌드·정적 검증 완료, 에디터 재시작 후 PIE 수동 상호작용 QA 대기
- 관련 문서: `Feature/architect/2026-07-25_equipment-preview-mouse-rotation.md`, `Feature/doc/2026-07-25_equipment-preview-mouse-rotation.md`, `DevLog/20260725.txt`

# Sprint#2-9 - 몬스터 경험치 보상 및 25종 로스터

- 목표: 몬스터 처치 경험치를 플레이어 성장에 연결하고 Data Asset 기반 25종 로스터를 준비한다.
- 주요 변경: 서버 권한 마지막 공격자 경험치 지급, 중복 지급 방지, 전투 로그, MonsterRank 및 공격 Fallback 수치 추가
- 콘텐츠: 일반 20종, 중간 보스 4종, 최종 보스 1종 Data Asset 생성
- 밸런스: 일반 경험치 18~140, 중간 보스 300~900, 최종 보스 3,000
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, Unreal Python Validator 25종·20/4/1 통과, 생성물·diff 검사
- 상태: 구현·에셋 생성·빌드·정적 검증 완료, 에디터 재시작 후 PIE 처치 보상 수동 QA 대기
- 관련 문서: `Feature/architect/2026-07-25_monster-experience-roster.md`, `Feature/doc/2026-07-25_monster-experience-roster.md`, `DevLog/20260725.txt`

# Sprint#2-10 - 캐릭터 커스터마이징·생성·접속

- 목표: 게임 시작 시 3슬롯에서 캐릭터를 생성·선택하고 해당 외형으로 접속하는 로비 흐름을 제공한다.
- 주요 변경: 인간·드워프·오크 남녀 6개 아키타입 카탈로그, 이름·외형 SaveGame, 생성·선택·삭제·접속 UI, 좌클릭 회전 프리뷰 추가
- 런타임 적용: 선택 프로필을 플레이어 메시와 PlayerState 표시에 적용하고 기존 1인칭 팔·전투·장비창 프리뷰 흐름 유지
- 저장 경계: 이름·외형·선택 슬롯만 저장하며 인벤토리·경험치·스탯·월드 진행도는 후속 캐릭터 저장 기능으로 분리
- 검증: UE 5.8 UHT 및 Rogue10mEditor 빌드 성공, Unreal Python Validator에서 6개 아키타입·필수 에셋·위젯 바인딩 통과, 생성물·diff 검사
- 상태: 구현·에셋 생성·빌드·정적 검증 완료, 에디터 재시작 후 PIE 생성·저장·접속·애니메이션 수동 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-25_character-customization-lobby.md`
  - `Feature/doc/2026-07-25_character-customization-lobby.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260725.txt`

## 2026-07-26 보완 - 캐릭터 선택 후 접속 실패

- 증상: 슬롯 선택 후 `게임 접속`을 눌러도 로비가 닫히지 않고 외형 적용 실패 로그가 반복됨
- 원인: `BP_FirstPersonCharacter` CDO의 `CustomizationCatalog` 참조 누락
- 주요 변경: Character Blueprint 카탈로그 기본값 저장, C++ 기본 경로 fallback, 로비 포커스 활성화
- 회귀 방지: 에셋 생성 스크립트에서 Character CDO 참조 설정, Validator에서 메시 컴포넌트와 카탈로그 CDO 검사
- 검증: 수정 전 Validator 오류 재현, 수정 후 6개 아키타입·로비·Player Character CDO 통과, UE 5.8 Editor 빌드 성공
- 상태: 원인 수정·에셋 저장·빌드·정적 검증 완료, 에디터 재시작 후 기존 슬롯 접속 PIE QA 대기
- 관련 문서: `Feature/doc/2026-07-25_character-customization-lobby.md`, `DevLog/20260726.txt`

## 2026-07-26 보완 - 상속 기반 캐릭터 외형 및 종족별 리타기팅

- 목표: 캐릭터 생성 후 입장 시 서로 다른 Skeleton의 Leader Pose 연결로 발생하는 머리·몸·팔 왜곡을 구조적으로 제거한다.
- 주요 변경:
  - `ARogue10mStylizedCharacter` 공통 외형 부모와 Human·Dwarf·Orc 남녀 6개 자식 Character Blueprint 추가
  - 숨김 Manny `AnimationSourceMesh`와 종족 전신 `Character Mesh`를 부모·자식으로 구성
  - 종족별 프로젝트 로컬 Target IK Rig, IK Retargeter, `Retarget Pose From Mesh` AnimBP 추가
  - 카탈로그 Archetype에 CharacterClass·Retargeter·RetargetAnimClass 연결
  - GameMode 선택 클래스 결정 및 실패 안전 Spawn/Possess 교체 흐름 추가
  - 서로 다른 Skeleton Leader Pose 제거, 동일 Skeleton Hair·Facial 파츠에만 Leader Pose 적용
  - 원본 Orc Male IK Rig의 무효 `Cape` 체인은 프로젝트 로컬 복제본에서 제거
- 검증:
  - Rogue10mEditor Win64 Development 전체 빌드 성공
  - 상속/IK/AnimGraph Validator 오류 0건, 경고 0건
  - 기존 캐릭터 로비/카탈로그 Validator 오류 0건, 경고 0건
  - `CheckGeneratedChanges.ps1` 통과
- 상태: 구현·에셋 생성·정적 검증 완료, 에디터 재시작 후 6조합 이동·공격·장비창 Preview 수동 PIE QA 필요
- 관련 문서:
  - `Feature/architect/2026-07-26_inherited-character-appearance.md`
  - `Feature/doc/2026-07-26_inherited-character-appearance.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260726.txt`
## 2026-07-26 보완 - 상속 CharacterClass 접속 실패 수정

- 목표: 선택 프로필의 자식 CharacterClass를 찾지 못해 게임 접속이 중단되는 문제 해결
- 주요 변경: 공통 외형 부모 BeginPlay 조기 적용 제거, 카탈로그 고정 경로 fallback, 종족·성별 자식 Blueprint class fallback 추가
- 검증: Editor 빌드 성공, 상속 Validator 통과, 실제 PIE에서 Human Male 자식 Pawn Spawn/Possess 및 EnterSelectedCharacter 후 로비 종료 확인, 최근 오류 로그 0건
- 상태: 수정 및 자동 PIE 검증 완료
- 관련 문서: `Feature/doc/2026-07-26_inherited-character-appearance.md`, `DevLog/20260726.txt`

# Sprint#3-2 - 몬스터 영역 스포너

- 브랜치: `Sprint#3-2-monster-area-spawner`
- 목표: 지정한 Box 범위 안에서 특정 몬스터를 최대 N마리까지 생성하고 선택적으로 생존 수를 유지한다.
- 주요 변경:
  - `ARogue10mMonsterSpawner` World Actor 추가
  - Monster Class, Box 범위, 최대 생존 수, 생성 간격 에디터 설정
  - 시작 즉시 최대 수 충원과 지속 보충·일회성 생성 선택
  - 지면 트레이스와 충돌 안전 생성
  - Tick 없는 타이머, 약한 참조, 파괴 Delegate 기반 생존 수 추적
  - Authority 전용 생성과 Blueprint 수동 제어 API
- 검증:
  - UHT 통과
  - `Rogue10mMonsterSpawner.cpp` UBT 단일 파일 컴파일 성공
  - UI Preview Actor의 Unity 상수 재정의를 기능별 접두사로 수정
  - `Rogue10mEditor` Win64 Development 전체 빌드 성공
  - `CheckGeneratedChanges.ps1`, `git diff --check`
- 상태: C++ 구현 및 전체 Editor 빌드 검증 완료, 레벨 배치 PIE QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_monster-area-spawner.md`
  - `Feature/doc/2026-07-28_monster-area-spawner.md`
  - `DevLog/20260728.txt`

## Sprint#3-3 - 모험가 기본 직업과 스폰 후 맨손 공격

- 목표: 캐릭터 선택 후 스폰·Possess된 Pawn에 기본 직업 모험가와 좌클릭 맨손 Primary 공격을 안정적으로 부여한다.
- 주요 변경: CharacterData 직업 필드, PlayerState 모험가 기본값, PossessedBy/OnRep 기반 로드아웃 재초기화, 종족명과 직업명 분리
- 에셋: `DA_Character_Default`를 Unarmed 기본 무기로 변경하고 Unarmed Profile·Primary 주먹 공격·BP CombatComponent 참조를 검증 및 저장
- 빌드 보완: UE Unity 빌드의 Preview Actor 익명 namespace 상수 재정의 충돌 제거
- 검증: UE 5.8 Rogue10mEditor 전체 빌드 성공, 모험가/Unarmed Editor Python Validator 통과, 6개 상속 Character 및 Retarget AnimBP Validator 통과
- 상태: 구현·에셋 설정·빌드·정적 검증 완료, 에디터 재시작 후 실제 좌클릭 PIE 체감 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_adventurer-unarmed-spawn.md`
  - `Feature/doc/2026-07-28_adventurer-unarmed-spawn.md`
  - `Docs/CharacterDataOwnership.md`
  - `DevLog/20260728.txt`
## Sprint#3-4 - 3인 캐릭터 선택 무대

- 브랜치: `Sprint#3-4-character-selection-stage`
- 목표: 기존 캐릭터 로비 기능을 유지하면서 저장된 최대 3명의 캐릭터를 판타지 배경 앞에 동시에 표시한다.
- 주요 변경:
  - CharacterLobbyWidget의 Draft Preview 1개와 Slot Preview 3개 수명주기
  - 숨김 Manny Idle Source Mesh와 6개 종족·성별 Retarget AnimBP 적용
  - 선택 캐릭터 명도·슬롯 카드 강조와 각 프리뷰 좌클릭 회전
  - 오리지널 16:9 판타지 성채 배경과 1920×1080 WBP 무대형 레이아웃
  - 기존 생성·삭제·선택·게임 접속·외형 편집 BindWidget 유지
  - 빈 슬롯 캡처 비활성화와 로비 종료 시 Preview Actor 정리
- 검증:
  - UE 5.8 UHT 및 Rogue10mEditor Win64 Development 전체 빌드 성공
  - 기존 캐릭터 커스터마이징 Validator 통과
  - 신규 3인 선택 무대 Validator 통과
  - 최종 WBP 재로드 시 Blueprint/Python 오류 없음
- 상태: 구현·배경·WBP·빌드·에셋 정적 검증 완료, 에디터 PIE 육안 QA 대기
- 관련 문서:
  - `Feature/architect/2026-07-28_character-selection-stage.md`
  - `Feature/doc/2026-07-28_character-selection-stage.md`
  - `DevLog/20260728.txt`

### Sprint#3-4 보완 - Main Menu / Lobby / Component 경로 분리

- 목표: 캐릭터 로비 비율·상호작용을 보완하고 기본 메인 메뉴에서 로비와 인게임으로 이어지는 화면 흐름 구성
- 주요 변경:
  - `Content/Widget/Lobby/WBP_CharacterLobby`로 로비 이동
  - 기존 공용 메뉴 위젯을 `Content/Widget/Component`로 이동
  - `Content/Widget/Menu/WBP_MainMenu` 신규 생성 및 게임 시작·종료 연결
  - Slot Preview 폭 360px 제한, `UI_SelectedCharacterInfoText` 제거, 하단 선택/생성 버튼 배치
  - 캐릭터/슬롯 정보 더블클릭 접속과 빈 슬롯 캐릭터 생성 지원
  - PlayerController 흐름을 `Main Menu → Lobby → 현재 Start Map`으로 변경
- 검증: UE 5.8 Rogue10mEditor 빌드 성공, Widget Flow/Selection Stage/Customization/Inherited Character 검증 통과
- 상태: 구현·에셋 이동·자동 검증 완료, PIE 수동 체감 QA 대기
- 후속: `Start Map → World Partition Cell 오픈월드` 설계 및 구현
- 관련 문서:
  - `Feature/architect/2026-07-28_character-selection-stage.md`
  - `Feature/doc/2026-07-28_character-selection-stage.md`
  - `Docs/CurrentProjectArchitecture.md`
  - `DevLog/20260728.txt`
## Sprint#3-5 - 몬스터 Behavior Tree 전투 AI

- 브랜치: `Sprint#3-5-monster-behavior-tree-ai`
- 목표: 몬스터 이동을 홈·순찰·추격 범위로 제한하고 거리 감지 또는 피격 조건에서만 플레이어와 전투
- 주요 변경:
  - Tick 추적 제거, AIController + AI Perception + Blackboard + Behavior Tree + NavigationSystem 적용
  - Sight/피격 어그로, 기억 시간, 최대 추격 거리, 홈 복귀, 순찰 도착 대기 구현
  - 공통 `BB_Monster`, `BT_Monster`와 런타임 안전 대체 트리 추가
  - 25개 Monster Data Asset에 BT·거리 설정 연결
  - BeginPlay 데이터 적용 후 AI 재초기화로 UE Possess 순서 대응
  - 로비 `UI_StatusText` 제거, 3개 프리뷰 정면 배치, 16:9 배경 채움
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - Monster Behavior Tree AI Validator 통과(Blackboard 8키, 2 Task, 25 Data Asset)
  - Character Selection Stage Validator 통과
- 상태: 코드·에셋·자동 검증 완료, NavMesh 레벨에서 순찰·피격 어그로·추격 해제 수동 PIE QA 필요
- 관련 문서:
  - `Feature/architect/2026-07-28_monster-behavior-tree-ai.md`
  - `Feature/doc/2026-07-28_monster-behavior-tree-ai.md`
  - `Docs/CurrentProjectArchitecture.md`
  - `DevLog/20260728.txt`
### Sprint#3-5 보완 - Main Menu UIOnly 포커스 오류

- 문제: `FInputModeUIOnly`가 포커스 불가능한 Main Menu `SObjectWidget`을 대상으로 지정해 PlayerController 오류 로그 발생
- 수정: `URogue10mMainMenuWidget::NativeConstruct()`에서 Main Menu 루트를 포커스 가능하게 설정
- 검증: UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
- 상태: 코드·빌드 검증 완료, PIE에서 오류 로그 미발생 확인 필요
- 관련 기록: `DevLog/20260729.txt`
### Sprint#3-5 보완 - 캐릭터 선택 슬롯 및 프리뷰 복구

- 문제: BottomBar가 버튼 입력을 차단하고, 일시정지 중 SceneCapture 갱신 실패로 Slot Preview가 흰색으로 표시됨
- 수정: 버튼 ZOrder 8, 장식/프리뷰 Hit Test 비활성, 조명·메시 활성화 후 수동 Capture, Pause 중 메시·Capture 갱신
- UI: Lobby Background Image 유지, 전체 화면 Stretch 앵커와 0 오프셋 적용
- 에디터 적용: 공식 Unreal Python Remote Execution으로 현재 PIE 종료 후 WBP 재구성
- 검증: UE 5.8 Editor 빌드 성공, 강화된 Character Selection Stage Validator 통과
- 상태: 코드·WBP·자동 검증 완료, 에디터 재시작 후 PIE에서 실제 프리뷰와 클릭 체감 확인 필요
- 관련 문서: `Feature/doc/2026-07-28_character-selection-stage.md`, `DevLog/20260729.txt`
### Sprint#3-5 보완 - 캐릭터 프리뷰 소스 로드 및 WASD 입력 복구

- 문제: Manny 프리뷰 메시의 불완전한 Soft Object Path로 프리뷰 초기화 실패
- 문제: Main Menu와 Lobby에서 이동·시점 입력 차단이 중복 누적되어 접속 후에도 WASD 비활성
- 수정: 메시 경로를 `.SKM_Manny_Simple`까지 포함한 전체 오브젝트 경로로 변경
- 수정: UI 진입 시 입력 차단 중복 방지, 접속 시 `ResetIgnoreMoveInput`/`ResetIgnoreLookInput` 적용
- 진단: 접속 직후 입력 차단 및 일시정지 상태 로그 추가
- 검증: UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
- 상태: 코드·빌드 검증 완료, 에디터 재시작 후 PIE 프리뷰/WASD 체감 QA 필요
- 관련 문서: `Feature/doc/2026-07-28_character-selection-stage.md`, `DevLog/20260729.txt`
## Sprint#3-6 - Menu Map 기반 UI와 인게임 전환

- 브랜치: `Sprint#3-6-menu-map-flow`
- 목표: 메뉴·캐릭터 선택을 별도 맵으로 분리하고 선택 캐릭터로 인게임 맵에 진입
- 주요 변경:
  - `/Game/Rogue10m/Maps/L_Menu` 신규 생성
  - `Rogue10mMenuGameMode`, `Rogue10mMenuPlayerController` 추가
  - PlayerController의 Menu World / Gameplay World 초기화 분리
  - 선택 프로필 저장 후 `Lvl_FirstPerson?StartRun=1` OpenLevel
  - `EditorStartupMap`, `GameDefaultMap`을 `L_Menu`로 변경
  - Main Menu 전체 화면 Stretch 및 제목·버튼 확대
- 검증:
  - UE 5.8 Editor 빌드 성공
  - Menu Map Flow Validator 통과
  - Widget Flow Validator 통과
  - 실제 PIE에서 `L_Menu → Lobby → Lvl_FirstPerson` 이동 성공
  - 선택 Pawn Spawn/Possess, 이동·시점 입력 및 Pause 해제 확인
- 상태: 구현·맵 자산·설정·자동 PIE 검증 완료
- 관련 문서:
  - `Feature/architect/2026-07-29_menu-map-flow.md`
  - `Feature/doc/2026-07-29_menu-map-flow.md`
  - `DevLog/20260729.txt`

## Sprint#3-6 - 로비 캐릭터 프리뷰 정면·동일 크기·투명 배경

- 목표: 캐릭터 선택 로비에서 세 캐릭터를 동일한 크기의 정면 고정 프리뷰로 표시하고 배경 위에 캐릭터만 합성
- 주요 변경:
  - 프리뷰 Yaw -90도 고정
  - 세 Slot Preview Y=15, 360×720 통일
  - SceneColor HDR/Inverse Opacity + UI Translucent 합성
  - M_CharacterPreviewTransparent 추가
  - Lobby 배경 Shade 및 Slot Stage Glow 제거
  - Lobby 드래그 회전 제거, Equipment 회전 유지
- 검증:
  - UE 5.8 Rogue10mEditor 빌드 성공
  - Character Selection Stage Validator 오류 0건
  - Character Lobby Flow Suite 오류 0건
- 상태: 구현 및 자동 검증 완료
- 관련 문서:
  - Feature/architect/2026-07-28_character-selection-stage.md
  - Feature/doc/2026-07-28_character-selection-stage.md

- 추가 런타임 검증: Menu Map PIE에서 Lobby 표시 및 프리뷰 액터 4개 생성, 관련 로드 오류 0건

## Sprint#4-1 - 로비 프리뷰-슬롯 선택 동기화

- 브랜치: `Sprint#4-1-lobby-preview-slot-selection`
- 목표: 로비의 캐릭터 프리뷰 클릭과 하단 캐릭터 슬롯 선택 상태를 일치시킨다.
- 주요 변경:
  - 프리뷰 단일 좌클릭 입력 처리 추가
  - 프리뷰 화면 좌표를 슬롯 인덱스로 판정
  - 프리뷰와 하단 슬롯 버튼이 기존 `SelectSlot()` 경로 공유
  - 프리뷰 더블클릭 즉시 접속 기능 유지
  - 생성 화면에서는 슬롯 선택 입력 제외
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - `Scripts/CheckGeneratedChanges.ps1` 통과
  - `git diff --check` 통과
- 상태: 코드 및 자동 검증 완료, Menu Map PIE 수동 클릭 QA 대기
- 관련 문서:
  - `Feature/architect/2026-08-04_lobby-preview-slot-selection.md`
  - `Feature/doc/2026-08-04_lobby-preview-slot-selection.md`
  - `DevLog/20260804.txt`

## Sprint#4-2 - 고딕 전투 HUD 비주얼 개편

- 브랜치: `Sprint#4-2-gothic-combat-hud`
- 목표: 기존 HUD 배치를 유지하면서 레퍼런스와 유사한 어두운 고딕 액션 RPG 스타일을 적용한다.
- 주요 변경:
  - 흑철·청동·붉은 룬 기반의 독창적인 하단 HUD 프레임 이미지 생성
  - 투명 원본에서 좌우 바, 중앙 메달, 경험치, 슬롯 프레임 텍스처 파생
  - UI용 Texture2D 5개 임포트 및 `WBP_Rogue10mMainHUD`/HUD 파트에 적용
  - 기존 기능 위젯의 좌표·크기·ZOrder·바인딩 이름 보존
  - 재현 가능한 UMG 빌드 및 검증 스크립트 추가
- 검증:
  - 수정 Widget Blueprint 8개 컴파일 및 저장 성공
  - Gothic Combat HUD Validator 통과
  - 기존 주요 위젯 9개의 배치 값 일치
  - Texture2D 5개 및 QuickSlot 필수 바인딩 로드 성공
  - `Scripts/CheckGeneratedChanges.ps1` 통과
- 상태: 이미지 제작, Unreal 임포트, 실제 UMG 배치, 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-05_gothic-combat-hud.md`
  - `Feature/doc/2026-08-05_gothic-combat-hud.md`
  - `DevLog/20260805.txt`
## Sprint#4-3 - Obsidian 아키텍처 결합도·응집도 연동

- 브랜치: `Sprint#4-3-obsidian-architecture-metrics`
- 목표: 현재 C++ 파일 의존 관계를 Obsidian에서 반복 생성하고 결합도·응집도로 탐색한다.
- 주요 변경:
  - `Source/Rogue10m` 내부 include 분석기와 지표 검증기 추가
  - C++ 파일 97개의 유입·유출 Wiki Link 노트 생성
  - 폴더 응집도·불안정도 표와 Mermaid 의존 그래프 생성
  - 폴더 10개를 연결한 Obsidian Canvas와 원시 `metrics.json` 생성
  - `Docs/Obsidian/ArchitectureMetrics` 대시보드 및 분석·검증·Obsidian 실행 진입점 추가
  - 범용 학습·면접 Vault 폴더와 생성 도구 제거, 지표 산출물을 프로젝트 문서 영역으로 이전
- 검증:
  - 소스 파일 97개와 생성 노트 97개 일치
  - 프로젝트 내부 include 관계 197개 추출
  - 생성 Wiki Link, Canvas JSON, 지표 범위 검증 통과
  - Obsidian 실행 스크립트 PowerShell 구문 검증 통과
- 상태: Obsidian 1.13.6 설치, Rogue10m Vault 등록, Dashboard 자동 열기, 시각화·자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_obsidian-architecture-metrics.md`
  - `Feature/doc/2026-08-12_obsidian-architecture-metrics.md`
  - `DevLog/20260812.txt`

## Sprint#4-4 - 전체 폭 경험치·레벨 바 이미지

- 브랜치: `Sprint#4-4-full-width-xp-level-bar`
- 목표: 화면 하단 전체 폭을 사용하는 고딕 경험치 트랙과 레벨 메달 이미지를 제작한다.
- 주요 변경:
  - 좌측 빈 레벨 메달과 우측 끝까지 이어지는 단일 경험치 트랙 생성
  - 크로마키 제거 후 1920×353 투명 PNG 제작
  - UI용 `T_HUD_GothicXPLevelBar` Texture2D 임포트
  - 기존 경험치 프레임과 Widget Blueprint는 변경하지 않음
- 검증:
  - 네 모서리 알파 0 및 전체 폭 가시 영역 확인
  - Texture2D 크기 1920×353 확인
  - `TEXTUREGROUP_UI`, `NeverStream=true` 확인
  - UnrealEditor-Cmd 오류 0, 경고 0, Validator 통과
- 상태: 이미지 제작, 투명화, Unreal 임포트 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_full-width-xp-level-bar.md`
  - `Feature/doc/2026-08-12_full-width-xp-level-bar.md`
  - `DevLog/20260812.txt`

## Sprint#4-5 - 고딕 몬스터 정보 프레임

- 브랜치: `Sprint#4-5-gothic-monster-info-frame`
- 목표: 몬스터 조준 시 표시되는 420×68 Info 영역을 기존 HUD와 같은 고딕 스타일로 꾸밀 이미지 자산을 준비한다.
- 주요 변경:
  - 상단 이름/레벨 명패와 하단 체력 트랙을 포함한 프레임 제작
  - 추후 속성·상태 아이콘용 작은 좌우 마름모 홈 구성
  - 840×136 투명 PNG와 UI용 Texture2D 임포트
  - 기존 `WBP_MonsterInfo` 배치와 바인딩은 변경하지 않음
- 검증:
  - 네 모서리 알파 0 및 가시 영역 확인
  - Texture2D 크기 840×136 확인
  - `TEXTUREGROUP_UI`, NoMipmaps, `NeverStream=true` 확인
  - UnrealEditor-Cmd 오류 0, 경고 0, Validator 통과
- 상태: 이미지 제작, 투명화, Unreal 임포트 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-monster-info-frame.md`
  - `Feature/doc/2026-08-12_gothic-monster-info-frame.md`
  - `DevLog/20260812.txt`

## Sprint#4-6 - 공통 캐릭터 애니메이션·VFX

- 브랜치: `Sprint#4-6-common-character-animation`
- 목표: 모든 종족이 같은 스킬에서 동일한 원본 모션을 사용하도록 걷기·뛰기·점프·구르기·주먹 공격과 연계 효과를 구축한다.
- 주요 변경:
  - 숨김 Manny 공통 Source AnimBP와 기존 종족별 Retarget AnimBP를 연결
  - 이벤트·타이머 기반 이동/점프/구르기 AnimationComponent 추가
  - 걷기·뛰기·점프·2단 점프·착지·구르기 Niagara 6종 구성
  - 단일·차징·3연계 주먹 Montage와 Cast/Charge/Impact Niagara 6종 구성
  - 공격/모션 Data Asset에서 Montage와 Niagara를 교체할 수 있도록 확장
  - Unarmed 1단, StoneFist 2단 점프 프로필 검증
- 검증:
  - UE 5.8 Rogue10mEditor Win64 Development 빌드 성공
  - Unreal Asset Validator 30개 자산 통과
  - 공통 검증기: Locomotion 5종, Montage 5개, Niagara 12개, 외형 6종, 공격 8개, 점프 프로필 1/2 통과
  - `Scripts/CheckGeneratedChanges.ps1`, `git diff --check` 통과
- 상태: 코드·자산·Data Asset 구성과 자동 검증 완료, PIE 체형별 육안 QA 권장
- PIE 직접 검증:
  - Human Male에서 공통 `ABP_Common_Unarmed`, Idle Locomotion 확인
  - 기본 주먹 `AM_Punch_01`, 특수 주먹 `AM_Punch_03`, 구르기 `AM_Dodge_Roll` 런타임 재생 확인
  - 점프 이벤트와 Jump Niagara, 공격·구르기 Niagara 런타임 컴파일 확인
  - 지속 입력이 필요한 뛰기·2단 점프·차징·체인 전체 재생은 수동 입력 QA 대기
- 관련 문서:
  - `Feature/architect/2026-08-12_common-character-animation-vfx.md`
  - `Feature/doc/2026-08-12_common-character-animation-vfx.md`
  - `DevLog/20260812.txt`

## Sprint#4-8 - 묵직한 전리품 주머니 Actor

- 브랜치: `Sprint#4-8-loot-bag-actor`
- 목표: 인벤토리 월드 드롭과 몬스터 전리품이 공통으로 사용할 수 있는 묵직한 주머니 Actor Blueprint를 제공한다.
- 주요 변경:
  - `ARogue10mDroppedItem` 상속 `BP_HeavyLootBag` 생성
  - 눌린 주머니 본체, 목, 매듭, 좌우 끈의 5개 StaticMeshComponent 구성
  - 프로젝트 천·목재 머티리얼 재사용 및 금빛 이름 표시 적용
  - 공통 주머니 외형 유지를 위해 상속된 개별 아이템 메시 숨김
  - 본체만 `OverlapAllDynamic`, 장식은 `NoCollision`, Tick·동적 조명 없음
  - 재현 가능한 Unreal Editor 생성·검증 스크립트 추가
- 검증:
  - UnrealEditor-Cmd Blueprint 생성·컴파일·저장 성공
  - 새 UnrealEditor-Cmd 프로세스에서 부모·컴포넌트·메시·머티리얼·충돌·가시성 검증 `RESULT=PASSED`
  - `Scripts/CheckGeneratedChanges.ps1` 통과
  - `git diff --check` 통과
- 상태: Actor `.uasset` 제작 및 자동 검증 완료, 인벤토리/몬스터 스폰 클래스 연결은 후속 통합 작업
- 관련 문서:
  - `Feature/architect/2026-08-12_heavy-loot-bag-actor.md`
  - `Feature/doc/2026-08-12_heavy-loot-bag-actor.md`
  - `DevLog/20260812.txt`

## Sprint#4-7 - 고딕 UI 통합 적용

- 브랜치: `Sprint#4-7-gothic-ui-application`
- 목표: 준비된 고딕 HUD·인벤토리·경험치·몬스터 정보 자산을 실제 프로젝트 UMG에 적용한다.
- 주요 변경:
  - 신규 1120×1220 투명 인벤토리 외곽 프레임 제작 및 `WBP_InventoryWindow` 적용
  - 원형 레벨 장식을 보존한 1920×112 실사용 전체 폭 경험치 바 제작 및 `WBP_LevelExperiencePanel` 적용
  - 840×136 몬스터 정보 프레임을 `WBP_MonsterInfo`에 적용
  - 기존 C++ `BindWidget` 이름과 인벤토리/몬스터 배치 보존
  - `ProgressionWidget`을 화면 최하단 전체 폭, ZOrder 0으로 조정
  - 실제 조합 확인용 `WBP_GothicUIShowcase`와 1920×1080 적용 프리뷰 추가
- 검증:
  - 관련 Widget Blueprint 컴파일 성공
  - TextureGroup UI, NoMipmaps, NeverStream 및 해상도 검사 통과
  - 필수 바인딩 누락 0개
  - `GothicIntegratedUIValidation.txt` 결과 `PASSED`
  - Unreal Editor Widget Designer 시각 확인 완료
- 상태: 텍스처 제작, Unreal 임포트, UMG 적용, 자동 검증, 적용 프리뷰 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-ui-application.md`
  - `Feature/doc/2026-08-12_gothic-ui-application.md`
  - `DevLog/20260812.txt`

## Sprint#4-9 - 던전 보상 보물상자 Actor

- 브랜치: `Sprint#4-9-dungeon-reward-chest`
- 목표: 던전 클리어 보상 지점에 배치하거나 스폰할 수 있는 보물상자 Actor `.uasset`을 제작한다.
- 주요 변경:
  - `/Game/World/Rewards/BP_DungeonRewardChest` Blueprint Actor 생성
  - 목재 본체·둥근 뚜껑·금속 띠·잠금장치를 포함한 11개 메시 컴포넌트 구성
  - 후속 개방 연출용 `LidPivot`, 상호작용용 `RewardInteractionZone`, 보상 강조용 `RewardGlow` 추가
  - `DungeonRewardChest`, `RewardContainer` 태그와 Tick 비활성화 적용
  - 재현 가능한 Unreal Editor 생성·검증 스크립트 추가
- 검증:
  - UnrealEditor-Cmd Blueprint 생성·컴파일·저장 성공
  - 새 UnrealEditor-Cmd 프로세스에서 부모·태그·Tick·컴포넌트·메시·재질·충돌·오버랩·조명 검증 `RESULT=PASSED`
  - 오류 0건; 기존 `AdvancedPortalsSystemVFX/SM_Plane` 관련 물리 경고만 확인
- 상태: Actor `.uasset` 제작 및 자동 검증 완료, 개봉·보상 지급·중복 수령 방지·저장 연동은 후속 통합 작업
- 관련 문서:
  - `Feature/architect/2026-08-12_dungeon-reward-chest-actor.md`
  - `Feature/doc/2026-08-12_dungeon-reward-chest-actor.md`
  - `DevLog/20260812.txt`
---

## Sprint#4-10 - 고딕 전투 HUD 하단 배치 보정 (2026-08-12)

- 목표: 경험치 바를 화면 최하단 전체 폭에 붙이고 초록색으로 변경하며, 체력/스태미나를 구분하고 스킬/아이템 외측 빈 공간을 고딕 장식으로 채운다.
- 주요 변경:
  - 1920x52 전용 하단 경험치 프레임과 초록색 진행 바 적용
  - 화면 중앙 기준 체력/스태미나/메달리온/슬롯 패널 대칭 재배치
  - 5칸 기능 슬롯을 유지한 좌측 스킬 날개 및 우측 아이템 날개 Texture2D 제작/연결
  - 스태미나 런타임 색상을 황금색 계열로 정리
- 검증:
  - Editor target build 성공
  - `Saved/GothicIntegratedUIValidation.txt`: `RESULT=PASSED`
  - Unreal Editor `WBP_GothicUIShowcase` Designer 실제 표시 확인
- 상태: 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_gothic-hud-layout-refinement.md`
  - `Feature/doc/2026-08-12_gothic-hud-layout-refinement.md`

## Sprint#4-11 - 기본 이동 파티클 절제 조정 (2026-08-13)

- 브랜치: `Sprint#4-11-subtle-basic-motion-vfx`
- 목표: 과도한 걷기 파티클을 참조 영상처럼 짧고 둥근 먼지 Poof로 바꾸고 기본 동작 전체의 화면 점유와 누적을 줄인다.
- 주요 변경:
  - StarterContent 연기 원본을 UE 5.8 공식 Cascade→Niagara 변환기로 프로젝트용 소프트 먼지 Niagara로 변환
  - 걷기·뛰기·점프·2단 점프·착지·구르기 Niagara 6종을 같은 먼지 계열로 통일
  - 걷기 0.12배/0.68초, 뛰기 0.16배/0.38초 등 동작 강도별 스케일과 발생 간격 적용
  - 동작별 0.08~0.14초 방출 종료 Timer와 Niagara 시간 배율 3.0 적용
  - 좌우 발 오프셋과 첫 발자국 지연을 적용하고 공격 VFX는 보존
  - 변환된 자산의 에디터 전용 플러그인 런타임 의존성 검증 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 구성 스크립트 `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
  - 검증기: 이동 상태 5종, Niagara 12종, 절제 이동 VFX 6종, 1단/2단 점프 프로필 통과
  - 변환기 런타임 패키지 의존성 0개 확인
  - Human Male 실제 PIE에서 걷기·점프·구르기 입력 실행, 기존 청색 파편 반복 및 이동 VFX 누적 미발생 확인
  - 짧은 방출의 개별 먼지 프레임은 입력 자동화 타이밍상 정지 화면 분리가 제한적이며 뛰기·2단 점프는 자동 검증으로 확인
- 상태: 코드·Data Asset·Niagara 보정, 빌드, 자동 검증, 직접 PIE 기본 동작 QA 완료
- 관련 문서:
  - `Feature/architect/2026-08-12_subtle-basic-motion-vfx.md`
  - `Feature/doc/2026-08-12_subtle-basic-motion-vfx.md`
  - `DevLog/20260813.txt`

## Sprint#4-12 - 캐릭터 파티클 구조 및 출력 정리 (2026-08-14)

- 브랜치: `Sprint#4-12-organized-character-vfx`
- 목표: 기본 이동과 맨손 공격 파티클을 용도별로 정리하고, 화면을 가리지 않는 짧고 절제된 출력으로 통일
- 주요 변경:
  - 이동 Niagara 6종을 `VFX/Character/Movement`, 맨손 공격 Niagara 6종을 `VFX/Character/Combat/Unarmed`로 분리
  - 공격 Data Asset에 시전·차징·타격 크기, 방출 시간, Niagara 시간 배율 튜닝값 추가
  - 시전 0.22~0.32배/0.10~0.16초, 차징 0.18배, 타격 0.28배/0.14초로 축소
  - 사용이 끝난 일회성 Niagara를 Timer로 비활성화하고 기존 루트 중복 자산은 복구 가능한 `tmp/legacy-character-vfx-backup-20260814`로 이동
  - 구성 및 검증 스크립트가 정리된 경로와 출력 튜닝을 생성·검사하도록 갱신
- 검증:
  - UE 5.8 Editor target 전체 및 증분 빌드 성공
  - 공용 캐릭터 애니메이션 검증기 `RESULT=PASSED`, Niagara 12종과 이동/공격 각 6종 확인
  - Python 구성·검증 스크립트 문법 검사 통과
- 상태: 구현, 자산 정리, 빌드 및 자동 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-14_organized-character-vfx.md`
  - `Feature/doc/2026-08-14_organized-character-vfx.md`
  - `DevLog/20260814.txt`
## Sprint#4-13 - 애니메이션 단독 표시 및 파티클 분리 (2026-08-15)

- 브랜치: `Sprint#4-13-animation-only-presentation`
- 목표: 캐릭터 기본 모션을 파티클 간섭 없이 확인하고 애니메이션·VFX를 독립 관리
- 주요 변경:
  - 공통 이동 Data Asset과 공격 스킬 Data Asset에 독립 VFX 표시 스위치 추가
  - 이동 1개 및 공격 8개 Data Asset의 파티클 표시를 OFF로 저장
  - 이동 발걸음 Timer와 점프·착지·구르기 Niagara 생성 경로 차단
  - 공격 시전·차징·타격 Niagara 생성 경로 차단, 몽타주와 전투 판정은 유지
  - 애니메이션 `/Game/Rogue10m/Animation`, 파티클 `/Game/Rogue10m/VFX` 구조를 별도 카탈로그로 정리
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 구성 스크립트 `RESULT=COMMON_CHARACTER_ANIMATION_CONFIGURED`
  - 자동 검증 `RESULT=PASSED`, `presentation=animation_only motion_vfx=off attack_vfx=off`
  - 애니메이션 상태 5종, 몽타주 5개, 종족 6종, 공격 8종 연결 확인
- 상태: 구현·자산 저장·자동 검증 완료, 파티클 OFF 게임 창 실행 및 정상 응답 확인
- 관련 문서:
  - `Feature/architect/2026-08-15_animation-only-presentation.md`
  - `Feature/doc/2026-08-15_animation-only-presentation.md`
  - `DevLog/20260815.txt`
## Sprint#4-15 - 직업별 통합 하단 전투 HUD (2026-08-19)

- 브랜치: `Sprint#4-15-class-themed-bottom-hud`
- 목표: 마법사, 전사, 권사, 도적의 하단 전투 UI를 하나의 재사용 가능한 Bottom Widget으로 통합
- 주요 변경:
  - 1920×208 직업 프레임 4종과 실제 전투 화면형 시안 4종 제작
  - `URogue10mBottomHUDWidget`, `WBP_BottomHUD` 추가
  - HP, MP/스테미나, 아이덴티티, 스킬 5칸, 아이템 5칸, 레벨/경험치를 Bottom HUD 내부로 이동
  - Main HUD의 개별 하단 자식을 제거하고 `BottomHUDWidget` 한 개만 전체 폭 260px로 배치
  - 무기/아이덴티티 기반 자동 테마 판별과 Blueprint `ThemeOverride` 제공
  - 마법사는 활성 Mana View, 나머지 직업은 스테미나를 같은 자원 위치에 표시
  - 전체 폭 52px 초록색 경험치 바를 화면 최하단에 유지
- 검증:
  - UE 5.8 Editor target 전체 및 증분 빌드 성공
  - 네 Texture2D 1920×208, UI Group, NoMipmaps, NeverStream 통과
  - Bottom HUD 필수 바인딩 14개, 스킬/아이템 각 5칸 통과
  - Main HUD `BottomHUDWidget` 1개, 이전 하단 직접 자식 0개 확인
  - `Saved/ClassBottomHUDValidation.txt`: `RESULT=PASSED`
  - `Scripts/CheckGeneratedChanges.ps1`, `git diff --check`, Python 문법 검사 통과
- 상태: 코드, 직업별 텍스처, Widget Blueprint 조립, Main 통합, 자동 검증, 적용 프리뷰 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_class-themed-bottom-hud.md`
  - `Feature/doc/2026-08-19_class-themed-bottom-hud.md`
  - `DevLog/20260819.txt`


## Sprint#4-16 - 장착 무기 기반 스킬트리 자동 활성화 (2026-08-19)

- 브랜치: `Sprint#4-16-weapon-driven-skill-tree`
- 목표: 인벤토리에서 장착한 무기 타입과 활성 스킬트리·기본 입력 슬롯을 항상 일치시킨다.
- 주요 변경:
  - 무기 교체 시 동일 `WeaponType`의 `URogue10mWeaponSkillProfileDataAsset` 즉시 적용
  - 차징, 콤보, 예약 GAS 공격, 다중 타격 타이머 등 이전 무기 전투 상태 정리
  - 현재 활성 트리 밖 스킬의 입력 슬롯 지정 차단
  - 프로필이 정의되지 않은 입력 슬롯의 과거 공격 fallback 차단
  - 활성 트리 무기 타입과 스킬 소속 여부 Blueprint 조회 API 추가
  - 누락된 Bow 전용 5스킬 프로필, Montage 5개, Niagara 3개 생성 및 캐릭터 데이터 등록
  - 11개 무기 자동 전환 전용 런타임 테스트 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - Data Asset Validator: `RESULT=PASSED profiles=11 attacks=55 montages=55 niagara=33 combos=11 first_person_safe=55`
  - 런타임 전환: `RESULT=WEAPON_SKILL_TREE_PASSED weapons=11 failures=0`
  - 교차 무기 스킬 지정 거부, 기본 슬롯·회피·점프 적용 확인
- 상태: 코드, 활 콘텐츠, 캐릭터 데이터 등록, 자동 전환, 빌드 및 런타임 검증 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_weapon-driven-skill-tree.md`
  - `Feature/doc/2026-08-19_weapon-driven-skill-tree.md`
  - `DevLog/20260819.txt`

## Sprint#4-14 - 직업·무기별 전투 애니메이션 및 1인칭 안전 VFX (2026-08-19)

- 브랜치: `Sprint#4-14-class-combat-animation-vfx`
- 목표: 요청한 10개 전투 스타일에 종족 공통 공격 모션과 1인칭 시야를 가리지 않는 Niagara 효과를 구성
- 주요 변경:
  - 단검, 표창, 쌍단검, 장검, 대검, 쌍검, 방패, 한손검·작은방패, 마법사, 권사 프로필 제작
  - 스타일별 기본 3연계, 특수, 차징을 포함한 Montage 50개와 Attack Skill 50개 구성
  - 공격용 Niagara 30개를 애니메이션 자산과 별도 경로로 관리
  - 공통 Manny 소스와 종족별 AnimBP 연결을 통해 같은 스킬의 종족별 모션 통일
  - 로컬 1인칭 위치·회전·크기·방출시간 보정과 양손 보조 효과 지원
  - 차징 및 일회성 Niagara의 즉시 비활성화·파괴로 효과 누적 방지
  - 민첩/마법/권법 계열 선택형 2단 점프, 전사 계열 1단 점프 설정
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 요청 10종 자산 검증 `RESULT=PASSED profiles=10 attacks=50 montages=50 niagara=30 combos=10 first_person_safe=50 races=6`
  - 1인칭 튜닝 `RESULT=FIRST_PERSON_CLASS_COMBAT_VFX_TUNED styles=10 attacks=50 charge_multiplier=0.12`
  - 실제 게임 실행에서 요청 10종, 30회 연계, 10회 차징, 캡처 10장, 실패 0건
  - 쌍단검·쌍검·권사 최종 캡처 육안 검토에서 중앙 조준 영역 유지
- 상태: 코드, 자산, 종족 공통 연결, 1인칭 VFX 튜닝, 실제 게임 실행 QA 및 문서화 완료
- 관련 문서:
  - `Feature/architect/2026-08-19_class-combat-animation-vfx.md`
  - `Feature/doc/2026-08-19_class-combat-animation-vfx.md`
  - `DevLog/20260819.txt`


## Sprint#4-17 - 권사 무협 수련 스킬트리 (2026-08-19)

- 브랜치: `Sprint#4-17-martial-artist-skill-tree`
- 목표: 권갑 장착 시 활성화되는 무협 기반 권사 전용 스킬트리와 수련·토벌 해금 방식을 구현
- 주요 변경:
  - 입문·초식·절기·심법 4경지, 10개 무공 데이터 구성
  - 실제 무공 사용 횟수와 특정 몬스터 처치 횟수를 PlayerState에 기록
  - 선행 무공과 복합 조건을 만족하면 후속 무공을 연쇄 자동 해금
  - 금강불괴 해금 시 받는 피해 15% 감소, 입력 슬롯 장착은 차단
  - 권갑 프로필은 연환권 1개만 시작 해금하고 나머지 무공은 진행형으로 전환
  - 흑철·고동·옥빛 무협 프레임 Texture2D 제작 및 UMG 적용
  - 좌측 권맥 경지, 중앙 경맥형 분기 트리, 우측 비급 상세 패널과 노드 진행률 구성
  - 실제 게임 UMG 포함 1920×1080 자동 캡처 명령 추가
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 권사 전용 데이터·위젯 검증, 전체 메뉴 검증, Widget Flow 검증 오류 0건
  - 런타임 `RESULT=WEAPON_SKILL_TREE_PASSED weapons=11 failures=0`
  - 권사 후속 무공 9개 연쇄 해금, 금강불괴 15% 감소, 패시브 슬롯 차단 확인
  - `git diff --check` 통과
- 상태: 코드, 데이터 에셋, UI 텍스처, Widget Blueprint, 런타임 진행도, 자동 검증 및 실제 캡처 완료
- 관련 문서:
  - `Feature/architect/2026-08-19-martial-artist-skill-tree.md`
  - `Feature/doc/2026-08-19-martial-artist-skill-tree.md`
  - `DevLog/20260819.txt`

## Sprint#4-18 - 오버워치풍 1인칭 전투 VFX 가독성 개선 (2026-08-26)

- 브랜치: `Sprint#4-18-overwatch-inspired-combat-vfx`
- 목표: 영웅 슈터의 선명한 공격 피드백을 유지하면서 1인칭 조준점과 적 실루엣을 가리지 않는 직업별 Niagara 구성
- 주요 변경:
  - 10종 핵심 공격 50개에 `HeroShooterFP_v1` 스케일·수명·오프셋 프로필 적용
  - Primary 1·2·3, Special, Charged 단계와 무기 무게에 따른 효과 강도 계층화
  - 양손 보조 파티클의 1인칭 최대 배율을 0.70으로 분리
  - 전투 Niagara 30개에 공통 `NET_CombatReadable` Effect Type 배정
  - 실제 권사 진행 스킬 4개에 권사 Montage·Niagara presentation 연결, 피해·타격 수·해금 조건 보존
  - 권사 첫 Niagara 에디터 컴파일을 워밍업 처리하는 실게임 테스트 보완
- 검증:
  - UE 5.8 Editor target 빌드 성공
  - 핵심 자산 `RESULT=PASSED profiles=10 attacks=50 montages=50 niagara=30 first_person_safe=50`
  - 권사 진행 스킬 `RESULT=MARTIAL_ART_READABLE_VFX_PASSED skills=4 montages=4 first_person_safe=4`
  - 실게임 `RESULT=CLASS_COMBAT_RUNTIME_PASSED styles=10 combos=27 charged=9 screenshots=10 failures=0`
  - 캡처 10장 육안 QA에서 중앙 표적 영역 유지, 효과 누적 없음
  - `CheckGeneratedChanges.ps1` Harness 경로 검사 통과
- 상태: 데이터 프로필, UE 자산 저장, 권사 실사용 경로, 빌드, 자동 검증, 실제 게임 및 시각 QA 완료
- 관련 문서:
  - `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx.md`
  - `Feature/architect/2026-08-26_overwatch-inspired-combat-vfx-amendment.md`
  - `Feature/doc/2026-08-26_overwatch-inspired-combat-vfx.md`
  - `DevLog/20260826.txt`

## Sprint#4-19 - 로스트아크풍 HUD 개선 기획 (2026-09-08)

- 브랜치: `Sprint#4-19-lostark-hud-design-plan`
- 목표: 현재 UI의 크기·구조 문제를 소스 기준으로 진단하고 전투 HUD 개선안을 시각화한다.
- 주요 변경:
  - 기획1·구조 검증1·시각 검증1의 독립 조사 및 통합.
  - stretch 배경과 고정 좌표 혼용, 260px 하단 높이, 52px 경험치 영역을 개선 대상으로 기록.
  - 1080p 중앙 1000×160 코어, 행동5칸·현재 잠금 아이템4칸, 경험치4px 기획.
  - 로스트아크풍 생성 콘셉트와 최종 프롬프트, 후속 개발2·검증2 패킷 작성.
- 검증: 소스/문서/생성 그림 대조. 문서 경로·공백 검사를 실행하며 C++ 빌드/실제 PIE는 후속 범위.
- 상태: 기획·콘셉트 완료, 게임 적용 전. 최신 런타임 문제 재현은 미실시.
- 관련 문서:
  - `Feature/architect/2026-09-08_lostark-hud-design-plan.md`
  - `Feature/doc/2026-09-08_lostark-hud-design-plan.md`
  - `ui-concepts/2026-09-08/rogue10m-lostark-hud-concept.png`
  - `DevLog/20260908.txt`
- 최종 문서 검사: git diff --check 및 Harness path check 통과. 정확한 치수 도면 `ui-concepts/2026-09-08/hud-layout-1920x1080.svg` 추가.

## Sprint#4-20 - 화면 비율 대응 전투 HUD 적용 (2026-09-08)

- 브랜치: `Sprint#4-20-responsive-combat-hud`
- 목표: 화면 크기·비율 변화에 대응하는 중앙 전투 HUD를 현재 UE5.8 프로젝트에 적용한다.
- 주요 변경:
  - 공통1000×160 코어, DownOnly 균등 축소, 엔진 DPI 상속, 여백24, 전체폭 XP4.
  - 행동5·아이템4의56 정사각형 슬롯과 직업 문장76 정사각형.
  - 상단 대상 정보, 우상단 타이머, 실제 시스템 로그2줄·획득 알림3줄.
  - 기존 Main/Bottom 두 자산 수정 및 전용 파트6개 생성, 구형 생성기 덮어쓰기 보호.
  - 리뷰 결과에 따라 XP 예시값 잔류와 획득 수량 표시 누락 수정.
- 검증:
  - 설치 UE5.8.2 Editor 타깃 최종 빌드 성공.
  - `RESULT=RESPONSIVE_HUD_SETUP_PASSED`, Editor 자산8개 저장.
  - `RESULT=RESPONSIVE_HUD_PASSED cases=6 failures=0`.
  - 720p·1080p·1440p·3440×1440·800×900/UI125%·1080p 복귀 실제 렌더링 검사.
  - 좁은 창 코어816.6×130.7 및 문장62.1 정사각형 측정, 직업4종·마나/스태미나·타이머·획득 수량 검사.
  - git diff --check 및 Harness 경로 검사 통과.
- 상태: 구현·Editor 적용·빌드·실제 게임 검증 완료. 패키지 빌드 및 신규 스킬 아트는 범위 밖.
- 관련 문서:
  - `Feature/architect/2026-09-08_responsive-combat-hud.md`
  - `Feature/doc/2026-09-08_responsive-combat-hud.md`
  - `Feature/doc/images/responsive-hud-20260908/`
  - `DevLog/20260908.txt`

## Sprint#4-21 - 참조 이미지 기반 금속 전투 HUD (2026-09-09)

- 브랜치: `Sprint#4-21-reference-metal-hud`
- 목표: 첨부 HUD의 검은 금속 프레임·중앙 원형 게이지·아이콘 아래 입력 표기를 실제 게임에 반영한다.
- 주요 변경:
  - 1200×208 코어, 아래56·좌우24, DPI 상속과 균등 축소 유지.
  - 생성 프레임 원본과 Native AtlasImage UV3조각으로 금속 외곽 구성.
  - 실제 자원 원형 게이지, 아이콘 없는 경우 UI 대체 그림, 실제 상태의 쿨다운·잠금 유지.
  - 전용 파트7개와 기존 Main/Bottom2개, 프레임 Texture2D1개 저장.
  - 독립 리뷰의 수평 게이지 중복 및 실제 캡처의 프레임 접합·빈 문장 문제 해결.
- 검증:
  - UE5.8.2 최종 Editor 빌드 성공.
  - `RESULT=REFERENCE_METAL_SETUP_PASSED`
  - `RESULT=REFERENCE_METAL_HUD_PASSED cases=6 failures=0`
  - 720p·1080p·1440p·울트라와이드·좁은 창/UI125%·1080p 복귀 실게임 확인.
  - 독립 최종 시각 QA3장 PASS, Python AST·공백·Harness 경로 검사 통과.
- 상태: 구현·빌드·Editor 적용·실게임 및 시각 검증 완료. 커밋·푸시 없음.
- 관련 문서:
  - `Feature/architect/2026-09-08_reference-metal-hud.md`
  - `Feature/doc/2026-09-09_reference-metal-hud.md`
  - `Feature/doc/images/reference-metal-hud-20260909/`
  - `DevLog/20260909.txt`


## Sprint#4-22 - 레퍼런스형 1인칭 주먹과 주변 HUD (2026-09-09)

- 브랜치: `Sprint#4-22-first-person-fist-presentation`
- 목표: 양손 가드·전방 펀치·복귀와 안정된 시야, 가장자리 경험치 및 상단 대상 정보를 실제 게임에 적용한다.
- 주요 변경: 전용 프레젠테이션 컴포넌트·네이티브 FistAnimInstance·DefaultSlot 기반 양팔 IK·손목 보정·타 무기 복원. XP 좌우24/아래16/높이10과 상단 대상560×68/위24. Editor Widget4개 적용.
- 검증: UE5.8.2 빌드 성공63.03초, 실제 공격3회/120프레임 실패0, HUD7조건 실패0, 독립 소스·시각 QA 및 Python/공백/Harness 검사 통과.
- 상태: 구현·Editor 적용·실게임 및 시각 검증 완료. 현재 Manny/테스트 맵으로 실제5초 GIF 보관. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-09_first-person-fist-presentation.md`, `Feature/doc/2026-09-09_first-person-fist-presentation.md`, `Feature/doc/images/first-person-fist-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-23 - 양손 방향과 근접 주먹 자세 개선 (2026-09-09)

- 브랜치: `Sprint#4-23-quick-melee-hand-orientation`
- 목표: 양손이 돌아가 보이는 문제를 수정하고 오버워치 quick melee의 짧고 선명한 자세 원칙을 참고한다.
- 주요 변경: 고정 180도 회전 제거, 실제 골격 기준 손·전완 정렬, 가드 15/타격 35도, 손가락 굴곡 100/100/75도와 엄지 배치, 포즈 캐시 및 관련 런타임 검증 확장.
- 검증: UE 5.8.2 빌드 성공 40.82초, 실제 공격 3회/120프레임 실패 0, 시선 ±25도·울트라와이드 3조건, 무기 복원, 독립 소스·시각 6장 검토, GIF 및 공백·Harness 검사 통과.
- 상태: 구현·실게임 및 독립 검증 완료. 공식 글의 원칙을 참고했으며 외부 클립 직접 재생은 도구 ACL 오류로 미확인. 오른 엄지의 작은 시각 비대칭 잔여. 기존 게임플레이·카메라·HUD 자산 변경 및 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-09_quick-melee-hand-orientation.md`, `Feature/doc/2026-09-09_quick-melee-hand-orientation.md`, `Feature/doc/images/quick-melee-20260909/`, `Feature/doc/evidence/quick-melee-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-24 - 몸통 기반 1인칭 근접 공격과 카메라 동조 (2026-09-09)

- 브랜치: `Sprint#4-24-body-driven-first-person-melee`
- 목표: 전신 몽타주의 체중 이동·휘두르기를 팔과 표시 카메라에 전달한다.
- 주요 변경: 골반·척추·쇄골 CS 보정 후 팔 IK, signed 3D 손 궤적과 반대손 가드, 안전한 애니메이션 snapshot→최종 POV 동조, 강도·범위·보간 설정, 비활성·무기·사망·시점 전환 초기화.
- 검증: UE 5.8.2 통합 빌드 25.95초/최종 증분 8.89초 성공, 실제 120프레임 실패 0, 시야 3조건, 추가 공격의 활성 강도 0·무기 전환·사망 상태 검사, 독립 소스 및 시각 21장 검토 통과.
- 상태: 구현·실게임·독립 검증 완료. 영상 직접 재생은 도구 오류로 미확인이고 Mixamo 파일 미적용. 전신 전체 노출 없이 어깨·팔과 POV로 체중 이동을 전달한다. 타격·HUD·바이너리 자산 및 커밋·푸시 변경 없음.
- 관련 문서: `Feature/architect/2026-09-09_body-driven-first-person-melee.md`, `Feature/doc/2026-09-09_body-driven-first-person-melee.md`, `Feature/doc/images/body-melee-20260909/`, `Feature/doc/evidence/body-melee-20260909/`, `DevLog/20260909.txt`.

## Sprint#4-25 - 기본 직업 좌우 연타·차징 어퍼컷·점프 (2026-09-10)

- 브랜치: `Sprint#4-25-basic-brawler-inputs`
- 목표: 기본 Unarmed의 왼손→오른손 연계, 오른손 차징 어퍼컷, 실제 머리/카메라 동조와 일반 점프를 구현한다.
- 주요 변경: 전용 입력 상태 컴포넌트, 기본 공격 Data Asset 4개, 프로필 연결, 본 기반 FP 포즈와 점프 표현, 취소/쿨다운 보호.
- 검증: UE 5.8.2 최종 빌드 8.57초, Editor 자산 적용 통과, 기본 직업 480프레임 실패 0, 기존 너클 120프레임/시야 3조건 실패 0. 독립 소스·시각 검토와 10초 GIF/자산 해시/경로 검사 통과.
- 상태: 구현·통합·회귀·독립 시각 검증 완료. 원본 YouTube 영상 재생·직접 비교는 반복된 접근 오류로 미완료이며 영상 파일 제공 또는 도구 복구 대기(blocked). 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-10_basic-brawler-inputs.md`, `Feature/doc/2026-09-10_basic-brawler-inputs.md`, `Feature/doc/evidence/basic-brawler-20260910/`, `DevLog/20260910.txt`.
## Sprint#4-26 - 자연스러운 기본 격투가 모션 (2026-09-10)

- 브랜치: `Sprint#4-26-natural-brawler-motion`
- 목표: 준비·타격·회수의 리듬과 몸·머리·카메라 연결을 자연스럽게 다듬는다.
- 주요 변경: 현재 위치에서 연타 연결, 빠른 신전과 곡선 회수, 가드/반대손 비대칭, 상체 선행·머리 후행, 기본 Unarmed 카메라 임계감쇠, 두 어퍼컷 rate1.5와 실제 타격 약0.422초, 30fps 캡처/카메라 경계검사.
- 검증: UE5.8.2 빌드40.23초, Editor 자산 적용, 기본480프레임/너클120프레임·시야3조건 실패0, 독립 소스·시각 검증, 실제10초 GIF 검증 통과.
- 상태: 구현·실게임 검증 완료. 원본 유튜브 직접 재생은 컴퓨터 플러그인 초기화 ACL 오류로 미완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-10_natural-brawler-motion.md`, `Feature/doc/2026-09-10_natural-brawler-motion.md`, `Feature/doc/evidence/natural-brawler-20260910/`, `DevLog/20260910.txt`.

## Sprint#4-27 - 팔만 보이는 1인칭 격투 시야 (2026-09-12)

- 브랜치: `Sprint#4-27-arms-only-first-person`
- 목표: 아래를 볼 때 몸통·하체가 보이지 않도록 팔만 표시하고 기존 격투 동작과 머리 기반 카메라 동조를 유지한다.
- 주요 변경: Editor에서 생성한 팔 전용 메시, 전체 뼈89·재질2 유지와3LOD 검증, 메시·재질·애니메이션·숨김 본·렌더 설정 복원, 실제 최소피치와 -89도 스트레스 검증. 회귀 테스트의 외부 시야 입력 분리.
- 검증: UE5.8.2 통합빌드118.35초/최종20.00초 성공, 자산 저장 및 원본해시보존, 팔전용390·기본480·너클120프레임/시야3조건 실패0. 독립 소스·시각14장 검증, 공백·Harness 경로 검사 통과.
- 상태: 팔 전용 구현·Editor 적용·실게임 회귀 완료. 고릴라 암즈 비교 방향은 기록했으며 원본 영상 직접 비교는 computer-use 초기화 오류로 미완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-12_arms-only-first-person.md`, `Feature/doc/2026-09-12_arms-only-first-person.md`, `Feature/doc/evidence/arms-only-20260912/`, `Feature/doc/images/arms-only-20260912/`, `DevLog/20260912.txt`.

### Sprint#4-27 참고 자료 보완 — 사용자 녹화본 확인 (2026-09-12)

- 사용자 제공 갈브레나 1인칭 녹화본을 로컬에서 직접 분석해 브라우저 접근 실패로 남았던 해당 자료의 시각 확인을 보완했다.
- 공격별 팔 진입·회수와 연계 자세, 강공격·착지 카메라 반응, 명중 표현의 비교 방향을 기록했다.
- 검증: 전체 흐름 대표 프레임 및 공격/이동 구간8fps 대조, 서브에이전트 독립 확인. 오디오와 엔진 내부 카메라 원인은 미확인.
- 상태: 해당 녹화본 비교 완료. 신규 게임 기능 구현은 없음. 고릴라 암즈 등 다른 원본 영상 비교는 별도 미완료.
- 관련 문서: `Feature/doc/2026-09-12_recorded-galbreena-reference.md`, `Feature/doc/evidence/recorded-reference-20260912/`, `DevLog/20260912.txt`.

### Sprint#4-27 참고 자료 보완 — 단계별 타격감 영상 (2026-09-12)

- 목표: 두 번째 녹화본의 공격 효과·적 반응·카메라·UI·락온 설명을 현재 격투와 대조한다.
- 주요 기록: 기존 명중 VFX/피해 숫자는 재사용하고, 적 반응·명중 전용 카메라 충격·접촉 정보·체력 감소 강조를 같은 명중에 맞추는 설계 제안. 기존 팔 궤적 참고와 연결.
- 검증: 주 담당의 실제 프레임/자막 관찰, 서브에이전트의 읽기 전용 소스 대조, 문서 공백 검사. 오디오 및 실제 Data Asset VFX 활성 여부는 미확인.
- 상태: 참고 분석 완료. 신규 게임 효과 구현은 없음.
- 관련 문서: `Feature/doc/2026-09-12_impact-animation-reference.md`, `Feature/doc/evidence/impact-reference-20260912/`, `DevLog/20260912.txt`.

## Sprint#4-28 - 레퍼런스 기반 기본 복싱 4동작 (2026-09-12)

- 브랜치: `Sprint#4-28-reference-boxing-combos`
- 목표: 사용자 녹화 분석을 토대로 좌클릭 왼잽→오른잽, 우클릭 짧게 스트레이트/0.85초 모아 놓으면 오른훅을 실제 게임에 구현한다.
- 주요 변경: 네 절차형 동작과 준비·회수 연결, 충전 해제 팔꿈치 연결, 스트레이트 상체 전진, 훅 반대가드 분리. 전용4스킬·프로필 Editor 적용. 확정 명중에서 적 메시 움찔·플래시 및 실행당1회 카메라 충격, 기존 Niagara 재사용. 시작 무기 소유권과 팔 전용 시야 유지.
- 검증: UE5.8.2 최종빌드20.20초 성공; 기본480·명중280·팔시야390·너클120프레임/시야3조건 실패0. 독립 소스·시각 검토 통과. 실제 손 도달 대비 피해 최대2프레임 차이(30fps), 골격 길이 보존. 실제게임10초MP4와 증거 보관, diff/하네스 경로 검사 통과.
- 상태: 구현·Editor 적용·실게임 검증 완료. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-12_reference-boxing-combos.md`, `Feature/doc/2026-09-12_reference-boxing-combos.md`, `Feature/doc/evidence/boxing-combos-20260912/`, `Feature/doc/images/boxing-combos-20260912/`, `DevLog/20260912.txt`.

## Sprint#4-29 - Obsidian 소스 전용 그래프 (2026-09-22)

- 브랜치: Sprint#4-29-obsidian-source-graph
- 목표: DevLog·README를 제외하고 .h/.cpp 파일 사이의 내부 include 관계를 탐색한다.
- 주요 변경: 소스 노트 경로 필터·설정 백업, 실행기 통합, 역방향·대시보드 링크 제거, 소스 121개/관계 292개 재생성, 유출 링크 검증.
- 검증: 생성·링크·지표 검사, PowerShell 4개 구문 및 그래프 설정 보존 검사 통과. Unreal 변경이 없어 엔진 빌드 생략.
- 상태: 구현·파일 검증 완료. 실행 중인 Obsidian 화면 반영 미확인, 필요 시 그래프 검색창 필터 수동 입력. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — Feature 문서 제외
- 목표·변경: 소스 전용 그래프 검색에 Feature 경로 제외를 명시하고 생성기·실행 설정·사용 안내를 일치시켰다.
- 검증·상태: 필터 저장 및 소스 121개/관계 292개 생성·검증 통과. 현재 앱 화면 반영 미확인.
- 관련 문서: Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — 기능 노드와 관련 소스 연결
- 목표: Feature 작업 문서를 숨기면서 전투·인벤토리·스킬 등 기능별 관련 소스는 표시한다.
- 주요 변경: 기능 노드 10개, 기능-소스 연결 164개, 검토 가능한 JSON 분류 규칙과 생성기, Files/Functions 결합 필터 및 금색 기능 그룹.
- 검증: 소스 121개/include 292개 보존, 기능 매핑·링크·연결 수 검사, 미분류 0개, PowerShell 5개 구문과 그래프 설정 검사 통과.
- 상태: 구현·파일 검증 완료. 현재 앱 화면 반영 미확인. 기능 연결은 호출 관계가 아닌 탐색용 분류다. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

### Sprint#4-29 보완 — Feature 본문 근거 연결
- 목표: 기존 Feature 설계·결과 본문을 참고하여 기능 노드와 소스를 연결한다.
- 주요 변경: 파일명 패턴 분류를 문서 주제·본문 참조 해석으로 교체, 클래스 정의와 축약명/파일 쌍 해석, 문서 경로·행별 근거 기록, 기능 10개/연결 397개 재생성.
- 검증: 170개 문서 검사·157개 주제 문서 사용, 모든 연결의 실제 근거 확인, 소스 121개/include 292개 보존, 참조 해석 회귀 검사 통과.
- 상태: 구현·생성 검증 완료. 현재 런타임 호출이 아닌 설계/과거 결과를 포함한 문서 연관도. 실행 중 앱 화면 반영은 미확인. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-09-22_obsidian-source-graph.md, Feature/doc/2026-09-22_obsidian-source-graph.md, DevLog/20260922.txt.

## Sprint#4-30 - Martelo 원본 동작 미리보기 (2026-09-22)

- 브랜치:`Sprint#4-30-martelo-motion-preview`
- 목표:사용자 FBX의 실제 동작과 현재 팔 전용 1인칭 적용 시 차이를 확인한다.
- 주요 변경:SDK 기반65뼈/40샘플 분석,사선·측면 골격 GIF/MP4,현행 절차형 포즈와 리타깃 필요성 비교 문서.
- 검증:원본 Import/평가 성공,왼발 주도 발차기 수치·대표6프레임 직접 확인,서브에이전트 독립 코드·수치 검토.
- 상태:원본 동작 분석·미리보기 완료. 실게임 리타깃/공격 연결은 수행하지 않음. 런타임/Content 변경과 UE 빌드·커밋·푸시 없음.
- 문서:`Feature/architect/2026-09-22_martelo-motion-preview.md`,`Feature/doc/2026-09-22_martelo-motion-preview.md`,`DevLog/20260922.txt`.


## Sprint#4-31 - Martelo 실제 게임 1인칭 미리보기 (2026-09-22)

- 브랜치: `Sprint#4-31-martelo-first-person-preview`
- 목표: 사용자 FBX를 실제 플레이어의 팔 전용 메시에서 재생한 게임 화면을 제공한다.
- 주요 변경: Mixamo→Manny 프리뷰 자산, 재사용 가능한 Editor 생성·포즈 검증, 독립 게임 촬영 명령, 원래 속도·0.5배속 MP4.
- 검증: UE 5.8.2 빌드 성공, 소스 40시점×65뼈 위치 보존, 새 Editor 자산 재사용 성공, 최종 게임 209프레임·실패 0, 공용 자산 네 파일 해시 보존, 소스·화면 검토 및 변경 경로 검사 통과.
- 상태: 실제 적용 미리보기 완료. 고정 카메라·블렌딩 없는 전환이며 팔 전용 설정상 발차기는 보이지 않는다. 기존 공격 입력·피해 판정에 연결하지 않았다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-22_martelo-first-person-preview.md`, `Feature/doc/2026-09-22_martelo-first-person-preview.md`, `DevLog/20260922.txt`.


## Sprint#4-32 - 팔·몸통·다리가 보이는 전신 1인칭 (2026-09-22)

- 브랜치: `Sprint#4-32-full-body-first-person`
- 목표: 팔 전용 캐릭터를 몸통·다리까지 보이는 1인칭으로 변경하고 실제 게임 화면을 제공한다.
- 주요 변경: 전체 Manny/머리 가림, 캡슐 기준 전신 부착, 일반 월드 투영으로 재질 가림 우회, 전신 아래보기 -85도, 카메라·손 목표 및 맨손 전용 가드 보정, 팔 전용 옵션 유지.
- 검증: UE 5.8.2 빌드 22.86초 성공. 기본 격투 480·너클 120·팔 전용 390·전신 시연 269프레임 모두 실패 0. 너클 상하/울트라와이드 추가 3시점과 실제 화면 검토. 공용 자산 네 파일 보존 및 변경 경로 검사 통과.
- 상태: 구현·통합 검증 완료. 실제 전신 아래보기와 Martelo 원래 속도/0.5배속 촬영. Martelo 입력 매핑·피해 및 보행/발 접지 애니메이션은 추가하지 않음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_full-body-first-person.md`, `Feature/doc/2026-09-22_full-body-first-person.md`, `DevLog/20260922.txt`.


## Sprint#4-33 - 머리 움직임을 따르는 전신 1인칭 카메라 (2026-09-22)

- 브랜치: `Sprint#4-33-head-attached-camera`
- 목표: 달리기·점프·공격 시 머리에 붙은 듯 움직이고 실제 머리 회전을 따라 후방도 보는 카메라.
- 주요 변경: 숨김 이전 native/SingleNode 머리 포즈 추종, 눈 위치 회전 보정, 실제 이동 기반 상체 흔들림, 연속 yaw/반강도 각도 경계, 입력·팔 전용 모드 보존.
- 검증: UE 5.8.2 빌드 27.01초 성공. 후방 회전 1,446샘플, 기본480·너클120/시야3·팔390·최종 시연419프레임 실패 0. 독립 소스/시각 검토, 공용 자산 및 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 약14초 실제 영상 제공. Martelo는 원본 머리 yaw 약39도이며 180도 후방은 별도 임시 포즈 검증. 발 보행·접지와 공격 입력 매핑 추가 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_head-attached-camera.md`, `Feature/doc/2026-09-22_head-attached-camera.md`, `DevLog/20260922.txt`.


## Sprint#4-34 - Boxing.fbx 기반 권사 자세와 공격 (2026-09-22)

- 브랜치: `Sprint#4-34-boxing-reference-stances`
- 목표: 실제 복싱 동작과 여러 격투 자세를 참고하여 권사 가드·전신 공격을 개선한다.
- 주요 변경: 활성 스택 왼발 전진/왼손 직선 분석 도구, 기본 복싱·롱가드·골반 회전형 프리셋, 반대손 보호·팔꿈치 모음·골반 참여·지지발 보정과 이동/공중 보간, 비교 촬영.
- 검증: UE5.8.2 빌드30.86초 성공. 세 자세 각480·너클120/시야3·팔전용390프레임 실패0. 실제 손 정점/피해 차이 최대1프레임. 독립 소스/시각 검토, 원본과 공유자산4파일 보존, 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 세 자세 각각6.67초 실게임 영상 및 원본 골격 영상. 기존 C++ 절차형 공격 보완이며 FBX 직접리타깃·선택UI·지형IK 추가 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_boxing-reference-stances.md`, `Feature/doc/2026-09-22_boxing-reference-stances.md`, `DevLog/20260922.txt`.

## Sprint#4-35 - Boxing 원본 직접 적용과 머리 카메라 미리보기 (2026-09-22)

- 브랜치: `Sprint#4-35-boxing-direct-head-preview`
- 목표: Boxing.fbx 원본을 플레이어 체형에 적용해 머리 카메라로 보는 실제 결과를 제공한다.
- 주요 변경: 격리된 Mixamo→Manny 리타깃 자산·검증 스크립트, 원본 프레임0 눈 위치와 감쇠 없는 머리 추종 촬영 fixture, 정면/아래40도·UI제외 비교 영상.
- 검증: 최종 UE5.8.2 빌드7.86초 성공. 정면278·최종보조278프레임 실패0. 머리추종 오차0cm/0.000002도·제한도달0, 원본 타격13프레임 일치·손/골반/발 궤적보존. 독립 소스/시각검토, 원본/공유자산4파일 보존 및 경로·diff 검사 통과.
- 상태: 직접 적용 미리보기 완료. 낮은 원본 가드로 정면 손 가독성은 낮고 보조 시점에서 동작 확인 가능. 기존 공격 입력/피해·일반 카메라 기본값 변경 없음. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_boxing-direct-head-preview.md`, `Feature/doc/2026-09-22_boxing-direct-head-preview.md`, `DevLog/20260922.txt`.

## Sprint#4-36 - 일반 Idle과 복싱·권사 공격 연결 (2026-09-22)

- 브랜치: `Sprint#4-36-relaxed-idle-martial-combat`
- 목표: 손을 내린 평상시 Idle과 복싱·권사 공격을 자연스럽게 연결한다.
- 주요 변경: 전신 맨손 전용 Idle/전투 가중치, 손·팔꿈치 경로 IK와 손목·손가락 보간, 최초 타격/콤보 준비 충돌 방지, 대기/복귀/재입력 검증과15초 실제 게임 영상.
- 검증: UE5.8.2 빌드11.69초 성공. 시연453·재입력513·기존격투480·너클120/시야3·팔전용390프레임 실패0. 손 이동최대16.937cm·팔 길이오차0, 피해오차1프레임이내. 독립 소스/시각 검토, 공용 자산4파일 보존 및 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 새 Mixamo 미리보기는 브라우저 초기화 오류로 직접 재생 불가하여 기존 Boxing.fbx 분석을 참고했으며 신규 에셋 리타깃은 하지 않았다. 기존 입력/피해/카메라 기본값 유지. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_relaxed-idle-martial-combat.md`, `Feature/doc/2026-09-22_relaxed-idle-martial-combat.md`, `DevLog/20260922.txt`.


## Sprint#4-37 - 머리 카메라 기본 시선 30도 조정 (2026-09-22)

- 브랜치: `Sprint#4-37-forward-head-framing`
- 목표: 선호한40도 구도에서 시선을10도 올리고 실제 시작 시선과 자유 입력을 함께 유지한다.
- 주요 변경: 로컬 전신 수평 스폰 최초 -30도 적용, 캐릭터별 처리 상태, 입력·무기 전환 유지 검사, 원본40/30도 비교 영상.
- 검증: UE5.8.2 최종 빌드11.22초 성공. 초기90·원본촬영278프레임 및 머리 회귀1,446합성샘플 실패0. 독립 소스/시각 검토, 공용자산4파일 및 생성 경로·diff 검사 통과.
- 상태: 구현·통합 검증 완료. 실제 시작 방향 변경이며 고정 하향 카메라가 아니다. 비교 원본의 눈 위치 보정은 촬영용이며 게임HUD·애니메이션·FOV는 유지. 커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-22_forward-head-framing.md`, `Feature/doc/2026-09-22_forward-head-framing.md`, `DevLog/20260922.txt`.


## Sprint#4-38 - Boxing 기본 공격 연결 착수 (2026-09-23, 09-26 정리)

- 브랜치: `Sprint#4-38-boxing-basic-attack`
- 목표: Boxing 미리보기 동작을 기본 공격에 연결한다.
- 주요 변경: 기존 First Person Mesh 경로의 원본 동작 연결 및 production 원본 클립 준비.
- 검증: 당시 Editor 빌드 완료. 실행 검증은 사용량/도구 제약으로 완료하지 못했다.
- 상태: 후속 사용자 요청에 따라 표시 경로를 Sprint#4-39의 Appearance 기반 실제 몽타주 방식으로 대체. 이 항목 자체의 런타임 검증 완료로 기록하지 않는다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-23_boxing-basic-attack.md`, `Feature/doc/2026-09-26_appearance-head-camera.md`.

## Sprint#4-39 - Appearance Mesh 머리 카메라 (2026-09-26)

- 브랜치: `Sprint#4-39-appearance-head-camera`
- 목표: 실제 외형의 머리에 카메라를 두고 원본 전신 모션을 본다.
- 주요 변경: Appearance raw head pose 카메라, 기존 Source/Retarget ABP 유지, First Person Mesh 표시·평가 비활성화, 실제 좌우 Boxing 몽타주 및 좌클릭 연결, 실제 시야 기준 공격·외형 기준 이펙트.
- 검증: UE5.8.2 최종 빌드9.40초 성공. 애셋 제작·저장, 게임570프레임·cache570샘플 실패0. 위치0cm/회전0.00000382도 오차. 공유애셋7개 보존, 소스/시각 검토·생성경로·diff 검사 완료. 실제19초 영상.
- 상태: 지정 범위 QA 완료. 원본 스트레이트 머리 회전으로 정면 표적을 빗맞히는 동작은 독립 collision 질의로 확인했으며 그대로 유지한다. 최초 모든공격명중 가정의 실패로그 보존. 큰 시야 회전·팔 가림이 있고 발 전체 가독성은 제한. 생명주기/외형교체/멀티플레이 개별실행 검증 제외. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-26_appearance-head-camera.md`, `Feature/doc/2026-09-26_appearance-head-camera.md`, `DevLog/20260926.txt`.


## Sprint#4-40 - V키 3인칭 전신 확인 (2026-09-28)

- 브랜치: `Sprint#4-40-third-person-inspection`
- 목표: 현재 외형과 모션을 V키로 3인칭에서 확인하고 다시1인칭으로 복귀한다.
- 주요 변경: 최종화면POV 관찰전환,300cm거리·캡슐중심0cm높이,12cm벽스윕,머리/외형파츠 가림복원,UI입력차단. 실제머리카메라·공격기준과FP비활성 유지.
- 검증: 최종UE5.8빌드12.43초 성공. 실제V입력왕복3회,280프레임실패0. 1인칭135/3인칭145샘플,머리위치0cm/회전0.00000418도오차,좌우잽각12재생샘플,벽10샘플300→128cm와복귀,UI차단검사. 독립소스/시각검토·경로·diff검사완료.9.33초실행영상.
- 상태: 지정범위QA완료. 공격중정확한토글순간/사망/외형교체/재빙의/네트워크 개별실행 제외. 최초몽타주회복구간검사오류와물리마우스간섭실패로그보존. 관찰시점과공격조준은시차가있다. 커밋·푸시없음.
- 관련 문서: `Feature/architect/2026-09-28_third-person-inspection.md`, `Feature/doc/2026-09-28_third-person-inspection.md`, `DevLog/20260928.txt`.


## Sprint#4-41 - 미사용 임시 파일 정리 (2026-09-28)

- 브랜치: `Sprint#4-41-unused-file-cleanup`
- 목표: 현재 참조와 복구 가능성을 확인하여 불필요한 파일을 정리한다.
- 주요 변경: 재생성 PNG2,781개와 중복본50개, 총2,831개/약2.55GB 삭제. 검증된 출력 폴더10개의 PNG Git 제외 규칙, 읽기 전용 감사 도구와 삭제·복구 매니페스트 추가.
- 검증: 원본·스크립트로 전 프레임 RGB 재생성 일치, 최종 영상 스트림·프레임 수 확인. 보존 의존성2,858개 전후SHA256일치, 삭제 영수증과 대상 부재 확인. 경로/diff검사. 게임 코드·애셋 변경 없어 UE 재빌드 생략.
- 상태: 정리 완료. 참조가 남은 기존FP코드·콘솔도구·원본·애셋·백업과 대응 최종 영상이 없는278프레임 보존. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_unused-file-cleanup.md`, `Feature/doc/2026-09-28_unused-file-cleanup.md`, `DevLog/20260928.txt`.

## Sprint#4-42 - 캐릭터 애니메이션 왜곡 안정화 (2026-09-28)

- 브랜치: `Sprint#4-42-animation-stability`
- 목표: 몸 흔들림과 손발 변형의 최초 발생 단계를 확인하고 기술적 왜곡을 수정한다.
- 주요 변경:6종 실제root 분리·추가IK 비활성·보조체인 오매핑 해제, Source FootIK→공격Slot 순서 교정, 오른잽 하체 보존·가슴CS 회전·손IK 동기화, 전용 훅 진입blend0.10초. 생성기 재발 방지와 단계별 감사·runtime 기록 도구 추가.
- 검증: UE5.8 빌드8.40초, 통합450프레임/실패0, V키280프레임/실패0. 연결길이비1.000 복원, 왼잽 오른발step150.812→17.183도, 원본→Source 발회전 오차179.214→0.0001도 미만. 걷기187.74cm·점프89.99cm·착지 확인. 독립리뷰2건, 공유원본SHA 보존, AST/diff/Harness경로검사,15초 실행영상 검증.
- 상태: 확인된 기술적 왜곡 수정 및 지정범위 QA완료. 원본 훅의 빠른 큰 회전은 유지. Human Male 실제검증, 다른5종 개별영상·경사지 접지·네트워크·전체무기/회피 회귀 제외. 카메라 감쇠·입력/피해/차징 변경 없음. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_animation-stability.md`, `Feature/doc/2026-09-28_animation-stability.md`, `DevLog/20260928.txt`.


## Sprint#4-43 - 머리를 포함한 전신 그림자 복원 (2026-09-28)

- 시작 브랜치: `Sprint#4-43-full-body-shadow`. 공유 체크아웃의 후속 작업 브랜치 전환은 보존.
- 목표: Appearance 머리 카메라에서 머리를 가리면서 전신 그림자는 온전히 유지한다.
- 주요 변경: raw pose 기반 shadow-only Poseable Mesh, LeaderPose 얼굴·머리카락 동기화, 원본 CastShadow 보존, V/비활성 복원과 오래된 proxy 정리. 별도 FP 메시 비활성 유지.
- 검증: UE5.8 빌드7.28초 성공, 그림자390프레임/실패0·포즈330샘플/위치오차0cm·공격91샘플/4종, 동일자세 전후 머리 실루엣 확인, V 회귀280프레임/실패0. 독립 소스/런타임 검토, diff·Harness 경로 검사.
- 상태: 현재 Human Male/VSM 범위 완료. RT 그림자·반사, 전 외형·파츠 교체의 개별 실행, GPU 성능 측정 제외. 이번 애셋 편집·커밋·푸시 없음.
- 관련: `Feature/architect/2026-09-28_full-body-shadow.md`, `Feature/doc/2026-09-28_full-body-shadow.md`, `DevLog/20260928.txt`.

## Sprint#4-44 - Rogue10m 개인 스튜디오 (2026-09-28)

- 브랜치: `Sprint#4-44-personal-studio`
- 목표: 개발 일정과 현재 상황, 개선·수정 내역을 한곳에서 확인하고 관리한다.
- 주요 변경: 실제 Sprint/DevLog/Feature 연결, 현재 브랜치 현황, 주간 일정, 작업 추가·수정·삭제 및 로컬 파일 저장, 검색·원문 조회·일정 백업, 실행기와 반응형 한국어 대시보드.
- 검증: Node 구문·통합 테스트 4건, Edge 실제 UI 5개 뷰와 일정 CRUD/재접속/백업/검색/원문 열기, 390px 모바일 및 200% 글자 확대, 브라우저 오류 0건. 실제 서버 HTTP 200과 기록 연결 확인. 게임 코드·에셋 변경 없어 UE 빌드 생략.
- 상태: 로컬 구현 및 지정 범위 QA 완료. 다른 PC 접속·외부 호스팅은 미제공. 문서의 과거 검증과 현재 실행 상태를 구분한다. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_personal-studio.md`, `Feature/doc/2026-09-28_personal-studio.md`, `Studio/README.md`, `DevLog/20260928.txt`.

## Sprint#4-45 - 스튜디오 일정 검토·실행 연결 (2026-09-28)

- 브랜치: `Sprint#4-45-studio-task-execution`
- 목표: 등록된 개발 일정을 검토하고 적절한 순서로 실제 작업한 뒤 결과를 스튜디오에 반영한다.
- 주요 변경: 현재 채팅 5분 heartbeat, 시작 시각·선행 작업·우선순위 대기열, 단일 실행 claim과 파일 잠금, 진행 로그·산출물·검증 결과, 일시정지·중단·재검토, 실행 중 편집 차단, 15초 화면 갱신.
- 검증: Node 9개 검사, 독립 프로세스 중복 claim 차단, 기존5뷰 회귀 및 새 실행 UI 경로·문서 열기·390px 모바일 통과. 예약 ACTIVE 확인. 기존 등록 작업을 실제 조사·기획·검증·기록 경로로 처리.
- 상태: 실행 연결 및 지정 범위 QA 완료. PC/Codex 앱 구동 필요. 중단은 안전한 작업 경계에서 반영하며 커밋·푸시·배포는 자동 수행하지 않는다.
- 관련 문서: `Feature/architect/2026-09-28_studio-task-execution.md`, `Feature/doc/2026-09-28_studio-task-execution.md`, `Studio/EXECUTION.md`, `Studio/README.md`.

## Sprint#4-46 - 무기별 애니메이션 기획 작성 (2026-09-28)

- 브랜치: `Sprint#4-46-weapon-animation-plan`
- 목표: 스튜디오에 등록된 무기별 애니메이션 기획 작업을 실제로 실행한다.
- 주요 변경: 무기12유형의 대기·기본·특수·차징 동작, 공용/총기 대체 모션 교체 요구, 최신 Appearance 카메라·리타깃·그림자 계약, 데이터 연결, 제작 패킷과 QA 기준 문서.
- 검증: 실제 enum12개와 기획12행 일치, 근거 경로7개 존재, 생성기11종/5슬롯 및 맨손 전용 입력 대조, 기존 구현과 신규 제안 구분 확인. 문서 작업이므로 UE 빌드 생략.
- 상태: 기획 작성 완료. 신규 애니메이션·게임 코드·에셋 적용 및 전무기 실게임 QA는 수행하지 않음. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_weapon-animation-plan.md`, `Feature/doc/2026-09-28_weapon-animation-plan.md`, `DevLog/20260928.txt`.

## Sprint#4-47 - 스튜디오 즉시 에이전트 작업 흐름 (2026-09-28)

- 브랜치: `Sprint#4-47-studio-agent-workflow`
- 목표: 예정 등록을 계기로 AI가 역할을 배분하고 진행 중·검증 대기·완료를 관리한다.
- 주요 변경: 기존 heartbeat 중지, HTTP 저장 즉시 dispatcher 실행, 기획·최대2개 독립 구현·빌드·독립 리뷰·문서·최종 리뷰, 실제 Codex CLI, 상태/역할 UI, 중단 자식 종료 대기, 실행 잠금과 실패 복구, 완료 근거 강화.
- 검증: Node19건, Edge5개화면과 작업 흐름/모바일/글자확대 통과. 실제 격리 CLI는 Windows deny-read ACL 오류로 blocked·일시정지·완료 미진입 확인. 구문/diff 검사. 게임 변경 없어 UE 빌드 생략.
- 상태: 구현 및 모의 통합 QA 완료. 실제 무인 성공 실행은 환경 오류로 미검증. 사용자가 오류 시 확인 필요로 중지를 선택하여 자동 승인 확장 미적용, 라이브 엔진 확인 필요·일시정지 유지. 기존 완료 일정 보존. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-28_studio-agent-workflow.md`, `Feature/doc/2026-09-28_studio-agent-workflow.md`, `Studio/EXECUTION.md`, `DevLog/20260928.txt`.

### Sprint#4-47 보완 — 샌드박스 복구 (2026-09-29)

- 목표: 실제 에이전트의 읽기 명령을 막던 Windows 샌드박스 오류를 복구한다.
- 변경: 사용자 승인으로 22바이트 NUL 손상 권한 상태를 백업 보존하고 정상 샌드박스 초기화로 재생성. 승인 정책과 오류 시 중지 유지. 엔진 안내를 복구 완료로 변경.
- 검증: 일반 샌드박스 파일 읽기 및 실제 Studio 실행기 read-only 에이전트 성공, 종료 코드0. 유효한 상태 JSON과 백업 해시 일치 확인.
- 상태: 샌드박스 복구 완료. 기존 작업의 확인 필요 기록·전체 일시정지는 유지하며 실제 개발 작업 재실행 및 전체 완료 검증은 이번 범위에 포함하지 않았다.
- 관련 문서: `DevLog/20260929.txt`, `Feature/doc/2026-09-28_studio-agent-workflow.md`.


## Sprint4 종료 — 로컬 main 반영 (2026-09-29)

- 사용자 main 저장 요청에 따라 누적 작업1,332개 파일을 `8c9bb8a`로 저장했다.
- 목표: UI·전투·애니메이션·Appearance 카메라·그림자·Studio 및 개발 증거를 복구 가능한 상태로 보존한다.
- 검증: UE5.8 Editor 빌드 성공, Studio 루트 실행19개 테스트와 구문 검사 통과, staged diff와 Harness 경로 검사 통과. 개별 기능의 QA 한계는 기존 항목을 유지한다.
- 상태: 작업 브랜치 → develop → test → main 로컬 fast-forward 반영 완료. 후속 사용자 명시 승인으로 GitHub origin의 develop/test/main atomic push 성공(aa941d4), 원격 대기 해소. 다음 신규 작업은 main 병합 완료에 따라 `Sprint#5-1-<작업명>`부터 시작한다.
- 관련: `Feature/doc/2026-09-29_sprint4-main-save.md`, `Feature/doc/evidence/main-save-20260929/changed-files.tsv`, `DevLog/20260929.txt`.

## Sprint5

### Sprint#5-1 — 맨손·너클 애니메이션 제작과 1인칭 안정화 (2026-09-29)

- 브랜치: `Sprint#5-1-unarmed-knuckle-animation`
- 목표: 무기별 기획서의 맨손·너클 동작을 제작하고 머리 흔들림·정면 가독성·손발 본 정상성을 실제 게임에서 검증한다.
- 주요 변경: 맨손 4종·너클 5종 전용 Sequence/Montage 18개와 실제 DA 9개 연결. 오른잽/스트레이트 구분, IK 팔꿈치·양손 연타 연속성 보정, 오른손 타격 높이·어깨 방향 교정, 진입 블렌드 완화. 머리 회전 기여 15%·최대 8도, 초기 시선 -20도. 공유 원본·피해량·해금·히트 수 유지, 기존 너클 지연 0초만 접촉에 맞게 조정. 구형 생성기 4개에 새 연결 덮어쓰기 방지.
- 검증: UE Editor 빌드, 9클립·14접촉·48원본 해시, 6체형 각 840프레임, 자세 711,486행 구조 검사 PASS. Human Male 9종 실제 피해 14회 및 지연/연타 간격 검사 PASS. 초기 시야 90·V 관찰 280·Appearance 카메라 570프레임 회귀, Harness 경로·diff·Python 문법 검사 통과. 실제 실행 영상과 수치 증거 저장.
- 상태: 구현 및 명시 범위 QA 완료, 작업 브랜치 보존. 새 커밋·푸시 없음. 너클 장비 모델/모든 방어구 관통, 경사면·네트워크·사망/재빙의·극단 공격속도·RT 그림자는 미검증. Sprint5는 main 병합 전으로 유지한다.
- 관련 문서: `Feature/architect/2026-09-29_unarmed-knuckle-animation.md`, `Feature/doc/2026-09-29_unarmed-knuckle-animation.md`, `Feature/doc/evidence/fist-combat-20260929/`, `DevLog/20260929.txt`.

## Sprint#5-2 - 스튜디오 Git 조회 및 중단 안내 교정 (2026-09-29)

- 브랜치: `Sprint#5-2-studio-review-diagnostics`
- 목표: 기획 중단을 검증 실패로 오해하는 표시와 Windows sandbox Git 소유권 오류를 해결한다.
- 주요 변경: 현재 저장소로 한정한 명령별 Git 조회 지침, 경로 인용, 역할별 중단 안내, 기존 구현 우선 확인. 전역 신뢰·ACL·승인 정책 유지.
- 검증: Node22건 통과, 실제 read-only 역할의 Git status/branch 모두 종료0 및 브랜치 일치, diff 검토. 게임 변경 없어 UE 빌드 생략.
- 상태: Git 조사 및 단계별 안내 수정 완료. 기존 맨손·너클 구현 보존. 자동 작업 전체 완료는 별도 검증 대상. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-29_studio-review-diagnostics.md`, `Feature/doc/2026-09-29_studio-review-diagnostics.md`, `DevLog/20260929.txt`.

## Sprint#5-3 - 스튜디오 맨손·너클 작업 인계 및 완료 검증 (2026-09-29)

- 브랜치: `Sprint#5-3-studio-task-completion`
- 목표: 반복 확인 필요 작업을 기존 구현 인계·실제 검증·독립 검토 후 완료로 연결한다.
- 주요 변경: 원 작업 완료와 활성 충돌 없음 확인, 기존 미커밋 구현 인계 확정, 중복 제작 없이 스튜디오 검증 진행 기록. 기존 구현만으로 확인을 반복 요구하지 않는 coordinator 지침 보완.
- 검증: UE빌드 성공4.09초, 새 HumanMale 9종840프레임/실패0·실제14회 피해, 자세118,581행 구조 PASS/비유한0·머리회전 최대7.0021도. V280·카메라570프레임 회귀 실패0. 독립 소스/실행자료 리뷰와 원본48개 SHA 일치.
- 상태: 등록 요청 범위 완료 검증. 기존 게임 소스·에셋 보존, 커밋·푸시 없음. 나머지5체형은 기존 증거 확인이며 이번 재실행 아님. 장비별 관통·네트워크 등 기존 미검증 한계 유지.
- 관련 문서: `Feature/architect/2026-09-29_studio-fist-completion.md`, `Feature/doc/2026-09-29_studio-fist-completion.md`, `Feature/doc/evidence/studio-fist-completion-20260929/`, `DevLog/20260929.txt`.

## Sprint#5-4 - 격투가 스킬트리와 테스트 해금·키 설정 (2026-09-30)

- 브랜치: `Sprint#5-4-fighter-skill-tree`
- 목표: 맨손/너클 격투가 스킬을 기존 K 스킬트리에 연결하고 테스트 전체 해금·스킬별 키 설정을 제공한다.
- 주요 변경: 공용14노드(공격13·심법1), 세션 테스트 해금과 Shipping 제외, Enhanced Input 키 지정/해제·로컬 SaveGame·충돌 검사, 차징/공중/쿨다운/GAS 검증·메뉴/정지/무기/키 변경 취소, 실제 위젯·설명 겹침 개선, Editor 구성 및 전투 값 보존 검사.
- 검증: UE5.8 Editor 빌드10.44초 성공, Editor 두 프로필14노드/위젯 PASS, 실제 게임107항목/13개 공격 발동·몽타주 실패0, 새 프로세스 키 복원2항목 PASS, 기존11무기 전환 PASS, 9종840프레임·실제14회 피해 PASS, 자세118,581행 구조PASS/비유한0·머리회전7.0021도. 실제1920×1080 캡처와 독립 교차 리뷰, Python 문법·diff 검사.
- 상태: 구현 및 명시 범위 검증 완료. 기존 미커밋 동작·카메라·Studio 변경을 보존했으며 커밋·푸시 없음. 전체13개 공격의 모든 상황별 피해·장비관통·온라인 복제와 영구 수련 저장은 범위 밖.
- 관련 문서: `Feature/architect/2026-09-30_fighter-skill-tree.md`, `Feature/doc/2026-09-30_fighter-skill-tree.md`, `Docs/SkillTreeLoadoutGuide.md`, `Feature/doc/evidence/fighter-skill-tree-20260930/`, `DevLog/20260930.txt`.

## Sprint#5-5 - 내장 AI Assistant 웹 인증 오류 처리 (2026-09-30)

- 브랜치: `Sprint#5-5-editor-ai-assistant-error`
- 목표: 사용자 로그의 Cloudflare iframe 스크립트 차단·IsAvailable 취소 오류를 프로젝트에서 처리한다.
- 주요 변경: 프로젝트 AIAssistant 플러그인만 비활성화하고 내장 Epic AI 창의 웹 연결을 중지. Studio CLI·Unreal MCP·AllToolsets·게임 기능 설정 보존, 적용·복원 안내 추가.
- 검증: 프로젝트 JSON·필수 플러그인 및 단일 설정 변경 확인, UE Editor 빌드120.41초 성공. 실제 새 Editor 시작·정상 종료0, AIAssistant 마운트/로그0건, MCP·AllToolsets·Python 마운트 확인. 관련 diff 검사 통과.
- 상태: 요청된 AI Assistant 오류 경로 처리 완료. 인증 서비스 자체의 수정은 아니며 내장 Epic AI 창은 비활성화된다. 변경 전부터 있던 엔진 초기화 Condition failed15건은 범위 밖으로 기록. 기존 변경 보존, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-09-30_editor-ai-assistant-error.md`, `Feature/doc/2026-09-30_editor-ai-assistant-error.md`, `Docs/UE58MCPSetup.md`, `Feature/doc/evidence/editor-ai-assistant-20260930/`, `DevLog/20260930.txt`.


### Sprint#5-6 — close-combat-animation-preparation

- 상태: 애니메이션 적용 준비·정적 QA 완료, 게임 통합·화면·표적 판정 미검증
- 목표: 보유 Close Combat의 훅·어퍼컷·킥·가드·방향 회피를 Manny Preview로 검토하고 안정화 결과 보존
- 주요 변경: 원본33패키지 동일 바이트 설치, Manny11시퀀스와 리그3개 생성, 새 Preview root lock, 준비/감사/분석 스크립트와 후보 문서
- 검증: Unreal exit0, 983포즈 구조·root lock·DA11계약 PASS, 기존5,630파일·VaultCache121파일 SHA256 보존, AST·Harness·diff 검사
- 한계: 직선 킥의 큰 회전·회피 덤블링/긴 스텝 보류, Rolls and Dodges 로컬 미확인, 패리와 타깃 판정 미검증
- 관련 문서: Feature/architect/2026-10-02_close-combat-animation-preparation.md, Feature/doc/2026-10-02_close-combat-animation-preparation.md


## Sprint#5-7 — 격투가 스킬별 애니메이션 연결 (2026-10-02)

- 목표: 현재14노드의 실제 스킬마다 의미에 맞는 동작을 연결하고 안정화/밸런스 보존.
- 변경: 기존9개 유지, 유운수·금강쇄·무극패왕권 새 몽타주/파생 클립6개 및 DA3개의 몽타주/지연 필드만 적용. 태산압정은 소스 부족·기존 연결 유지, 금강불괴는 상시패시브.
- 검증: Editor14노드 전체 필드/에셋/root/limb PASS. 실제13개 키 발동 확인. 늦은 유운수 몽타주1항목 미통과. 기존9개 실제 정면타깃·머리카메라840프레임 회귀 PASS.
- 상태: 가능한12개 적합 연결 저장 완료. 실제 렌더·새3개 개별 접촉 미검증, 태산압정 소스 필요. 셰이더 준비 제한 후 NullRHI 최소검사 사용. commit/push 없음.
- 문서: Feature/architect/2026-10-02_fighter-skill-animation-binding.md, Feature/doc/2026-10-02_fighter-skill-animation-binding.md.


### Sprint#5-7 후속 검사 완료

- 유운수 미통과는 정상 블렌드 아웃 후의 늦은 활성 검사였다. 에셋 변경 없이 실제 수락 실행의 몽타주 가중치/시간 진행 관측으로 보강.
- 최종 전체검사120항목/13개키실행 PASS. 새3개 production DA는135cm 정면타깃 각피해1회·한 프레임 이내 시간오차·기존카메라상한 PASS.
- 검사 C++2파일/실행 스크립트만 변경, 에셋·전투/카메라/밸런스 보존. 승룡권은 기존 Knuckle_Uppercut 유지.
- 실제 렌더/스킨 접촉 미검증과 태산압정 소스 부족 유지. 근거: 스킬별 연결 결과 문서 후속진단 및 followup-validation.json. commit/push 없음.

## Sprint#5-8 — 격투가 팔 휘두르기 강화 (2026-10-05)

- 브랜치: `Sprint#5-8-fighter-arm-swing`
- 목표: 기본 맨손·너클·금강쇄 팔 움직임을 더 크게 만들고 타격/손발/머리 시야 안정성을 보존한다.
- 주요 변경: opt-in AuthorFistSwing, 후방 준비·회수/훅 원호/어퍼컷 낙폭 강화, 접촉 주변 샘플 키 양팔 보호, 금강쇄 FK 기반 IK 재동기화, 시퀀스10개 Editor 저장, 기준선·재로드·실제 게임 검사 도구와 영상.
- 검증: Editor 빌드 성공, 원시120Hz10종 PASS, 실제841프레임/118,581포즈·14회피해 PASS, V580/260샘플 PASS, StoneFist3종1,234검사 PASS, 렌더840프레임 PASS. 실제Appearance 손 경로1.03~9.62% 증가, 카메라전후7.002도 유지. 허용10개외 Content4,205개 같은바이트 확인. 독립 코드/실행 검토 PASS.
- 상태: 요청 범위 적용·저장·실행 검증 완료. 원시/실제 속도 수치 구분, HUD/모션블러로 미세 손가락·발 전체 육안 확인 제한. 기존 미커밋 작업 보존, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_fighter-arm-swing.md`, `Feature/doc/2026-10-05_fighter-arm-swing.md`, `Feature/doc/evidence/fighter-arm-swing-20261005/`, `DevLog/20261005.txt`.
## Sprint#5-9 — 격투가 전투 대기 자세 (2026-10-05)

- 브랜치: `Sprint#5-9-fighter-combat-stance`
- 목표: 기존 손을 내린 일반 Idle과 구분되는 맨손·너클 가드를 실제 Appearance Mesh에 연결한다.
- 주요 변경: 기존 공격 시작 포즈의 2초 호흡 루프, native AnimInstance 부모와 FootIK→자세 블렌드→DefaultSlot 연결. 공격·차징·회피 후 3초 유지, 진입 0.20초/복귀 0.45초. 이동·공중 우선, 무기·메뉴·사망·조종 해제 정리. 기존 FootIK 준비기의 새 연결 인식 보강.
- 검증: Editor 빌드, 재로드 원시241포즈, 통합640프레임·90,381포즈·렌더640프레임 PASS. 평균 외형 손 높이24.90cm 상승, 실제 발 높이 차이2.9345cm, 카메라최대7.0144도. FistCombat9종·피해14회·V580/260샘플 및 StoneFist3종1,234검사 PASS. 독립 코드·모션·선택 화면 검토, Python문법·Harness·diff 검사 통과.
- 보존: 기존4,186개 uasset 중 공통AnimBP1개만 수정, 신규가드1개 생성, 나머지4,185개 같은SHA256. 기존 강화공격10종·DA·몽타주·카메라 설정 보존.
- 상태: 요청 범위 구현·저장·실행 검사 완료. 21.33초 무음 영상 첨부. 실제 1인칭 오른손·3인칭 오른발 일부는HUD에가려짐; 이동가드레이어·별도 사망/재빙의 실행·전체체형/장비관통·온라인 미검증. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_fighter-combat-stance.md`, `Feature/doc/2026-10-05_fighter-combat-stance.md`, `Feature/doc/evidence/fighter-combat-stance-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-10 — 기본맵 기반 시작 마을 (2026-10-05)

- 브랜치: `Sprint#5-10-start-village`
- 목표: 기본 플레이맵을 복제하고 보유 Stylized Village 에셋으로 시작 마을을 제작하여 메뉴·준비 구역·전투 포탈을 연결한다.
- 주요 변경: 신규 StartHub 120m × 120m, 집 외관 10채·우물 광장·네 방향 길·준비 구역 4곳·남쪽 PlayerStart·북쪽 포탈·외곽 BlockAll 경계. 신규 소유 액터 667개와 머티리얼 12개. 메뉴 이동 목적지 StartHub·StartRun 옵션 제거, 포탈 후 기존 전투맵 런 시작. 공식 MCP JSON-RPC 클라이언트와 생성·검사 도구, 자동 시작·8001 포트 기본값.
- 검증: UE5.8 Editor NoHotReload 빌드·CheckGeneratedChanges PASS. 메뉴 정적·실제 PIE PASS. 허브/포탈 50항목 PASS, 시작점 실제 접지·허브 1200초 대기·2초 보행 약 11.50m·실제 native 포탈 overlap·전투 1200→1198초 확인. Python AST 검사와 Unreal API 노출 리뷰 완료.
- 보존: 기존 Content 4,302개 크기·수정 시각 동일, 선정 70개 SHA256 동일. 신규 Content 720개는 새 맵·전용 콘텐츠·해당 외부 액터/객체 범위 안에 있음. 기존 전투·애니메이션 미커밋 작업 보존.
- 상태: 공간 제작·저장·메뉴 연결·실제 런타임 검사 완료. 최종 전체/플레이어 화면과 독립 배치 검토, 명시적 서버 시작 없는 재기동·8001 자동 연결·StartHub 재로드 및 공식 MCP 저장 확인 완료. NPC 서비스·실내·대규모 오픈월드 확장·성능/패키징/온라인 검사는 미구현 또는 범위 밖. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-village.md`, `Feature/doc/2026-10-05_start-village.md`, `Feature/doc/evidence/start-village-20261005/`, `Docs/UE58MCPSetup.md`, `DevLog/20261005.txt`.

## Sprint#5-11 — 오픈월드 플레이맵 (2026-10-05)

- 브랜치: `Sprint#5-11-open-world-playmap`
- 목표: 기본 World Partition 맵과 보유 지형·환경 에셋으로 실제 탐험·전투 가능한 플레이맵을 제작하고 시작마을 포탈에 연결한다.
- 주요 변경: 신규 OpenRunMap 1800m × 1800m Landscape·256지형/충돌 컴포넌트·16공간 프록시,6개 지역·711길 패치·교량3개·야영지·풍차·북쪽 유적. 공식 PCG 그래프1개·183타일·나무2806개. 맵 전용 동적 Navigation invoker·256m 셀/700m 로드, 지역 스포너5개·몬스터 자식 Blueprint6종·고정 보스 접지. StartHub 포탈 목적지와20분 런 연결, 전용 강 재질과17패널 저장. PCGPrimitives 활성화, editor 한정 Landscape helper와 재현/QA 도구.
- 공용 수정: 몬스터 지면 스폰에 캡슐 반높이+2cm 및 소유 스트리밍 레벨 적용. AI 도달 검사에서 이동자·목표 양쪽 반경을 제외한 중심 거리 계약으로 공격 범위 밖 정지 방지. 원본 전투 데이터는 보존.
- 검증: 최종 Editor NoHotReload 빌드14.59초 성공, 프로젝트 파일 생성·재로드 PASS. 실제 포탈·1200초 런·지역/교량 보행·내비게이션·야영지 unload/reentry·보스 접지·native 몬스터 추적/피해99항목 PASS. 별도 짧은 사거리2종 경계 회귀30항목 PASS, 플레이어 정지 상태 실제 접근·피해 확인. 공식 MCP 실제 화면4개·명시적9패키지 저장·10패키지 미저장 상태false. Python AST13개·프로젝트 JSON·diff·CheckGeneratedChanges PASS.
- 보존: 기준선 Content5022개 전체 크기·SHA256 검사, StartHub 맵/포탈 외부 액터2개만 허용 변경, 나머지5020개 동일. 신규1268개는 OpenRunMap·전용 콘텐츠·해당 외부 액터/객체 경로 안에 있음. 기존 미커밋 작업 보존.
- 상태: 초기 플레이맵 제작·저장·명시 범위 검증 완료. 보스 처치 승리 연결·상세 미술/NPC/실내·전체20분 종료·전체 경로 연속 순회·다른 맵의 공용 AI/스폰 회귀·패키징/성능/온라인은 후속 범위. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-playmap.md`, `Feature/doc/2026-10-05_open-world-playmap.md`, `Feature/doc/evidence/open-world-20261005/`, `Docs/WorldStructure.md`, `DevLog/20261005.txt`.

## Sprint#5-12 — 시작마을 바닥 텍스처 개선 (2026-10-05)

- 브랜치: `Sprint#5-12-start-village-ground`
- 목표: 잔디·흙길 텍스처의 뭉개짐과 방향성 늘어짐을 수정하고 광장 바닥 질감을 개선한다.
- 주요 변경: 마을 전용 World XY 재질, 잔디180cm·흙길90cm·돌120cm 반복. 기존 인스턴스2개 부모 교체, 광장·준비 구역·포탈 바닥6개 돌 재질 연결. 생성기 튜닝 재사용·읽기 전용 사전 검사, 조사/수정/캡처/재로드/Content 감사 도구.
- 검증: native·공식 MCP 재질 컴파일 및 배선 PASS, 명시5패키지 저장·dirty=false, 맵 해제·재로드43항목 PASS. 기존677액터 배치·메시·충돌·포탈 보존, Content6290개 전체 SHA256에서 허용9개 외6281개 동일·신규2개. 실제 전후·동서 길·마을 화면 검토, 생성기 실제 사전 검사·Python AST·diff·하네스 경로 검사.
- 상태: 요청 범위 적용·저장·재로드 검증 완료. 원본 에셋과 그래픽 전역 설정 보존. 재질 전용 변경으로 C++ 빌드·PIE 전투 재검사·패키징/성능 측정 미실행. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-village-ground.md`, `Feature/doc/2026-10-05_start-village-ground.md`, `Feature/doc/evidence/village-ground-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-13 — 제작된 오픈월드 탐험 지도 (2026-10-05)

- 브랜치: `Sprint#5-13-open-world-map`
- 목표: 완성된 OpenRunMap을 실제 배치와 일치하는 한국어 지도로 제공한다.
- 주요 변경: 실제R16 지형 음영·10m등고선,6지역·3길·유한60m강·교량3개·조우/보스·입장 표시, 별도StartHub 포탈 삽입도·방위/500m축척. 2200×2550PNG·좌표/근거JSON·재현 렌더러와 문서.
- 검증: 독립 좌표 검토, 제작POI/교량 일치·PCG183타일 완료, R16기준5점 양자화오차1cm이내·라벨범위/비중첩·실제PNG 육안 검토, Python AST·PNG무결성·diff 검사.
- 상태: 지도 제작 완료. 수목/건물은 지도 기호이며 개별 인스턴스 정사영 캡처가 아니다. Unreal 콘텐츠·C++·설정 변경과 빌드/PIE 실행 없음, 기존 작업 보존·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-map.md`, `Feature/doc/2026-10-05_open-world-map.md`, `Feature/doc/evidence/open-world-map-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-14 — 기본 몬스터와 시작마을 안내 NPC (2026-10-05)

- 브랜치: `Sprint#5-14-open-world-monsters`
- 목표: 오픈월드 기존 6종에 Idle·이동·기본 공격을 구현하고 시작마을에 간단한 대화와 기본 아이템 지급 NPC를 배치한다.
- 주요 변경: 타이머 기반 기본 근접 공격 Profile/Component와 opt-in AI 연결, 보유 6종 모델·5개 동작 클립 및 전용 DataAsset/Profile 12개 연결. 원본 능력치·보상·스포너 정책 보존. 길잡이 아린의 F 대화·기존 회복 포션 2개 지급·중복 방지·만석 재시도, 지급 스택의 실제 잔여 수량만 OpenLevel 포탈 뒤로 전달. NPC native 카드 RebuildWidget 표시 수정, 전용 BP/DA·StartHub 액터 저장 및 검사 도구.
- 검증: Editor NoHotReload 빌드 111.69초 및 표시 보완 70.11초 성공. 저장 재로드 100/100, 최종 실제 몬스터 PIE 219/219와 NPC PIE 57/57 통과. 6종 Idle·이동·native 추적/피해·회피/사망 취소, 물약 실제 사용·실제 포탈 후 잔여 1개·1200초 런 확인. 실제 F 열기/지급/닫기와 한글 카드 화면 2개, 독립 코드·화면 검토·문법/JSON/diff/하네스 검사.
- 보존: 기준선 Content 6,292개 전체 SHA256에서 허용 기존 10개 변경·신규 15개, 6,282개 동일. 기존 마을 677액터·원본 25종·구매 모델/클립/재질·다른 미커밋 작업 보존.
- 상태: 요청 범위 제작·저장·최종 집중 검사 완료, 에디터 StartHub 대기. 세션 단일 플레이어 보급이며 특성 강화·보스 특수 패턴/승리 목표·전체 20분·네트워크·패키징/출시 성능은 후속 범위. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_open-world-monsters.md`, `Feature/architect/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/2026-10-05_open-world-monsters.md`, `Feature/doc/2026-10-05_start-hub-tutorial-npc.md`, `Docs/OpenWorldMonsterDesign.md`, `Feature/doc/evidence/open-world-monsters-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 추가 — 길잡이 아린 시각 콘셉트

- 목표: 시작마을의 대화·출발 보급 역할에 맞는 NPC 이미지를 제작한다.
- 주요 변경: imagegen 내장 도구로 친근한 성인 안내인·초록 여행복·가죽 가방·물약 두 병·밝은 기와 마을의 전신 콘셉트 1024×1536 PNG를 생성하고 프로젝트에 사본·프롬프트를 저장했다.
- 검증: 인물·손·물약·전신 구도 육안 검토, PNG 형식/크기와 원본·사본 SHA256 일치, diff 검사.
- 상태: 이미지 제작 완료. 시각 콘셉트와 문서만 변경했으며 엔진 에셋·C++·설정·3D 모델·메시 교체는 이번 범위에 없음. 빌드/PIE 재실행과 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/2026-10-05_start-hub-tutorial-npc.md`, `Feature/doc/evidence/start-hub-tutorial-npc-image-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 후속 — 아린 FBX 제작·적용 준비

- 목표: 기존 아린 콘셉트를 실제 리깅된 FBX로 제작하고 StartHub NPC에 적용한다.
- 준비한 변경: 동일 외형의 흰 배경·무그림자 정면 전신 T포즈 참조와 프롬프트, A1~A4 패킷, 새 Skeleton용 Idle 임포트/리타깃 경로 문서. 기존 NPC와 엔진 콘텐츠는 유지했다.
- 검증: 실제 NPC Skeleton 일치 조건·기존 FBX/IK Retarget 코드 조사, 참조 PNG/원본 사본 해시·육안·diff/하네스 검사. Tripo 공식 인증·리깅·FBX 명세 확인.
- 상태: 참조 준비 완료, TRIPO_API_KEY 연결 대기. 유료 생성 태스크·잔액 조회·실제 FBX·새 메시·맵 적용은 미실행이며 완료로 처리하지 않는다. 빌드/PIE는 실제 적용 시 수행. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_arin-npc-fbx.md`, `Feature/doc/2026-10-05_arin-npc-fbx.md`, `Feature/doc/evidence/arin-npc-fbx-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-15 — 하단 전투 HUD 시안 적용 (2026-10-05)

- 브랜치: 작업 시작 시 `Sprint#5-15-compact-combat-hud` 생성·전환. 공유 체크아웃은 병행 작업에서 이후 `Sprint#5-14-open-world-monsters`로 전환됨.
- 목표: 승인된 imagegen 시안의 낮은 검정·청동 HUD를 실제 UMG에 적용하고 경험치·레벨·획득 알림을 정리한다.
- 주요 변경: 1200×196 코어·하단20·양옆24, 패널 내부 경험치, 실제 무기명·잠금 기호·준비 강조·HOLD 두 줄, Compact 5파트와 Main/Bottom 연결. 좁은 화면에서 획득 카드가 코어 위로 이동. HUD C++·Editor 재현/검증·프레이밍 보호·실제 게임 QA 추가.
- 검증: UI 전체 Editor 빌드28.42초, 최종 별도 모듈 빌드25.40초 성공. 7패키지 validator PASS·공식 MCP dirty=false. 실제 StartHub 6해상도/DPI단계 922검사 PASS·실패0, PNG6개 크기/시각과 일반/좁은 화면 육안 확인. 독립 C++/UMG 리뷰·AST·PowerShell parse·diff·CheckGeneratedChanges·Harness 경로 PASS.
- 보존: 기존 UI112개 중 Main/Bottom만 수정, 나머지110개 SHA256 동일. 신규 Compact5개. 기존 전투/애니메이션/맵 작업과 일반 수량0 로그 필터 유지.
- 상태: 적용·저장·게임 재로드·검증·기록 완료. 패키징/온라인/장시간 성능 검사 범위 밖. 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_compact-combat-hud.md`, `Feature/doc/2026-10-05_compact-combat-hud.md`, `Docs/CompactCombatHUDGuide.md`, `Feature/doc/evidence/compact-hud-20261005/`, `DevLog/20261005.txt`.

### Sprint#5-14 후속 — Apollo 아린 정적 3D·FBX 제작 및 적용 검사

- 목표: 사용자가 지정한 Apollo 스킬로 아린을 실제 FBX로 제작하고 검증을 통과하면 시작마을 NPC 외형에 적용한다.
- 주요 변경: 프로젝트 전용 Python·실제 Apollo API/stdio MCP·공식 체크섬 검증 portable Blender, 단일 이미지2.5D FBX·정사각3뷰 참조·원본 ApolloHullAdapter의 실제 입체 GLB·공식 MCP 업로드/preview/export/download·검사/포장 스크립트6개와 정적 입체 FBX·GLB·설명 패키지.
- 검증: 닫힌 표면·양의 체적·단일 연결 등11/11, 최종 FBX 실제 재수입 높이1.7m·24,880삼각형·뼈0·Action0, 원본GLB 보존 SHA256·manifest·ZIP CRC, 실제 정면/측면/사선·독립 검토·AST·하네스 경로. 두 팔 자세 리깅 검사는 최대 늘어남7.38/6.34배로 실패했고 사선 얼굴 중복·망토 투영 늘어짐을 확인했다.
- 상태: 정적 입체 초안 FBX 전달 완료. 게임용 리깅·Idle·NPC 적용은 실제 품질 불합격으로 미완료이며 기존 안내 NPC·대화·지급을 유지했다. 업로드 score1.0/ready를 NPC 품질 승인으로 해석하지 않는다. CPU local_draft 출처와 실패 근거 동봉, C++/엔진 콘텐츠 변경·빌드/PIE 재실행·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-05_arin-npc-fbx.md`, `Feature/doc/2026-10-05_arin-npc-fbx.md`, `Feature/doc/evidence/arin-apollo-20261005/`, `DevLog/20261005.txt`.

## Sprint#5-16 — Blender MCP 아린 입체 모델·NPC 적용 (2026-10-07)

- 브랜치: `Sprint#5-16-blender-arin-npc`에서 개발 후 사용자 품질 개선 지시에 따라 `Sprint#5-17-arin-quality-refinement`로 전환.
- 목표: 사용자 지정 Blender MCP를 실제 설정하고 리깅된 아린 FBX를 시작마을 NPC에 적용한다.
- 주요 변경: 프로젝트 전용 Blender4.5.14/Python/MCP addon·loopback9877·Codex blender 항목, 재현 시작/호출 도구. 실제 입체175메시/42,764삼각형·23본·2초 Idle·BLEND/FBX/GLB. 전용 UE19에셋과 guide 외형 참조·BP/외부 액터 저장.
- 검증: 실제 MCP ping/version/scene/python/viewport, FBX18/18, 실제 NPC PIE64/64, 맵 해제·StartHub 재로드와 외형/보호설정 검증 PASS. 실제 마을 정면/얼굴/사선 캡처. Content6312기준 신규19·변경3·삭제0·나머지6309크기/수정시각 동일. 대화·정확한 물약2개·만석/중복/소모 후 포탈 잔여1개 유지.
- 상태: 스타일화된 v1 제작·적용·저장·집중 검사 완료. 단색15재질·UV/텍스처/LOD/얼굴·손가락 리그 없음. Blender 재수입은 live 소유 프로세스였으며 이후 v1 원본 fresh-process 검사에서는 새 품질 계약의 UV/PBR 기준 미달을 사실대로 기록. 품질 개선용 동결 기준선으로 보존. C++ 빌드/패키징/성능/네트워크 미실행, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-07_blender-arin-npc.md`, `Feature/doc/2026-10-07_blender-arin-npc.md`, `Feature/doc/evidence/arin-blender-20261007/`, `Scripts/Blender/README.md`, `DevLog/20261007.txt`.

## Sprint#5-17 — 아린 Blender 품질 개선 (2026-10-07)

- 브랜치: `Sprint#5-17-arin-quality-refinement`.
- 목표: 다섯 Blender 스킬·imagegen 참조·반복 검증으로 실제 아린 모델을 개선하고 UE용 UV/PBR·LOD·FBX를 적용한다.
- 주요 변경: 다섯 스킬 설치와 v1 동결, Q1~Q6·imagegen 참조·독립 critic. 실제 얼굴/눈/입·두피/땋기·손/손톱·연결 바지·외투/앞치마 변형 개선. 174편집 부위·4런타임 메시·51,634삼각형·23제작 본·2초 Idle, 4개 UV/PBR atlas·16PNG(Cloth4k/나머지2k). BLEND·결합/분리 FBX·GLB 및 실제 렌더/GIF 전달. UE 전용 24에셋·기존 guide 외형 연결·3LOD 저장.
- 검증: 새 Blender 프로세스 3포맷 검사·22개 최종 FBX 자세 렌더 검토, 실제 2초 Idle 13프레임. 최종 NPC PIE67/67·GUI 맵 해제/재로드·별도 UE Cmd 프로세스 저장 참조 검사 통과. 실제 UE 9화면에서 LOD1/2의 초기 의상 패치·홍채 손실을 발견해 실제 축소 비율1/.25/.125→1/.85/.65·경계 잠금으로 수정하고 재검토 통과. 최종 UE 정점40,211/35,681/28,851·각4sections·LOD0/골격 보존. 텍스처 임시32px는 배치 속성 설정+공식 컴파일 완료 후 실제2k/4k 검증. Python AST24개·diff·CheckGeneratedChanges 통과.
- 보존: Content6,331개 전체 SHA256에서 기존 안내인 DA/BP/소유 외부 액터3개만 변경, 신규RefinedV2 24개·삭제0·나머지6,328개 동일. 대화·물약2개·중복/만석 재시도·실제 소모/포탈 잔여1개 유지. 에디터 검토용 카메라·강제LOD·백그라운드 갱신 설정 복원.
- 상태: 스타일화된 안내 NPC 품질 개선·최종 적용·저장·집중 검증 완료. 근접 얼굴/헤어의 단순화·목 음영 경계·고정 망토 한계와 개별 손가락/얼굴 리그·보행/전투·Manny 호환·패키징/성능/온라인 미검증을 명시. C++ 변경/빌드 없음. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-07_arin-quality-refinement.md`, `Feature/doc/2026-10-07_arin-quality-refinement.md`, `Feature/doc/evidence/arin-quality-20261007/`, `DevLog/20261007.txt`.

## Sprint#5-18 — 사용자 FBX·이미지 기반 아린 v3 개선 (2026-10-09)

- 브랜치: `Sprint#5-18-arin-reference-refinement`.
- 목표: 첨부 판타지 모험가 FBX와 전신 이미지의 얼굴·헤어·후드·자수 의상·장비를 실제 아린 모델에 반영하고 기존 시작마을 NPC 기능을 보존한다.
- 주요 변경: R1~R6 패킷·도너 조사와 원본/v2 동결, 도너 메시·UV·2K 색 atlas를 기반으로 헤어/팔보호대/부츠 국소 수리·재질 팔레트·피부 마스크·팔/무릎 가중치 개선. 최종45,205정점·90,214삼각형·1메시/1재질·23제작 본·최대3영향·170cm·2초 Idle, 실제4K PBR 네 장과 BLEND·결합/분리FBX·GLB·실제GIF. 전용 RefinedV3 새9에셋·guide 외형/Idle 연결·3LOD 저장. 임시32px resource 문제는 검사한7개 에셋을 재가져오기/삭제 없이 복구하고 실제4K gate 뒤 누락 재질2개를 준비했다.
- 검증: fresh Blender3포맷과 최종FBX8뷰·canonical 동등형상27뷰/clay·native packed4PBR SHA,13실제 Idle 샘플/12프레임2000ms, source/engine 독립 검토. 실제UE9화면 강제/예측LOD0·1·2/1section 유지·카메라와 임시 설정 복원. LOD정점110,688/100,176/81,219·목표1/.85/.65·경계 잠금·LOD0/골격 보존, 정확한 UE LOD삼각형 수는 미노출로 명시. 실제 NPC PIE67/67, GUI 맵 해제·재로드, 별도 UE Cmd 프로세스 저장 검사 exit0, 최종 출처/원본 보존 해시23개·신규9Python AST·diff/harness 경로 통과.
- 보존: Content6,355개 전체 SHA 감사에서 기존 guide DA/BP2개만 변경, 나머지6,353개 동일·새9개·삭제0·범위 밖 변경0. 소유 외부 액터 패키지는 저장했으나 SHA 동일. 기존 대화·포션2개·중복/만석 재시도·실제 소모와 포탈 잔여1개 유지. 사용자 첨부 원본과 v2 여섯 산출물의 SHA 동일.
- 상태: 아린 v3 모델 개선·시작마을 적용·저장·집중 검증 완료. 원본2K 얼굴/헤어 세부·약한 목 측면 음영·열린 의상 경계와 비다양체 표면·개별 얼굴/손가락 리그 부재를 기록한다. 보행/전투·Manny 호환·천 시뮬레이션·성능/패키징은 미검증이다. C++ 변경/빌드 없음. Sprint5 유지, 커밋·푸시 없음.
- 관련 문서: `Feature/architect/2026-10-09_arin-reference-refinement.md`, `Feature/doc/2026-10-09_arin-reference-refinement.md`, `Art/NPC/Arin/ReferenceRefined/README.md`, `Feature/doc/evidence/arin-reference-refinement-20261009/`, `DevLog/20261009.txt`.

## Sprint#5-19 — 아린 기본 걷기·대화 애니메이션 (2026-10-09)

- 브랜치: `Sprint#5-19-arin-walk-talk-animation`.
- 목표: 기존 아린 v3에 기본 Walk/Talk를 제작·적용하고 실제 이동·대화에 따른 자동 전환을 제공한다.
- 주요 변경: Blender 30fps·Walk1.2초/Talk3초와 별도 BLEND·FBX2개, C++ 전용 애니메이션 컴포넌트, Guide DataAsset의 클립·속도 설정, 같은 프레임의 타이머 중복 호출 보호, 기존 골격에 새 UE클립2개와 Guide DA참조 저장.
- 레퍼런스: Mixamo의 공개 걷기·대화 프리뷰. 적용 클립은 직접 제작한 키프레임이며 다운로드한 Mixamo FBX는0개.
- 검증: 최종 Editor 빌드 성공·재시작, fresh FBX Walk37/Talk91 전체 자세, 원본28/독립FBX8/실제UE6화면, 실제 PIE159/159(기존67 전부·신규23단계), 임시5FPS 저프레임 검사 후120FPS 복원, 새 Editor-Cmd PID51740 저장 재로드 통과.
- 보존: 기존 Content6363개 SHA동일·Guide DA1변경·신규클립2개·삭제/범위밖변경0. 기존 아린9패키지·Art59파일·대화/포션2/중복·만석/포탈잔여1 유지.
- 상태: 완료. 커밋·푸시 없음. Talk는 몸짓이며 얼굴 립싱크·개별 손가락·AI순찰·지형발IK·의상물리·전환블렌딩은 포함하지 않는다.
- 관련 문서: [설계](../Feature/architect/2026-10-09_arin-walk-talk-animation.md), [결과](../Feature/doc/2026-10-09_arin-walk-talk-animation.md), [사용 안내](../Art/NPC/Arin/Animation/README.md), [통합 검증](../Feature/doc/evidence/arin-walk-talk-20261009/tutorial-runtime.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-20 — 이동 중 상체 공격·하체 보행 분리 (2026-10-09)

- 브랜치: `Sprint#5-20-moving-upper-body-attack`.
- 목표: 플레이어가 걷는 중 공격할 때 다리가 전신 공격 몽타주로 고정되는 문제를 해결한다.
- 주요 변경: 실제 수락 스킬/몽타주·지상 속도에 따른 native 상체 조건, 보행과 몽타주 결과의 별도 캐시, spine_01/ik_hand_root 상체 마스크, 정지0.12초 전신 복귀. 회피 weighted tail·점프/공중 유지, 이동 시 정지 가드 해제. 기존11노드·DefaultSlot·몽타주·Data Asset 보존 후 새9노드 추가. Editor 전용 제작/읽기 감사와 기존 가드·FootIK 검증 계약 갱신.
- 검증: Editor 최종 빌드·프로젝트 재생성, 컴파일 오류/경고0, fresh 재로드와 legacy guard/FootIK 감사 통과. 원본 그래프 대조1100프레임 하체474실패→수정1100프레임·258066검사 실패0. 실제 렌더1100·가드640·기존 전투840프레임 실패0. 맨손/너클9종에서 source/retarget 하체 보행 최대차0.000002415도·source 위치차0cm, 실제 체력16펄스·타이밍 유지.
- 보존: Content6366개 중 기존 Common AnimBP1개만 변경·6365개 SHA 동일·추가/삭제0. 기존 아린·월드·HUD와 미커밋 작업 유지. StartHub Editor·PIE 종료·dirty0·120FPS 복원.
- 상태: 구현·저장·집중 QA 완료. 현재 맨손/너클 경로에 집중 검증. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_moving-upper-body-attack.md), [결과](../Feature/doc/2026-10-09_moving-upper-body-attack.md), [검증 비교](../Feature/doc/evidence/moving-upper-body-attack-20261009/runtime-comparison.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-21 — 1인칭 카메라 하향·공격 전방 점검 (2026-10-09)

- 브랜치: `Sprint#5-21-lower-camera-forward-attacks`.
- 목표: 1인칭 카메라를 조금 낮추고 현재 플레이어 공격의 전방 방향과 실제 피해를 점검한다.
- 주요 변경: native EyeOffset.Z 4→-3cm로 약7cm 하향. 초기 pitch=-20도·조준·3인칭 검사·몽타주·피해 시점 유지. 읽기 전용 전체 공격 감사와 실제 타격손/앞뒤 대상 피해/동일 자세 카메라 비교·1인칭 캡처 도구 추가.
- 검증: 최종 Editor 빌드 성공. 74DA/13Profile 참조·타이밍 이상0, 현재 맨손/너클23공격·16시퀀스120Hz 방향 조사 이상0. 실제13종+회전2건=15건의 runtime/render 각각1,572프레임·4,978/4,979검사·실패0, 체력21펄스·15접촉/2idle화면. 카메라 동일 자세7.000000cm·독립 오차0. 기존 머리 카메라570·초기 시선90·3인칭280·이동 상하체1,100프레임 회귀 실패0.
- 보존: Content6,366파일 전부 SHA 동일·추가/삭제/변경0, 집중207패키지/타격 데이터/프로필/몽타주 동일. 기존 아린·월드·HUD·미커밋 작업 유지. 여섯 Blueprint 기본값 확인, StartHub Editor·PIE 종료·dirty0·120FPS 복원.
- 상태: 구현·집중 QA·독립 리뷰 완료. 실제 손 방향/전투 범위는 현재 맨손·너클·무투가 경로다. 원형 내려찍기의 후방 피해는 의도된 범위 판정이다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_lower-camera-forward-attacks.md), [결과](../Feature/doc/2026-10-09_lower-camera-forward-attacks.md), [원본 감사](../Feature/doc/evidence/camera-forward-attacks-20261009/audit.json), [일일 로그](../DevLog/20261009.txt).

## Sprint#5-22 — Walking.fbx 기반 보행 손바닥 보정 (2026-10-09)

- 브랜치: `Sprint#5-22-walking-palm-reference`.
- 목표: 첨부 Walking.fbx의 손바닥 방향을 참고해 현재 플레이어의 맨손·너클 보행을 개선한다.
- 주요 변경: Blender에서 65본·30FPS 원본을 해부학적 손 평면으로 분석했다. 원본 Walk/Jog 16개를 복제해 손목→중지 축의 제한된 회전만 적용하고 새 BlendSpace로 연결했다. 모든 위치·스케일, 손가락·하체·IK 원본 채널과 27표본·Idle·타이밍·마커·공격 그래프를 보존했다. 첫 보조 IK 보정의 실행 중 이동 문제를 발견해 원본 IK를 복원하고 더 엄격한 검증을 추가했다.
- 검증: UE5.8 Editor 빌드27.97초 성공. native101,953키쌍·16클립 검사 통과, 보호 위치/스케일 오차0·회전0.000116414도 이내. 실제 render650프레임·95,970검사·9화면과 여섯 외형 각각650프레임·95,976검사 실패0. 독립 보행 대조220프레임에서 보조 손 IK 위치 변화0cm, 손바닥 평균 방향16조합 모두 개선. 이동 공격1,100프레임·258,066검사·피해16건, 전방/카메라1,572프레임·15건·피해21펄스 회귀 통과.
- 보존: 기존 Content6,366개 중 Common AnimBP1개만 변경·나머지6,365개 SHA 동일, 신규17·삭제0. 제공 FBX SHA 동일. 새 에디터 재로드에서 그래프/표본/키 검사 통과, 여섯 카메라(8,0,-3)cm·pitch-20도 유지. StartHub·PIE 종료·dirty0·120FPS 복원.
- 상태: 적용·저장·집중 QA·독립 정적/기하/시각 리뷰·문서 완료. 전체 Mixamo 보행을 이식하지 않고 손 방향을 반영했으며 모든 프레임의 참조 일치를 주장하지 않는다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-09_walking-palm-reference.md), [결과](../Feature/doc/2026-10-09_walking-palm-reference.md), [독립 실행 검토](../Feature/doc/evidence/walking-palm-reference-20261009/reference/runtime-review-v2.json), [일일 로그](../DevLog/20261009.txt).

### Sprint5 운용 기록 — 스튜디오 활성화와 후속 개발 검토 (2026-10-11)

- 목표: 개인 개발 스튜디오를 활성화하고 남은 개발 후보를 제시한다.
- 변경: 기존 실행기로 로컬 서버 시작, 개발 일정 화면 열기 요청, 최신 결과 문서 기반 6개 후속 후보 정리 및 한국어 개발 기록 추가.
- 검증: 실제 API 응답에서 ready·paused=false·진행/실행대기0·등록2건 모두 completed 확인.
- 상태: 활성화·목록 검토 완료. 후보는 미등록이며 게임 구현·빌드/PIE·브랜치 변경·커밋/푸시 없음. 기존 게임·Studio 작업 보존.
- 관련 문서: `Feature/doc/2026-10-11_studio-backlog-review.md`, `DevLog/20261011.txt`.

## Sprint#5-23 — 아자토르 보스 전용 패턴 (2026-10-11)

- 브랜치: Sprint#5-23-azathor-boss-patterns.
- 목표: 기존 아자토르에 예고가 있는 정면/범위 특수 공격과 체력 단계 패턴을 구현하고 실제 검증한다.
- 주요 기록: 사용자 요청으로 스튜디오 예정에 높은 우선순위·자동 실행으로 등록. 기존 미커밋 작업과 기본 보스 능력치 보존, 실제 빌드/PIE/재로드/독립 검토를 작업 범위에 명시.
- 검증: API 저장 확인·기존2건 보존·새 작업 ID a359620e-46c8-409f-a2e2-6e991387d1da. dispatcher active와 engine busy, 기획 에이전트 running 및 실제 세션 시작 확인.
- 상태: 확인 필요(blocked·일시정지). 기획 에이전트가 PATH의 python 부재로 중단했다. 설치된 Python 3.12.14의 절대 경로와 작업 메모를 보완했지만 샌드박스 읽기 전용 MCP 확인은 WinError 10013으로 실패했다. 사용자가 허용한 동일 확인 명령 하나를 승인 심사에 제출해 실제 실행했으나 이번에는 WinError 10061(연결 거부), 종료 코드 1로 실패했다. 현재 UnrealEditor 프로세스 0개이며 기본 MCP 설정은 자동 시작·8001/mcp다. PIE/미저장 상태 미확인·구현 패킷 미확정·게임 구현/빌드/QA 미실행. 단일 명령 허용을 지속 실행 설정 변경이나 자동 재검토 권한으로 확대하지 않았다. 커밋·푸시 없음.
- 후속 지시: 사용자가 에디터 실행·재검토를 요청한 뒤 다른 채팅의 Studio 제거와 충돌을 확인했고, "스튜디오 없이 아자토르 직접 개발"을 선택했다. 아래 상태가 현재 개발 상태이며 위 blocked는 제거 전 초기 Studio 실행의 기록이다.
- 현재 변경: 직접 개발로 보스 opt-in·기본→정면→기본→영역 순환·고정 직사각형/원형 경고·HP50% 강화·특수 쿨타임·범위/높이/가림/단일 타격·취소/사망/EndPlay 정리를 C++ 컴포넌트에 구현했다. 전용 프로필/데칼 4개를 새로 만들고 기존 아자토르 DA의 참조 한 곳만 연결했다. 기본 HP10000·피해70·EXP3000·이동240과 기존 애니메이션·다른5종 보존.
- 현재 검증: Editor 빌드68.49초 성공, 실제 PIE22사례·480확인 실패0·55.688초, 강화 예고/피해/회복 시간의 관측 오차 구간까지 검증. 주황 직사각형·보라 원·붉은 강화 실제 화면3개 확인. 새 에디터 재로드와 콘텐츠 기존6296개 동일·기존DA1변경·4추가·삭제0 및 독립 리뷰 통과. 마지막 OpenRunMap·PIE없음·dirty0·원래 CPU제한/FPS 복원.
- 현재 상태: 구현·저장·집중 QA·독립 검토·문서 완료. 첫 화면 확인의 GPU 메모리 오류는 기록 후 에디터 재실행·재로드·slow-motion 화면 확인으로 복구했다. 장시간 GPU 안정성·북부 이동/전체 AI 전투·물리 회피·패키징·네트워크·최종 밸런스는 별도 검증이다. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_azathor-boss-patterns.md, Feature/doc/2026-10-11_azathor-boss-patterns.md, Feature/doc/evidence/azathor-boss-patterns-20261011/, DevLog/20261011.txt. 제거 전 일정·실행 이력은 Studio 제거 문서의 로컬 백업에 보존되어 있다.

### Sprint5 운용 기록 — 개인 게임 개발 스튜디오 제거 (2026-10-11)

- 목표: 사용하지 않는 개인 개발 스튜디오와 전용 실행·예약 연결을 제거한다.
- 주요 변경: 실행 작업 없음 확인 후 서버 PID28988 종료, heartbeat rogue10m 삭제, Studio 전체 및 BAT·PowerShell 실행기 삭제(추적 파일21개), 현재 MCP 문서의 개인 Studio 설명 정리. 기존 앱·일정·실행 이력75파일을 Git 제외 로컬 ZIP에 백업·검증했다.
- 검증: 실제 프로세스/포트 소유권 및 종료, 4317 리스너 없음, 자동화 삭제 응답과 설정 부재, ZIP 전항목 경로·크기·SHA256 일치, 제거 경로 부재·활성 참조 감사, CheckGeneratedChanges 종료 코드0.
- 상태: 제거 완료. 과거 기록·게임 코드·에셋 보존. 성장·스킬·인벤토리 일정 추가는 이 채팅에서 미실행. 다른 채팅의 직접 개발과 공유 브랜치를 유지했고 UE 빌드/PIE·커밋/푸시 없음.
- 관련 문서: `Feature/architect/2026-10-11_remove-personal-studio.md`, `Feature/doc/2026-10-11_remove-personal-studio.md`, `DevLog/20261011.txt`. 로컬 백업: `tmp/studio-removal-20261011/studio-backup.zip`.

## Sprint#5-24 — 마을 NPC 서비스와 캐릭터 진행 영구 저장 (2026-10-11)

- 작업 브랜치: `Sprint#5-24-npc-services-persistence`의 별도 작업 폴더. 원본 동시 작업의 현재 `Sprint#5-26-monster-test-ui` 브랜치 보존.
- 목표: 아린의 물약 구매·주무기/방어구 강화·활력/힘 특성·다음 탐험 난이도를 실제 게임에 추가하고 성장·수련/토벌·스킬 진행·인벤토리를 프로필별로 영구 저장한다.
- 주요 변경: Data Asset 기본값과 PlayerState 전용 특성 컴포넌트, 프로필 GUID SaveGame, 변경 이벤트 1초 자동 저장·포탈/종료 저장, 전체 인벤토리와 아이템 GUID/강화/퀵슬롯, 수령 기록과 프로필 전환 격리, 난이도 및 XP/골드 보상. 복원 실패는 롤백하고 기존 파일을 보존한다.
- 검증: 격리 최종 Editor 빌드80.86초·원본 통합 빌드29.64초 성공. 원본 서비스 Write60/Read25와 실제 포탈/런 Write42/Read15 실패0. 오류 저장27검사 실패0·원본 바이트 동일. 720p/1080p 실제 입력/화면 각9검사 통과, 최신 원본 F8 입력과 함께 쓰는 NPC 메뉴9검사 추가 통과. 독립 리뷰·생성 출력 검사·scoped diff 검사 통과.
- 보존: 기존 미커밋 작업·보스와 테스트 UI 변경 유지. 기능 Source34개 반영 후32개 해시 일치·BasicMonster2개 동시 테스트 인스턴스 변경 별도 대조. 본 기능의 바이너리 에셋·모듈/Build.cs 변경 없음, Studio 제거 유지.
- 상태: 구현·원본 반영·빌드·실제 기능/화면 검증·독립 리뷰·문서 완료. 로컬 단일 플레이 저장을 지원하며 현재 런·현재 HP/MP/스태미나·월드 드롭 액터는 저장 대상이 아니다. 커밋·푸시 없음.
- 관련 문서: [설계](../Feature/architect/2026-10-11_town-services-persistence.md), [결과](../Feature/doc/2026-10-11_town-services-persistence.md), [사용 가이드](TownServicesPersistenceGuide.md), [독립 리뷰](../Feature/doc/evidence/town-services-persistence-20261011/review.md), [개발 일지](../DevLog/20261011.txt).

### Sprint5-26 — 현재 위치 몬스터 테스트 UI (2026-10-11)

- 목표: 넓은 맵을 이동하지 않고 일반 몬스터와 아자토르를 현재 위치에서 바로 시험한다.
- 주요 변경: 별도 C++ UMG 창/테스트 컴포넌트, F6 및 안내 버튼, 실제6종 안전 지면 소환·최대6마리·소유 대상 정리, 기본/정면/영역·HP40% 강화·초기화·자동 AI·회복·무적, pause/focus/캐릭터 전환 복원, 테스트 대상 보상 차단. F8 에디터 단축키 충돌을 피함. 바이너리 에셋·맵·원본 데이터·스튜디오 변경 없음.
- 검증: 최종 Editor 빌드9.86초 성공; F6 실제 게임1280×720 80확인·실패0, 1920×1080 추가80확인·실패0; 실제 Slate hit-test 마우스 클릭·키/ESC·피해·보상·수명/안전 경계·화면; 새 Editor PIE 창/pause/blocking·공식 MCP 캡처; 최종 OpenRunMap·PIE없음·dirty0, CheckGeneratedChanges/diff --check 통과. 초기 실패와 수정 이력 보존.
- 상태: 구현·집중 QA·검토·문서 완료. 작은 에디터 뷰포트는 F11 또는 새 에디터 창 사용 권장. Shipping 패키징·멀티플레이·장시간 GPU 안정성은 별도. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_monster-test-ui.md, Feature/doc/2026-10-11_monster-test-ui.md, Feature/doc/evidence/monster-test-ui-20261011/, DevLog/20261011.txt.

### Sprint5-28 — 몬스터 시야 추적과 종별 Niagara (2026-10-11)

- 목표: 일반5종과 아자토르의 플레이어 인식→추적→공격을 보강하고 공격·실제 명중을 종별 효과로 표시한다.
- 주요 변경: 주변360도 시야·정지/재개·리시 재인식 차단·이동 Actor 경로 유지, F6 자연 인식과 짧은 사거리 테스트 보정, 종별 CPU burst6개+보스 특수2개 제작 및 프로필6개 연결, AutoRelease 완료 참조 정리, 다중 Editor 프로젝트 경로 보호.
- 검증: 원본 통합 빌드32.42초 성공, 최종 격리110 및 원본110검증 실패0, 기존 F6 UI80검증 실패0, Niagara8개 컴파일/스택 오류·경고0, 실제 종별/보스 화면, 생성 경로·diff·Python 문법 검사 통과. 초기 실패와 수정 기록 보존.
- 상태: 구현·한정 원본 통합·빌드·실제 플레이 QA·검토·문서 완료. 다른 채팅의 특성·저장 변경 보존. 로컬 단일 플레이 검증; 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_monster-effects-chase.md, Feature/doc/2026-10-11_monster-effects-chase.md, Feature/doc/evidence/monster-effects-chase-20261011/review.md, DevLog/20261011.txt.

## Sprint#5-27 — 특성 16종·강화 UI 실제 적용 (2026-10-11)

- 목표: 설계한 16종을 실제 전투에 적용하고 아린의 독립 특성 강화창·영구 저장·무료 재분배를 제공한다. 인벤토리·스킬·성장은 각각 별도 관리한다.
- 주요 변경: 카탈로그 Data Asset/CDO16종, 실제 비용·누적 보너스 투자 기록, 16종 능력치/조건부 효과, v1→v2 보존 전환과 손상 거절, 쓰기 실패 재시도, 네이티브 UMG 필터·상세·입력 복구, 공식 Editor 임포트 UI Texture1개.
- 검증: 최종 Editor 빌드9.76초 성공. 효과100·저장125+227·UI260·기존 서비스60+25/런42+15, 총854확인·실패0. 실제 Slate 키·마우스·16카드 스크롤·재분배 모달·저장 재시도, 독립 검토, 생성 경로·공백 검사, 새 Editor 클래스3개/16종/Texture 재로드·dirty0 확인.
- 브랜치: 최초 Sprint#5-27-trait16-runtime에서 시작. 동시 몬스터 작업이 공유 원본을 Sprint#5-28-monster-effects-chase로 전환하여 현재 브랜치와 다른 변경을 보존했다.
- 상태: 구현·빌드·실제 단일 플레이 QA·독립 검토·문서 완료. 초기 실패 증거 보존. Shipping cook·멀티플레이·장기 밸런스는 별도. 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-11_trait16-runtime.md, Feature/doc/2026-10-11_trait16-runtime.md, Docs/TraitEnhancementGuide.md, Feature/doc/evidence/trait16-runtime-20261011/, DevLog/20261011.txt.

## Sprint#5-29 — 플레이어 시야에 맞춘 상체 애니메이션 (2026-10-13 완료)

- 브랜치: `Sprint#5-29-view-aligned-upper-body`. 10월11일 시작한 작업을13일 최종 검증했다.
- 목표: 현재 외형 메시의 상체·팔 애니메이션이 플레이어의 시야각을 따라 움직이고 하체·손목·손가락과 머리 카메라의 회전 계약을 보존한다.
- 주요 변경: native AnimInstanceProxy 최종 포즈 보정, pitch75도·잔여 yaw45도 제한, 척추25%·양팔 나머지75% 분배, 지수 보간, 목/머리 원본 회전 복원과 FK/IK 손 동기화. 독립 원본 대조·실제 입력·외형 리타깃 QA, 기존 이동 원본 검사용 중립 조준 선택과 회귀 QA 저장 경로 격리.
- 검증: 최종 Editor 빌드8.23초 성공. 집중 통합/실제 렌더 각각21상황·1,575프레임·460,488검사·실패0, 원본 대조459,385검사·실패0, 부정 대조는 조준 관련4종만2,456건 검출. 이동1,100/가드640/카메라1,572/맨손 전투840프레임 회귀 모두 실패0. 20초 주요 공격·52.5초 전체 실행영상과 독립 소스/실제 화면 검토 완료.
- 보존과 복원: Content6,359개 크기/mtime·추가·삭제 변경0, Common AnimBP SHA 동일. 초기 QA 저장 접두사 오류로 변경된 진행도는 작업 전 전체 SHA가 같은 원본 바이트로 Unreal API를 통해 복원했고 생산 저장3개 SHA를 재확인했다. 일회 복구 도구는 Source에서 제거했다. 기존 OpenRunMap·PIE 종료·dirty0과 새 native 기본값 확인.
- 상태: 구현·빌드·범위 내 QA·독립 검토·문서 완료. 현재 인간 남성 LOD1 검증이며 극단 상향 V회피의 근접 클리핑·고각 머리 위치 변화는 한계로 기록했다. 기존 미커밋 작업을 보존했고 커밋·푸시는 없다.
- 관련: [설계](../Feature/architect/2026-10-11_view-aligned-upper-body.md), [결과](../Feature/doc/2026-10-11_view-aligned-upper-body.md), [독립 QA](../Feature/doc/evidence/view-aligned-upper-body-20261011/qa-review.md), [최종 소스 리뷰](../Feature/doc/evidence/view-aligned-upper-body-20261011/final-source-review.md), [일일 로그](../DevLog/20261013.txt).

## Sprint#5-30 — 마우스 기준 머리 안정화·안전 모션·속도 연동 (2026-10-13)

- 브랜치: `Sprint#5-30-mouse-head-stable-locomotion`.
- 목표: 걷기2도·달리기4도·공격3도 안으로 머리/카메라 잔여 회전을 제한하고, 상체 시선·하체 보행·공격/이동 속도를 일치시킨다. 스핀·뒤돌아차기·구르기를 활성 경로에서 제외한다.
- 주요 변경: mouse head native proxy와 평가 직전 제한 재캡처, 포즈/basis snapshot·반복 camera read·작은 위치 잔여, Movement→Source→Appearance 순서, 비용 전 금지 모션 gate, DA35개와 회피 안전 연결, 파생sequence7+montage33, grounded BlendSpace rate getter와 발자국 cadence. 원본 첫root 높이·게임플레이 수치·섹션/Notify 보존.
- 검증: 최종 Editor 빌드56.43초. 실제 통합/렌더 각각24상황·1800프레임·489428기록 검사·실패0. 비활성 대조21상황은 머리 관련3151건 예상 검출. Fist840프레임·9스킬14실제 피해/머리 그림자/V, Moving1100프레임·258033검사·9스킬16피해 회귀 통과. fresh74DA/13프로필·unsafe active0, 독립source20파일·PNG9장 검토. 25초/60초 실행영상.
- 보존: source455개 중14개 수정·7개 추가·삭제0. Content6359개 중 의도한37개 변경·40개 추가·다른 변경/삭제0, 생산 저장3개 SHA/목록 동일. OpenRunMap·PIE종료·dirty0·native2/4/3기본값 확인.
- 상태: 구현·빌드·범위 내 수치/렌더·독립 검토·문서 완료. 초기 실패 증거 보존. 인간 남성LOD1 단일 플레이 범위, 기본1인칭 HUD 손 가림·모든 target 손가락/지면접촉 불인증을 명시했다. 기존 미커밋 작업 보존, 커밋·푸시 없음.
- 관련: Feature/architect/2026-10-13_mouse-head-stable-locomotion.md, Feature/doc/2026-10-13_mouse-head-stable-locomotion.md, Feature/doc/evidence/mouse-head-stable-locomotion-20261013/qa-review.md, Feature/doc/evidence/mouse-head-stable-locomotion-20261013/final-source-review.md, DevLog/20261013.txt.

## Sprint#5-31 — 대검·맨손 아이덴티티 이미지 (2026-10-14)

- 목표: 중앙 원형 HUD와 어울리는 대검·맨손 상징을 투명 PNG 원본으로 제작한다.
- 주요 변경: built-in imagegen 자산별 생성1회·여백 편집1회, 흑철/황동/붉은 대검과 돌주먹/금빛/비취 기운, SourceArt 원본·정확한 프롬프트 보존. .gitignore에서 최종 PNG2개만 예외 처리.
- 검증: 최종2개1254×1254 RGBA/alpha0~255, 본체 높이75.76%·72.65%, 실제 원본 독립 기술·시각 검토 PASS, 파일SHA·프롬프트·Git제외안됨 확인. Unreal source/Texture 변경이 없어 빌드 미실행.
- 상태: 이미지 제작·원본 보존·검토·문서 완료. 실제 HUD import/binding와68px 실행 배치는 별도. 기존 작업 보존, 커밋·푸시 없음.
- 브랜치: Sprint#5-31-identity-image-assets.
- 관련: Feature/architect/2026-10-14_identity-images.md, Feature/doc/2026-10-14_identity-images.md, Feature/doc/evidence/identity-images-20261014/file-qa.json, Feature/doc/evidence/identity-images-20261014/visual-qa.md, DevLog/20261014.txt.


## Sprint#5-32 — 좌우 하단 전투 HUD 적용 (2026-10-14)

- 브랜치: Sprint#5-32-split-combat-hud. 기존 Sprint#5-31 원본과 미커밋 작업 보존.
- 목표: 승인한 몬스터헌터 참고 imagegen 시안대로 좌 생존/LV/EXP·우스킬/아이덴티티/잠금4를 실제 게임에 적용하고 중앙 시야를 확보한다.
- 주요 변경: native 반응형 두코어·고정gap96·초록HP 표시·마력교체보존·획득피드위배치, beveled native frame, 최신맨손/대검2Texture 표시override, 신규Split6파트/기존MainBottom2연결, 기존Compact모드복구flag.
- 검증: 최초 Editor 빌드45.12초·프레임수정후최종링크3.88초성공. 최종7조건·1,334확인·실패0, PNG7장·기존.sav67개byte보존. UI80기존중78개동일/2변경/8신규/삭제0, rawPNG2동일, 저장계약·새Editor재로드·dirty0·PIE종료, 독립정적/3해상도시각리뷰·생성경로·공백/문법검사통과.
- 상태: 구현·저장·검증·재로드·문서 완료. 발견한frame배열assert/비율glyph테두리겹침과검증API문제수정, 실패증거보존. 인벤토리·스킬·성장·특성관리/저장스키마·밸런스·맵보존. Shipping/네트워크/장기GPU·아주작은글자별도. 커밋·푸시없음.
- 관련: Feature/architect/2026-10-14_split-combat-hud.md, Feature/doc/2026-10-14_split-combat-hud.md, Docs/SplitCombatHUDGuide.md, Feature/doc/evidence/split-hud-20261014/, DevLog/20261014.txt.


### 사용자 승인 커밋·푸시 준비 (2026-10-14)

- 범위: HUD 네이티브6개·전용10에셋·원본PNG2개·공통 작성도우미와 검증/문서/증거. 그 외 기존 미커밋 기능은 제외한다.
- 보완: 통합 QA의 별도 진행 저장 의존성을 조건부 컴파일·실행 전 검사로 명시한다. 에디터는 종료하지 않고 임시 standalone에서만 오류 종료한다.
- 추가 검증: HEAD 추적 파일+스테이징 HUD 소스7개만 있는 독립 Editor 빌드와 최종 QA 파일 재컴파일·링크 성공. 독립 범위/종료 보호 검토 미해결0.
- 상태: 사용자 승인에 따라 이 작업 브랜치의 커밋과 일반 원격 푸시를 진행한다. Sprint는 main 병합 전이므로5를 유지한다.
- 파일 목록/검증: Feature/doc/evidence/split-hud-20261014/commit-files.txt, commit-clean-build.log, commit-clean-build-final.log.

## Sprint#5-33 — 중앙 확장형 혼합 성장 스킬트리 설계 (2026-10-18)

- 브랜치: `Sprint#5-33-hybrid-skill-tree-design`. 기존 Sprint#5-32 체크아웃과 미커밋 작업을 유지하며 새 로컬 브랜치로 전환했다.
- 목표: 사용자 참조 이미지의 중앙→아이덴티티·기본기→핵심 기술 구조와 격투 숙련·레벨업 포인트 혼합 성장 규칙을 설계한다.
- 주요 변경: D1~D4 패킷, 기존 초기5기술 보존과9잠금 무공의 숙련/스탯 경로 제안, 아이덴티티 입문1P·일부 심화2P 또는 숙련 성장·성질2P·양부모 공명, 특성 공유 포인트 원장, 일회 보상·활성1계열·재분배/저장 이전 정책. 대표 정권 경로 클릭 구조도와 참조/검토 증거.
- 검증: 독립 조사·최종 설계 검토 미해결0, 단계/선행·스탯/XP·포인트 산술, 구조도19노드·736/320/다크/터치 선택·요약·넘침/겹침/스크립트 오류0, 원본 참조SHA일치, Harness 경로 통과. 문서/로그 공백·링크와 기존 변경 보존 확인.
- 상태: Pre-production 설계·검토·문서 완료. 실제 C++/원형UMG/노드DataAsset/숙련XP/공유 원장/새 저장·밸런스 검증은 후속 Production. 소스·설정·Unreal 바이너리·생산 저장 변경 없음, Editor 빌드 미실행. 커밋·푸시 없음, Sprint5 유지.
- 관련: [설계](HybridSkillTreeDesign.md), [계획](../Feature/architect/2026-10-18_hybrid-skill-tree-design.md), [결과](../Feature/doc/2026-10-18_hybrid-skill-tree-design.md), [검토](../Feature/doc/evidence/hybrid-skill-tree-20261018/review.md), [일일 로그](../DevLog/20261018.txt).

### imagegen 화면 시안 추가 (2026-10-18)

- 목표: 사용자 후속 요청에 따라 확정 성장 규칙을 실제 화면 형태의 이미지로 보여준다.
- 주요 변경: D5 패킷, built-in 참조 생성1회·상태/글자 교정1회, 흑철·황동 방사형 트리의 최종PNG와 두 프롬프트 보존. LV4/M12·내공 심화0P·잔액0P·활성내공·미래기술/공명잠금 표시.
- 검증: 최종 이미지 시각 검토·PNG 치수·생성 원본과 복사SHA 일치. 첫 출력의M9 스태미나/비활성입문 상태·일부 기술명 교정. 일부 중간 노드는 대표 표현으로 생략함.
- 상태: 화면 시안 제작 완료. 실제 원형UMG/공유 원장/숙련 시스템 구현은 후속 범위. 기존 브랜치 유지, 커밋·푸시 없음.
- 관련: [최종 이미지](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-mockup-v2.png), [생성 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v1.txt), [교정 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v2.txt).

### 격투가 아이덴티티 내공·투지 확정 (2026-10-18)

- 목표: 사용자 확정에 따라 격투가는 내공·투지만 사용하도록 설계와 최신 이미지 시안을 정리한다.
- 주요 변경: 격투가의 마나 표/성질/산식 제거, 직업 허용 목록 검증 명시, built-in 편집1회로마나 가지를 제거한v3 PNG·프롬프트 보존. 포인트/숙련 혼합 해금과활성1계열 정책 유지.
- 검증: 상세 설계와v3 이미지의내공·투지 두계열 대조, 기존성장/잔액/수련0P유지, 생성원본/복사SHA동일, 문서공백·링크/생성경로검사.
- 상태: 설계·최신화면시안수정완료. 기존HTML·v2는변경전기록, 실제런타임구현후속. 기존브랜치유지·커밋/푸시없음.
- 관련: [설계](HybridSkillTreeDesign.md), [현재v3 이미지](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-mockup-v3.png), [현재 편집 프롬프트](../Feature/doc/evidence/hybrid-skill-tree-20261018/imagegen-prompt-v3.txt), [일일 로그](../DevLog/20261018.txt).

## Sprint#5-34 — Unreal MCP 설치 스킬 Epic 최신 원본 보완 (2026-10-19)

- 브랜치: `Sprint#5-34-unreal-skill-upstream-sync`. 기존 Sprint#5-33과 미커밋 작업 보존, main 병합 전이므로 Sprint5 유지.
- 목표: 설치된 개인 Codex Unreal MCP 스킬과 Epic 최신 원본을 비교하고 누락 지침을 Codex 방식으로 반영한다.
- 주요 변경: Epic 3.1.1/a6aa73ad 기준 Markdown 9개 보완. AllToolsets/provider, proxy status·catalog·복구, 독립 호출 병렬/의존 순차, native dispatch, toolset async/타입/테스트, Agent Skill class path·저장, 실제 설치 경로와 Saved/커밋 규칙, 원본 대응표.
- 검증: candidate/설치본 공식 구조 검사·설치 전용 검사, 참조15·예제3·스크립트 AST/YAML/MIT 대조, 독립6사례·timeout 모호성1건 수정/재확인, 설치20파일 SHA256·파일 수 확인, 생성 경로/문서 검사 통과.
- 상태: 비교·보완·개인 설치 반영·정적/행동 검토·문서 완료. 원본20파일 로컬 백업 보존. 8001·자동 호출·runner8·MIT·기존 프로젝트 변경 유지. 실제 엔진에는 proxy가 없어 라이브 MCP/PIE/빌드·품질 향상은 검증하지 않음. 커밋·푸시 없음.
- 관련: [계획](../Feature/architect/2026-10-19_unreal-skill-upstream-sync.md), [결과](../Feature/doc/2026-10-19_unreal-skill-upstream-sync.md), [검토](../Feature/doc/evidence/unreal-skill-sync-20261019/review.md), [일일 로그](../DevLog/20261019.txt).

## Sprint#5-35 — Unreal MCP 플러그인 직접 설정 (2026-10-19)

- 브랜치: `Sprint#5-35-unreal-plugin-setup`. 기존 Sprint#5-34와 미커밋 작업 보존, main 병합 전이므로 Sprint5 유지.
- 목표: 현재 설치 스킬에 필요한 Unreal 플러그인과 에디터 설정을 직접 보완하고 실제 연결을 확인한다.
- 주요 변경: LiveCodingToolset Editor 한정 활성, 공식 설정 API의 Live Coding 활성·저장, tracked LiveCoding 기본값 추가. 기존 MCP/AllToolsets·8001 자동 시작/search·AIAssistant 비활성·Codex config 보존.
- 검증: DLL/BuildId 55116800·의존성·Editor 제한 독립 검토, 실제 loopback8001/필수3플러그인/53toolset·3meta/CompileLiveCoding schema, settings·PIE/Entry 조회, 소유 에디터 정상 재시작 후 LiveCoding=True와 MCP4설정 유지, 문서/생성 경로 검사.
- 상태: 플러그인·기능 활성·지속성·실제 MCP 연결·독립 검토·문서 완료. 에디터 실행 중, compile/게임 빌드/PIE/패키징 미실행. native 도구 목록은 필요 시 Codex 기존 연결 재연결. 커밋·푸시 없음.
- 관련: [계획](../Feature/architect/2026-10-19_unreal-plugin-setup.md), [결과](../Feature/doc/2026-10-19_unreal-plugin-setup.md), [가이드](UE58MCPSetup.md), [검토](../Feature/doc/evidence/unreal-plugin-setup-20261019/review.md), [일일 로그](../DevLog/20261019.txt).

## Sprint#5-36 — Fab 보유 에셋 우선 재사용 원칙 (2026-10-19)

- 브랜치: `Sprint#5-36-fab-owned-asset-reuse`. 기존 미커밋 작업과 Sprint5 번호 유지.
- 목표: 사용자 보유 Fab 에셋을 우선 활용하도록 실제 목록과 프로젝트 자원을 대조하고 개발 지침에 저장한다.
- 주요 변경: Fab 내 라이브러리 최신순 일부/Combat/Monster 카드30개와 로컬 주요8팩 조사. AGENTS 재사용 규칙, 영역별 후보·기존 계약 보존·선별 반입/검증 가이드, 출처/현재 상태 구분과 부분 조사 기록.
- 검증: 실제 런처 화면 확인·독립 로컬 조사/문서 검토, JSON/문서 링크·기존 로그/프로젝트 변경 보존·생성 경로 검사.
- 상태: 조사·상시 지침·활용 가이드·기록 완료. 전체 보유 목록·버전/라이선스·실제 적용은 미검증. 구매/다운로드/설치/import·게임 소스/설정·빌드/PIE 변경 없음. 사용자 런처 입력 감지 후 추가 조작 중단, 커밋·푸시 없음.
- 관련: [가이드](FabAssetReuseGuide.md), [계획](../Feature/architect/2026-10-19_fab-owned-asset-reuse.md), [결과](../Feature/doc/2026-10-19_fab-owned-asset-reuse.md), [보유 카드 기록](../Feature/doc/evidence/fab-owned-assets-20261019/launcher-observations.json), [일일 로그](../DevLog/20261019.txt).

### Sprint#5-36 후속 — 기본 애니메이션·VFX 편집 우선 (2026-10-19)

- 목표: 기본 동작·효과 제작을 보유 에셋 수정/편집 중심으로 구체화한다.
- 주요 변경: AGENTS와 활용 가이드에 파생본 편집 기본 경로, 애니메이션/효과별 조정 범위와 원본·카메라·판정 보존, 새 제작이 필요한 경우의 이유 기록을 추가했다.
- 검증/상태: 문서 검토·경로/공백 검사, 선호 반영 완료. 실제 에셋·소스·설정·빌드/PIE 변경 없음, 같은 작업 브랜치·기존 변경 보존, 커밋·푸시 없음.
- 관련: Docs/FabAssetReuseGuide.md, Feature/architect/2026-10-19_fab-owned-asset-reuse.md, Feature/doc/2026-10-19_fab-owned-asset-reuse.md, DevLog/20261019.txt.

## Sprint#5-37 — 현재 상태 main 통합과 완료 후 자동 병합 (2026-10-20)

- 브랜치: `Sprint#5-37-main-checkpoint`. 원격 확인 후 기존 누적 변경을 유지하며 통합 전용 작업 브랜치를 만들었다.
- 목표: 현재 상태를 main에 커밋·푸시하고, 이후 기능 완료마다 main까지 통합하는 사용자 상시 승인을 저장한다.
- 주요 변경: 누적 Sprint5 게임/에셋/도구/설계/증거의 스냅샷, AGENTS·Git/Lazy 규칙 보완, P1 조사·P2 검증·P3 게시 패킷과 한국어 기록. 이번 통합에서 새로운 게임 동작은 구현하지 않는다.
- 검증: 원격 브랜치 일치/조상 관계·파일 크기/고신뢰 자격 증명·생성 경로 조사, 프로젝트 파일 생성 성공(16.47초). Editor 빌드·스테이징 검사와 원격 게시 결과는 완료 전에 아래에 추가한다.
- 상태: 현재 검증·게시 진행 중. main 병합 성공 후 Sprint5 종료, 다음 기능은 `Sprint#6-1-...`.
- 관련: [계획](../Feature/architect/2026-10-20_main-checkpoint.md), [결과](../Feature/doc/2026-10-20_main-checkpoint.md), [증거](../Feature/doc/evidence/main-checkpoint-20261020/), [일일 로그](../DevLog/20261020.txt).

### Sprint#5-37 검증 완료

- 프로젝트 생성16.47초·전체 Editor 빌드144.03초/77 actions 성공, 격리 Write/Read 통합 QA 실패0·exit0, 생성 경로/활성 파일 공백 검사 통과. 기존 증거 원문 공백 보존과 캐릭터 교체 실패 저장 연결 후속 점검을 결과 문서에 명시했다. 스냅샷 커밋·승격·원격 게시 준비 완료.

### Sprint5 체크포인트 로컬 main 반영

- 스냅샷: `207c595`(Source103개·전체6084개 변경). 별도 인덱스로 검증 후 병행 구현9개를 제외하고 작업→develop→test→main fast-forward 완료.
- 상태: 로컬 main 통합 완료, Sprint5 체크포인트 종료. 신규 브랜치는 Sprint#6-1부터, 이미 만들어진 Sprint#5-38과 미완성 스킬트리 코드는 보존한다.
- 원격: chickenbuger/Rogue10m의 사용자/소유자·admin/push 권한·공개 상태를 확인했으나, 자동 승인 검토가 공개 자료 전송의 별도 승인을 요구해 push를 차단했다. 구체적 목적지/자료 범위를 제시하여 사용자 승인 대기. 원격 성공으로 보고하지 않는다.

## Sprint#5-39 — 보스 처치·런 정산·시작마을 복귀 (2026-10-20)

- 브랜치: `Sprint#5-39-boss-run-completion`. Sprint5 main 병합 전에 생성한 번호를 유지한다. 병행 스킬트리와 원본을 보존하는 별도 작업 폴더에서 개발했다.
- 목표: 아자토르의 실제 사망을 승리·기존 보상 정산·진행 저장·결과 화면·StartHub 귀환으로 연결하고 일반 사망/시간 초과도 종료한다.
- 주요 변경: R1 GameState 최초 종료 가드와 GameMode 소유 RunCompletionComponent·사망 연결, R2 네이티브 결과 UI/입력 차단·저장 실패 재시도·옵션 없는 마을 이동, R3 격리 QA/빌드/독립 검토와 기존 특성 fixture의 종료 계약 보완, R4 결과·DevLog·최종 통합 준비. 기존 보스 XP3,000/골드300을 한 번만 지급하며 새 보상·SaveGame 스키마·스킬트리 경제·원본 바이너리는 변경하지 않는다.
- 검증: 콘텐츠 고정 Editor 빌드90.27초 성공. 렌더된 실제 게임 Write53·Read12·Failure58·FailureRead12·Timeout33·Death32, 합계200검사·실패0·여섯 프로세스 exit0. 기존 Trait Effects 회귀101검사·실패0·exit0, 총301검사 통과. 실제 StartHub 포탈→OpenRunMap 저장 보스→플레이어 좌클릭 마지막 피해→결과창 Enter→StartHub 및 별도 프로세스 복원. Windows 읽기 공유 잠금의 실제 저장 실패·귀환 차단·재시도 보상 불변, 전체 Growth/Inventory GUID·골드/Combat/Town 스냅샷 일치. 실제 승리/실패/시간 초과 화면과 독립 코드 검토 완료.
- 보존과 한계: Write/Read 사이 공유 콘텐츠 변경을 검출한 뒤 연결을 제거하고207c595의6,417콘텐츠 파일·트리890c83ca26eb5ac410bbd9b0d34e406c9a3c44e3를 고정했다. 비교 완화 없이 재검증, 원본 소스·스킬트리·에셋 유지. 순간이동·AI정지·준비 중 임시 무적·1HP 보스 마무리·1초 타임아웃 fixture 범위이며 전체20분/밸런스/Shipping/D3D12/장시간GPU/네트워크는 미검증이다.
- 상태: 구현·빌드·핵심 QA·특성 회귀·독립 정적 검토·문서 완료. 최종 경로 검사·안전한 원본 통합·커밋·로컬 승격은 준비 중이다. 이번 기능의 push는 미시도이며 기존 체크포인트 채팅의 공개 전송 승인 대기 상태가 이어진다. main/원격 반영 완료로 보고하지 않는다.
- 관련: [설계](../Feature/architect/2026-10-20_boss-run-completion.md), [결과](../Feature/doc/2026-10-20_boss-run-completion.md), [최종 실행](../Feature/doc/evidence/boss-run-completion-20261020/frozen-runtime-launcher.log), [콘텐츠 고정](../Feature/doc/evidence/boss-run-completion-20261020/content-freeze.json), [독립 검토](../Feature/doc/evidence/boss-run-completion-20261020/review.md), [일일 로그](../DevLog/20261020.txt).

- Sprint#5-39 최종 통합: 필수 경로·67파일 범위·공백·PowerShell 구문 검사 통과 후 기능 커밋 `c680f1ba689d643956c954023d306c45404c8a54`, develop→test→main 로컬 fast-forward 완료. 원격 fetch와 조상 관계·체크아웃·이전 해시 대조 완료. 원본5-38 브랜치/HEAD 및 병행 스킬트리 보존을 Source14개 독립 대조로 확인. [통합 결과](../Feature/doc/evidence/boss-run-completion-20261020/local-integration.json). 최종 기록 문서는 별도 커밋으로 같은 경로로 승격한다. 공개 전송 승인은 계속 대기 중이며 push 미시도다.

## Sprint#5-38 — 격투가·대검술사 혼합 성장 스킬트리 (2026-10-20)

- 브랜치: `Sprint#5-38-hybrid-skill-tree-runtime`. main 병합 전 생성한 번호를 유지하고, 공유 보스 정산 `f49915b4`를 기준으로 기존 작업을 보존했다.
- 목표: 확정한 중앙 확장형 시안과 숙련/레벨업 포인트 혼합 설계를 두 직업의 실제 성장·전투·저장·K 메뉴에 연결한다.
- 주요 변경: 53노드 카탈로그, PlayerState 숙련·공유 실제 지출 원장, Save v3/legacy 이전, 명중 실행당1XP, 격투가 내공·투지/대검 투지·마나, 성질·공명·MP 소비·부분 환급, 대검 파생5개, native 원형 UI·조건·키 지정. 기존 기본 공격·프로필 입력·몽타주 보존.
- 검증: 최종 Editor 빌드 성공(83.55초), Automation3개 Success, 실제 게임50개/새 프로세스 저장 복원14개 확인에서 실패0·exit0. 원본 공격19개·초기 계약 보존, 1080p/720p 실제 화면 검토. 생성 경로·스테이징 목록/공백·스크립트 구문·문서 링크 검사 통과.
- 상태: 구현·통합 QA·독립 리뷰·기록 완료. 기능 커밋 `a2a1db8b`와 로컬 develop→test→main fast-forward 승격 완료, 최종 통합 기록은 후속 문서 커밋으로 확정한다. 공개 푸시는 선행 체크포인트의 공개 자료 전송 승인 대기를 존중하며 우회하지 않는다. 전용 무기 애니메이션·무기별 archive·20분 전체 플레이·Shipping·멀티는 별도 범위다.
- 관련: [계획](../Feature/architect/2026-10-20_hybrid-skill-tree-runtime.md), [결과](../Feature/doc/2026-10-20_hybrid-skill-tree-runtime.md), [설계](HybridSkillTreeDesign.md), [실제 격투가 화면](../Feature/doc/evidence/hybrid-skill-tree-runtime-20261020/brawler-1080p.png), 실제 대검 화면〔중간·보조 자료 정리로 삭제됨; [보존 목록](../Feature/doc/evidence/README.md)〕, [검토](../Feature/doc/evidence/hybrid-skill-tree-runtime-20261020/review.md).

### 공개 GitHub 게시 승인과 통합 게시 재개 (2026-10-20)

- 사용자가 현재 자료와 향후 완료 기능을 공개 chickenbuger/Rogue10m에 게시하도록 명시 승인했다. AGENTS·Git/Lazy 지침에 목적지와 계속되는 승인을 기록한다.
- 범위: 최초 체크포인트207c595/05f74f1 + 보스c680f1ba/f49915b4 + 스킬트리a2a1db8b/77a52b6a. 분리 worktree의 미완성 무기/애니메이션 제외, 최초 Sprint#5-37 참조 보존.
- 검증: 원격 main 대비7 ahead/0 behind, 추가139파일·151blob·크기/자격증명/생성경로 검사, 최종 Source/DataAsset tree 일치. 보스301검사·스킬트리자동화3/실행50/복원14 통과, 통합 Editor 빌드83.55초 성공 기록 확인.
- 상태: 공개 승인 차단 사유 해소, 문서 한정 커밋·일반 atomic push 및 원격 SHA 검증을 진행한다. 실제 완료 결과는 뒤에 추가한다.
- 관련: Feature/doc/2026-10-20_main-checkpoint.md, Feature/doc/2026-10-20_boss-run-completion.md, Feature/doc/2026-10-20_hybrid-skill-tree-runtime.md, DevLog/20261020.txt.

### 공개 원격 게시 완료

- 일반 atomic push 성공. 실제 원격 main/develop/test와 게시 작업 브랜치 b57985e1을 직접 확인했다.
- 원격 작업 참조: Sprint#5-37=05f74f1, Sprint#5-38=77a52b6a, Sprint#5-39=f49915b4. 체크포인트·보스·스킬트리와 공개 게시 상시 승인 규칙이 모두 원격에 포함된다.
- 진행 중인 무기/애니메이션·파일 최적화는 미포함, 강제 푸시·공유 체크아웃 변경 없음. 공개 승인 차단 해소.
- 원문: Feature/doc/evidence/main-checkpoint-20261020/public-push.log. 직접 원격 해시 검증: public-push-result.json. 이 성공 기록만 후속 문서 커밋·푸시한다.

## Sprint#6-2 — 과거 제작 파일과 중복 산출물 최적화 (2026-10-20)

- 목표: 과거 제작·검증 파일의 참조와 중복을 확인해 현재 작업 폴더의 디스크 부담을 줄인다.
- 주요 변경: 동일 SHA 보존 사본이 있는 Blender 백업4개 정리, 120Hz JSON3개 무손실 gzip 보관, 기존 검증기의 gzip fallback과 압축 baseline 재캡처 차단. 원본345,395,090바이트→새 archive36,988,290바이트, 순감소308,406,800바이트(294.12MiB; 새 기록 크기 별도).
- 검증: 원본7개/보존 사본4개 독립 SHA·크기 대조, gzip 전량 해제 해시, 실제7개 복구→해시 확인→재정리. 실제 reader/main 오프라인7검사·압축본3개 각10클립 파싱 통과. 현재 Source/Config/Content 변경0으로 Editor 빌드·PIE 반복 없음. 최종 경로·구문·스테이징·통합 결과는 후속 기록.
- 보존: 승인 기준 .blend1·고유 모션 백업, FBX .fbm 재수입 경로, 현재 아트/입력/스크립트, Fab 원본과 실패 로그, 등록 worktree·진행 중 무기 작업. tmp 전체 감사는 비용 때문에 읽기만 중단했고 tmp 삭제0.
- 상태: 정리·복구 검증·독립 검토·문서 완료. 선행 공개 게시 main97cc9191을10개 파일 blob 동일 대조 후 작업 파일 덮어쓰기 없이 반영했다. 최종 커밋·승격·푸시 준비 중.
- 관련: [계획](../Feature/architect/2026-10-20_unused-file-optimization.md), [결과](../Feature/doc/2026-10-20_unused-file-optimization.md), [확정 목록](../Feature/doc/evidence/unused-file-optimization-20261020/selection.json), [복구](../Feature/doc/evidence/unused-file-optimization-20261020/RESTORE.md), [개발 일지](../DevLog/20261020.txt).

- Sprint#6-2 최종 파일 검사: 변경32개 명시 목록, Python6개·JSON8개·문서 링크10개·append-only DevLog/Sprint·Source/Config/Content 변경0 통과. 생성 경로 검사 통과, 스테이징/공백 및 커밋·승격 준비.

- Sprint#6-2 완료: 기능 커밋 `a70c521649eca752ef2e3f0e17f9ae7f216b9050`, 변경32개 스테이징/필수 경로/공백 통과, 원격·조상·worktree 확인 후 develop→test→main CAS fast-forward와 일반 atomic push 완료. 원격4개 직접 SHA 일치 확인. [로컬 통합](../Feature/doc/evidence/unused-file-optimization-20261020/local-integration.json), [원격 검증](../Feature/doc/evidence/unused-file-optimization-20261020/public-push-result.json). 첫 줄바꿈 입력 오류는 무변경 거절 후 LF로 수정했다. 이번 완료 기록도 같은 순서로 문서 커밋·반영한다.

## Sprint#7-2 — 보스 정산 Save v3 통합 보완 (2026-10-20)

- 목표: 스킬트리 병합 뒤 남은 보스 종료·저장·귀환 통합 검증과 최종 명중 숙련의 초기 저장 순서를 보완한다.
- 범위: V1 QA v3 전체 원장/디스크 비교·재현, V2 RunCompletion 저장 확정 순서·대기 가드, V3 빌드/렌더6프로세스·리뷰·기록·main 게시. 게임 보상/Save 버전/에셋 경제 변경 없음.
- 상태: 기준 main ddc79e5a 확인·작업 브랜치 생성·정적 원인 확인. 구현/런타임 재현·수정은 진행 중이며 기존301개를 최신 통합 성공으로 재사용하지 않는다.
- 관련: [계획](../Feature/architect/2026-10-20_boss-save-v3-integration.md), [결과](../Feature/doc/2026-10-20_boss-save-v3-integration.md), [일일 로그](../DevLog/20261020.txt).


## Sprint#7-1 — 미완료 tmp 감사와 후속 정리 (2026-10-20)

- 목표: 앞선 정리에서 중단된 전체 tmp 감사를 완료하고 입증된 과거 출력만 후속 정리한다.
- 변경: 후보/참조 단계별 보호 subtree·checkpoint 입구 제외, 중첩 프로젝트 저작 참조 유지, suffix 선별·진행/범위 기록, timestamp 새 출력·기존/동시 evidence 덮어쓰기 방지. 두 검토자 교집합인 Arin stdout2개/JSON2개만 정리(4개11947바이트).
- 검증: 최종12회귀 PASS, 실제 감사79.828초exit0·issues0·후보탐색2659/해시1060/참조2578·제외77/프로젝트6. 초기471개 전량 검토→checkpoint242 보호→최종229 중225보존·15참조보존. 원본4개/추적 canonical3개 해시·실제 복구→재정리·독립 최종 검토 PASS.
- 보존: 기존87개, 제작/glob/basename/CSV 입력, 고유 대형 출력·Fab/Content·현재 게임 소스. 수동 크기필터 오류는 삭제 전 수정·보고 재검증. 별도 보스 저장 작업의 Source3개·문서·공유 로그는 이번 커밋에서 제외하고 보존한다.
- 상태: 남은 감사·정리·검증·복구·리뷰 완료. 별도 인덱스의 명시 범위 검사 후 커밋·develop→test→main·공개 원격 반영 진행.
- 관련: [계획](../Feature/architect/2026-10-20_finish-unused-work-audit.md), [결과](../Feature/doc/2026-10-20_finish-unused-work-audit.md), [최종 감사](../Feature/doc/evidence/finish-unused-work-audit-20261020/tmp-audit-final.json), [복구](../Feature/doc/evidence/finish-unused-work-audit-20261020/RESTORE.md).

### Sprint#7-2 보스 Save v3 통합 — 구현·검증 완료, 게시 준비

- 작업 식별: `Sprint#7-2-boss-integration-validation`. 병행 `Sprint#7-1-finish-unused-work-audit` 번호와 기록을 보존하도록 보스 작업 번호를 조정한다.
- 완료: V1 최초 저장 누락 실제 재현, V2 terminal 즉시 고정·원래 Pawn/프로필GUID·약한 다음 틱 저장/UI·EndPlay 정리·pending 귀환 거절, V3 빌드·렌더 QA·회귀·독립 검토·문서 완료. Source3개만 변경하며 보상·Save 버전·원본 Content/Config 변경0이다.
- 수정 전: 빌드99.68초 성공, saved=1 후146ms의 최초 디스크/메모리 비교에서49개 중2개 예상 실패. 마지막 실제 공격의 숙련+1은 통과했다. 게임 exit0이어도 FAILED 마커/실패 수로 실행기exit1이 거절했으며 최종 성공 수치에 포함하지 않는다.
- 수정 후: 빌드180.25초 성공. v3green1 Write74·Read16·Failure76·FailureRead16·Timeout52·Death51=285개·실패0, 여섯 게임/전체 실행기 exit0. 기존 특성 효과101개·실패0·exit0을 합쳐386개 통과. 자동저장/귀환 전 최초 Save v3의 마지막 명중 숙련·전체 원장, 실패 재시도·하드 트래블·독립 재시작과 보상 불변을 확인했다. 승리/저장 실패 화면 직접 확인과 정적 검토 완료.
- 범위: 실제 마지막 타격·플랫폼 저장·UMG Enter·마을 이동을 통제 fixture로 검사했다. 전체20분/밸런스/Shipping/D3D12/네트워크/장시간 안정성은 별도이며 실행 중 공격 참조 제거를 확정 결함으로 주장하지 않는다.
- 상태: 기능·빌드·386검사·리뷰·기록 완료. Source/모듈 해시·생성 경로·스테이징 최종 검사와 커밋·로컬 main 승격·공개 게시 준비 중이다. 실제 커밋/원격 SHA는 성공 후 기록한다.
- 통합 준비: 병행 감사가 공유 브랜치/main을 전진시켰지만 Source/Config/Content는 동일하다. 감사 종료 후 별도 게시 인덱스·명시40파일·main compare-and-set으로 공유 브랜치와 인덱스를 보존하며 반영할 예정이다. 실제 승격·전송 성공은 아직 기록하지 않는다.
- 관련: [최신 결과](../Feature/doc/2026-10-20_boss-save-v3-integration.md), [Green 실행](../Feature/doc/evidence/boss-save-v3-integration-20261020/runtime-green-launcher.log), [검증 요약](../Feature/doc/evidence/boss-save-v3-integration-20261020/validation-results.json), [독립 검토](../Feature/doc/evidence/boss-save-v3-integration-20261020/review.md), [개발 일지](../DevLog/20261020.txt).


- Sprint#7-1 완료: 기능 커밋 `007c0902b661f69edb64602f70311dc55f34e041`, 별도 인덱스29개·구문/JSON/링크/범위/필수경로/공백 PASS. 원격·조상·worktree 확인 후 develop→test→main fast-forward와 일반 atomic push, 작업/통합4개 원격 SHA 일치. 병행 보스 소스/문서/로그 보존·게시 제외. [로컬 통합](../Feature/doc/evidence/finish-unused-work-audit-20261020/local-integration.json), [원격 대조](../Feature/doc/evidence/finish-unused-work-audit-20261020/public-push-result.json). 완료 기록도 같은 방식으로 반영한다.

- Sprint#7-2 공개 게시 완료: 기능 커밋 `64935351`, 별도 인덱스40개·필수경로/범위/공백 검사 PASS, 검증 뒤 추가 감사 b11aefe0의 게임 tree 동일 대조·이력 보존. 공유 브랜치/HEAD/index 유지, develop→test→main CAS fast-forward·일반 atomic push·원격4개 SHA 직접 일치. 빌드180.25초·386검사 실패0. [통합](../Feature/doc/evidence/boss-save-v3-integration-20261020/local-integration.json), [공개 확인](../Feature/doc/evidence/boss-save-v3-integration-20261020/public-push-result.json). 이 완료 기록도 후속 문서 커밋으로 같은 순서로 게시한다.

## Sprint#7-2 — 혼합 스킬트리 도형·드래그·실입력 마무리 (2026-10-20)

- 브랜치: `Sprint#7-2-hybrid-tree-input-polish`. 독립 작업 폴더에서 최신 main `e12df414`를 포함해 최종 빌드했다.
- 목표: 이전 visual-review에 남긴 아이덴티티 마름모·공명 이중 표식과 실제 HUD 드래그·키 선택/발동 검증을 마무리한다.
- 주요 변경: 외곽 클릭 영역·라벨 간격, drag 중 패널 투명/통과와 실제 HUD 장착, Drop/Cancel/창 닫힘·컨텍스트 종료 복원/소유 operation 취소, 기술명·아이콘 드래그 표시. 격투가 개념도의 남은 마나 경로 정리. 성장 수치·Save v3·Content 에셋 저작 변경 없음.
- 검증: 최종 Editor 빌드103.04초 성공, Automation3 Success/errors0, 실제 게임176검사·새 프로세스 복원/재입력77검사에서 실패0·exit0. 실제 Primary 장착·E거절·놓기 전 창 닫기 취소·외곽 클릭·Esc/예약키/해제·정확 기술1회 발동,1080p/720p PNG 확인. 최종 생성 경로·범위·공백·커밋/게시 검사는 이어서 확정한다.
- 사고와 복원: 초기 Content junction의 sparse 정리가 공유 추적 에셋6,423개를 삭제했다. 사용자 고지 후 실제 부재 경로만 한정 복원·기존 존재파일 덮기0, 전량4,480,714,917B blob/텍스트 clean-filter 대조로 불일치0. 독립 실제 Content로 전환하고 재발 방지 지침을 추가했다. 초기 실패와 사고 기록을 보존한다.
- 상태: 구현·통합 QA·독립 검토 완료, 문서/최종 검사·작업 커밋→develop→test→main·일반 푸시 준비. 완료 게시 SHA는 뒤에 추가한다.
- 관련: [계획](../Feature/architect/2026-10-20_hybrid-tree-input-polish.md), [결과](../Feature/doc/2026-10-20_hybrid-tree-input-polish.md), [증거](../Feature/doc/evidence/hybrid-tree-input-polish-20261020/), [사고·복원 검토](../Feature/doc/evidence/hybrid-tree-input-polish-20261020/incident-review.md).

- Sprint#7-2 완료: 기능 `e6269dfab273b7eb36feaae64804434050cc1a0d`, 명시48파일 스테이징·생성 경로·공백·JSON/25링크·append-only·고신뢰 비밀 패턴 검사 통과. 최신 fetch/조상/worktree 점유 후 develop→test→main CAS fast-forward 및 일반 atomic push 성공, 실제 원격4개 SHA 일치 확인. Source tree fc0d9591/Content tree561dc863. [통합](../Feature/doc/evidence/hybrid-tree-input-polish-20261020/local-integration.json)·[게시](../Feature/doc/evidence/hybrid-tree-input-polish-20261020/public-push-result.json). 복원 사고 기록과 원본 실패도 포함한다. 이번 완료 기록은 Source/Content 변경 없는 문서 커밋으로 반영한다.

## Sprint#8-1 — World Partition 맵 메이커 (2026-10-24)

- 목표: 현재 OpenRunMap을 구역별로 수정하는 전용 Unreal Editor 도구를 만든다.
- 주요 변경: 기존 WP RuntimeHashSet 유지, 256m 편집 격자/소유 로더/환경물 선택·배치·이동/Undo/맵+외부 액터 저장, 새 Editor 전용 모듈과 임시 WP 자동화 fixture.
- 검증: 프로젝트 파일 생성 통과. 첫 Slate API 컴파일 문제와 독립 리뷰의 이동 잠금·PIE 갱신 문제를 수정했고 최종 빌드/자동화 진행 중이다. 생산 콘텐츠 변경 없이 원본 맵 SHA 대조를 실행한다.
- 상태: Production 구현·리뷰 보완 후 Builder/QA 진행. 완료·게시 결과는 검증 후 기록한다.
- 관련: [계획](../Feature/architect/2026-10-24_world-map-maker.md), [결과](../Feature/doc/2026-10-24_world-map-maker.md), [사용 가이드](WorldMapMakerGuide.md), [개발 일지](../DevLog/20261024.txt).

### Sprint#8-1 검증 완료 / 게시 준비

- Editor 최종 빌드 18.62초 성공, 3개 Automation 42.39초 전부 Success·오류/실패 0. 네이티브 경고 4개는 원본 보고서에 보존한다.
- 검증: 실제 Landscape 지면 배치·높이/yaw/scale·소유 WP 로더·잠금/혼합 보호·Undo/Redo·필터·OFPA 저장/재로드·Slate 생성. 임시 GUID 외부 패키지까지 네이티브 삭제 후 잔여 파일 0. 생산 OpenRunMap 1,260파일 SHA 변경 0 및 Content 변경 0.
- 실제 맵을 저장 없이 읽어 WP RuntimeHashSet/streaming, 지형·충돌 256개와 공간 프록시 16개를 확인했다. NullRHI이므로 렌더 UI/생산맵 편집 저장/20분 런/cook·Shipping은 미검증이다.
- 상태: 구현·검증·리뷰·문서 완료, 명시 변경 범위 검사 후 커밋·develop→test→main·공개 게시 준비. 결과 SHA는 성공 후 추가한다.

- Sprint#8-1 완료 (2026-10-25): 기능 커밋 `49bbd46df68334233be4edfc21140e0a103a7346`, 명시 46파일·필수 경로/구문/링크·스테이징·공백 PASS. develop→test→main fast-forward와 작업·통합 네 브랜치 atomic 일반 push 완료. [원격 직접 SHA 확인](../Feature/doc/evidence/world-map-maker-20261024/publication.json) 모두 일치. 원본 Content 변경 0. 이 완료 기록은 후속 문서 커밋으로 같은 흐름에 반영한다.

## Sprint#9-1 — 맵 메이커 실제 에디터 QA (2026-10-25)

- 목표: 실제 Unreal Editor 화면과 주요 기능을 조작해 검증하고 발견한 문제를 수정한다.
- 주요 발견/변경: 356px 패널 격자 글자 잘림 재현→고정 가독 폭/가로 스크롤, 남쪽 작업 구역의 얕은 시점이 경계 벽 뒤로 이동하는 문제→에디터 카메라 프레이밍 보완.
- 검증: OpenRunMap 실제 렌더/Window 메뉴 열기·구역 로드·청록 표시·저장 없음 확인. 원본 1,260파일 기준과 초기 dirty 0 기록. 수정/빌드/주요 조작 재검증 진행 중.
- 상태: 실제 QA 후 범위 수정 중. 완료와 통합·게시 결과는 검증 후 기록한다.
- 관련: [계획](../Feature/architect/2026-10-25_map-maker-live-qa.md), [결과](../Feature/doc/2026-10-25_map-maker-live-qa.md), [사용 가이드](WorldMapMakerGuide.md).

### Sprint#9-1 실제 GUI·회귀 검증 완료 / 게시 준비

- 최종 변경: 400px 가독 격자·축소 금지 가로 스크롤·560×900 초기 독립 창. 남쪽 야영지와 최외곽 `(3,-4)` 포커스를 -80°로 보완했다. -70° 재현 뒤 Reviewer/Architect로 돌아가 경계 가림을 재검토했고, 현재 월드의 활성 viewport만 조작하며 pilot/lock을 거절한다.
- 빌드/회귀: 공식 Live Coding timeout 후 UBT 성공과 별개의 런타임 패치 LNK2011/LNK1120 실패를 보존하고 정상 종료·정식 빌드 115.11초 성공으로 복구했다. 최종 Automation 3개 Success·실패/오류 0·57.22초·실행기/에디터 exit0. 자동화 전후 생산맵 1,260파일 SHA 변경 0이다.
- 실제 GUI: 좁은 창 양 끝 스크롤, 외곽/야영지 -80° 포커스, Engine Cube Height125/Yaw45/Scale20 배치, 실제 Ctrl+Z/ Ctrl+Y 제거·복원, `OW_MM_Cube` 필터 선택 1개, 목적지 `(1,-3)` 이동 후 Z/yaw/scale 유지, native Landscape/WP 열기, Simulate 편집 차단·종료 복귀, 다른 맵 거절·원본 복귀, clean 맵 저장 버튼 성공을 확인했다.
- 보존/한계: 임시 액터·폴더 dirty 2개만 확인 후 저장 없이 폐기·원본 reload. 최종 로드 액터 1,243개·maker 0·dirty map/content 0, Editor 정상 종료·Content 변경 0이다. 실제 변경 저장/재로드는 격리 WP fixture에서 검증했다. 전체 20분/Shipping/실제 지형 sculpt·paint는 미검증이며 네이티브 경고 4개를 보존한다.
- 상태: GUI·빌드·회귀·독립 코드 리뷰 완료. GUI 종료 후 최종 증거 검사도 원본 1,260파일 SHA 변경 0·Content 변경 0·임시 소품/dirty 0으로 통과했다. 명시 변경 범위·문서 경로 검사 뒤 커밋·develop→test→main·일반 push 예정이며 이번 QA 수정의 게시 성공 SHA는 아직 기록하지 않는다.
- 관련: [최종 결과와 검증표](../Feature/doc/2026-10-25_map-maker-live-qa.md), [자동화](../Feature/doc/evidence/map-maker-live-qa-20261025/automation-final/result.json), [최종 증거 검사](../Feature/doc/evidence/map-maker-live-qa-20261025/final-checks.json), [최종 GUI 상태](../Feature/doc/evidence/map-maker-live-qa-20261025/live-final.json), 외곽 화면〔중간·보조 자료 정리로 삭제됨; [보존 목록](../Feature/doc/evidence/README.md)〕, [개발 일지](../DevLog/20261025.txt).

- Sprint#9-1 게시 완료: 기능 `8ca0523309866aed5aa48fc4b435c02845241d8a`, 명시 94파일·생성 경로·공백·JSON/Python/문서 링크·DevLog 추가 전용 검사 통과. 최신 원격 변경 재확인 뒤 develop→test→main fast-forward 및 작업/통합 네 브랜치 일반 atomic push 성공, [직접 원격 SHA](../Feature/doc/evidence/map-maker-live-qa-20261025/publication.json) 전부 일치. 원본 Content 변경 0·QA dirty 0. 이 완료 기록은 소스/콘텐츠 변경 없는 후속 문서 체크포인트로 동일하게 게시한다.


## Sprint#9-2 — R11 공격 자원 합산·원자적 결제 (2026-10-26)

- 목표: 중복 비용 행의 합계 부족을 사전에 거부하고 부분 차감 없이 공격을 수락한다.
- 변경: 공통 자원 예산, 기본+선택 마나 비용 계획/결제 후 아이덴티티 확정, 재진입 방지, 신규 자동화4건 및 검증 스크립트.
- 검증: UE5.8.3 Editor 빌드 성공, 신규4+기존숙련3 Success/errors0/warnings0. 원본 clean 및 저장98개 해시 동일. 최초 컴파일/fixture 실패와 수정 증거 보존.
- 상태: 독립 복제에서 구현·검증 완료, 로컬 체크포인트. 자동 병합/push/PR 없음. R02/R03 및 R05/생산 맵은 미착수.
- 문서: [계획](../Feature/architect/2026-10-26_atomic-attack-costs.md), [결과](../Feature/doc/2026-10-26_atomic-attack-costs.md), [증거](../Feature/doc/evidence/atomic-attack-costs-20261026/).


## Sprint#9-3 — R02/R03 조건 데이터·공통 평가 기반 (2026-10-26)

- 목표: 필수 AND를 모든 획득 경로에 적용하면서 기존 빈 조건·OR·선행 그룹과 저장 형식을 보존한다.
- 변경: 새 요구조건 타입/명시적 기본0 Mage 입력, 공통 eligibility/부족 목록, UI backend·구매·자동해금·후보 검증 연결. 실제 신규 검술/랭크 저장과 미확정 정책은 미적용.
- 검증: Editor 빌드 성공, 새조건4+R11자원4+기존숙련3 Success/errors0/warnings0, 최소 실제 공격 수락 통합 통과. 원본 clean·저장98개 해시 변경0. 초기 테스트 이름/fixture 오류와 수정 증거 보존.
- 상태: 독립 복제에서 구현·검증 완료, 로컬 체크포인트. R05·생산 맵·자동 병합/push/PR은 수행하지 않음.
- 문서: [계획](../Feature/architect/2026-10-26_progression-requirements.md), [결과](../Feature/doc/2026-10-26_progression-requirements.md), [검증](../Feature/doc/evidence/progression-requirements-20261026/validation-summary.json).


### 최신 사용자 규칙 인계 — 런 성장과 로비 강화 분리
숙련 영구 성장 추천을 폐기한다. 20분 초과는 게임 오버이며 해당 런에서 오른 숙련/능력치는 초기화하고 로비용 재화를 정산한다. 로비 재화로 구매한 영구 강화는 보존한다. 게임 마무리 후 재화로 영구 강화한다. 이는 요구사항 기록이며 이번 조건 평가 packet에 초기화/정산/영구 저장을 구현하지 않았다. 사망·승리·장비·기술 해금 지속성은 미확정이다. 기존 실제 세이브 삭제·초기화·이행은 하지 않았다.


## 추가 사용자 확정 방향 — 조합별 호환성과 보스

검술 변형 전체를 한 슬롯만 활성화한다는 이전 추천은 폐기한다. 한 캐릭터가 여러 무기를 배우는 것은 허용한다. 조합별 호환성으로 표현해야 하며 마나+내공 동시 사용은 불가, 내공+투지는 허용되는 예시다. 자원 보유·동시 축적·전환 시 잔량 처리는 미정이다. RequiredAll 조건 평가와 호환성/장착 정책을 결합하지 않는다.

이번 새 타입에는 단일 장착 슬롯/상호배타 채널/자원 전환 정책을 추가하지 않았다. 기존 legacy 아이덴티티 선택·자원 동작은 호환 기준선으로 남아 있으며, 이를 새 조합 호환성 시스템으로 구현 완료했다고 주장하지 않는다. 새 호환성 데이터와 전투 효과 연결은 별도 packet이다.

보스는 시작 직행 상태의 승리가 현실적으로 매우 어렵지만 불가능하지 않으며 여러 기믹/기술 대응을 요구한다. 이번 packet에서 보스 밸런스/AI/생산 맵을 수정하거나 직행 승리 가능성을 플레이 검증하지 않았다.


### 사용자 답변6 — 전투 중 무기·사전 세팅 전환
무기별 사전 스킬 세팅은 비전투 중 편집하고 전투 중 무기 교체 시 연결된 세팅으로 전환한다. 교체는 무적을 부여하지 않는다. 전투 중 교체 전면 금지 추천은 적용하지 않는다. 교체 시간/공격 취소/쿨다운 공유/자원 잔량 세부는 미정이다. 이번 조건 기반 코드에는 구현하지 않았다. 최신 확정/미정 규칙의 통합본은 `Docs/RapidFieldRules.md`를 따른다.


## 사용자 답변7 — 첫 버전 무기·필드 보스·퀘스트·재화

첫 버전 무기는 대검/너클 두 가지다. 특정 위치의 선택형 필드 보스로 캐릭터 성장의 선택을 제공한다. 필수 중간 관문이 아니며 필드 보스 수는 아직 정하지 않았다.

퀘스트는 비급서를 받는 연계 퀘스트와 이번 판 아이템 구매 재화를 버는 의뢰까지 확대한다. 퀘스트 수는 아직 미정이다. 이번 판 구매 재화는 종료 시 초기화되는 별도 재화이고 로비 영구 강화 재화와 분리한다. 현재 조건 테스트에서 기존 Gold를 충분히 채운 것은 조건 우회 방지 검증일 뿐, 기존 Gold를 신규 런/로비 재화로 확정한 구현이 아니다.

사망/승리 리셋, 장비·기술 지속성, 로비 보상식, 필드 상점 방식은 후속 답변을 기다린다. 이번 packet에서는 새 경제·지속성·퀘스트·필드 보스 배치를 구현하지 않고 요구사항만 기록했다.


### 종료 정책 최종 추가 확정
승리·사망·시간초과 모두 런에서 오른 숙련/능력치 초기화와 로비 재화 정산 대상이다. 로비 영구 강화는 유지한다. 재화량과 장비/기술 지속성은 아직 미정이다. 앞선 사망/승리 미정 표시는 이 최신 사용자 답변으로 대체한다. 이번 체크포인트는 요구사항 기록만 갱신하며 실제 초기화/정산/세이브 이행은 하지 않았다.


## 지속성 추가 확정 — 런 보유와 영구 Discovery 분리

장비·아이템·비급서·배운 기술도 승리/사망/시간초과 등 런 종료 시 사라진다. 단 상위 기술의 최초 습득 기록은 영구 정보 공개용 Discovery로 남긴다. 처음에는 획득 조건을 `???`로 숨기고, 한 번 습득한 기록이 있으면 이후 해당 기술의 정보/조건을 볼 수 있다.

Discovery는 현재 런 Learned/Equipped와 별도 도메인이다. 영구 사용권, 자동 장착/학습 또는 RequiredAll 조건 우회의 근거가 아니다. 새 런에서는 조건을 다시 충족해야 하며 정보 공개 기록만 남는다.

이번 체크포인트는 정책 문서만 갱신했다. Discovery 데이터/저장·UI `???` 마스킹·런 종료 삭제/정산은 아직 구현하지 않았다. 기존 실제 세이브 이행/초기화도 하지 않는다. 현재 새 조건 API는 판정 backend이며, 상위 기술의 정보 노출 정책은 후속 UI에서 Discovery를 통해 별도로 적용해야 한다.


## 경제·상점 추가 확정

로비 재화는 처치·퀘스트·필드 보스 성과에 최종 보스 승리 보너스를 더하는 방식이다. 구체 숫자·산정 계수는 미정이다. 이번 판 구매 재화와 로비 영구 강화 재화의 분리는 유지한다.

구매는 실제 필드/마을 상점을 방문하는 방식이다. 어디서나 여는 구매 메뉴로 정하지 않는다. 위치·재고·가격·상호작용 세부는 아직 확정하지 않는다.

시작 지급 무기, 무기 교체/취소 타이밍, 쿨다운 유지, 메뉴 일시정지, 마나↔내공 전환과 잔량은 질문 중이다. 이 항목들은 추정 구현하지 않는다. 영구 Discovery는 정보 공개용이고 런 Learned/Equipped와 분리한다는 직전 규칙을 유지한다.

이번 수정은 최신 규칙 문서 기록이다. 검증된 R02/R03 조건 기반 코드는 변경하지 않았으며 경제·상점·정산·실제 세이브 이행은 구현하지 않았다.


## 시작 무기·숙련 승급 추가 확정

초기 지급 무기는 너클만이다. 대검은 필드에서 획득한다. 첫 버전 무기 범위가 대검/너클이라는 결정과 초기 지급 범위를 구분한다. 이번 문서 기록에서 실제 시작 장비나 필드 드롭을 변경하지 않았다.

무기 숙련은 특정 행동 없이도 숙련100을 채우면 레벨업할 수 있어야 한다. 필드 보스 처치 등의 행동은 추가 수련 경로이며 필수 승급 gate가 아니다. 수련 행동 목록은 사전에 공개하지 않는다. 자동/수동 승급, 행동 달성 후 목록 공개 여부는 아직 질문 중이다.

숙련 승급 규칙은 상위 기술의 RequiredAll 습득 조건과 별개다. 무기 숙련 레벨을 평가하는 입력이 특정 몬스터 처치/행동을 강제하는 의미는 아니다. 상위 기술의 영구 Discovery 정보 공개 기록과 숨겨진 숙련 수련 행동 목록도 별도 정책이다.

현재 R02/R03는 숙련 레벨과 명시적 MageRank를 읽는 조건 평가 기반이다. 숙련100 승급/수련 행동/목록 공개/너클 지급/대검 드롭은 구현하지 않았고 기존 숙련 공식과 실제 세이브를 변경하지 않았다.


## Sprint#9-4 — 무기 교체·문맥 메뉴 정지 영향 분석 (2026-10-26)

실제 무기 setter/인벤/Combat transition·binding·cooldown·입력·메뉴·런 timer 코드를 읽었다. 현재 교체는 진행 공격을 취소하고 기본 세팅을 재구성하며, 기존 숫자 cooldown은 남지만0.5 floor는 없다. 일반 패널에 위치 기반 pause도 없다. 요청 gate/런 세팅/쿨다운/메뉴 pause를 분리한 변경 범위와21개 미실행 테스트를 작성했다. 이번에는 런타임 코드를 바꾸거나 빌드/테스트를 재실행하지 않았다.

최신 미니게임 실패 규칙도 기록했다. 일반 돌파는 조건 재충족 후 재도전 가능, 융합은 사망 가능하나 확정 사망 아님. 실패 생존 손실도 이번 판 내이며 다음 판 초기화한다. 로비 강화·로비 재화·정보용 Discovery는 보존한다. 일반 런 성장/손실의 지속성을 반복 질문하지 않는다. 최초 큰 포인트의 범위/재화와 융합 실패 생존 시 기회 소모 등은 미정이다.

Notion 요약 후보: 무기 교체·메뉴 정지 사전 분석 완료, 실제 구현은 미착수. 최신 요구사항 통합은 Docs/RapidFieldRules.md. 분석은 Feature/architect/2026-10-26_weapon-switch-menu-analysis.md, 결과는 Feature/doc/2026-10-26_weapon-switch-menu-analysis.md. 자동 병합/push/생산맵/세이브 변경 없음.


### 숙련 자동 승급 확정
사용자 답변으로 숙련100 도달 시 자동 승급이 확정됐다. 특정 행동이나 수동 버튼 gate는 없다. 초과 숙련 처리 등 수치 세부와 행동 달성 후 목록 공개는 미정이다. 이번 읽기 전용 분석에서 숙련 공식/승급 코드는 수정하지 않았다.


### 획득처·미래 확장 추가 기록
대검은 고정 장소·상점·퀘스트에서 획득 가능하다. 첫 버전은 너클/대검이며 미래 무기 확장 가능 구조를 지향한다. 상점 기본 등급, 퀘스트 더 희귀한 등급, 드롭/특정 히든 피스는 매우 좋은 등급 기회다. 모든 드롭 최고 등급이나 확률·표를 확정하지 않는다. 이번에는 규칙 기록/영향 분석만 수행했다.


## 장비별 방어 규칙 추가

장착 장비에 따라 기본 회피/가드 스킬을 제공하고 방패를 장착하면 패링을 연다. 방패 슬롯과 대검/너클 동시장착 허용은 질문 중이다. 임의 보조 슬롯, 모든 무기에 대한 패링, 가드/패링 중 교체 허용 정책을 추가하지 않는다. 이번 packet은 요구사항 기록만이며 새 방어 스킬/슬롯/전투 동작은 구현하지 않았다.


## 액티브4슬롯/보스 기준 추가 분석

Ctrl/Q/E/R 독립4슬롯, 좌/우클릭 별도, 무기별4슬롯 프리셋을 기록했다. E는 현재 회피 및 예약키와 충돌한다. Ctrl은 modifier 검증, modifier-held 거부, key selector에서 막힌다. HUD 기존4칸은 마우스/차징 의미이고 새로운4개 액티브가 아니다. 기존 enum을 재정렬하지 않는 별도 슬롯 모델과 Enhanced Input·HUD·드래그/편집 UI 영향 범위를 분석에 추가했다. 방어 키는 미정이다.

입력7건과 특정 기술 없이 기본 조작으로 대응 가능한 보스 패턴 기준3건을 추가해 총31개 미실행 시나리오다. 실제 입력/슬롯/방어/보스 코드는 변경하지 않았다.


### 분석 체크포인트 최종 통합
Ctrl/Q/E/R 독립4슬롯·E회피/Ctrl제한/HUD 충돌과 보스 기본조작 대체 대응 기준을 포함해31개 미실행 테스트 계획으로 통합했다. 융합 실패 생존은 이번 판 회복불가 손상으로 조건 미달, 다음 판 초기화다. 프로필별 해당 융합 최초1회 로비 재화 보상은 반복 성공/콜백/저장 재시도로 중복 지급하지 않는다. 금액은 미정이다. 방패/패링은 향후 한손검·둔기 후속이며 현재 너클/대검에 추가하지 않는다. 이번 변경은 문서/읽기 전용 증거이며 실제 구현·새 테스트·빌드는 하지 않았다. 최신 규칙/분석/결과를 하나의 최종 체크포인트로 제출한다.


## 2026-10-27 Sprint#9-5 무기 타입 교체 쿨타임

중단 문서6개를 원문/patch와 415dd1e7 로컬 커밋으로 보존하고 재개했다. 공유 공격 쿨타임의 긴 절대 만료 시각을 유지하며 실제 무기 타입 교체에 max(남은 시간,0.5초)를 적용했다. 초기화/복원은 별도 C++ 경로이며 Blueprint 기존 서명과 저장 형식은 유지했다. 콤보 취소 뒤 미래 쿨타임 시작 전 우회도 차단했다.

UE5.8.3 기준 빌드와11개 회귀 통과 후 변경본16/16 자동화 통과. 마지막 빌드7.85초, 원본 세이브98개 SHA256 차이0/원본 git clean. 테스트 준비의 WorldContext 누락 및 float/double 오차 비교 실패 이력도 증거에 보존했다. NullRHI 자동화이며 렌더링 플레이/키 입력은 미실행이다.

이 묶음은 공유 공격의 타입 전환 범위만 완료했다. 전체 무기 교체/공격·사망 상태 제한/동일 타입 다른 아이템 식별/무기별4슬롯/입력·HUD/별도 쿨타임/안전지대 메뉴 정지는 남았다. merge/push/PR/새 설치/실제 세이브 이행/생산맵 변경 없음. 설계 Feature/architect/2026-10-27_weapon-switch-cooldown.md, 결과 Feature/doc/2026-10-27_weapon-switch-cooldown.md, 증거 Feature/doc/evidence/weapon-switch-cooldown-20261027/checkpoint.json.


## 2026-10-27 무기 교체 사전 상태·아이템 식별 완료

공격·사망 중 장비 변경을 인벤토리 쓰기 전6개 경로에서 차단하고, 대기/걷기/달리기는 허용했다. 공격 행동 시간은 몽타주·판정 구간을 따르며 긴 공유 쿨타임과 분리했다. 같은 타입 다른 GUID는 실제 교체로 처리하고 중복 GUID/반복 입력은 효과를 재적용하지 않는다. 장비·자원 알림 재진입 중 공격/중첩 장착도 차단했다. 교체 무적 없음.

UE5.8.3 최종 빌드8.81초 통과, 새8개+기존16개=24/24 자동화 통과, 오류·테스트 경고0. 첫 컴파일 이름 충돌과 맨손 준비 프로필 누락 경고 이력은 보존 후 수정했다. 원본 세이브98개 차이0, 원본 코드/맵/저장 이행 없음. 실제 렌더링 플레이/키 입력은 미실행이고 프리셋·입력/HUD·개별 스킬 쿨타임·메뉴 정지는 후속이다.

사용자가 기능 단위 GitHub push와 원본 DevLog 아래 기능별 실제 검증 화면 보존을 추가 승인했다. 선행 원시 로그의 기기 식별자는 공개하지 않도록 로컬 원문/이력을 유지하고 별도 공개용 기능 브랜치에 코드와 요약을 구성한다. 사진은 실제 자동 테스트 결과 화면으로만 기록하며 플레이 증거와 구분한다. 원격 SHA와 미디어 검증 결과는 최종 체크포인트로 확인한다.


2026-10-27 D 프로젝트 별도 브랜치 반영과 렌더링 검증
세이브98개/기존 DevLog16개를 독립 백업하고 reviewed6246d60c를 Sprint#9-6-desktop-integration에 반영했다. 이전 브랜치 보존, 사용자 파일114개 최종 해시 동일. D 빌드와 최종24개 회귀 통과. Editor 전용 렌더링 QA34개 통과, 실제 InputKey 이동138.615cm/표적 피해23.783. 교체 상태/0.5초 하한/긴 쿨타임 보존 확인. 수치는 QA 사례이지 성능/밸런스 지표가 아니다.
DevLog/20261027_DesktopIntegration_DPlayB_20261027에 실제 Unreal 뷰포트 사진6개와 범위/재현 문서를 남겼다. A 첫 실행 원본6개도 보존했으며 HUD 갱신 전 프레임 한계를 표시했다. 수동 물리 입력은 CUA 캡처·키 유지 제한으로 미검증(지표0). 새 QA 슬롯과 저장 연결 해제로 사용자 슬롯에 쓰지 않았다. 미디어·원시 로그는 Git에 추가하지 않는다.
실제 새 캐릭터가 대검으로 시작하는 차이와 기존 v3의 런/영구 성장 통합 저장을 다음 핵심으로 기록했다. 너클 정상 시작과 런/로비 분리를 먼저 연결하고 기존 포털·정산·귀환을 재사용한다. 이번에 새 게임 규칙/맵/저장 이행은 하지 않았다. Feature/doc/2026-10-27_desktop-integration.md 참조.
[Notion 요약 후보] D 경로 반영/24개 회귀/34개 렌더링 QA 완료. 수동 입력·정상 완주 미검증을 구분하고 다음은 너클 시작 및 런/로비 분리다.


## Sprint#9-7-run-lobby-domains (2026-10-29)
- 목표/완료: 너클 시작과 런/로비 성장 예산·저장·정산 분리. 기존 v1~3 슬롯 불변, v4 별도 저장.
- 검증: Editor 빌드 PASS, 기존24개 PASS, 실제 귀환68/68/69 assertion PASS, 렌더링 InputKey34 PASS, 기존102세이브+66증거파일 해시 불변.
- 제약: 수동 완주/60초 실측/정산 UI 로비 재화 표시/??? UI/퀘스트·필드보스 추가 보상/중도 종료 정책은 후속. 원격 변경 없음.
- 관련: Feature/architect/2026-10-29_run-lobby-domains.md, Feature/doc/2026-10-29_run-lobby-domains.md


## Sprint#9-8-result-lobby-ui (2026-10-29)
- 완료: 정산획득/보유/런재화표시,포기기준선복원·무보상·Discovery유지,중단허브복구,v5보존형저장.
- 검증: Editor빌드/자동화30/UI7시나리오/기존종료3종회귀PASS,기존270파일SHA256불변,실제PNG13장.
- 제약: 수동완주/실제프로세스강제종료미검증,콘솔/API프로토타입포기,설정버튼후속. push/merge없음.
- 문서: Feature/architect/2026-10-29_result-lobby-ui.md, Feature/doc/2026-10-29_result-lobby-ui.md.


## Sprint#9-9-abandon-menu (2026-10-29)
설정 포기 버튼·정책 확인·취소/닫기·문맥 재검증·연타 방어를 구현했다. Editor 빌드, 기존 회귀30개, 최종 메뉴 정상/실패 198항목 PASS. 실제 PNG6장 생성·디코딩·시각 검토. 보존438파일 해시 불변. 일시적 연결/렌더러 실패 뒤 새 QA 슬롯으로 완료했으며 물리 입력·수동 완주·강제종료는 미검증이다. 원격 변경 없음. 계획 Feature/architect/2026-10-29_abandon-menu.md, 결과 Feature/doc/2026-10-29_abandon-menu.md.


## Sprint#9-10-weighted-first-person (2026-10-29)
기존 Appearance 1인칭을 보존한 opt-in 시야/머리 안정화·상완 sway·제한된 공격 회수/준비·전환 프로필을 추가했다. 기본OFF, 원본 입력·공격 clock·자산·세이브 불변. Editor 빌드/회귀17개/일반 렌더와2배속30fps 상한 QA PASS, 실제 PNG9장. 원본516파일 해시 불변. 현재 맨손, 이후 아이템별 너클 모델 방향 확정. 물리 입력·실제맵 완주·전면 clipping은 미검증. 로컬 커밋만, push/병합 없음. 계획/결과 Feature/*/2026-10-29_weighted-first-person.md.

## Sprint#9-11-first-person-settings (2026-10-29)

이미 1인칭인 기존 카메라 위에 FPHeavy 토글·0~100% 동작 강도·기존 표현 복원을 O/Esc/F10 설정 메뉴로 제공한다. 실행 한정 설정이며 세이브/키/맵을 변경하지 않는다. 최종 Editor 빌드·HeadB560항목·기존 저장실패 메뉴97항목 PASS. 실제 UI PNG8장 디코딩과 가독성 확인. RenderC 전체 ABBA는 입력 해제로5실패하므로 미완전으로 기록하고 성능 개선을 주장하지 않는다. 기존 sprint PP 경로/정상 비교값으로 블러0.35/fringe0.6을 분리했다. 물리 입력·필드 완주·콜드 공격 hitch 원인은 미검증. 보존606파일 해시 불변, 로컬 커밋만, push/병합 없음. 계획/결과 Feature/*/2026-10-29_first-person-settings.md.

## Sprint#9-12-input-validation (2026-10-29 UTC)

제품 동작을 바꾸지 않고 공식 입력/Flush/포커스 관측과 viewport 입력·시야 오염 gate를 QA에 추가했다. native569/Offscreen565항목 PASS로 실제 이동 ON/OFF 비교를 완결했다. 프로그램 포커스 양성 대조의 정상 두키해제/새입력복구 확인. 촬영 없는 첫 공격 ABBA4프로세스/8공격142항목 PASS, 공통 첫 사용 수락75~77ms·첫 피해63~68ms 프레임을 재현했으나 내부 원인은 미확정. 과거 RenderC 원인도 단정하지 않는다. 빌드·663파일 원본/복사본 보존 PASS, 실제사진16장/대표5장 기능폴더 보존. 물리 입력·필드 완주 미실행, 로컬 커밋만. 계획/결과 Feature/*/2026-10-29_input-frame-validation.md.


## Sprint#9-13-look-dash-interaction (2026-10-29 UTC)

NPC가 소비한 F 입력의 중복 공격을 수정하고 실제 비용형 Special 양성 대조 포함39항목 PASS, 회귀17개/Editor 빌드 PASS. 아래보기·달리기·E 회피·벽/쿨다운 새 QA와 PNG 실측을 추가했다. **카메라 몸통 가림은 미완료**이며 단순 위치 후보를 채택하지 않고 실험 패치로 보존했다. Map Maker/NPC 키 사용법은 실제 소스로 확인했다. 원본747파일·QA 슬롯/증거 보존, 로컬 체크포인트만 생성, push/병합 없음. 계획/결과 Feature/*/2026-10-29_look-dash-interaction.md.


## Sprint#9-14 owner first-person 재개 체크포인트 (2026-10-31)
<!-- OwnerFirstPerson_1031_Checkpoint -->
- 목표: 누락Or_F복구와아래보기몸통가림진단.
- 변경: full polygon authoring/Or_F단일복구,owner-only leader proxy,기본OFF,QA명시활성화/20장phase사진/손궤적/애니메이션기록지침.
- 검증: 최종빌드PASS,회귀30PASS,Weighted4552/0·Original4664/0,NPC39/0,포기Normal89/0·SaveFailure97/0,43PNGdecode.
- 상태: PARTIAL. 중앙몸통가림제거,손프레이밍/극하향·벽팔절단면미완료. defaultOFF. 원본·세이브보존,원격변경없음.
- 문서: Feature/doc/2026-10-31_owner-first-person-resume.md, DevLog/20261031_OwnerFirstPerson_Review/AnimationRecord.md.


## Sprint#9-15 owner view framing (2026-10-31)
<!-- OwnerViewFraming_1031_Final -->
- 목표: 극하향·벽근접 팔절단면/손프레이밍 보정.
- 변경: owner-only posecopy/4회전IK·화면goal범위·기존leaderfallback; actualproxy getter; shadow/mesh/CPUnear분리QA와전후·애니메이션사진기록.
- 검증: BuildG PASS,관련회귀30PASS,진단252506/0각15사진,Weighted103802/0·Original106518/0각20사진.
- 상태: 재현장면보정검증완료/기본OFF. 원본·팔6·세이브270보존. 수동입력·일반필드완주·패키징/모든외형은미검증. push/병합없음.
- 문서: Feature/doc/2026-10-31_owner-view-framing.md, DevLog/20261031_OwnerViewFraming_Review/.


## Sprint#9-16 — 기본 플레이 연결 (2026-11-01)

필드 수동 학습 권한 분리, 기존 대검 런타임 단일 배치, 숙련도100 비율 표시, 타이머/F6 겹침 수정, 새 QA 슬롯과 네이티브 관찰기. 빌드 PASS; 신규QA87/88/87, 렌더90, 관련회귀30/0. 실제 입력 완주는 CUA 키 유지 미지원으로 남아 있고 관찰 I/O 부분 실패도 보존. 시작 저장폴더284개+별도 증거용27개(총311개 .sav)/31패키지 SHA 유지, 현재 저장폴더304개(신규QA20개). 초기311개 집계 범위 정정. 로컬 체크포인트이며 원격 작업 없음. Feature/doc/2026-11-01_basic-play-loop.md 및 Docs/BasicPlayGuide.md 참조.


## Sprint#9-17 — Alt 회피·대시 입력 분리

- 목표: E 액티브와 회피 충돌 제거. 왼쪽Alt/HUD/예약키 단일 정의. 비용·스키마·모션 유지.
- 검증: Editor2회빌드, Hybrid207+89/LookDash535/NPC39/핵심30통과. OwnerFraming15장과 CUA마을Alt 입력 확인. failedA 보존, fixture수정B통과.
- 보존: 실제세이브304개/자산31개해시 동일, 종료후318세이브. 28장증거 디코딩. 원격변경 없음.
- 상태: 로컬 입력 기능검증 완료. 사람의 물리입력/정상필드완주/4슬롯프리셋/안전지대메뉴정지는 별도.
- 문서: Feature/architect/2026-11-01_alt-dodge.md, Feature/doc/2026-11-01_alt-dodge.md, DevLog/20261101_AltDodge_Review/README.md.


최종 QA 실행기 리뷰 보정: PASS를 세이브 보존 검사 뒤로 옮기고 runtime 완료 영수증(label/namespace/로그 SHA/보존 성공)이 있어야 read를 허용한다. 새 AltGate1101C runtime207/read89 통과, 두 단계 모두 non-QA318개 해시 보존. 현재 실제 세이브321개이며 시작304개 모두 동일하다. 제품 바이너리/규칙 변경 없이 wrapper gate만 강화했다. 기존 failedA 결과 보존.


## Sprint#9-18 — 기본 정상 플레이 연결 검증 (진행 중)

- 실제 배치NPC F안내소실복구: discovery수명physical overlap기준,320cm서비스불변.
- NPC44회귀통과/331세이브SHA보존,정상E의NPC→필드→대검→전투XP→K학습확인. E보스입구자동경로미완료/새F검증중.
- 맵/원본에셋/애니메이션/게임수치변경없음. DevLog/20261101_NpcDiscovery_Review에전후/fixture6장.
- 계획Feature/architect/2026-11-01_basic-run-playtest.md, 결과Feature/doc/2026-11-01_npc-overlap.md. 원격push/병합없음.


## NPC 기능 체크포인트 및 정상 루프 진단 확정

NPC44검사와 핵심6suite30검사를 모두 통과했다. 정상 E/F/G는 메뉴→캐릭터→마을F대화/보급→도보포털→대검overlap→자연처치XP→K포인트학습을 통과했으나 보스 접근은 미완료다. G의 실제 Pawn 천장 line은Z9500.998에서보호바닥아랫면에start penetration/normalZ=-1을기록했다. 이동모드Walking/MaxWalkSpeed620/CanJump1/RootMotion0이므로입력·스태미나잠금으로해석하지않는다. E의별도위치는보호바닥밖이라같은원인으로합치지않는다.

시작실제세이브321개SHA와원본/팔31패키지SHA는전부같다. 현재실제세이브338개이며새QA만추가했다. 맵·원본자산·저장스키마변경없고원격작업없다. 아직실제보스전종료/결과/마을복귀를일반입력한판에서검증하지못했다. 다음은기존보스추락방지바닥을유지한채좁은남문보행경사로를runtime에만추가하는가역후보이며현재적용하지않았다.


### Sprint#9-19 — 보스 남쪽 진입 경사로

- 목표: 기존 보스 보호 바닥 아랫면에 걸리던 진입을 가역적 단일 보행 경로로 연결.
- 변경: Bootstrap h/cpp의 transient ramp/독립 타이머/실제 지형 검사, normal-input QA 경로와 형상 검증 스크립트. 원본 맵/바닥/애니메이션/세이브 형식 보존.
- 검증: 빌드31.52초·형상2개 PASS. H355.076초 정상 메뉴→성장→실제 보스 바닥→자연 사망→정산+6→마을 귀환.338세이브/31자산/맵 SHA256 동일. 실제 키보드·승리·패키징 미검증.
- 상태: 남쪽 경로와 사망 귀환 packet 완료; 기본 한 판 전체 승리 기준은 계속 진행. 원격 변경 없음.
- 문서: Feature/architect/2026-11-01_runtime-boss-south-approach.md, Feature/doc/2026-11-01_boss-south-approach.md, DevLog/20261101_BossSouthApproach_Review/README.md.



## Sprint#9-20 — 로비 강화 조건·재화 표시

- 목표: 최고 런 레벨 기록과 카드 잠금을 맞추고 로비 재화와 판 내 성장 포인트의 표시를 구분한다.
- 기준: `9578fa52`, 브랜치 `Sprint#9-20-lobby-trait-display`.
- 변경: TraitWidget/TutorialNPC 카드·라벨, TownProgression `NoPoints` 현지화 문구. 가격·조건·경제·저장·원본 자산은 무변경.
- 검증: 제품28.53초/테스트 실제 컴파일9.79초/최종24.24초 빌드 성공. 최초0 action은 컴파일 증거에서 제외. UI A36/0 보존, 최종 B38/0·PNG3장 직접 열기·디코딩·SHA256 확인. `RunCurrencyBaseline_Lobby1101Final`6/0·경고0·미실행0 통과. wrapper 성공 코드 누락으로 자동 연쇄가 실행되지 않은 부분을 보존 후 수정했고6개는 별도로 실행했다.
- 보존: B 정상 exit0/PID41880 종료, 해당 실행 전후340개 세이브 SHA 동일. 전체 작업 종료 뒤 독립 확인에서도 세이브340개·원본31자산·맵 SHA256 동일.
- 상태: 로컬 UI 표시 묶음 검증 완료. 실제 구매·저장/다음 판 강화/승리/물리 완주 및 옛9포인트 TraitUI fixture 미검증. 원격 push·병합 없음.
- 문서·증거: `Feature/doc/2026-11-01_lobby-trait-display.md`, `DevLog/20261101_LobbyTraitDisplay_Review/README.md`, `DevLog/LobbyTraitDisplayQA/LobbyDisplay1101B/Driver`, `Feature/doc/evidence/lobby-trait-display-20261101/`.



### Sprint#9-21 — prepared-run-input-qa

- 브랜치: `Sprint#9-21-prepared-run-input-qa`, 기준 HEAD `1928869d`(parent 제공). 최종 로컬 커밋은 별도 기록하며 원격 push/병합 없음.
- 상태: QA source/launcher 구현·빌드 완료, 프로그램 입력의 자연 사망/정산/마을 귀환 확인. 정상 보스 승리·물리 완주·애니메이션 시각 품질은 미완료. 문서 초안이며 D 반영 전.
- 목표: 실제 자원 예산으로 준비된 보스 입력 전략을 검증하고 기존 공격/회피 자산·시간·사진을 정확히 관찰한다.
- 주요 변경: 기본 OFF `RogueBasicRunPreparedBoss`/`RogueBasicRunAnimationEvidence`, 정상 입력의 자원 예산·Windup 회피·Recovery 공격, 기회 관찰 캡처 및 world 좌표 기록, wrapper 두 옵션 echo 검증. C4458 지역 `State` 이름을 `PhaseState`로만 수정. 제품 규칙/수치/맵/애니메이션 원본/세이브 형식 변경 없음.
- 검증: 첫 빌드32.58초 실패 보존 후11.45초 성공. wrapper text fixture10/10(UE 아님). BasicRun1101I wall400.675초, 메뉴/NPC F/필드/대검획득/자연처치XP/UMG학습/보스도달/자연사망정산/Hubreset·Discovery gate 통과. 회피요청5/실제수락5/미관측0, 실제 Stamina6 차감과 velocity 관측. 보스HP10000→최저9937.703, 마지막9939.703. +6 로비 정산, full_boss_completion0, 물리입력없음.
- 보존: 이전340개 저장 SHA 전부 보존, 신규 허용QA2개만 추가해342개. 31개 자산·맵 SHA256 동일, 사진9개 복사·디코딩·해시 확인. 새 에셋/애니메이션 원본 수정 없음.
- 제한: 사진6개의2개 phase미확정, 나머지4개도 자세품질 PASS 아님. 기본 body 가림·손 미노출·블러를 실패/제약으로 보존. 정상승리·전체20분·물리입력·60초맵·4액티브슬롯/프리셋·전체밸런스·패키징은 미완료.
- 관련 문서: `Feature/doc/2026-11-01_prepared-run-input-qa.md`, `DevLog/20261101_PreparedRunInput_Diagnostics/README.md`.
- 실행 근거: `DevLog/BasicRunPlayQA/BasicRun1101I/`의 session/save-comparison 및 Driver CSV/summary/screenshots. 프로그램 입력과 authored 값, post-tick 관측, 실제 시각 판정은 서로 구분한다.


## Sprint#9-22 — 정상 장착 UI·로비 강화 왕복 QA

- 브랜치 Sprint#9-22-gear-ui-input-qa, 기반3a306c8c. 제품은 식별 getter2개만, editor-only 정상 입력 driver/안전 launcher/개인 증거 제외 규칙 추가.
- 로비 B 실제 구매·저장·다음 판·포기·귀환27/0(9항목), OSexit0/원본344SHA유지; A 수집오류 원기록·부록 보존.
- J는 인벤토리 우클릭 전 시간 초과·장착0, wrapper1. 원본346SHA유지→348. 후속 진단 이름 충돌 수정 후11.84초 실제 컴파일 통과. K 보이는 창 검증은 UI재양도 대기.
- headless6suite30/0, 원본31에셋/맵 해시 유지. 실제 사진/미검증은 기능 문서·DevLog에 구분. push/병합 없음.
- 관련: Feature/architect/2026-11-01_gear-ui-input-qa.md, Feature/architect/2026-11-01_lobby-upgrade-roundtrip-qa.md, Feature/doc/2026-11-01_gear-ui-input-qa.md, Feature/doc/2026-11-01_lobby-upgrade-roundtrip-qa.md.


### Sprint#9-22 K 검증 마무리

- 실제 native 창에서 고유 GUID 대검 우클릭 장착·기본 공격 전환·공유 쿨타임 0.500초 관측.
- 정상 게임 시간 1200초 후 timeout 정산+6 저장·Hub 복귀·선택 런 값 reset/Discovery 유지, OS/launcher exit0. 보스 승리0/물리 입력 없음.
- 기존348 SHA 유지→350(QA2개), 원본31에셋/맵 동일, 사진5장 직접 검토·디코딩. CSV120000행 상한·블러·귀환 재질 경고 분리 기록.
- 실제 BP 연결 데이터에서 보스 HP10000/regen2 및 GS 기본 공격 비용0 확인. 다음 전략 후보의 Idle 공격 기회 차단을 정적 발견하여 미적용 중. 제품 수치/원격 변경 없음.


### Sprint#9-23 정상 성장 보스 QA — 진행 중

- 목표: 강제 수치 변경 없이 정상 대검 성장·스킬·보스·결과/귀환 연결 확인.
- 변경: 기본 OFF QA 성장/궤도 입력 전략, 읽기 전용 Alt 관찰기와 결과 원장 검증.
- 검증: 실제 Editor compile12.64초, parser26/preflight11 PASS; L은 자연 LV3/GS M3→마나 학습/활성→보스 사망→+13 저장/귀환. FullBoss0이며 완료로 처리하지 않음.
- 보존: 기존 저장350 SHA 불변, QA2 추가; 에셋31/맵 불변. 사진5장 실제 디코딩/검토.
- 다음: 원형 경고 재진입과 무료 공격 reserve QA 결함을 수정해 정상 승리 재검증.
- 문서: Feature/architect/2026-11-01_normal-growth-boss-qa.md, Feature/doc/2026-11-01_normal-growth-boss-qa.md


9-23 후속: L근거를37aa0056으로보존한뒤 QA장판바깥유지·무료기본공격예산검사를수정,실제Editor12.66초재컴파일성공.352sav변경0. M실제실행은UI반환대기중이며보스승리완료아님.


9-23 M결과: 정상성장/마나학습→보스실제처치→승리저장+24→마을귀환 PASS(fullBoss1/exit0). 제품수치/피해판정불변, 기존352sav/31패키지/맵 보존, QA2추가. 다음판/legacy기술초기화범위는계속검증하며 사람직접완주·시야·패키징완료아님. Feature/doc/2026-11-01_normal-growth-boss-qa.md와 M_NormalVictory 기록 참고.


### Sprint#9-24 동일 프로필 다음 판 연결 / 녹화 준비

- QA continuation·4슬롯 비교·실제 메뉴 포기 관찰·옵션 녹화 게이트 추가, 제품 수치/원본 에셋 변경 없음.
- 실제 Editor build13.13초 성공, HTTP7검사 PASS. N은 브라우저 URL 식별 정책 차단 후 Hub ready90초 timeout: blocked, 영상/다음 판 미실행.
- 기존354sav 불변, QA2추가356. 로컬 WIP 보존, 원격 변경 없음.
- 문서: Feature/architect/2026-11-01_next-run-connection.md, Feature/doc/2026-11-01_next-run-connection.md.

9-24 기록 갱신: 녹화 요청 취소, 실제 M 캡처6+공개 설명2를 DevLog/20261101_ScreenshotDevelopmentRecord에 보존. 원본 SHA/decode/privacy검수 PASS. 현재 wrapper 파서23/13·guard2/2·모델25 PASS; 실제 다음 판 미완료. Library 준비 API 미가용으로 공유 이관 미완료, 우회 없음.

9-24 스크린샷 이관 후속: 공식 create fallback으로 원본6+설명2 Library 저장 및 개인Page 원본참조 확인 완료. 로컬 xattr는 Windows 미지원으로 실패, ID sidecar 보존. 다음 판 미검증/전역입력 중단 유지.


## Sprint#9-25 — 안전 문맥 메뉴 정지와 남은 개발 계획 (2026-11-03)

- 목표: 기존 미완료 next-run QA를 보존하면서 확정된 출발 전 마을 메뉴 정지 계약을 구현한다.
- 변경: 정확한 StartHub/WaitingToStart provider, 컨트롤러별 pause handle 소유권, I/P/K/O/Esc/F10 pause 중 닫기, 중첩 창/phase/EndPlay 정리, 미래 공격 입력만 취소하는 Combat/BasicBrawler API. 맵/에셋/저장 스키마/밸런스는 변경하지 않는다.
- 검증: 최종 Editor CPP 컴파일·링크47.69초 성공. NullRHI71/0, D3D11 RenderOffscreen71/0, 기존6suite30/0, 런처 사전 파일 거부2개·AST 통과. 실제 PNG3개 디코딩·SHA·시각 검토, 원본356저장 보존/QA4개 추가.
- 상태: 기능 검증 완료. 작업 브랜치 기록 대상이며 develop/test/main 승격은 기존 #9-24 연속 플레이 미검증과 분리한다. 사람 물리 입력·실제 다음 판·20분·패키징은 미완료다.
- 관련: Feature/architect/2026-11-03_safe-menu-pause-and-roadmap.md, Feature/doc/2026-11-03_safe-menu-pause.md, DevLog/20261103.txt, Feature/doc/evidence/safe-menu-pause-20261103/VERIFICATION.json.

## Sprint#9-26 — 동일 프로필 다음 판·포기 왕복 검증 (2026-11-03)

- 목표: #9-24에서 남은 실제 다음 판 연결을 녹화 없이 완료하고 기존 첫 승리 결과를 보존한다.
- 변경: 기본 OFF QA의 실제 UMG 포기 취소 확인, 다음 필드/포기 귀환의 기존 4개 유효 공격 슬롯 비교, 취소/슬롯 요약 gate. QA append 읽기 공유와 공유 reader 관찰기, 실제 UE NativeIO 회귀를 추가했다. 맵/원본 에셋/제품 밸런스/저장 스키마는 변경하지 않는다.
- 실행: O389.908초는 `evidence_write_failed`로 보스 이동 중 blocked(엔진0/실행기1), 실패 원본 보존. P1129.537초는 실제 정상 보스 승리→같은 프로필 다음 필드→UMG 포기 취소·확정→저장귀환 완료(엔진0/실행기0, fullBoss1/nextComplete1). A→B→C 서로 다른 RunId와 단일 프로필, 1200초 타이머·너클·선택 성장 초기화/Discovery 보존을 확인했다.
- 검증: Editor 빌드42.00/18.73/7.77초 성공. Windows 파일 공유 충돌 별도 재현 후 append100회 무손실, NativeIO 자동화1/0, 합성 파서28/28·AST0은 별도 집계. Alt 요청/수락8/8, HP 감소 관측1회는 인간 회피 난이도 판정이 아니다.
- 보존: O360→362, P362→364. 각 기존 세이브 SHA256 변경/누락0·허용 QA2개씩 추가. P45장/O31장 전체 디코딩·SHA256, P 주요5장 Root 시각 검토와 기능별 원본 보존을 완료했다.
- 범위: 첫 `returned_victory`/`Victory` 상태 고정, 실제 취소 동작1, 필드/귀환 슬롯 비교1. 공개 유효 슬롯 비교이며 모든 private legacy 원장 초기화 인증은 아니다. P의 저장 실패 재시도·사람 물리 입력·20분 시간초과·패키징은 미실행이다.
- 상태: #9-24의 실제 동일 프로필 다음 판 미검증은 이번 #9-26으로 해소. 이번 기록에서 develop/test/main 병합은 하지 않았다. 보스 속도/지상형 후보·Fab·정면 시야/크로스헤어·테스트 판정 표시·처치 드롭은 최신 사용자 요청의 다음 packet이다.
- 문서: `Feature/architect/2026-11-03_next-run-validation.md`, `Feature/doc/2026-11-03_next-run-validation.md`, `DevLog/BasicRunPlayQA/BasicRun1103O/`, `DevLog/BasicRunPlayQA/BasicRun1103P/`, `Feature/doc/evidence/next-run-validation-20261103/NativeIO1103A/`.


## Sprint#9-27 — Combat readability and monster loot (2026-11-03)

- Goal: readable boss anticipation, forward initial framing/crosshair, opt-in actual query debug, data-driven death loot with existing atomic pickup/persistence.
- Changes: owned boss profile variant/basic warning and actual-target windup HUD; normal pitch -10; live physics query shapes/executed pulses/accepted hits with owned cleanup; provisional world Slime potion25%×1.
- Validation: UE5.8.4 Editor build77.82s; native40/40; Boss551/0 across18 cases/HUD/menu; Spatial85/0 with37skeletal+1capsule; camera5×49/0 and25PNG pixel checks; loot26+9/0 across disk restart;54PNG decoded/hashed.364existing saves unchanged,22ownedQA adds.
- Status: implemented/validated work checkpoint. Actual physical-input feel, all terrain/animation quality, ground-boss asset application, expanded loot balance and packaging/full20min QA remain. Fab paid Golem not acquired; account ownership unverified.
- Docs: Feature/architect/2026-11-03_combat-readability-and-monster-loot.md; Feature/architect/2026-11-03_remaining-development-roadmap.md; Feature/doc/2026-11-03_combat-readability-and-monster-loot.md; Feature/doc/evidence/combat-readability-20261103/final/verification.json.

### Sprint#9-29 인간형 보스·NPC 애니메이션 및 출력 오류 점검 (2026-11-10, 검증 중)
- 목표: 보유 Fab/Mixamo 파생 자산으로 인간형 보스와 자연스러운 NPC 대기를 구성하고 UE5.8 BT/BB·AnimInstance/Montage 규칙과 전투 타이밍을 유지한다.
- 변경: 최종 보스 Garret 메시, 파생 공격 프로필, 네이티브 HumanoidAnimInstance 및 몽타주5개. 아린의 새 호흡 대기 유지. 맵·능력치·보상·기존 패턴 판정은 보존.
- 검증: B2 실제3인칭18/18케이스·431검사 실패0·저장386개 보존·빨간 오류0. 실제 PNG에서 가림/데칼/셰이더 문제를 발견해 시각 합격은 보류했고 C3에서 보완. C3 전체 빌드 성공, 후속 런타임·피격/사망 검증 진행 중.
- 상태: 작업 브랜치 유지, 공개 커밋/푸시 없음. 1인칭 신규 개발·검수는 최신 요청에 따라 중단하고 기존 코드는 보존.
- 문서: Feature/architect/2026-11-10_humanoid-boss-npc.md, DevLog/20261110_HumanoidBossNpc/Boss/AnimationRecord.md, Feature/doc/2026-11-10_output-log-diagnosis.md.

### Sprint#9-29 3인칭 기본 시점·마우스 입력 중재 (2026-11-10, 구현/전용 검수 완료)
- 목표: 기본3인칭 플레이, 기존1인칭 보존/신규 검수 중단, LMB/RMB/차징/동시 입력 충돌 방지와 뉴비 기본 주먹 유지.
- 주요 변경: 어깨85cm/-12도 기본 구도, 벽 카메라 충돌·복원,3인칭 메시/데칼 처리, 마우스 입력 중재 및 취소 경계.
- 검증: D native 성공. TP_D1 786체크 실패0,13입력 경로 통과, 실제 신규 LeftJab+1/QER·RMB·Ctrl 무기능력 거부. 카메라331.25→140.78→331.25cm, 회피248.08/벽39.97cm, 기존390저장 보존, 엔진red0.
- 상태/한계: 전용 프로그램 입력 검수 완료. 물리입력·저장재시작·전체런·근접무기접촉은 미검증. 맵동선 변경·공개커밋·푸시 없음.
- 문서: Feature/doc/2026-11-10-third-person-default.md, Feature/doc/2026-11-10-mouse-skill-input.md, DevLog/20261110_ThirdPerson/TP_D1/README.md.


## 2026-11-10 인간형 보스·NPC 최종 체크포인트
- 목표/변경: 보유 Garret와 Arin 파생 애니메이션, 기존 BT/BB 유지, opt-in AnimInstance·DefaultSlot montage, source timing/legacy 복원/RateScale 회귀 교정. 맵·vendor 원본 불변.
- 검증: E2 성공(14.08초), Human3P_E1 451검사 실패0·18전투+피격/사망·2603프레임·22PNG 개별 판독, 기존394저장 보존, 엔진 red0. D5+D2인간형2 strict단위 통과.
- 상태: 이번 인간형 보스/NPC 구현·격리3인칭 검증 완료. 무기 활성/아이덴티티는 별도 진행 중. 물리접촉·지속시체·전종 자연AI·전체런·AAA최종품질은 미인증. 공개 commit/push 없음.
- 로그: ko 엔진 smoke15 영어비교 실패와 -game Editor Python AgentSkill 오류 원인 분리. QA프로세스 en/en-US·DisablePython만 적용, 엔진/사용자언어 설정 불변.
- 문서: Feature/doc/2026-11-10_humanoid-boss-npc.md; Feature/doc/2026-11-10_output-log-diagnosis.md; Feature/doc/evidence/humanoid-boss-npc-20261110/Human3P_E1/README.md.
- 설계 문서: Docs/WeaponClassDesign.md·SkillMotionVFXDesign.md의10무기100후보 정리 완료(구현100개 완료 아님).

## Sprint#9-29 NPC 대화 선택 재개 (UTC 2026-11-10)
- 목표: NPC 선택 불가와 중단된 F 빌드를 복구한다.
- 변경: C4458 세 건, 특성 복귀 focus, 메뉴 takeover 안내문, 맨손 QA와 캡처/수동 결과 범위 교정.
- 검증: 최종F4 통합빌드 성공, NPC58서비스검사 실패0·PNG6개·기존400저장 보존. 관련 회귀는 진행중.
- 상태: NPC 기능 검증 통과, 통합 회귀 진행 중. 커밋·푸시 없음.
- 문서: Feature/architect/2026-11-10_npc-dialogue-selection.md, Feature/doc/2026-11-10_npc-dialogue-selection.md.


### Sprint#9-29 NPC 선택·고정 입력 packet 최종 QA 완료 (UTC 2026-11-10)
- 상태: F9 Editor 빌드 성공 및5단계 회귀 통과. 커밋·푸시·main 승격 없음.
- 추가 변경: 실제 Q/E/R 입력 연결 누락 교정, 비활성 FP montage가3인칭 무기변경을 막던 문제 교정. 전투 타격/비용·맵·바이너리 보존.
- 검증: 입력92/서비스58/마을71+27/3인칭782/마우스13·단위8 모두 실패0, 원본398저장SHA보존, 최종로그8개harderror0.
- 한계: 프로그램입력·격리fixture이며 물리입력/풀런·대검최종시각 미인증.1인칭 신규개발 보류유지.
- 문서: Feature/doc/2026-11-10_npc-dialogue-selection.md, 최종 manifest는 해당문서참고.


## Sprint#9-29 다섯 마을·선택 경로 보완 설계 (UTC 2026-11-10)
- 목표: 직행 보스 입구1분 이내, 선택 콘텐츠 경유 입구 약15분, 퀘스트 없는 이동·마을 생략·재방문을 구체화한다.
- 주요 변경: 기존 초안은 보존하고 별도 명세·결과·전용 설계 증거 추가. 다섯 마을의 고유 의뢰/상점/던전/보상 후보, 교차로 포함10지점19양방향 간선,5개 경로, 두900초 활동 예산. 보스전180초+여유120초로 기존20분 상한 유지.
- 검증: 정적199검사 실패0, 생성파일 경로 검사 통과. 독립 소스/과거 증거 감사로 속도·시계·StartHub 전용 안전/포털·구현 공백 확인. 신규60초/15분 실제 플레이·빌드·PIE 미실행.
- 상태: 설계 완료·정적 검증 통과, 배치·구현·플레이 검증 전. 공유 소스/에셋/설정/저장/브랜치 보존, 커밋·푸시 없음. 기존 북부 보스 보존·진입점 근접 재배치 권장, 실제 앵커·품목 수치·교전 후 이탈 정책은 후속 조율.
- 관련 문서: Feature/architect/2026-11-10_five-village-route-spec.md; Feature/doc/2026-11-10_five-village-route-spec.md; Feature/doc/evidence/five-village-route-spec-20261110/.

## Sprint#9-29 다섯 마을 별도 그레이박스 제작 준비 (UTC 2026-11-10)
- 목표: 확정된 선택형 다섯마을 경로를 기존월드와 병렬대검작업을 보존하는 별도레벨 후보로 구체화한다.
- 주요 변경: 신규 purePython 배치생성기와 Editor저작코드,92액터·13재질 제작JSON/SVG/검증증거.10지점·19양방향8m도로·15건물·15예약서비스·보스입구/부지. 대상은 Maps/Prototypes/FiveVillageGraybox20261110, 기존월드/포털연결없음.
- 검증: 정적544검사 실패0, 도로/건물최소여유1.567581m, Python AST·결정성·XML·출력경계·장애물음성대조군통과. 독립UE5.8 API검토의2개수정반영. 실제Editor저작/저장/재로드/주행은미실행.
- 상태: 분리제작준비완료, 명시적Editor인계대기. 대검8방향QA가소유중이므로 동시Editor/커맨드렛/빌드없음. 기존후보덮어쓰기와dirty작업저장/폐기차단. 서비스는예약마커,1분/15분은설계와실측구분. 커밋·푸시없음.
- 관련문서: Feature/architect/2026-11-10_five-village-graybox.md; Feature/doc/2026-11-10_five-village-graybox.md; Feature/doc/evidence/five-village-graybox-20261110/README.md.

### 2026-11-10 ResumeSword1: 대검 준비·이동 실제 적용
- 목표: 누락 작업 asset 저장, 검 준비/이동 실제 연결, 양손 그립 및 기존 무기 회귀.
- 구현: 27 sample/17 unique armed stride BlendSpace + Common AnimBP 연결, section/notify preflight, event-driven 양손 palm grip와 fallback/소유권 보존, 전용 렌더 QA 추가.
- 검증: Editor build 성공, SwordResumeB 기존10스킬/34PNG, SwordMoveA20케이스/20PNG 및 실제 이미지 검수, 기존432 save 보존. Magic buffer3파일 독립1,701검사 통과. 공식 MCP로 slash VFX 후보 편집/저장·compile오류경고0.
- 상태: 이동/전환 적용·검증 완료. 새 대검 공격5개 스킬 연결/접촉 시점, slash VFX 실제 게임 승인, 수정한 장착 unit fixture 재검증은 남음. 1인칭 보류.
- 문서: Feature/doc/2026-11-10_sword-knuckle-visuals.md, Feature/doc/evidence/sword-knuckle-20261110/ResumeSword1/.

## Sprint#9-29 다섯 마을 별도 그레이박스 실제 제작 완료 (UTC 2026-11-10)
- 목표: 인계받은 단독 에디터에서 설계 경로를 신규 레벨로 생성·저장·재로드하고 실제 엔진 충돌/화면을 확인한다.
- 주요 변경: Maps/Prototypes/FiveVillageGraybox20261110 레벨1개, 전용 MaterialInstance13개.92액터·10지점·19양방향8m도로·15건물·15예약서비스. 기존 월드·포털·보스·NPC·대검·세이브 통합은 수행하지 않았다.
- 검증: 정적544/엔진748 실패0.38방향 capsule sweep·636바닥표본·양성 대조군,92액터 재로드, 실제 MCP 전체/사선/근접 시각 검수. 기존3맵 SHA256 불변. 최초 재질 반환값 오류를 읽기 검증으로 복구했고 초기 노출 경고 캡처도 보존했다.
- 상태: 별도 그레이박스 실제 제작/저장/기술 검증 완료. 생산 캐릭터 보행·60초/15분 실측·퀘스트/서비스·기존 월드 연결은 미완료. PID53928 정상 종료20:20:54UTC 및 UE/UBT/dotnet0 확인. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-11-10_five-village-graybox.md; Feature/doc/2026-11-10_five-village-graybox.md; Feature/doc/evidence/five-village-graybox-20261110/final-verification.json.


## ResumeSword1 실제 공격 활성화 및 통합 회귀 후속

- 대검 5개 편집 Montage와 접촉 지연을 실제 스킬에 적용했다. 원본 FBX/애니메이션과 기존 밸런스/슬롯/VFX 필드는 보존했다.
- SwordAuthoredA 실제 게임 렌더에서 기존 너클/대검 10개 공격과 PNG 34장을 검증했다. 대검 5개의 최초 HP 감소 관측에서 실제 타깃 캡슐과 칼날 선분 거리 조건을 통과했다. 실패 0, 기존 저장 432개 해시 보존이다.
- 34장 모두 직접 확인했다. 대검의 공격과 무장 자세 복귀, 너클의 주먹 공격과 검 잔존 없음은 보인다. 후방 시점·블러·지연 캡처 때문에 정확한 접촉 프레임, 손가락 표면, 연속 동작까지 승인한 것은 아니다. 기존 두 스킬의 시전/적중 VFX는 사진에서 뚜렷하지 않아 시각 완료로 처리하지 않았다.
- 공유 콘텐츠에서 CatalogSharedA 소비/정책/성장 6개 검사가 통과했다. 20개 신기술 콘텐츠 및 카탈로그 활성화 완료를 뜻하지 않는다.
- WeaponIntegratedA는 10/11, BossTargetA는 1/5였다. Staff fixture의 ASC 초기화 누락과 Boss fixture의 abstract 캐릭터 생성 실패를 확인하여 검사 준비 코드만 수정했다. 실패 기록을 보존했고 새 빌드/새 라벨 재검증 전까지 통과로 계산하지 않는다.
- 드롭 자동 재획득 4파일 패치를 백업 후 통합했다. 파티클 30개는 아직 실제 생성 전이며 검증 준비 도구만 작성했다.
- 근거: SwordAuthoredA/rendered-review.json, catalog-consumption-20261110/CatalogSharedA, weapon-presentation/WeaponIntegratedA, tmp/work-boss-ai-target-loss-20261110/Engine_BossTargetA.


## 파티클 실제 제작 및 통합 검사 후속

- 공식 Unreal MCP로 너클·대검 파티클30후보를 실제 생성/컴파일/저장했다. 오류0, 스택 경고0, 원본4개 해시 보존. 시전·단발 잔상·적중 후보이며 게임 스킬 연결과 연속 리본은 미완료다.
- 첫 프리뷰는 도구의 SimCache 반환형 검사에서0PNG로 멈췄다. realtime override ensure도 확인해 수정 중이다. 보호 파일44개 해시를 보존했고 소유 PID13140 정상 종료, UE/빌드0 확인 후 마법 담당에게 인계를 제안했다.
- WeaponIntegratedB 무기11개, BossTargetB 타깃 생명주기5개, CatalogSharedA 소비/성장6개, LootRetryB 신규 드롭3개 검사가 통과했다. 첫 실패 fixture 기록은 보존했다.
- 자연 AI 회귀의 보스2실패는 실제 windup1.6/1.9초보다 이른 고정1.3/1.6초 검사에 기인했다. 검사만 실제 locked windup 기반 유한 관측으로 바꿔 빌드했으며 재실행 전까지 통과로 계산하지 않는다.
- 드롭 Write/Read A는 게임 assertions 성공이나 불필요한 Editor Python 오류가 있어 깨끗한 통과로 승인하지 않았다. -DisablePython/오류로그/전체 저장해시 검사 보강 후 B는 엔진 selftest 텍스트를 Error verbosity로 오인한 regex 문제로 Write 뒤 멈췄다. 정확한 Error: 판별로 고쳐 다음 fresh 실행을 대기한다. 저장 원본 변경 근거는 없다.
- 남은 작업: 후보 실제30렌더/시각 검수, 드롭 전체8개와 강화 Write/Read, 자연AI 재검증. 관련 문서: Feature/doc/2026-11-10_weapon-skill-particle-candidates.md.

[Notion 요약 후보] 너클·대검 파티클30개를 UE MCP로 실제 제작했다. 게임 연결 전 후보 검수 단계이며 첫 프리뷰 도구 실패를 수정 중이다. 무기11·보스5·카탈로그6·드롭3 회귀는 통과했고 나머지 저장/자연AI 회귀를 별도 추적한다.


## 드롭 자동 재획득 통합 검증 완료

- LootAllA 전체8개 자동화와 LootResumeC 실제 Write26/Read9 검사를 통과했다. 오류로그0, 기존 저장 변경/소실0, 예상 밖 신규 저장0이다.
- fixture의 QA profile/EndPlay 정리와 runner의 Editor Python 제외·정확한 Error: 판별·전체 저장 해시 검사를 적용했다. 과거 실패 증거는 보존했다.
- 단일플레이 통제 fixture이며 물리 입력·전체 런·정상 드롭 확률 검증과 구분한다. 마법8자산의 인계 해시도 보존했다.
- 문서: Feature/doc/2026-11-10_monster-loot-pickup-retry.md.
[Notion 요약 후보] 가방이 가득 차서 놓친 드롭을 제자리에서 공간 확보 후 자동으로 다시 줍도록 통합했다. 전체8개 회귀와 실제 저장/재로드35개 확인을 통과했다.


## 회수 후 자연 AI·파티클 실제 검증 후속

- BossNaturalB 자연 AI/공격/정리 회귀는 112개 성공, 실패0, 기존 저장 보존이다. 실제 wave1.6초/zone1.9초 windup을 관측했다. 고정 검사 시점을 제거한 QA 수정의 재검증이며 AI/밸런스를 바꾼 결과가 아니다.
- 관련 PNG8장을 직접 확인했다. 파동의 분홍 효과는 보이나 장판은 첫 Niagara 컴파일 이전에 캡처를 요청해 작은 점과 Preparing Niagara Systems가 남았다. 장판 시각·첫 사용·패키지 검증 완료로 승인하지 않는다.
- 파티클 최소 ProbeA는 running/paused/restored 캡처 차이와 실제 Age 키·입자1을 실증했다. 이를 적용한 PreviewC는30개 모두 실제 데이터 age/입자1 검증 및 PNG30장 확보, 보호44개 해시 보존에 성공했다.
- C의 대표 사진에서 카메라와 target cylinder 가림 때문에 효과를 판독하기 어려워 시각 승인하지 않았다. 색/재질/자산은 건드리지 않고 반대 카메라·가림 제거의 대표6개 DiagnosticD로 검토를 좁혔다.


### 단일 밝기 개선 프로토타입 실제 제작·비교

- 공식 MCP에서 새 MIC1개와 이를 쓰는 대검 기본1타 Cast NS1개만 생성·컴파일·저장했다. inherited CueTint의 linear RGB를8배로 바꾸고 alpha1을 보존했다. 원본56개 해시는 동일하다.
- 실제 ReadabilityPrototypeF1장과 baselineE 사진을 비교해 금색 세로 시전 flash가 더 뚜렷함을 확인했다. 프리뷰는58개 파일 해시 및 실제 age/입자1을 보존·검증했다.
- 이는1개 고정 포즈 후보 비교다. 기존30개로 HDR를 확대한 것도, 게임 스킬에 연결한 것도 아니며 체감 밝기8배나 실제 전투 가시성 통과를 뜻하지 않는다.
- Shader schema의 flat property map을 잘못 가정한 ReadabilityGain8A는 생성0건으로 중단했고 근거를 보존했다. 실제 schema 회귀검사 후 새B라벨에서 제작했다.


## 통합 회귀 및 파티클 전체 실제 화면 검토 마무리

- 무기11, 타깃 생명주기5, 카탈로그 소비/성장6, 드롭 전체8 및 Write26/Read9, 자연AI112개 검사 결과를 통합했다. 기존432개 저장 해시를 현재 디스크와 재대조해 변경·소실0을 확인했다.
- SkillParticlesReviewG에서 기존30후보 전부 실제 age/입자1·Color/Size/NormalizedAge와 PNG완료를 확인하고30장을 직접 보았다. 보호56개 해시 동일, 소유 임시 NiagaraActor30개 정리, 오류/ensure0이다.
- 기본 효과 형태는 보이지만 일부 시전은 몸/손에 겹치고 전반 대비가 낮다. 최종 게임 가시성/스킬 연결 승인은 아니다. 별도 MIC/NS gain8 프로토타입1개는 더 명확한 금색 시전 flash로 확인했으며 기존30개에는 확대하지 않았다.
- 장판 첫 사용/cooked 패키지 가시성, 실제 스킬 scale/dilation/부착/타격 연계, 연속 ribbon, 지팡이 모델 지정, 신규20 기술 콘텐츠는 후속 범위다. 1인칭은 계속 보류했다.
- 소유 PID55388은22:28:07 UTC 정상 종료 로그를 남겼고22:30:28 UTC UE/빌드/셰이더0을 확인했다. 엔진 종료 뒤 launcher의 PowerShell false/$false 오타가 있었으나 실제 작업은 반복하지 않고 캡처·종료로그·프로세스 상태를 독립 확인해 근거를 보완했다.
- 최종 근거: Feature/doc/evidence/sword-knuckle-20261110/particle-candidates/final-integration-summary.json 및 SkillParticlesReviewG/visual-review.json.
[Notion 요약 후보] 대검 편집 모션과 통합 기능 회귀를 검증했고, 너클·대검 파티클30후보 전부 실제 렌더를 검토했다. 최종 게임 연결 전 대비/위치/실제 전투 조건 보완이 남아 있으며 원본·저장·마법 자산은 보존했다.

### 2026-11-10 지팡이 소유 자산 재사용·활성 VFX 시인성 재개
- 목표: 확인된 소유 모델/모션으로 지팡이 장착을 검수하고 기존 활성 VFX의 실제 게임 시인성을 개선한다.
- 변경: 검증된 로컬 Wizard 지팡이5원본 설치·MCP Staff01 파생1개, StaffStrike/Shot Manny/Human 파생4개, 기존 두 스킬의 밝기 파생 VFX 연결, 3인칭 VFX의 FP 메시 선택 오류 수정.
- 검증: 원본17/모델5 보존, retarget8PNG, 빌드22.66s/15.02s, 실제 게임10스킬34PNG와 첫 피해 대검5 proxy/실제3P부모·scale·Niagara age 통과, 저장444보존.
- 상태: 지팡이 production 기본값·실제 플레이어 검수 진행 중. cast 최종 시인성과30후보 활성화는 미완료. 공개 source 배포권 미확인으로 커밋/푸시 없음.
- 문서: Feature/doc/2026-11-10_staff-equipment.md, Feature/doc/2026-11-10_active-weapon-vfx-readability.md.


## Sprint#9-29 지팡이 실제 장착·대기 방향 (UTC 2026-11-11)
- 목표: Staff 모델 기본값 미연결 및 대기/회복 뒤집힘을 실제 코드로 해결한다.
- 변경: native 모델/grip/policy, 소유권을 보존하는 finalized-bone 방향 보정과 DefaultSlot 블렌드, 실제 grid 장착 fixture 및75조건 수학 회귀.
- 검증: Editor53.54초 성공, StaffNativeResumeC461프레임·8PNG·2클립·실제 합성아이템 장착 실패0, 저장444·보호25패키지 보존,8장 직접 검수. 무기 회귀 별도 결과 대기.
- 상태: 로컬 구현/장착·표현 검증 완료. 전투DA 신규연결/피해·전종족·풀런 미인증,1인칭 보류·공개배포 없음.
- 문서: Feature/architect/2026-11-11_staff-upright-idle.md; Feature/doc/2026-11-11_staff-upright-idle.md.

무기 최종 회귀: StaffNativeUnitC 실제 report12/12성공·경고0·오류0, 저장444보존.75조건 StaffUprightMath 포함. 실행기 요약의 과거 개수11표기는 원본을 보존한 reconciled-summary.json으로 정정했고 검증 gate는 처음부터12개였다.


### Sprint#9-29 전리품 QA 보호 통합 (UTC 2026-11-11)
- 목표/변경: 기존 기능 보존, 전체·숨김 저장 및 suite/error 검사를 보강한 QA 스크립트 통합.
- 검증: 격리29/29, 현재DLL ResumeQA1111전체8/8성공, 원본변경0·예상밖신규0·suite red0. 사전 self-test red15분리.
- 상태: 로컬 통합·현재DLL검증완료, Write/Read재실행·물리입력/풀런미실행. 커밋/푸시없음.
- 문서: Feature/doc/2026-11-11_loot-qa-resume.md.


### 2026-11-11 마법 스킬 VFX 연결 구현 패킷
- 목표: 기존 Niagara 후보를 현재 Staff 스킬 경로에 연결할 실행 코드를 만들고 공유 에디터/빌드와 충돌 없이 검증한다.
- 주요 변경: Primary01 visual-only 계약, 공식 MCP prepare/inspect/apply/verify, 전체 필드 보존·stale/dirty/charge 사전 감사·저장 파일 해시 고정, 오프라인 테스트.
- 검증: unittest29개·Python AST4파일·CheckGeneratedChanges 통과. 후보8개 해시 일치. live MCP/UE/build/DA 수정 없음.
- 상태: 구현·오프라인 검증 완료, 실제 연결과 실전/cold reload 검수는 통합 담당자 에디터 사용권 대기. Circle/Release·기존 Charge는 미연결/보존.
- 관련 문서: Feature/architect/2026-11-11_magic-skill-vfx-binding.md, Feature/doc/2026-11-11_magic-skill-vfx-binding.md, Feature/doc/evidence/magic-skill-vfx-binding-20261111/validation.json.

## Sprint#9-29 다섯 마을 런 연결 우선 구현 (UTC 2026-11-11)
- 목표: 기존 월드를 보존하며 후보를 정상 Hub·생산3인칭·1200초 런에 연결할 신규 구현을 마련한다.
- 주요 변경: 새 Portal/GameMode, PlayerState 관측 Component, RouteDataAsset 스키마·독립 상태 모델, 담당자용 포털 추가/보존/재로드 코드, UE Automation2개와 portable C++회귀. 기존 파일·맵·무기·카메라·세이브는 변경하지 않았다.
- 검증: 독립 MSVC52검사·실패0와7만 회 재진입, 활성화 AST/guard2개 통과. 독립 검토에서 포털 lateral축 오류를 수정했다. UE 컴파일·자동화2개·실제 진입/timer/귀환은 미실행.
- 상태: 신규 소스 구현 완료·독립 검증 통과, 공유 지팡이/통합 담당자의 빌드·에디터 활성화 대기. NavMesh/보스·서비스/의뢰/던전·마을pause/cook·60초/15분 실측은 후속. 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-11-11_five-village-runtime-connection.md; Feature/doc/2026-11-11_five-village-runtime-connection.md; Feature/doc/evidence/five-village-runtime-20261111/README.md.


### Sprint#9-29 보스·카탈로그 현재 DLL 통합검증 (UTC2026-11-11)
- 목표/변경: 기존 gameplay 보존, readiness/QA실행기와 격리검사·문서 통합.
- 검증: 보스격리49, 타깃수명5, 자연AI112, 카탈로그격리8/엔진6 모두 통과. 원본 저장 보존·예상밖신규0. 카탈로그 테스트/종료red0,카탈로그사전red0(전리품사전15와구분).
- 상태: 로컬통합/현재회귀완료. 장판시각·20개콘텐츠/선택UI/저장이관·풀런미완료,1인칭보류·공개배포없음.
- 문서: Feature/doc/2026-11-11_monster-ai-resume-readiness.md; Feature/doc/2026-11-11_catalog-resume-validation.md.


### Sprint#9-29 Staff Primary01 VFX 실제 연결 (UTC2026-11-11, 검증중)
- 변경: 백업 후 공식MCP 단일DA8표현필드저장. FP-only배율0.5·비시각필드·나머지자산보호.
- 검증: live serialization교정/FP보존36단위, 전체snapshot·저장hash apply/readback성공.
- 상태: 실제연결완료, cold readback·3전투케이스11PNG 검증진행. Circle/Release/Charge/피해·모션불변·1인칭보류·공개배포없음.
- 문서: Feature/doc/2026-11-11_magic-skill-vfx-binding.md.


## Staff Primary01 VFX cold reload·실전 정합성 검증 (UTC2026-11-11)
- 실제8필드 연결 후 서로 다른 Editor PID41608→41280의 종료/재시작·cold readback을 통과했다. FP전용0.5·기존비시각필드·나머지자산보존이다.
- 통합 Editor28.15초 성공. 실제3요청 정상피해/무적/빗나감,11callbackPNG·Niagara age/parent/scale·native montage 연속재생·원본저장/파일보존 실패0.
-11장 모두 직접 보았으나 기본3인칭 cue는 가림/대비/이른 표본 때문에 뚜렷하지 않아 시각 완료로 승인하지 않았다. 첫HP감소는 기존delay0의 관측이며 접촉승인이 아니다.
- 기술연결/정합성 완료와 시각개선 대기를 구분하고 맵 검증을 우선한다.1인칭보류·공개커밋/푸시없음.
- Notion 요약 후보: 지팡이 기본공격 VFX를 실제연결하고 저장재로드와 피해정합성을 검증했다. 실제 화면에서 가독성 개선이 남아 별도 추적한다.

## 다섯 마을 별도 런 입구 실제 활성화 (UTC2026-11-11, 여행 검수 대기)
- 새 GameMode/PlayerState 방문 Component/전용 Portal/경로정의 source를 실제 UHT·Editor 빌드에서 통과시켰다. 최종 통합 빌드28.15초 성공이다.
- 현재 DLL의 Rogue10m.FiveVillage RuntimeDefaults/RouteDefinition2개가 정확경로·queue·종료0·원본저장/DLL보존으로 통과했다. 이2개는 여행 검사가 아닌 기본값/스키마 검사다.
- Hub.umap과 외부Actor/Object709개를 SHA일치 백업했다. 단독 clean Hub Editor PID29248에서 지면/캡슐통과 검사 후 새 입구를(600,4270,180)에 추가·저장했다.
- L_Menu→StartHub 실제 재로드에서 새 Portal1개·기존 Portal1개와 원래 목적지/위치/옵션 불변을 확인했다. 후보/기존 OpenRunMap/L_Menu 파일SHA는 동일이다.
- 디스크 변화는 Hub.umap수정1개와 새 Portal/폴더 외부패키지3개뿐이다. 기존709개 중 Hub.umap 외 패키지 변경은 없다.
- 정상 프로필 진입·방문/재방문·타이머·정산·귀환·새 런 초기화는 후속 실제 travel fixture에서 검증한다. 보스/NavMesh/서비스/의뢰·60초/15분 실측·cook/full run은 미완료다.
- Notion 요약 후보: 다섯 마을 후보로 가는 별도 입구를 Hub에 실제 배치하고 저장재로드를 확인했다. 기존 입구는 보존했고 정상 런 여행 검수는 별도 진행한다.
- 근거: Feature/doc/evidence/five-village-runtime-20261111/{VillageUnitA,EditorActivation1111A}, entry-activation-*.json.
### Sprint#9-29 다섯 마을 실제 여행 QA fixture·실행기 (UTC2026-11-11)
- 목표: 정상 입구 여행·1200초 시계·직행/방문·재방문·포기 정산/저장·Hub 귀환을 실제로 검증할 격리 fixture와 실행기를 준비한다.
- 변경: Source 설치 대상 C++ 한 파일을 Feature/implementation에 준비했다. 계획 기본값/사용권·해시·DLL·프로세스 guard, 두 QA 저장 namespace, 원본 저장 보존과 엄격한 결과 판정 helper·오프라인 검사를 추가했다. 공유 Source·에셋·설정은 수정하지 않았다.
- 검증: 계약83·계획27검사 실패0, 프로세스 시도0, AST·정적/독립 검토 통과. 통합 담당자의 새 포털 저장·재로드 기록을 확인했다. fixture UE 컴파일·실제 여행/60초는 미실행이다.
- 상태: 격리 구현·오프라인 검증 완료. Windows 패키징 동결/공유 사용권 해제 후 Source 한 파일 설치·새 Editor DLL 빌드·실제 Direct/Revisit 실행 대기. 정산은 무보상 포기 범위이며 보스 승리·비영 재화 rollback·15분 콘텐츠·물리 입력/전체 런은 후속이다.3인칭 유지·커밋/푸시 없음.
- 관련 문서: Feature/architect/2026-11-11_five-village-travel-fixture.md; Feature/doc/2026-11-11_five-village-travel-fixture.md; Feature/doc/evidence/five-village-travel-20261111/README.md; preparation-validation.json.


## Windows 실행 패키지 빌드 환경·독립 체크포인트 (UTC2026-11-11, 진행중)
- Editor DLL 빌드와 별개로 Win64 Game Development·cook/stage/Pak+IoStore/archive를 실제 수행한다.
- 기존 VS2022/MSVC14.44·SDK10.0.22621을 사용하며 Turnkey VerifySdk가 실제 VALID/종료0을 확인했다. 새 소프트웨어 설치나SDK업데이트는 하지 않았다.
- Source/Config/저장된Content4.48GiB·6665개를 D:/R10/Win1111A/Rogue10m의 독립물리 체크포인트로 복사했다. Source/Config 전체SHA와4맵/기본StaffDA를 대조했다. 공용Source/Config/Content는 복사 이후 별도Work가 계속 가능하다.
- PackageWindows.ps1에 명시4맵·IoStore/Prereqs·SDK확인·fresh경로·실제결과기록을 추가했다. cook출력은 Windows로 끝나는 별도경로,stage/archive도분리한다. 처음독립체크포인트는Editor모듈도 함께컴파일한다.
- 초기실행기 timestamp/Join-Path 인자조립문제를ActualSDK뒤에 발견해 교정했다. 전체UAT인자를Preflight에서먼저조립/저장하도록고쳤고 실제BuildCookRun을시작했다. UE컴파일실패로 오인하지않는다.
- 이전Build/Windows·기존Saved캐시는 덮거나clean하지 않았다. 신규자동전리품/퀘스트/무기스킬/보스cadence/VFX가독성/실travelQA 대기패킷은 이번체크포인트에미포함이다.
- 실제패키지cold실행/메뉴PNG/필수Cook자산·저장경로검수는 빌드후수행하며 아직완료로보고하지않는다.


### 2026-11-11 Windows 실행용 패키지
- Editor DLL과 구분되는 실제 Rogue10m Win64 Development Game 컴파일·4맵 cook·IoStore·stage·archive 성공. SDK 자동 설치 없이 기존 환경 검증.
- native soft runtime Widget/무기/동작/로비재질 cook 누락을 실제 실행에서 수정. 42개 exact 자산 검사, missing-package/모든 Error 실패 gate, bootstrap 자식·격리 저장 경로 검증 추가.
- 최종 D: BCR94.24초 exit0, cook0오류3기존경고. 내부EXE/루트런처 DX12 cold 메뉴 및4PNG 확인, 저장447개 변경0. 전체 패키지 런/전투/포털·Shipping 검증은 미완. 자동획득 등 다음 패치는 미포함. Feature/doc/2026-11-11_windows-executable-package.md 참조.


### Sprint#9-29 하위 작업: 몬스터 자동 입고·초과 수량 드롭 (2026-11-11)

- 목표: 사냥 보상을 인벤토리에 먼저 입고하고 부분 스택을 채운 뒤 들어가지 못한 수량만 바닥에 드롭한다.
- 주요 변경: 전용 인벤토리 입고 API·실제 처치자 연결·월드 수명 잔량 보관·바닥 실패 재시도·receipt 중복/거부 상태·런 종료 가드. 기존 줍기·저장 형식·골드·경험치·3인칭 유지.
- 검증: 실제 수량 정책 standalone clang 컴파일, 9개 사례·무작위 5만 회·2,103,255검사 통과. 신규 UE 5개 작성 및 기존 저장 fixture 갱신. UE 빌드·자동화·실게임 미실행.
- 상태: 격리 구현·독립 검증 완료, 공유 통합·Editor 빌드·신규/기존/저장 회귀·Windows 패키지 반영 대기. 공유 파일·프로세스 미변경, 커밋·푸시 없음.
- 관련 문서: Feature/architect/2026-11-11_monster-auto-loot-overflow.md, Feature/doc/2026-11-11_monster-auto-loot-overflow.md.


## Sprint#9-30 근접 신규 기술 콘텐츠 구현 패킷 (UTC 2026-11-11, 격리)
- 목표: QA 실행기 준비에서 나아가 G04/G10 실제 전진 종점 단타와 K01~K10/G01~G10 독립 데이터 제작 경로를 구현한다.
- 주요 변경: 기본 비활성 전진 옵션/타이머 컴포넌트·Combat 피해 연결·회피 상호 배제, 승인 snapshot/바닥·취소·재진입 보호, 원본 필드 보존과 제작중 metadata를 갖는20개 Editor 레시피/제작기. production 프로필·성장·세이브는 연결하지 않았다.
- 검증: 독립 MSVC 시간축10시나리오56검사·모의 Unreal16개 통과,18원본 로컬 파일/해시 확인·읽기 전용 소스 리뷰. UE 빌드·실제 DA 생성/재로드·물리/시각 검증은 미실행이다.
- 상태: 격리 구현/독립 검증 완료·패치 통합 인계. 공유 반영과 UE 사용권은 담당자 조율 후 진행. G05/G06/G08 고유기믹·G10 경로피해·전용모션/VFX·UI/저장이관은 미완료다.1인칭 보류·커밋/푸시 없음.
- 관련 문서: Feature/architect/2026-11-11_melee-skill-content.md; Feature/doc/2026-11-11_melee-skill-content.md; Feature/doc/evidence/melee-skill-content-20261111/README.md.

### Sprint#9-30 전진 기술 중단 계약 보강 (UTC 2026-11-11, 격리 후속)
- 목표/변경: 통합 검토에서 발견된 paid 동기취소 쿨다운 누락과 외부 몽타주 중단 뒤 잔여 이동/타격을 수정했다. 지불 직후 공유쿨다운1회·긴 deadline metadata보존, 승인 playback instance poll과 Combat 마지막 접촉검사를 추가했다.
- 검증: 독립 소스/API·콜백 리뷰에서 완료 listener 경계를 수정하고 추가 결함 없음. 실제 자원 delegate/몽타주 상태 UE 회귀를 작성했으나 공유 UE/빌드 소유권으로 컴파일·실행은 미실행이다. 기존56/16검증과 구분한다.
- 상태: V1 보존·수정본/전용delta 인계. 의도적 중단 후 커밋은 디자인 미결정이며 기본 취소다. 공유 파일·에셋/세이브 변경0·UE/UBT0·커밋/푸시0.
- 문서: Feature/doc/2026-11-11_melee-skill-content.md; tmp/SkillContent20261111/Delivery/InterruptionFixB/README.md.

### Sprint#9-30 피격 기본 취소 구현 (UTC 2026-11-11, 격리)
- 목표/변경: 사용자 결정에 따라 양수 실제피해→기술취소를 Character/Combat/Basic에 연결했다. 비용/use/쿨다운 보존, 남은 연타/입력/콤보/GAS·전진 정리, local pulse snapshot과 반사피해 hit/miss 확정, exact playback 중단을 구현했다. 면역/슈퍼아머/피격모션은 추가하지 않았다.
- 검증: 새 UE Automation9개 작성 및 독립 소스/API·콜백 리뷰 완료. 엔진 컴파일/실행은0으로 기존 독립56/모의16과 구분한다. 신규native 표적UCLASS의 inline 구현으로Shipping링크 위험을 줄였다. 실제Windows빌드·패키징/플레이는 상위담당자 범위다.
- 상태: 구현9/UE회귀3파일의 수정본 full 및 V1 delta 인계. 공유 Source·에셋·설정 동결 준수·UE/빌드 사용0·커밋/푸시0. 면역 도입 여부는 사용자 실플레이 테스트 뒤 결정이다.
- 문서: Feature/doc/2026-11-11_melee-skill-content.md; tmp/SkillContent20261111/Delivery/InterruptionFixB/README.md.


### Sprint#9-29 다섯 마을 선택 회수 의뢰 수직 구현 (UTC2026-11-11)
- 목표: 길을 잠그지 않는 V1수락→V3회수→V1재방문/보상 흐름을 기존 게임 계약에 연결할 격리 구현을 준비한다.
- 주요 변경: 신규 C++7개(모델/DataAsset/PlayerState Component/상호작용Actor), 비충돌2actor 오프라인배치준비기·명세/검사. 수락전fact·같은RunId상태·새Run초기화·실제인벤토리성공후1회확정·실패재시도/재진입차단. 기존 NPC/게임모드/세이브·전투·카메라 수정없음.
- 검증: MSVC17 W4/WX 모델69/0·2만회stress·compile/run0, 배치25/0·원본scene/맵/물약SHA보존, 독립API/수명검토에서추가critical/medium없음. 공유Source설치·UHT/UE전체빌드·실제F/지급/재로드/정산은미실행.
- 상태: 격리구현·오프라인검증완료, 공유담당자사용권/동결해제후Source7개설치·빌드·기존후보에Actor2개additive배치/실제검증대기. 전체5마을상점/던전/보스·안전메뉴·60초/15분·cook미완료.3인칭·피격취소유지·면역보류·커밋/푸시없음.
- 관련 문서: Feature/architect/2026-11-11_five-village-optional-quest.md; Feature/doc/2026-11-11_five-village-optional-quest.md; Feature/doc/evidence/five-village-quests-20261111/{README.md,implementation-validation.json,placement-plan.json}.

## Sprint#9-31 — 월드맵 실제 제작 참조 통합 (2026-11-12)
- 브랜치: Sprint#9-31-world-map-production-reference
- 목표: 대화의 최신 기획과 생성 지도를 실제 맵 제작자가 함께 참조하도록 프로젝트에 보존한다.
- 주요 변경: 제작 기준·이미지/생성기록·배치/획득처 JSON, 시작 마을/3거점/5초소/15일반+4중간+1최종·직업 경로·NPC 대사·최신 입력/비급 정책 통합. AGENTS와 기존 제작 가이드 진입 링크 추가.
- 검증: JSON/참조/거리/이미지해시·로컬링크·생성경로 검사. 최종 결과는 Feature/doc/evidence/world-map-production-reference-20261112/validation.json 참조. UE 빌드/PIE/실제 맵 적용은 문서 범위 밖이다.
- 상태: 제작 참조 문서화. 실제 맵/신규AI/비급 제작은 계획 단계이며 기존 WIP를 완료로 승격하지 않는다.
- 관련 문서: Docs/WorldMapProductionGuide.md; Docs/References/WorldMap/README.md; Feature/architect/2026-11-12_world-map-production-reference.md; Feature/doc/2026-11-12_world-map-production-reference.md.
- 최종 검증 추가: 정적584/링크36 통과, PNG 원본SHA 일치, 기존 WIP67개 해시 보존. 독립검토2건 반영. 원격 통합 기준보다 기존28커밋이 앞서 문서 작업 브랜치 체크포인트만 공유하고 통합 승격은 별도 검증까지 보류한다.

## 게임 종료 UI (2026-11-11)
- 목표: 모든 메뉴의 종료·확인·취소와 저장 실패 보호.
- 변경: 공통 C++ UMG 확인창, Controller 컨텍스트 검사, 정상 저장 후 종료, 기존 런 중단 정책 유지.
- 검증 및 상태: Feature/doc/2026-11-11_game-quit-ui.md 참조.
- 동시 작업 중인 브랜치·에디터·기존 변경은 유지. 공유 원본의 빌드/commit/push는 수행하지 않음.

### Sprint#9-32 마비노기 비교 맵 규모·밀도 기획 (한국시간2026-11-12)
- 목표: 사용자 맵과대/마비노기참고피드백을 현재맵증거·공식자료로비교하고5핵심거점/갈림길/재방문·직행1분/성장15분과직접편집가능성을보존하는기획서를작성한다.
- 주요내용: 실제1.8km사방·21앵커760×340m·프로그램48.537초(안정화2초/허브준비teleport구분)·실제캡처2장분석. 공식마을서비스/주변활동과우리제안을분리.900×562.5m사본의면적84.375%축소산술안과기능밀도·검증/편집계획.
- 검증: 기존terrain/scene/runtime/portal증거및PNG직접검토,마비노기PC공식3페이지확인. 새맵·높이파일·Source/설정·카탈로그수정/빌드/Editor/MCP/게임실행없음. 새후보플레이·충돌·콘텐츠·성장15분은미실행.
- 상태: 기획서안전저장완료·사용자지시에따른개발일시중단. 소유UE/UBT프로세스0,타인프로세스미조작,읽기조회1건세션완료상태미확인. 커밋/푸시없음.
- 관련문서: Feature/architect/2026-11-12_mabinogi-map-density-plan.md; Feature/doc/2026-11-12_mabinogi-map-density-plan.md.

### Sprint#9-32 맵/공유통합 재개 CheckpointA (UTC2026-11-13)
- 목표: 현재WIP보존·단독UE/빌드사용권확인후최신맵축소의독립사본을작은실행단위로준비한다.
- 변경: 체크포인트/복제실행기·설계/인계문서 추가,2478파일/자동저장9개백업,신규WP맵 WorldLayoutCompact20261113A 저장. 기존Source/Config/원본맵/포털교체없음. 후보helper2개는격리준비만.
- 검증: 정규UHT/Editor134.95초exit0·Source/Config339기존SHA변화0,정식duplicate exit0·외부Actor1360/Object58·보호2478파일SHA변화0. map QA입력20/종료manifest해시일치,실행기/후보helper정적독립리뷰통과.
- 상태: 첫실행체크포인트완료. 실제crop/재로드·현재레이어/페인트/액터동등성·통행/15분/패키지는미검증.900×562.5기획과9×5보존대안구분,3인칭/피격취소유지·면역보류·커밋/푸시없음.
- 문서: Feature/architect/2026-11-13_compact-world-resume.md; Feature/doc/2026-11-13_map-integration-checkpoint-a.md; Feature/doc/evidence/map-resume-20261113/{BaselineA,DuplicateA}.


### Sprint#9-32 변경 — 2026-11-13 비GUI 맵 체크포인트 B

- 목표: 현재 편집 가능한 Landscape 데이터의 보존 기준 확보와 화면조작 중단 준수
- 변경: 소유 helper2개 v2, 읽기 전용 commandlet, 상태/해시 근거 문서
- 검증: UHT/Editor exit0·30.55초; commandlet exit0·21.67초; 지형256/충돌256/프록시16·179488샘플; 원본2478·후보1419 SHA불변
- 상태: 후보는1800×1800m 그대로. native Manage/Delete 및 실제 UI 검증 보류; 통합 소스 후속 진행 중
- 관련: Feature/doc/2026-11-13_map-integration-checkpoint-b.md


### Sprint#9-32 변경 — 2026-11-13 통합 체크포인트 C

- 목표: 다른 Work의 검토된 소스를 보존 통합하고 GUI 없이 실제 UE 검증
- 변경: G08+VFX29/NPC30 소스·도구·안내, UHT 배열 Units2곳 호환 수정, public Import 보존 검토
- 검증: UHT/Editor exit0·104.31초; native43/43·37 clean+6 warning cases; 오프라인30; 같은DLL cold reload exit0·25.67초·지형기준값동일
- 보존: 기존 Content2139/Config9와 baseline 패킷외2471파일 유지; 후보지형1800×1800m·1360액터 그대로; GUI입력0
- 상태: 소스통합 검증 완료, 실제 맵축소/NPC배치/영상·물리플레이/Windows패키징은 미완료; 기존 사망귀속문제 미해결
- 관련: Feature/doc/2026-11-13_map-integration-checkpoint-c.md 및 체크포인트B


### Sprint#9-32 변경 — 2026-11-13 D 진행

- 목표: GUI없이rawBA/의존성보존조사및별도미검증내부Windows개발빌드진전
- 실제: actor9×5bounds모두포함; registry1419패키지·원본private향0/외부incoming0; SDKValidexit0·8.50초; 입력9524와기존save458보호기준
- 상태: v3staged준비중·shared미설치; 현재1800m·NPC미배치·Game/Cook미실행·GUI입력0
- 관련: Feature/architect/2026-11-13_terrain-dependencies-smoke-package.md; Feature/doc/2026-11-13_map-integration-checkpoint-d-progress.md


### Sprint#9-32 변경 — D raw실자료/최종native

- 변경: readonly helper v3/v4, raw8채널45tile·SHA연결, 별도exactEnglish/startupRED0 validator
- 검증: v4Editorexit0·21.63초; raw360BIN·패키지/semantic불변; 독립QA실패0·pending19·exit3; 최종DLLnative43/43·전체RED0
- 정정: 원래A시작smoke오류15줄은보존/미승인,새영어nativeB/C는전역RED0실제확인
- 상태: 최종입력캡처후내부Windowsplan검증중·Game/Cook미실행; crop/NPC/기본맵변경0·GUI0·releasefalse
- 관련: Feature/doc/2026-11-13_map-integration-checkpoint-d-progress.md


### Sprint#9-32 추가 — Game fixture guard와 내부 smoke 재검증

목표: Editor 전용 G08 fixture의 Game 컴파일 실패 수정. 변경: CPP guard 1행과 native D 출력 라벨, 수정 전 백업. 검증: run02 UAT exit6/Cook0, 입력10456·Editor6·저장2root 불변; 수정 후 Editor exit0/12.2146초. 상태: native D 및 새 봉인 후 내부 패키징 진행 중, release 승인 아님. 관련: Feature/doc/2026-11-13_map-integration-checkpoint-d-progress.md, Feature/doc/evidence/map-resume-20261113/GameGuardFixD/changes.json.


### Sprint#9-32 추가 검증 — NativeD와 Windows run04

검증: guard 수정 DLL native43 성공, 37+6사례/12경고, 전체 RED0, SourceConfig366 불변. 전체입력10456 중 의도한2파일 외 불변, Content9521·저장2root 보존. 상태: 새 내부 run04 pre-lease 검증 중, UAT/Cook 미판정/releasefalse. 병렬: crop pending19 증거 계획과 F 입력 회귀 메모 완료, World Map Maker API 부분검증 준비. 관련: Feature/doc/2026-11-13_map-integration-checkpoint-d-progress.md.


### Sprint#9-32 추가 — 실제 staging 실패 및 출력 격리 계획

Game fresh link/metadata와 Cook2323 완료, UAT staging exit1. 원인: cook root와 stage Windows 하위 경로 불일치. 입력차이: 생성 Build/Windows/FileOpenOrder/EditorOpenOrder.log 1개 발견, old seal은 실패 유지. 조치: 새 내부 derivative의 CookOutputDir Windows·FOOBASEDIR·정규 Game Rebuild 검토, 원본strict/B/B2 불변. 상태: Windows archive 미완료, releasefalse. 관련: Feature/architect/2026-11-13_internal-smoke-output-isolation.md.


### Sprint#9-32 — 내부 Windows E5 실제 아카이브 완료

목표: 원본 보존을 유지한 정규 내부 Win64 Development build. 구현: Cook Windows 경로/FOO 로그 격리/정규 Game Rebuild, Editor fixture guard. 검증: Editor0+Native43D/globalRED0; 실제Game99/Cook/Stage/Archive UAT0/317.48초; 전체입력10456·Editor6·저장2root불변; 공식IoStore5맵; payload50files/1.948GB 동일SHA. 상태: 내부아카이브 완료, releasefalse/UI·물리·저장라우팅 미검증; 맵1800m·crop19pending·NPC미배치. 관련: Feature/doc/2026-11-13_internal-smoke-output-isolation.md, tmp/InternalWindows20261113E5_UNVERIFIED/DELIVERY.json.


### Sprint#9-32 — World Map Maker 후속 QA 승인 차단

완료:9896물리사본/원본·Editor6·저장대조/새OriginGuard/고유map예약/metadata구체plan. 차단:root-startup-approval-A.json에approved=true 내부Root영수증생성을자동승인심사가실제승인전제우회위험으로거절(CreateProcess미실행). trustedhistory확인후새명시승인없음,재시도·우회0. QA UE/Actor/cold0,관측engine/build/game0. WindowsE5아카이브완성에는영향없음. 관련: Feature/doc/evidence/map-resume-20261113/MapMakerPhysicalQA/automatic-approval-block.json.


### Sprint#9-32 — NPC 처치 귀속 독립 통합 시작

목표:death 이력과정당한credit확정분리. 범위:7c31084 combined12만,full30덮어쓰기0. 사전검증:8기존 raw/blob일치·4부재·20관련불변·패킷SHA3일치·runtime/test분리동일. 상태:미적용,정규Editor와신규8/기존NPC회귀예정. metadata승인/cloneQA중단유지,E5는수정전고정. 관련:Feature/architect/2026-11-13_npc-combat-credit-integration.md.


### Sprint#9-32 — NPC 첫Native51 실패와fixture만보강

검증:Editor/UHT0/102.097초,현재설치standalone202871pass. 실제Native51A:43pass/신규8fail/RED57,SourceConfig368·DLL불변. 원인:APawn의AuthGameMode없는피해거부. 수정:공개nativeMode/Run/World초기화test CPP1개+positive조건, runtime9·피해API·assertion보존. 상태:재빌드/Native51B예정,기존실패보존. metadataQA중단유지. 관련:Feature/doc/evidence/npc-combat-credit-20261113A/FixtureDerivativeB/derivative.json.


### Sprint#9-32 — NPC 처치 귀속 소스 통합과 Native51B 완료

- 목표: 물리 사망 이력과 올바른 플레이어의 처치 인정을 분리하고 무귀속 복구·중복 보상·재진입·시체 파괴 후 지연 전달을 보호한다.
- 변경: combined 12개 파일을 한 번 적용, 기존 8개 백업과 신규 4개 추가. 테스트 fixture 1개에 공개 GameMode/Run/World 초기화를 보강했으며 runtime 9개는 전달 postimage를 유지한다.
- 실제 검증: Editor/UHT 0(최초 102.097초, fixture 후 41.881초), 설치 소스 standalone 202,871개 통과. Native51A의 43 성공/8 실패를 보존하고, 새 DLL Native51B 51/51(NPC 26 + G08/VFX 25), 전체 RED 0, 경고 11사례/19건을 확인했다. 독립 검토도 일치한다.
- 보존: 전체 입력 10,460개 중 지정한 기존 8개 변경/신규 4개만 확인. Content 9,521개·저장 2루트·이전 Windows E5 전체 payload와 Game exe 불변. E5는 수정 전 버전으로 고정한다.
- 상태: 소스 통합·격리 C++ 회귀 완료. 실제 Director·충돌·loot·production GUID·F/UI·새 런/저장/재시작은 미검증. 이번 작업 Computer Use 재허용을 반영했으나 실제 GUI 입력 0이며, 별도 Root 승인/metadata clone QA 중단과 재시도·우회 0은 유지한다. 사용자 WIP를 섞는 commit·push는 하지 않았다.
- 관련: Feature/doc/2026-11-13_npc-combat-credit-integration.md; Feature/doc/evidence/npc-combat-credit-20261113A/Native51B/reviewed-result.json; Feature/doc/evidence/npc-combat-credit-20261113A/preservation-result.json.


### Sprint#9-32 — Compact 초급 의뢰 실제 저장·cold 검증

- 목표: H 수렵촌에 초급 NPC1·회수목표2·골드40을 편집 가능한 DA/Director로 연결한다.
- 실제 변경: 후보 전용 DA1·Director external actor1·folder OFO3, 신규5파일. 기존 후보1,419/SrcCfg368/DLL 보존. 기존 아린 외형과 Engine marker를 재사용했다.
- 검증: normal live WP1360/기존Director0; 지면3 trace 일치; native DA validation; 소유Editor 정상 종료 후 새PID cold에서 DA/Director serialized 값·참조 일치. 기존Actor package 저장0, 지형 crop0.
- 상태: 콘텐츠 저장/cold 완료, 별도QA profile/Game F·수락·회수·보상·재방문·RunId 검증 진행 중. 실제 StartHub 포털에 Compact 연결0이므로 일반 진입 완성 아님. 기존 E5 수정 전 고정. metadata 승인/clone QA 차단·우회0 유지.
- 관련: Feature/architect/2026-11-13_compact-intro-quest-vertical-slice.md; Feature/doc/2026-11-13_compact-intro-quest-vertical-slice.md; Feature/doc/evidence/npc-intro-compact20261113B/cold-reload-result.json.

### Sprint#9-32 — Compact 초급 의뢰 실제 게임 수직 흐름 관찰

- 목표: 실제 DA/Director의 F 수락·회수2·재방문·중복 보상·새 RunId 초기화 확인.
- 검증: 정상 QA 프로필/허브 후 현재 DLL -game, 0/2→1/2→동일목표재접촉1/2→2/2, Gold0→40→재보고40/보상로그1. 일반 포기·정산·허브 후 같은 프로필의 새GUID와 수락 전0/2 확인. 실제 PNG6장 독립 검토, 정상 종료와 엔진/빌드/게임0.
- 한계: 후보는 개발용 open/위치 준비를 사용했고 ordinary Hub 연결0. 지형1800m·미축소. 연속 위치 이동 F 안내 소실/재진입 복구, 월드 한글 네모/기존 회색재질·누락 의존, 활성무기 F·combat loot·진행 영구저장·패키지/UI7/물리루트 미검증.
- 보존검사: Content5 외 Editor 생성 pyc1 추가를 좁게 확인·독립 bytecode 검토, 삭제/수정 없이 전체input10466으로 명시 예정. 기존10460의 불변 assertion 통과, 최종 통합 보존결과는 별도 기록.
- Windows 후속: 별도 Native51Smoke 도구33파일 exact ZIP 설치·코드 검토, 오프라인47/47 통과. 새 전체 capture/현재콘텐츠 freeze/소유세션 release 뒤 독립 Game/Cook/IoStore 경로 예정. 기존 E5는 수정 전 고정.
- 관련: Feature/doc/2026-11-13_compact-intro-quest-vertical-slice.md; Feature/doc/evidence/npc-intro-compact20261113B/runtime-log-observations.json; Feature/architect/2026-11-13_native51-intro-internal-windows.md.


### Sprint#9-32 — Compact 수직 흐름 최종 보존 확인

- 실제 결과: 기존10460입력·SrcCfg368·CA647 DLL·기존후보1419·보호 저장2루트·이전E5 payload50/1,948,283,459B SHA불변. 추가Content5+정확한Editor생성cache1로전체10466/Content9526.
- 검사기의 bytes/size 및 unexpected cache 중단을 보존하고 Architect/독립Reviewer 재검토 후 exact 비교 통과. cache 삭제/수정·broad exclude0.
- 상태: 실제 초급 수직흐름과 콘텐츠저장/cold 완료, 기존문서한계 유지. 새6맵 Windows 내부capture 진행, 아직Game/Cook0. 기존E5·strict/B/B2 및Root승인/metadataQA중단 유지.
- 관련: Feature/doc/evidence/npc-intro-compact20261113B/runtime-final-result.json; Feature/doc/evidence/npc-intro-compact20261113B/post-game-preservation.json.

### Sprint#9-32 — Windows 내부 산출물 완료·실제 시작 실패 원인 확인

- 실제 build: Game/Cook/Stage/Archive0/418.71초; EXE99E843…15969 chain일치.50파일/1,951,832,646B SHA2454269E…9F1A4. 공식IoStore+sealedvalidator0:6맵+새DA7개,2252exports/158WP셀.
- 실제 runtime: 무창실패2회, 두 번째777006. 메뉴/프로필/저장routing/UI7 미도달. 일반반복중단→Architect/독립Reviewer→소유PID debugger로원래AV0xc0000005/0.297초·일치PDB실제7unwind확보.
- 원인: producer40B/consumer48B 구조체등록ABI,UBTcompile-NoLiveCoding override로reload구조가달라짐. 사용자Source/NPC/맵/세이브변경0. generated직접수정0.
- 상태: 실패후보Native51IntroB/E5/strict/B/B2보존.새FixF에서compileflag만분리하고guards유지하는도구파생준비; 실제새build/macro/ABI/boot는미검증. root-startup승인/metadataQA중단유지.
- 관련: Feature/doc/2026-11-13_native51-intro-internal-windows.md; Feature/doc/evidence/npc-intro-compact20261113B/StartupFailureB/abi-mismatch.json.

### Sprint#9-32 — FixF49 통과·C 실행 중단 상태 보존

- FixF: original33보존/변경6,현재manifest47파일Root해시재확인;helper0CDDDF8F…D6E0C/core5ACCE0EB…EA192. 합성49/49·PSparse0·리뷰Critical/Medium0.
- C: fresh10466입력·maps동일SHA,새seal6762A669…8174C. 실제PS11792세션은대기호출abort뒤없어졌고journal은pre_sdk/running잔류. SDK/UAT/Game미시작,새ABI/boot미검증.
- 보존: 원문journal/출력/lease+별도관찰. 중단뒤Editor6·저장2루트일치,현재Game99E843실패B그대로. 중단뒤전체입력재해시는미수행. UE/UBT/Game0,강제종료/재시도/commit/push0.
- 다음: 외부Codex상태재확인→새물리출력소유세션→Game정규build/cook/IoStore→실제48/48와macro확인→저장routing/실제UI. 기존C/E5/B/승인차단보존.
- 관련: Feature/doc/2026-11-13_native51-intro-internal-windows.md; Feature/doc/evidence/npc-intro-compact20261113B/C-interruption-observation.json.

### Sprint#9-32 — D 독립 출력에서 FixF 정규 빌드 재개

- 목표: 중단 C를 보존하고 실제 Game48/48 및 시작 가능한 Windows 내부 산출물 검증.
- 변경: D 전용 실행기5개, fresh capture10466 입력 일치, 새 seal707AADD5…01A01F. 원본 Source/Config/Content·동결 FixF 변경0.
- 실제 실행: 소유PS55740에서 정규 Game/Cook/Archive 요청, 현재 사전 검사. 성공·ABI·startup·routing·UI는 아직 미관찰.
- 상태: 진행 중. C/B/E5와 strict/B/B2 보존, 별도 거절된 startup/metadata cloneQA 중단 유지.
- 관련: Feature/doc/2026-11-13_native51-intro-internal-windows.md; Feature/doc/evidence/npc-intro-compact20261113B/internal-smoke-D-content-checkpoint.json.

### Sprint#9-32 — D 정규 빌드·실제 샘플 ABI48/48 확인

- 실제 빌드: SDK0/BCR0·431.85초, wrapper postflight0. Source/stage/archive EXE BA992042…AAB4443/PDB919BDF07…919BC5 일치. 전체10466·Editor6·보호 저장2루트 보존.
- 검토: native PDB14질의0으로 array96/row48/Info32/Version16 타입연결, 실제 initializer→정확배열/count2→같은행 loop/+48/count/backedge 확인. 샘플 독립읽기리뷰통과.
- 변경: D 전용 오프라인 증거 수집/질의/compile-provenance/검토요약 helper 추가. frozen FixF 및 Source/Config/Content 변경0, generated직접수정0.
- 상태: 실제Game/UI/routing 미실행, IoStore identity검사중. 전체ABI/CRT·릴리스인증아님. B/C/E5/lease/별도승인차단보존.
- 근거: Feature/doc/2026-11-13_native51-intro-internal-windows.md; Feature/doc/evidence/npc-intro-compact20261113B/D-abi-reviewed.json.

### Sprint#9-32 — D 실제 패키지 시작 메뉴 확인·CUA 접근 시간 초과

- 실제 산출물: Game/Cook/IoStore0,6맵+DA7개·2252exports·158WP셀.50파일/1,957,690,950B·CF46302D…C137FA,별도Native51IntroD_UNVERIFIED.
- 실제 실행: bootstrap14384→packagedGame54244, nativePNG로시작메뉴확인. 실제GameEngine 스크린샷경로와QA UserDir일치·generic설정fallback. QA SaveGames0파일/프로필생성0/실제save IO미검증.
- 차단: cua.getApp(window9964506) 앱접근승인301초시간초과. GUI입력/재시도/우회0. 권한메시지조회는user abort/결과없음. Game강제종료0,alive PID보존; 프로필/NPC/F/quit/restart 미실행.
- 보존/검사: 최종10466변경0·Editor6·저장2루트·E5/B/D불변. C원문/복사6쌍SHA일치·lease존재/재사용0. AST9/9·diff0·GeneratedChanges0. 기존WIP·별도승인차단·맵1800m등남은범위유지.
- 근거: Feature/doc/2026-11-13_native51-intro-internal-windows.md; Feature/doc/evidence/npc-intro-compact20261113B/D-runtime01-checkpoint.json; D-abi-reviewed.json.

### Sprint#9-32 — Hub→Compact additive 입구 준비

- 목표: 기존3포털/기본맵불변, 별도기본RunPortal로Compact후보연결. C++/가드변경0, 새6개외부Actor만저장계획.
- 준비: 전체10466일치, Source/Config·Hub/후보2512파일물리백업, 보호저장2루트백업, D두물리payload일치. D14384/54244생존·입력/종료0.
- 상태: normal-project read-only commandlet 준비, 신규콘텐츠변경/실제travel0. 게임창승인timeout/별도metadata승인차단보존.
- 문서: Feature/architect/2026-11-13_hub-compact-entry.md; Feature/doc/evidence/hub-compact-entry20261113A/physical-checkpoint.json.

### Sprint#9-32 — Source Hub→Compact 후보 입구 저장·정밀 재로드

- 구현: 기본native 포털1+표시Actor5, 위치(-1440,4270,180)cm, Compact target/StartRun=1. 기존3포털·기본맵·C++/가드불변.
- 실제검증: inspect03/author02/collision01/cold03 exit0·RED0. 신규4Mesh 영구NoCollision·재질, sign/font/색상/정렬,6package/DLayer·전체Actor값·desc691·Visibility capsulefree 확인.
- 발견/수정: 준비barrier·HitResult바인딩·첫저장descriptor전제 교정; cold에서발견한기본충돌상속과Color위치인수문제는이번소유5package만보정. 실패·신규6개보정전물리본보존.
- 최종보존: 이전10466입력변경0+신규6=10472, Source/Config368·Editor6·보호저장2루트·D두물리payload일치,2512백업검증. AST5/5·diff0.
- 한계: 실제여행/프로필/NPC/F/quit/restart/saveIO/렌더링미검증. D는이추가전버전. 게임창승인timeout·별도metadata승인차단/맵1800m유지; CUA/새Game/빌드/강제종료/commit/push0.
- 문서: Feature/doc/2026-11-13_hub-compact-entry.md; Feature/doc/evidence/hub-compact-entry20261113A/final-result.json.

### Sprint#9-32 — 개발 병행 출력 오류 분석과 세션 로그 검사

- 목표: 개발 중 실제 Unreal 세션의 빨간 Error를 즉시 분리·분석하고 재발을 검증한다.
- 변경: severity/구간/기존·신규/한정watch 로그검사기, 테스트, AGENTS 상시규칙·사용가이드, 다음실행용 영어smoke→한국어세션복원 실행기와 증거. 기존Source/Config/Content·현재편집세션 직접수정0.
- 원인/검증: 한국어 startup15줄=UnifiedError2+StructuredLogFormat1. 동일429개 ko3실패/en429성공. 최종복원probe429성공·Error0·ko/ko-KR·설정저장false·정상종료0. 검사기14pass/권한skip1, AST3+2/guard5pass. 실제live30초 기존52줄/신규0. 독립review의 quoting/중복세션guard 수정 후 재검토완료.
- 개발오류 관찰: Undo/Redo composite Auto Layer seam(378,693) 실패2회와 소유개발의 native refresh 후 보존PASS 확인. 전역reflection미지원 실패·과거로그는보존.
- 상태: 도구·우회책범위검증완료. 현재세션교체0/전체UI시각복원미검증/한국어엔진test직접실행결함잔존. 공유지형기능과누적WIP가검증중이므로 전체commit·push·승격대기;main완료아님.
- 관련: Feature/architect/2026-11-16_output-log-error-triage.md; Feature/doc/2026-11-16_output-log-error-triage.md; Docs/OutputLogErrorWorkflow.md; Feature/doc/evidence/output-log-errors-20261116/verification-summary.json.

### Sprint#9-32 — Landscape 외곽211개 정리·45개 저장 및 cold 보존 검증

- 목표: 1차 좌표 기획 제시 후, 승인받은 독립 Crop 후보를1,012.5×562.5m Landscape로 축소한다. 기존21지점·29양방향도로·중앙300m·3큰마을·5초소와 원본 데이터를 보존한다.
- 실제 변경: 정상 에디터 복구·정규Editor빌드 성공 후 native Landscape Manage/Delete로 외곽211개 제거, 지형/충돌45개 유지. 공개 native 재합성1회 후 terrain external actor16개만 저장했다. baseline5,370개 중16개만 변경, 보호3,947개불변·누락0. 사용자 VS·원본맵·StartHub·런타임코드 변경0.
- 보존 검증: 삭제직후·재합성후·새프로세스 cold 재로드 각각26/26pass, raw/composite height·paint·layer/GUID·비Landscape 액터 일치. Undo256복원/Redo45복귀는 즉시 AutoLayer composite seam실패이며 raw층/B/A는 불변. 최종복구를 Undo/Redo즉시보존 합격으로 바꾸지 않는다. global reflection 미지원 실패도 보존한다.
- 통행 QA:29도로 지면2,076+중앙선6,532점,13,006캡슐결과 중13,002실제sweep clear·4ground미확보미실행. E19/E27정확격자선2raymiss로 strict실패. 추가±0.001cm이상X offset40ray와구체6회는hit, 근본원인/repair인증아님. 직접표본최대11.8805°는전체삼각형12°인수와구분. 중앙보행3회는실행준비중이다.
- 상태: 실제 지형후보 저장·cold 데이터보존 완료. Undo/Redo즉시보존/strict도로QA/보행·성장플레이 인수가 남아 통합·커밋·푸시·승격 보류. 누적WIP는 보존한다.
- 문서: Feature/architect/2026-11-16_landscape-production-plan.md; Feature/doc/2026-11-16_landscape-production-result.md; Feature/doc/evidence/landscape-production-20261116/cold-reload/comparison.json; terrain-collision-check.json; ray-miss-diagnostic.json; implementation-review.md.

#### 11/16 최종 인수 상태 추가

- 정상 메뉴 UI에서 고립 QA 프로필1개 생성·실제 게임접속→Hub는 확인했다. 도구의 보호 SaveGame 필드접근2회·미노출 API1회 실패를 보존하고 공개 typed UObject 관측으로 교정했다. 실제 표시이름이 CropWalkQA 대신 로그라 정상프로필 게이트실패, 중앙보행0회·완료라운드0회다. 캐릭터 vendor skeleton의 엔진 Humanoid 의존성 부재도 별도 LoadErrors로 기록했다.
- PIE정상종료 후 최종26/26보존·dirty0, baseline5,370개 중지형16개만변경·보호3,947개불변을 재확인했다. 실제지형 상부·사선캡처를 저장했고 도로시각불연속은 미인수로남겼다. 출력은startup기존15 Error행+준비신규46 Error행(4tracebacks와LoadErrors1),2736줄이후최종검증/캡처신규0이다. 독립오류46건이라는뜻이아니다.
- 최종: 지형범위정리·저장·cold데이터보존 완료, 전체지형/플레이인수 실패·미완료. 브랜치통합/커밋/푸시/승격0. 근거: final-scope-report.json; after-reload/comparison.json; runtime-walk/runtime-state-inspection.json; landscape-crop-final.png.

- 출력로그 조사 전달 후 작은 진단 보완: VerifyLandscapeProduction20261116의 snapshot 오류에 snapshot_errors JSON을 함께 표시한다. 성공조건은 불변이며 보존된5단계 AST표현식 검사에서 Undo/Redo실패·좌표/채널출력과 복구/cold/최종조건 통과를 유지했다. 추가 재합성·export·에디터 호출·자산변경0, 전체 인수/통합 상태는 그대로다. 근거: snapshot-error-diagnostic-check.json.

- 후속 C++ 결과 연결: 별도소유 참조scanner 미지원0·complete/success true·보존9/cold7항목 및 프로필초기화native회귀 통과 자료를 확인했다. 지형 검증wrapper는 과거unsupported허용예외를 제거하고 success+loaded walk 완료를 필수로강화, 기존9보존/잘림/원시데이터비교유지. 기록기반6경우 AST검사와독립리뷰통과; 에디터/지형/빌드재실행0. 과거26pass·Source보호해시는당시체크포인트로보존하며새정책재실행/현재Source불변으로소급하지않는다. 실보행0·도로/Undo인수미완료·미승격은유지한다. 관련: Feature/doc/2026-11-16_reference-scanner-compatibility.md; scanner-policy-update-check.json.

### Sprint#9-32 — 전역 참조 검사 UE5.8 호환성과 초기 프로필 이름 수정

- 목표: lazy/FieldPath/Optional/동적Struct·PropertyBag의미지원오류해결,개발로그새Error원인수정.
- 변경: FReferenceScan 비로딩실제값순회·loaded GUID index·wrapper상속/순환지원·원래failclosed유지,Character PossessedBy에서selectedprofile적용,transient회귀6개와한정headless검증도구.
- 검증: 정규UHT/Editor0(123.06초/fixture교정30.59초),scanner5/5(Error0Warning0),profile1/1(errors0warnings13),실제후보90810객체/1093395값·unsupported0/nontruncated/loadedwalkcomplete/success·보존9조건/이전cold7조건통과. 최종로그3개Error0/후보watch30초신규0. GeneratedChanges통과.
- 리뷰/실패보존: inheritednativepayload P2보강·fixture실제검출확인,초기abstractUObjectensure55줄보존후구체fixture로교정(assert유지). 최종독립review잔여actionable0.
- 상태: 본수정범위구현·빌드·회귀·실제후보검증완료. native/custom/unloaded전체안전인증아님. 기존vendor legacyHumanoid자산호환성문제·지형전체통행인수는별도잔존. 기존대량WIP/미완료지형QA와섞는commit·push·main승격대기.
- 관련: Feature/architect/2026-11-16_reference-scanner-compatibility.md; Feature/doc/2026-11-16_reference-scanner-compatibility.md; Feature/doc/evidence/reference-scanner-20261116/verification-summary.json; Docs/OutputLogErrorWorkflow.md.


### Sprint#9-33 — 기능별 즉시 commit·push와 출력 로그 도구 독립 게시

- 목표: 완료·검증한 기능을 기능별로 바로 commit·push하고 미완료 작업과 분리한다. 사용자의 상시 지시를 AGENTS.md에 기록했다.
- 변경: 실제 로그 severity·구간·watch 검사기와 회귀 테스트, 영어 startup 후 한국어 세션 복원 실행기, 계획·결과·사용법·정상/실패 증거를 원격 main 기준 독립 물리 checkout에 선별했다. C++·Config·Content 변경은 포함하지 않는다.
- 검증: 이전 실제 startup429/429·Error0·ko/ko-KR 복원·정상 종료 증거를 보존한다. 이번에는 독립 Python 회귀·구문 검사와 PowerShell parser, generated/path·diff 검사·독립 리뷰를 다시 수행한다.
- 상태: 독립 범위 게시 검증 중. 검증 후 작업 브랜치 push와 develop→test→main 순서의 fast-forward를 진행한다. 지형 도로 저장/cold QA 및 다른 누적 기능의 완료를 의미하지 않는다.
- 관련: Feature/architect/2026-11-16_output-log-error-triage.md, Feature/doc/2026-11-16_output-log-error-triage.md, Docs/OutputLogErrorWorkflow.md.


### Sprint#9-33 출력 로그 도구 게시 완료

- 기능 커밋: 7d4d30489b20953d68cbc54a75859339fc7b4351. 기능별 즉시 commit·push 규칙과 독립 로그 도구 38파일을 게시했다.
- 검증: Python 14통과·Windows hardlink권한1skip, AST3·PowerShell parser2·generated/path·staged diff 통과, 독립 리뷰 잔여 문제0. 보관 startup429/429·Error0 증거를 유지했다.
- 원격: 작업 브랜치 push 후 develop→test→main fast-forward 및 일반 atomic push 성공. 직접 ls-remote로 네 브랜치의 위 SHA 일치를 확인했다. 완료 기록은 별도 문서 체크포인트로 동일 브랜치들에 반영한다.
- 보존·한계: 원래 Sprint#9-32 checkout의 누적 C++·Config·Content·지형/전투 WIP는 포함하거나 전환하지 않았다. 본 기능 완료는 지형 도로 저장/cold QA나 다른 기능의 완료를 뜻하지 않는다.
- 관련: Feature/doc/2026-11-16_output-log-error-triage.md, Feature/doc/evidence/output-log-errors-20261116/publication-20261117.json.

### Sprint#10-1 — 마법 명령 입력 버퍼

- 목표: 완료·검증 가능한 전투 기반 기능부터 각각 commit·push한다.
- 주요 변경: 독립 Q/E/R 명령 버퍼, 실제 헤더를 사용하는 C++ 회귀, standalone 실행기, paired 문서·두 실행 증거를 선별했다.
- 검증: 원래 Publish1117A와 게시용 Pub1117B 각각 clang++19.1.5·6사례·1,701검사·exit0. API/수명 검토 완료, UObject·UE·Config·Content 의존 없음.
- 상태: 버퍼 구현·회귀 완료, 게시 인계. Ctrl→첫Q 기한→QQQ 및 실제 시전 연결은 후속 범위다.
- 관련: Feature/architect/2026-11-17_magic-command-buffer.md, Feature/doc/2026-11-17_magic-command-buffer.md, Feature/doc/evidence/magic-command-buffer-20261117/.

### Sprint#9-32 — Landscape 도로·통행 최종 검증
독립 null-RHI Editor-Cmd가 저장된 후보를 재로드해47/47항목을 통과했다. 지형45개 배열·등록·semantic/composite/raw/paint/B/A와 기존 액터가 보존됐고,29개 데칼의 크기·방향·비충돌 편집 표시·folder/fade0 및 재질9노드·3scalar·입출력 연결이 일치했다. 파일 범위9/9검사에서 기존663개 도로 패키지 변경, 신규29개 데칼+정리 폴더1개와 신규 재질1개만 확인했다. 삭제0, 기존 후보맵과16개 지형 외부 액터 바이트는 불변이다. 보호 입력3,951개와 비QA 저장458개의 해시·저장 목록도 유지됐다.
중앙300m 기본 보행3회는48.7379/48.8019/48.5884초, 외곽8간선 약885.89m는144.2785초였다. 렌더링된 PIE에서 프로그램 이동 입력을 사용했으며 물리 키보드·성장/전투·20분 완주와 구분한다. 승인된 현재 에디터 변경 패키지 저장1회를 완료했고 최종 후보는 열린 상태다.
Headless 로그 Error0, loader complete와 제한시간 전 프로세스 부재를 확인했다. 실제 프로세스 ExitCode는 미관측으로 기록한다. 시작 전 신원 가드의 JSON 날짜 재파싱9시간 오차는 ISO 문자열 보존으로 교정하고 실제 생성시각과0.032초 차이를 읽기 확인한 뒤 검증을1회 실행했다. 이번 재개에 C++/Build.cs 변경은 없어 기존 정규 DLL을 사용했다. Python/PowerShell 구문·경로·diff 및 실제 에디터/재로드 검증을 수행했다.
독립 검토에서 신규 도로 차단 문제는 없었다. 과거 Undo/Redo 즉시 composite·정밀 point-ray 실패, 전체 성장/전투 인수는 남아 있다. 도로는 아직 게시되지 않은 Crop 지형 후보에 의존하므로 해당 기반의 통합 조건을 확인하기 전 기능 독립 게시 완료로 보고하지 않는다.
관련: Feature/architect/2026-11-17_landscape-traversal-resume.md; Feature/doc/2026-11-17_landscape-traversal-resume.md; Feature/doc/evidence/landscape-road-surface20261117/final-cold/comparison.json; Feature/doc/evidence/landscape-resume-20261117/final-scope-report.json.

통합 의존성 추가 확인: 현재 main/origin/main(fdbe8c8c)에는 검증한 Crop 후보1,453개 패키지와 신규 재질, 실제 맵의 Rogue10mVillageQuestDirector 관련 네이티브 클래스가 없다. 이번 도로 변경694개만 게시하면 기반 맵과 클래스가 빠지므로 독립 기능 게시가 불가능하다. 미완료 기반까지 포함하는 무관 WIP 통합은 하지 않았다. 근거: Feature/doc/evidence/landscape-road-surface20261117/integration-dependencies.json.


### Sprint#11-1 — Landscape 검사 도구 독립 검증

- 목표: 누적 변경에서 Landscape 검사 도구를 기능별로 게시한다.
- 변경: 전용 코드·시험·계획·결과·증거만 선별했다.
- 검증: main 기반 UHT/Editor 113.28초·exit0, transient5/5·failed0/notRun0, 실제 AbsLog Error/Fatal/ensure0, 독립 리뷰 잔여 문제0. Content/Config/Build.cs 변경0.
- 상태: 기능 범위 검증 완료·commit/push 및 develop→test→main 반영 대상.
- 한계: 미게시 지형 맵과 NPC는 포함하지 않으며, 기존 지형 Undo/Redo·point-ray 실패를 해결한 것으로 확대하지 않는다.
- 관련: Feature/doc/2026-11-17_reference-scanner-publication.md.

### Sprint#12-1 — 전리품 스택 계산 독립 검증

완료 범위는 독립 수량 계산 헤더·C++시험·실행기다. 기존 스택과 신규 스택/초과분의 정수 보존을 검증했으며 9사례·50,000무작위·2,103,255검사를 통과했다. 독립 리뷰·parser·해시 통과. 게임 자동 획득/재시도는 선행 런상태·loot 기능이 없어 별도로 남긴다. 관련: Feature/doc/2026-11-17_loot-stack-transfer.md. 상태: 계산 범위 완료·검증 후 즉시 게시.

### Sprint#13-1 — 선택 프로필 초기화

선택한 프로필 이름·GUID가 possession 이후 적용되도록 Character의 include1개와11행만 분리했다. fixture2개와 함께 main기준 정규UHT/Editor빌드44.35초·회귀1/1·Error/Fatal/ensure0을 확인했다. 빈 QA 저장 첫읽기와 무기 미설정 경고15개를 보존한다. 정확슬롯 메모리 저장격리로 사용자 세이브는 수정하지 않는다. 관련: Feature/doc/2026-11-17_profile-initialization-publication.md. 상태: 검증완료.

### Sprint#14-1 — 저장소 파일 구성 분석·남은 기능 게시 기록

- 분석: 고정6bfc78c6 main은11,963파일·7.57GB다. 당시 로컬 변경17,271=수정74+미추적17,197이며 사용자가 본17,269는2개 차이로 과거 스냅샷 없이 특정하지 않는다. 미추적2.73GB 중 Feature/DevLog의 원본검증 자료가 대부분이다.
- 조치: PDB4개442MB만 정확경로 ignore, 물리본·해시보존. .blend1·벤더·WP·PNG·QA 저장은 삭제하지 않았다. CSV는로컬,집계JSON·보고서만게시.
- 완료 게시: 마법명령버퍼88d0bef2(6사례1701),지형참조검사6bf1c026(Editor113.28초·5/5·Error0),전리품스택계산6bfc78c6(9사례/50000무작위/2103255),프로필초기화bb7a6bd6(Editor44.35초·1/1·Error0·Warning15). 각기능 작업/develop/test/main원격SHA일치확인.
- 남음: Crop맵1453패키지+재질은 NPC클래스14개와Tutorial/Monster/Widget변경·원래Compact/HLOD2개에의존. 도로cold47/47은현재로컬DLL기준이며전체main독립폐쇄아님. Undo/Redo즉시composite·E19/E27정밀ray실패보존. 전투표현은main에없는슬롯/입력/무기·자산선행필요. 런타임전리품도main에없는기존loot/run-terminal선행필요. 미완료를합격으로바꾸지않았다.
- 관련: Feature/doc/2026-11-17_repository-file-audit.md, Feature/doc/2026-11-17_remaining-features-publication.md.


### Sprint#9-32 — 맵 제작 WIP Git 체크포인트

사용자의 최신 지시에 따라 현재 코드·설정·도구, 디스크저장 맵·자산, 계획·결과문서를 묶음별 commit·push한다. main/develop/test 승격은 맵제작 완료 전 중단한다. 현재 에디터 PID7684의 자연경계·장식 작업은 미저장 상태이고 후보1453개는 이전 저장본과 SHA가 같다. 이번 Git 작업은 에디터 저장·재시작·빌드를 실행하지 않는다. 실제로그2906줄 경계의 기존Error10행을 보존하고 성공으로바꾸지 않는다. 코드구문검사/원격확인/generated/diff 검사와 기능인수를구분한다. 생성fixture·PDB·비QA저장·큰원본백업은선별에서제외하고 로컬보존한다. 정확목록은 Feature/doc/evidence/git-checkpoint-20261117의manifest로남긴다. 관련계획/결과: 2026-11-17_map-production-git-checkpoint.md.


### Sprint#9-32 — 저장 자산 체크포인트

코드·도구 d2779c76을 작업브랜치에 push했다. 다음 범위는 Content4537개와 직접 제작한 무기OBJ2개·MTL1개다. 고정목록4540파일/104,531,736바이트를 재해시해 선택 이후 변경0을 확인했다. 현재 자연경계 편집은 미저장으로, 이 커밋은 이전 디스크저장 맵·도로·자산을 보관한다. 에디터저장·바이너리직접편집0, main/develop/test변경0. 관련: content-manifest.csv, content-stability-check.json.


G2 동시작업 경계: stage 후 자연경계 소유 작업이 디스크709개를 갱신했다. Git index의4540개를 선택시점 manifest와 별도로 대조해 Content오차0, 무기원본3개는 LF정규화만 있음을 확인했다. 이 커밋은 일관된 이전 저장본이며 이후 저장분은 남은변경으로 보존하고 별도체크포인트로 나눈다.


### Sprint#9-32 — 자연 경계 숫자검증 저장 체크포인트

선행 저장본 f137e2a9의 push를 완료했다. 이후 소유 에디터의 명시741package 저장성공·dirty741→0을 확인했고, 새 Content731개와 보정스크립트1개를 별도로 고정했다. 구조검사49개·캡슐544개는숫자검증이며 visual_reviewed=false/acceptance_complete=false다. 별도검사에서732개파일이선택후불변이고새PythonAST가통과했다. 에디터/저장실행은이번Git작업에서하지않았다. main병합없이작업중체크포인트로게시한다.



### Sprint#9-32 — 작업 브랜치 게시 결과와 main 보류

- 코드·도구550파일은 d2779c76, 기존 저장맵·자산4540개와 기록은 f137e2a9, 이후 자연경계731개+보정스크립트1개 및 기록은65a0bf5a로 각각 commit·push했다.
- 자연경계 저장은 소유 에디터가 명시741package를 저장했고 dirty741→0이다. 구조49·capsule544는 숫자검증이며 visual/cold/전체인수 완료는 아니다. Git 작업자가 에디터를 저장하거나 재시작하지 않았다.
- 고정 index의Python257·PowerShell63 및 추가Python1 구문통과, generated경로없음. 코드diff의파일끝빈줄21개는 원본보존WIP한계로기록했다. 실제editor로그2906줄이후신규오류0·기존이력11행을확인했으며기존오류를삭제하지않았다. C++/PIE재실행은현재에디터보존을위해하지않았다.
- main/develop/test는024ecfb7 기준을유지한다. 맵제작이끝나기전통합하지않는사용자지시를AGENTS에기록했다. 이번마지막계획·결과·참조·개발일지와고정manifest는문서커밋으로같은작업브랜치에게시한다.
- raw캡처·큰백업·PDB·비QA저장은로컬보존하며이번선별게시에서제외한다. 후속에디터변경은새체크포인트의대상이며현재맵전체완료로해석하지않는다.
- 관련: Feature/doc/2026-11-17_map-production-git-checkpoint.md, Feature/doc/evidence/git-checkpoint-20261117/publication-summary.json.

### Sprint#9-32 — DevLog 대표 사진 정리

- 목표: 메인 사진만 남겨 반복 캡처를 줄인다.
- 변경: 1,618장→35장, 보조1,583장·약2.04GB삭제. 대표목록·보관규칙·55문서/57링크정리.
- 검증: 내부경로/파일/SHA선확인, 대표35해시동일, 비사진누락0, 삭제파일잔존0, 삭제사진활성링크0, 갤러리링크모두존재.
- 상태: 사진정리완료. 작업브랜치만게시하며main병합보류.
- 관련: DevLog/README.md, Feature/doc/2026-11-18_devlog-photo-curation.md, Feature/architect/2026-11-18_devlog-photo-curation.md.

### Sprint#9-32 — 평탄한 필드·자연 외곽·마을 갈색 바닥

- 목표: 설계한 플레이 구역을 평탄하게 유지하고 외곽 이탈을 막으며, 도로와 잔디의 전이를 완화하고 마을 안팎을 명확하게 구분한다.
- 변경: 지형45개에 원본 보존 additive 높이층, 둥근 폐곡선 능선·Pawn 차단13개, 도로29개 가장자리 전이, H/M/T 울타리·석벽과14m 열린 출입구13곳, 갈색 마을 바닥3개, 보스 입구 시각 문주와 선택 환경 소품. 마을별 재료·색·실루엣에 AAA 제작 사례의 지역 구분 원칙을 반영했다.
- 검증: 정규Editor빌드28.99초, 구조59/59·정적 캡슐544/544·별도cold60/60·파일범위32/32 통과. 최종6패키지 한정 저장·dirty0. 기존 중앙3회48.53~48.55초, 선택8간선142.91초, 새 남쪽 대표2회 점프 요청 시험 통과. 세 마을과 전체 실제 화면 확인.
- 보존: 원본 지형층·paint,45지형/충돌, 기존 소유7자산, 공유 재질5개, 비선택 저장458개. 백업1468+7개 예상 집합 오류와 콜백 재진입 실패는 수정 후 별도 재검증하고 원문을 유지했다.
- 상태: 요청한 지형·마을 시각 보완의 로컬 저장·검증 완료, 작업 브랜치 게시 대상. 맵 제작 중 main 통합 보류. 기존 벤더Humanoid 참조 오류, 실제 동굴·전체 게임플레이·최종 미술·패키징 인수는 별도다.
- 관련: Feature/architect/2026-11-17_natural-world-boundaries.md, Feature/doc/2026-11-17_natural-world-boundaries.md, Docs/References/WorldMap/landscape-natural-boundary20261117.json, DevLog/20261118.txt.


### Sprint#9-32 — DevLog 핵심 JSON 정리

- 목표: 불필요한 개발 검증 기록을 선별해 줄인다.
- 변경: JSON326개→55개, 중간·중복271개와 약8.02MB삭제. 핵심목록·보관규칙·문서 보관상태를 정리했다.
- 보존: 최종 판정·최근 회귀·주요 실패·고유출처, 로컬 전용 QA7개. JSON원본값과 대표사진35장·비JSON파일을 유지한다.
- 검증: 내부경로/파일종류/SHA 사전확인, 보관해시·JSON파싱·삭제부재·사진·비JSON존재·링크검사.
- 상태: 기록정리완료. 작업브랜치만게시하고 main병합은보류한다.
- 관련: DevLog/Records.md, Feature/doc/2026-11-18_devlog-record-curation.md.


2026-11-18 자료 정리: 위 실행 결과·수치·실패 설명은 당시 기록이다. 중간·보조 사진/JSON의 현재 보관 여부는 [검증 자료 보존 목록](../Feature/doc/evidence/README.md)을 따른다. 삭제 자료를 다른 성공 증거로 대체하지 않았다.


### Sprint#9-32 — Feature 검증 자료 선별 정리

- 목표: 최종 결과와 중요한 실패를 유지하면서 반복 사진·중간 JSON을 줄인다.
- 변경: 사진 621장·JSON 3,293개, 총 3,914개 / 810,622,927바이트 삭제. 자료 보존 목록·파일별 manifest·문서 링크·보관 규칙을 정리했다.
- 보존: 사진 289장·JSON 2,762개, 코드 입력 339개, 원본 텍스처·고유 실패 전후·대표 동작·활성 자연 경계/동굴 전체. 기존 JSON 형식 오류 두 건도 그대로 남겼다.
- 검증: 전체 삭제 경로·일반 파일·크기/SHA 사전 확인, 비활성 보존 해시·코드 입력·비대상 파일·JSON·Markdown 링크 검사, generated 경로 및 staged diff 검사.
- 상태: 자료 정리 완료. 게임 기능의 새 통과 판정은 아니며 작업 브랜치만 게시한다. main/develop/test 통합은 보류한다.
- 관련: Feature/architect/2026-11-18_evidence-curation.md, Feature/doc/2026-11-18_evidence-curation.md, Feature/doc/evidence/README.md.

### Sprint#9-32 — Q/Z 지하 동굴과 실제 삼인칭 검증

- 목표: 지하 훈련고와 작업·보관 공간을 실제 동굴로 만들고 갈림길·비밀방·회수 지름길을 넣는다. 중앙로·세 마을 평지와 기존 획득 규칙을 유지한다.
- 변경: 두 국소 굴착, 40m 하행로, 구조286개·바위/지지대/소품426개·등불12개를 네 액터로 구성했다. 발헤임 공식 동굴을 참고한 암반·목재·국소 조명과43노드 석재 투영을 적용했다. 원본 지형층·45개 지형/충돌·기존1404액터·도로29개와 원본 재질을 보존한다.
- 검증: 같은2016개 충돌 질의와31개 정적 조건 통과. 실제 카메라 실패 원인인 바위2개를 보정하고1440방향 질의 실패288→0을 확인했다. 새 정상 삼인칭은12개 경로·276개 지점·6화면·2442개 지면 접촉을801.741초에 완료했다. 카메라 최소176.871cm·최대당김154.377cm였다. 독립 재로드도32/32조건·2016질의·저장변경0·종료0·로그 오류0으로 통과했다. 첫 어두운 화면·카메라 충돌·메타데이터/언어 비교 실패는 보존한다.
- 보존: 파일 범위46/46조건 통과. 보호 입력3962개·비대상 세이브458개·물리 백업1481개·기존 소유8개·원본 재질 파일44개는 바이트 불변이다. 한정 저장5회는 고유33패키지 안에 머물렀다.
- 상태: 지하 동굴과 기본 삼인칭 검증 완료, 검증한 체크포인트를 작업 브랜치에 게시한다. 전체 맵 제작이 끝나기 전 main/develop/test 통합은 보류하며, 최종 미술·아이템 지급·전체 전투/내비게이션·Shipping 인수와 구분한다.
- 관련: Feature/architect/2026-11-18_underground-caves.md, Feature/doc/2026-11-18_underground-caves.md, Docs/References/WorldMap/caves20261118.json, DevLog/20261118.txt.

### Sprint#9-32 — Q/Z 연속 암벽·천장 보완과 기본 카메라 검증

- 목표: 사각 벽·평평한 천장·반복 부착 바위의 인상을 없애고, 중앙로·마을 평지와 기존 동굴 동선을 유지한다.
- 변경: 불규칙한 방 가장자리와 아치 천장이 이어지는 원본 메시 두 개, 프로젝트 소유 재질 두 개, 기존 충돌과 같은 경사로의 시각 복제 두 개를 저작했다. 기존 네 동굴 액터 안의 여덟 패키지만 한정 저장했고 Landscape와 바닥·경사로 충돌·목재·조명·카메라 설정은 보존했다.
- 검증: 같은 2,016충돌 질의·실제 RenderData 대조·1,440방향/2,880카메라 질의 통과. 정상 PIE 12동선·276지점·6화면·2,689지면 접촉을 883.369초에 완료했다. 최소 카메라 거리 231.904cm, 최대 당김 99.344cm, 지면 이탈 0이다. 독립 재로드 20조건·2,016질의·실제 종료 0·로그 오류 0, 파일 보존 24조건과 Python 33파일 구문·generated 경로 검사도 통과했다.
- 보존: 액터 1,408개, 기존 1,404액터·47재질·지형/충돌 45개와 179,488높이 표본, 1,497백업·3,969보호 입력·458세이브를 보존했다. 첫 형상 거절, 면 방향·비교기·캡처·재로드 실행 실패와 도구 수정 근거를 남겼다.
- 상태: 요청한 동굴 미술 보완·기존 동선·기본 삼인칭 인수 완료. 검증한 체크포인트를 작업 브랜치에 게시하며 전체 맵 제작 완료 전 develop/test/main 승격은 보류한다. 전체 전투·내비게이션·최종 제품 미술·Shipping 인수는 포함하지 않는다.
- 관련: Feature/architect/2026-11-18_organic-cave-refinement.md, Feature/doc/2026-11-18_organic-cave-refinement.md, Docs/References/WorldMap/cave-organic20261118.json, DevLog/20261118.txt.

### Sprint#9-32 — 시작 마을 포탈의 이전 진행 저장 호환

- 목표는 이전 캐릭터 저장을 사용해 시작 마을에서 오픈 월드로 진입할 수 있게 복구하는 것이다.
- 저장 버전 필드가 생략된 이전 네이티브 저장을 확인해 기존 이행 경로로 연결했다. 프로필 ID와 명시된 버전의 검증 및 슬롯 우선순위는 유지한다.
- 실제 저장 복사본 17개 조건에서 기존 포탈 통과, `OpenRunMap`의 1,200초 런 시작, 진행과 입장 재화 기록을 확인했다. 합성 일곱 케이스와 기존 저장 460개 해시 대조를 통과했다.
- 현재 에디터에 라이브 코딩으로 적용했다. 미저장 작업을 유지한 상태에서 Development 모듈의 정규 빌드는 남아 있다. 전체 맵 제작 완료 전에는 작업 브랜치에만 게시한다.
- 관련 문서는 Feature/architect/2026-11-18_start-hub-portal-save-compatibility.md, Feature/doc/2026-11-18_start-hub-portal-save-compatibility.md, DevLog/20261118.txt다.

### Sprint#9-32 — 시작 마을 면적 1/4 축소

- 목표는 시작 마을의 플레이 면적을 1/4로 줄이고 준비와 출발 동선을 유지하는 것이다.
- 지면을 120×120m에서 60×60m로, 경계 안쪽을 116.4×116.4m에서 58.2×58.2m로 줄였다. 주택 열 채와 우물, 아린, 준비 구역 네 곳을 유지하고 북쪽 포탈 네 개를 옮겼다.
- 저장 전과 독립 재로드에서 지면 125점, 외곽 캡슐 28건, 보행 캡슐 13구간을 검사했다. 실제 저장 복사본으로 기존 포탈을 걸어서 통과했고, `OpenRunMap`과 20분 타이머를 포함한 17개 검증 조건이 통과했다.
- `StartHub` 외부 액터 660패키지를 저장했다. 같은 맵의 다른 패키지 66개와 나머지 프로젝트 콘텐츠는 이번 변경 범위 밖에 있다. 검증한 맵과 문서를 현재 맵 제작 작업 브랜치에 게시한다.
- 관련 문서는 Feature/architect/2026-11-18_start-hub-quarter.md, Feature/doc/2026-11-18_start-hub-quarter.md, DevLog/20261118.txt다.

### Sprint#9-32 — 시작 마을과 오픈 월드 두 맵 운영

- 시작 마을과 실제 클리어 맵을 프로젝트의 두 레벨로 정했다. `StartHub`에서 메뉴와 캐릭터 선택을 열고, 선택 뒤 같은 맵을 플레이 모드로 읽는다. 오픈 월드 작업은 기존 `OpenRunMap`에서 진행한다.
- 폐기 맵 33개와 관련 패키지 4,529개를 정리했다. `StartHub`의 후보행 포탈 3개·장식 10개, FiveVillage 전용 런타임과 실행기도 정리했다. 최신 후보 지형·동굴과 로컬 변경은 복구본으로 보관했다.
- Asset Registry에 레벨 2개와 폐기 경로 참조 0개를 확인했다. 정규 빌드, 메뉴 36조건, 공용 의뢰 26개, 실제 보스 승리·귀환 77조건과 독립 저장 재로드 17조건이 통과했다.
- 검증한 두 맵 통합을 맵 제작 작업 브랜치에 게시한다. 상세 범위와 복구 경계는 Feature/doc/2026-11-18_playable-map-consolidation.md에 기록했다.

### Sprint#9-35 — GitHub 게시 전 레거시 코드와 JSON 정리

- 목표는 사용처가 확인되지 않는 이전 코드와 중복 기록을 정리해 게시 범위를 줄이는 것이다.
- 초기 맵 메타데이터와 미니맵 샘플, 호출 없는 전투·인벤토리 헬퍼, 폐기 FiveVillage 도구와 구형 HUD 도구를 정리했다. 중복·중간 JSON 5개를 모두 합쳐 4,204,023바이트 줄였다.
- Unreal 패키지 6,386개와 실행 참조를 점검했다. Python·PowerShell 구문 검사, 오프라인 테스트 85개, 보존 JSON 대조와 격리 UE 5.8 Development Editor 빌드 228개 작업을 통과했다.
- 검증한 레거시 정리와 관련 기록을 현재 작업 브랜치에 별도 커밋으로 게시한다. 관련 문서는 Feature/architect/2026-11-19_legacy-cleanup.md, Feature/doc/2026-11-19_legacy-cleanup.md, DevLog/20261119.txt다.

### Sprint#9-32 — DevLog 대표 사진 정리 후속

- 완성·검증된 주요 기능의 최종 화면을 기능당 한 장씩 남기는 기준을 기존 사진에 적용했다.
- DevLog PNG 35장 중 대표 6장을 남기고 중간·보조 장면과 소규모 개선 사진 29장을 제거했다.
- 남긴 사진의 SHA-256, 삭제 대상 잔존 여부, 현재 사진 목록의 링크 6개와 기존 글·검증 파일의 보존을 확인했다. 사진 정리를 완료했다.
- 관련 기록은 DevLog/README.md, DevLog/20261119.txt, Feature/doc/evidence/devlog-photo-curation-20261119/photo-manifest.csv에 있다.

### Sprint#9-35 — 이전 맵 검증 자료와 중복 복구 사본 정리

- 이전 맵의 중복 복구본과 최종 보고서로 대체된 중간 자료를 조사하고 미추적 파일 4,584개, 47,432,798바이트를 삭제했다.
- 파일별 경로·해시와 로컬 보관본 또는 이전 Git 원본을 대조했다. 복구에 필요한 고유 파일, 최종 검증 및 별도 실패 기록을 보관했다.
- 로컬 정리와 검증을 완료했다. 정리 기록과 삭제 목록을 현재 작업 브랜치에 게시한다. 관련 문서는 Feature/doc/2026-11-19_older-map-evidence-cleanup.md다.

### Sprint#11-2 — 무기별 스킬 창과 너클 수련 트리

- 목표는 장착 무기 탭을 먼저 열고 확정 시안의 너클 수련 트리를 구현하는 것이다.
- 공통·너클·검·마법 탭을 연결했다. 너클은 기초 무공 4개와 연결 15개, 검은 포인트 투자 경로, 마법은 준비 화면을 표시한다. 이전 자동 전수 저장을 지원한다.
- UE 5.8 Editor 빌드, StartHub 게임 검사 229개와 독립 저장 재로드 91개를 통과했다. 두 실행의 로그 오류는 0개다.
- 기능 구현과 실행 검증을 완료했다. [작업 계획](../Feature/architect/2026-11-20_knuckle-training-tree.md) · [검증 결과](../Feature/doc/2026-11-20_knuckle-training-tree.md)

### Sprint#12-2 — 대검 스킬트리 거미줄

- 목표는 실제 대검 기술과 대체 선행을 검 탭에서 쉽게 구분하는 것이다.
- 20개 노드의 메달리온과 이름표, 기술 5개의 그림, 선택한 기술에 연결된 경로 강조를 적용했다. 상세 카드에 대체 경로별 완료 여부, 공명 성질의 위치와 실제 기술 슬롯을 표시했다.
- UE 5.8 Editor 빌드, 자동화 3개, StartHub 게임 검사 242개, 저장 재로드 91개를 통과했다. 1080p와 720p를 확인했다.
- 구현과 검증을 완료했다. [작업 계획](../Feature/architect/2026-11-20_greatsword-skill-web.md) · [검증 결과](../Feature/doc/2026-11-20_greatsword-skill-web.md)

### Sprint#13-2 — 공통 이동 스킬트리

- 공통 탭에 기초 단련과 이동·도약·회피 능력 15개를 배치했다. 연결 21개를 양방향으로 따라가며 1P씩 투자한다.
- 이단 점프·구르기·이동 패시브를 캐릭터 이동에 연결했다. 기존 내공·투지·마나는 공통 탭의 아이덴티티 화면에서 사용한다.
- 최신 통합본의 UE 5.8 Editor 빌드, 자동화 4개, StartHub 282개, 별도 프로세스 저장 읽기 109개를 통과했다. 1080p와 720p에서 실제 스킬 창을 확인했다.
- 구현과 실행 검증을 완료했다. [작업 계획](../Feature/architect/2026-11-20_common-skill-web.md) · [검증 결과](../Feature/doc/2026-11-20_common-skill-web.md)

### Sprint#14-2 — 시작맵과 오픈 월드 사전 로딩

- 목표는 시작 화면과 필드에서 다음에 이동할 맵 패키지를 준비하는 것이다.
- StartHub에서 OpenRunMap을 낮은 우선순위로 비동기 로딩한다. 필드에서는 StartHub를 준비하고 기존 레벨 이동으로 사용한다.
- UE 5.8 Editor 빌드, Win64 Shipping 대상 소스 컴파일, 캐릭터 선택과 게임 입장 341개, 실제 포털 4회, 렌더링 정산·복귀와 새 프로세스 저장 복원을 확인했다. 두 목적지에서 엔진의 사전 로드 재사용을 확인했다.
- [작업 계획](../Feature/architect/2026-11-20_startup-map-preload.md) · [검증 결과](../Feature/doc/2026-11-20_startup-map-preload.md)

### Sprint#14-3 — 공통 스킬트리 화면 개선

- 목표는 승인 시안의 검은 바탕과 금속의 명암, 메달리온의 입체감을 실제 K 창에서 구현하는 것이다.
- 공통 탭에 메달리온 16종과 숯빛 질감, 이름표, 선택 탭과 습득 경로 강조를 적용했다. 왼쪽 상세 카드에 선택 아이콘을 크게 표시한다.
- UE 5.8 Editor 빌드, UI 텍스처 2종 임포트, 자동화 4개와 StartHub 실제 게임 입력 검사를 통과했다. 1080p와 720p에서 최종 화면을 확인했다.
- 구현과 검증을 완료했다. [작업 계획](../Feature/architect/2026-11-20_common-skill-tree-art.md) · [결과](../Feature/doc/2026-11-20_common-skill-tree-art.md)
