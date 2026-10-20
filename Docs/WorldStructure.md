# Rogue10m World Structure

## Overview

Rogue10m uses two main map types:

- Start Map: NPC hub before a run.
- Play Map: first-person open-world run space.

## Start Map

The start map is a hub with NPC-driven systems.

### NPC Roles

- Item NPC
  - Lets the player receive, manage, or prepare items before a run.
  - Future link: inventory, starting item choice, reward claim.

- Trait NPC
  - Lets the player spend previously earned points on simple trait upgrades.
  - Future link: permanent progression, account-level or character-level traits.

- Difficulty NPC
  - Lets the player adjust run difficulty.
  - Future link: enemy scaling, boss modifiers, reward multiplier.

- Story NPC
  - Explains the starting story and run premise.
  - Future link: tutorial, quest intro, world lore.

## Play Map

The play map is an appropriately sized first-person open-world combat area.

### Size Target

The current design target is that walking from one edge of the map to the opposite edge takes about 5 minutes.

Unreal default character movement often uses about `600 cm/s` walking speed.
At that speed:

- 5 minutes = 300 seconds.
- 600 cm/s * 300 seconds = 180,000 cm.
- 180,000 cm = 1,800 meters.

Initial play map target:

- Edge-to-edge traversal: about 1.8 km at 600 cm/s.
- Center-to-edge traversal: about 900 m if the player starts near the center.

This should be tuned after the final player movement speed is decided.

## Run Rule

- The player must defeat the boss within 20 minutes.
- If the boss is not defeated within 20 minutes, the run fails.
- The current saved GameMode disables the prototype override and uses 1,200 seconds when entering through the StartHub run portal.
- The timeout rule is implemented. Final-boss-specific victory completion remains a separate gameplay task; placing the existing boss actor does not add that rule.

## Combat Starting State

- The player starts unarmed.
- The player can attack with fists in the default state.
- Weapon mastery progresses by weapon type.
- Planned weapon types:
  - Dagger
  - Great Sword
  - Dual Blades
  - Bow
  - Staff
  - Knuckle

## Customization

- Character customization is cosmetic only.
- Customization should not affect stats, mastery, damage, or difficulty.

## Open Decisions

- Exact start map layout and NPC placement.
- Whether NPCs use dialogue widgets, proximity prompts, or direct interaction menus.
- Play-map entry is the southern safe camp in the saved OpenRunMap.
- Exact player walk/sprint speed.
- The current boss location is the northern ruin plateau; dedicated encounter rules remain open.

## 포탈 및 플레이맵 이동 규칙

- 시작맵에서는 20분 런 타이머를 자동으로 시작하지 않는다.
- 시작맵에 `ARogue10mRunPortal` 또는 그 Blueprint 자식을 배치한다.
- 포탈의 `Target Level Name`은 기본적으로 `OpenRunMap`을 사용한다.
- 포탈의 `Start Run Timer On Travel`을 켜면 다음 맵 이동 옵션에 `StartRun=1`이 붙는다.
- 다음 맵의 GameMode는 `StartRun=1` 옵션을 확인한 뒤 20분 런 타이머를 시작한다.

## 다음 맵 데이터 추가 위치

- 실제 플레이맵 에셋 권장 위치: `Content/Rogue10m/Maps/OpenRunMap.umap`
- 시작맵 에셋 권장 위치: `Content/Rogue10m/Maps/StartHub.umap`
- 맵 메타데이터 추가 위치: `Source/Rogue10m/World/Rogue10mMapDataLibrary.cpp`
- 포탈 C++ 클래스 위치: `Source/Rogue10m/World/Rogue10mRunPortal.*`
- 에디터에서 새 플레이맵을 만들면 `Rogue10mMapDataLibrary.cpp`의 `LevelName`, `LevelAssetPath`, `TargetWorldSizeMeters` 값을 함께 갱신한다.

## 현재 저장된 오픈월드 플레이맵 (2026-10-05)

- 실제 맵: `/Game/Rogue10m/Maps/OpenRunMap`. 기존 기본맵의 환경·시스템을 복제하고 시험용 기하를 새 맵에서 정리했다. MapDataLibrary의 기존 경로·1800m 계약과 일치한다.
- 크기: 1800m × 1800m Landscape, 높이 약21~140m, 1009² 높이 샘플, 256컴포넌트·16스트리밍 프록시.
- 남쪽 야영지 `(0,-65000)`cm에서 서쪽 숲 `(-50000,-25000)`, 강 교차로 `(0,5000)`, 동쪽 풍차 `(50000,-5000)`, 바위 고원 `(-35000,42000)`, 북쪽 유적 `(15000,68000)`으로 연결된다.
- 폭60m 강과 폭18m·길이120m 교량3개, 약12m 폭 길을 배치했다. 북쪽 유적은 처음부터 접근 가능하며 직행길 약1357m를 걷는 데 기본600cm/s 기준 약226초가 걸린다.
- 강은 원본 물 재질을 부모로 하는 맵 전용 `MI_OpenWorldRiver`를 사용한다. 거품의 흰색 포화를 보정했으며 수면은 시각 표현이다. 새 수영 시스템은 없다.
- Epic PCG 단일 식생 그래프를183개 적법 영역에 적용했다. 보유 나무3종2,806개는 인스턴스 컴포넌트로 저장하고 길·강·공터를 비웠다. 에셋·밀도·크기·위치를 그래프 파라미터로 조절할 수 있다.
- 월드 전용 동적 내비게이션은 플레이어 주변60m에서 생성하고80m 밖에서 제거한다. 월드 파티션은256m 셀·700m 로드 범위다.
- 지역 조우는 기존 몬스터 Data Asset을 참조하는 전용 Blueprint 자식으로 구성했다. 숲·풍차·고원 스포너5개와 고정 보스1개다. Capsule 중심을 고려한 지면 스폰 보정과 소유 스트리밍 레벨을 적용했다.
- 시작마을의 기존 포탈만 새 맵으로 연결했다. 메뉴→StartHub 대기→포탈 overlap→OpenRunMap 20분 런 흐름이다. 맵을 직접 PIE로 열면 현재 GameMode는 포탈 옵션 없이 런을 자동 시작하지 않는다.
- 새 NPC 서비스·집 실내·보스 전용 패턴·승리 연결·패키징 성능은 후속 범위다. 자세한 실제 검사와 한계는 `Feature/doc/2026-10-05_open-world-playmap.md`에 기록한다.
