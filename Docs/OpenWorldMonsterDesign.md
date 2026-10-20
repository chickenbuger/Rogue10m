# 오픈월드 몬스터 기본 동작 설계

작성일: 2026-10-05. 대상 맵: `/Game/Rogue10m/Maps/OpenRunMap`.

사용자가 요청한 제작 범위는 간단한 NPC와 소유 몬스터의 Idle, 이동, 기본 공격이다. 기존 여섯 몬스터의 모델과 능력치를 유지하고 애니메이션을 연결한다. 시작마을 안내 NPC는 Idle을 재생하며 F 대화와 출발 보급품 수령을 제공한다. 구현 결과와 실제 실행 검증은 기능 결과 문서 및 에디터 증거에 기록한다.

## 기존 여섯 몬스터와 능력치

능력치 기준은 `Scripts/Editor/MonsterRosterDefinitions.py`의 공용 로스터다. 등급과 `MonsterId`를 유지해 기존 이름 표시, 경험치와 토벌 카운터가 이어지게 한다. 이번 작업은 난이도 재조정 작업이 아니므로 HP, 기본 피해, 경험치를 변경하지 않는다.

| 지역 | 이름 · MonsterId | 등급 | 레벨 | HP | 기본 피해 | EXP | 이동 속도 cm/s |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 서쪽 숲 | 초록 점액체 · `VerdantSlime` | 일반 | 1 | 70 | 6 | 18 | 220 |
| 서쪽 숲 | 어린 포자 버섯 · `Sporeling` | 일반 | 5 | 150 | 10 | 35 | 230 |
| 동쪽 풍차 | 붉은 점액체 · `CrimsonSlime` | 일반 | 2 | 90 | 7 | 22 | 235 |
| 바위 고원 | 철갑 등껍질 · `IronShell` | 일반 | 11 | 330 | 15 | 66 | 170 |
| 바위 고원 | 가시 성채의 수문장 · `ThornWarden` | 중간 보스 | 12 | 1,800 | 30 | 450 | 250 |
| 북쪽 유적 | 심연의 군주 아자토르 · `AbyssOverlordAzathor` | 최종 보스 | 25 | 10,000 | 70 | 3,000 | 240 |

보스 등급의 두 종도 이번 연결에서는 Idle, 이동, 기본 공격을 사용한다. 기본 피해는 데이터 필드 값이며 실제 적중 피해는 기존 전투 처리와 연결한 프로필을 기준으로 확인한다.

## 소유 메시와 애니메이션

메시 경로의 공통 시작 부분은 `/Game/MonsterForSurvivalGame/Mesh/`, 애니메이션 경로는 `/Game/MonsterForSurvivalGame/Animation/`이다. 아래 파일은 프로젝트에 존재하는 소유 에셋이다. 실제 연결 전에 에디터에서 메시와 시퀀스의 Skeleton 일치를 확인한다.

| 몬스터 | Skeletal Mesh | 애니메이션 폴더 | Idle | 이동 | 기본 공격 |
| --- | --- | --- | --- | --- | --- |
| 초록 점액체 | `Polyart/Slime_SK` | `Polyart/Slime` | `Slime_IdleNormal_ANIM` | `Slime_Walk_ANIM` | `Slime_Attack01_ANIM` |
| 어린 포자 버섯 | `Polyart/Mushroom_SK` | `Polyart/Mushroom/SmileVersion` | `Mushroom_IdleNormalSmile_ANIM` | `Mushroom_walkFWDSmile_ANIM` | `Mushroom_Attack01Smile_ANIM` |
| 붉은 점액체 | `PBR/Slime_SK` | `PBR/Slime` | `Slime_IdleNormal_ANIM` | `Slime_Walk_ANIM` | `Slime_Attack01_ANIM` |
| 철갑 등껍질 | `PBR/TurtleShell_SK` | `PBR/TurtleShell` | `TurtleShell_IdleNormal_ANIM` | `TurtleShell_Walk_ANIM` | `TurtleShell_Attack01_ANIM` |
| 가시 수문장 | `PBR/Cactus_SK` | `PBR/Cactus` | `Cactus_IdleNormal_ANIM` | `Cactus_WalkFWD_ANIM` | `Cactus_Attack01_ANIM` |
| 아자토르 | `PBR/Beholder_SK` | `PBR/Beholder` | `Beholder_IdleNormal_ANIM` | `Beholder_WalkFWD_ANIM` | `Beholder_Attack01_ANIM` |

원본 메시, 재질, 애니메이션과 25종 공용 로스터를 보존한다. 여섯 `BP_OW_*`에 연결할 프로필에는 위 Idle·이동·공격 시퀀스와 기본 공격의 범위·타이밍을 지정한다. 새 외형이나 리깅 제작 대신 기존 메시의 스켈레톤에 맞는 시퀀스를 사용한다.

## 기본 동작

몬스터의 이동과 대상 탐색은 기존 AI Controller와 Behavior Tree가 담당한다. 대기 중에는 Idle, 실제 속도가 발생하면 이동 애니메이션을 반복 재생한다. 공격 거리 안에 살아 있는 플레이어가 들어오면 이동을 멈추고 기본 공격 시퀀스를 한 번 재생한다. 짧은 준비 시간 뒤 고정된 정면 범위에서 한 번 적중을 판정하고, 회복 시간이 끝나면 Idle 또는 이동으로 돌아간다.

공격은 서버 권한에서 실행한다. 적중 시 대상 생존 여부, 현재 거리·높이·정면 범위와 가림 여부를 확인한다. 범위를 벗어났거나 벽 뒤에 있는 대상에게 피해를 주지 않는다. 공격 중 대상 소실, 몬스터 사망 또는 스트리밍 종료가 발생하면 공격 타이머를 정리한다. 전용 프로필을 연결하지 않은 기존 몬스터는 현재 공격 처리를 유지한다.

게임플레이 코드는 C++에 두고 Blueprint와 Data Asset은 모델·시퀀스·수치를 지정한다. 새 동작에는 매 프레임 Tick을 추가하지 않고 이벤트와 타이머를 사용한다. 기존 사망, 경험치 지급과 토벌 기록을 유지하며 같은 처치의 보상이 중복 지급되지 않는지 확인한다.

## NPC와 맵 배치

시작마을 안내인 아린은 소유 Quinn/Manny 계열 메시와 호환되는 Idle 시퀀스로 구성한다. 가까이 다가가 F를 누르면 한국어 안내가 열리고, 다음 F 입력으로 출발용 회복 물약을 받는다. 멀어지면 대화가 닫힌다. 보급 수령 기록은 현재 게임 세션과 PlayerState에 두어 중복 수령을 막고, 가방에 넣지 못한 경우에는 다시 시도할 수 있게 한다. NPC가 지급한 스택의 GUID와 남은 수량을 포탈 이동 직전에 확인해 목적지 인벤토리에 운반한다. 이미 사용하거나 버린 보급품은 다시 생성하지 않는다. NPC의 메시·Idle·대화 문구·보급 아이템·수량은 Data Asset에서 지정한다. 실제 저장된 에셋과 배치 위치는 제작 결과에 기록한다.

오픈월드의 기존 지역 배치를 유지한다. 제작 스크립트의 배치는 숲 점액체 3·버섯 3, 풍차 붉은 점액체 4, 고원 철갑수 4·수문장 1, 북쪽 유적 아자토르 1이다. 지역 스포너의 현재 개체 수 유지 설정과 기존 한 마리 보스 배치를 보존하며, 이번 동작 연결에 맞춰 실제 저장된 액터를 확인한다. 시작마을과 야영지 안전 구역, 지형·길·교량·포탈을 유지한다.

## 검증과 완료 기준

`Scripts/Editor/InspectOpenWorldMonsters.py`에서 여섯 Blueprint의 데이터 참조, 공용 능력치, 실제 메시·시퀀스·Skeleton과 지역 스포너를 읽는다. 에셋 파일의 존재 확인과 런타임 재생 확인을 구분한다.

1. 여섯 몬스터의 기존 HP·기본 피해·EXP·MonsterId·등급이 저장·재로드 후 유지된다.
2. 각 메시에서 Idle·이동·공격 시퀀스가 실제 재생되며 호환성 오류나 바인드 포즈 고정이 없다.
3. 기본 공격이 유효 대상에게 한 번 적중하고, 범위 밖·가림·죽은 대상에게 피해를 주지 않는다.
4. 공격 종료·취소 뒤 이동 또는 Idle로 복귀하며 사망·스트리밍 종료 뒤 남은 공격이 실행되지 않는다.
5. NPC의 Idle, 가까운 거리의 F 대화, 보급품 수령·중복 방지·가방 부족 시 재시도와 멀어진 뒤 대화 종료가 동작한다. 포탈을 통해 받은 보급품의 남은 수량만 한 번 운반되고, 이미 사용하거나 버린 수량은 복원되지 않는다. NPC는 기존 플레이어 통로를 막지 않는다.
6. 기존 지형, 포탈과 다른 몬스터의 공용 에셋이 보존된다.

자동 검증의 결과와 실제 화면을 함께 남긴다. 전체 20분 플레이나 출시 성능 검증을 수행했다고 표현하지 않는다.
