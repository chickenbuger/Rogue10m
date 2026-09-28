---
generated: true
generated_by: rogue10m-function-graph
node_kind: function
---
# 몬스터·AI

몬스터 본체, AI 판단, 몬스터 설정과 생성

> 기능 → 소스 연결은 Feature 본문에서 확인한 소스 언급입니다. 소스 → 소스 연결은 정적 include 의존성입니다.
> 설계/과거 결과/제거 계획의 언급도 포함될 수 있으며, 현재 호출 관계나 구현 완료를 보증하지 않습니다.

문서 주제 분류: Scripts/ObsidianFunctionGroups.json · 전체 행별 근거: functions.json

## 관련 소스

- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mAttackTargetInterface.cpp|Components/Rogue10mAttackTargetInterface.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:9` — `Rogue10mAttackTargetInterface`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:24` — `Rogue10mAttackTargetInterface`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:14` — `Rogue10mAttackTargetInterface.*`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mAttackTargetInterface.h|Components/Rogue10mAttackTargetInterface.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:9` — `Rogue10mAttackTargetInterface`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:24` — `Rogue10mAttackTargetInterface`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:14` — `Rogue10mAttackTargetInterface.*`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mCombatComponent.cpp|Components/Rogue10mCombatComponent.cpp]]
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:22` — `Rogue10mCombatComponent.*`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:25` — `ActiveAttackExecution`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mCombatComponent.h|Components/Rogue10mCombatComponent.h]]
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:22` — `Rogue10mCombatComponent.*`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:25` — `ActiveAttackExecution`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mVitalRegenerationComponent.cpp|Components/Rogue10mVitalRegenerationComponent.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:6` — `URogue10mVitalRegenerationComponent`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Components/Rogue10mVitalRegenerationComponent.h|Components/Rogue10mVitalRegenerationComponent.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:6` — `URogue10mVitalRegenerationComponent`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerController.cpp|Core/Rogue10mPlayerController.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:23` — `PlayerController`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:28` — `PlayerController`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:30` — `Rogue10mPlayerController.*`
  - 추가 근거 6건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerController.h|Core/Rogue10mPlayerController.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:23` — `PlayerController`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:28` — `PlayerController`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:30` — `Rogue10mPlayerController.*`
  - 추가 근거 6건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerState.cpp|Core/Rogue10mPlayerState.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:8` — `PlayerState`
  - 근거 (설계): `Feature/architect/2026-07-25_monster-experience-roster.md:59` — `PlayerState`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Core/Rogue10mPlayerState.h|Core/Rogue10mPlayerState.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:8` — `PlayerState`
  - 근거 (설계): `Feature/architect/2026-07-25_monster-experience-roster.md:59` — `PlayerState`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Data/Rogue10mAttackSkillData.cpp|Data/Rogue10mAttackSkillData.cpp]]
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:25` — `AttackSkillData`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Data/Rogue10mAttackSkillData.h|Data/Rogue10mAttackSkillData.h]]
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:22` — `Rogue10mAttackSkillData.h`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:25` — `AttackSkillData`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Data/Rogue10mMonsterDataAsset.cpp|Data/Rogue10mMonsterDataAsset.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:13` — `URogue10mMonsterDataAsset`
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:5` — `URogue10mMonsterDataAsset`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:52` — `URogue10mMonsterDataAsset`
  - 추가 근거 1건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/Data/Rogue10mMonsterDataAsset.h|Data/Rogue10mMonsterDataAsset.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:13` — `URogue10mMonsterDataAsset`
  - 근거 (결과 문서): `Feature/doc/2026-07-13_monster-data-regen-and-hud-cleanup.md:5` — `URogue10mMonsterDataAsset`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:52` — `URogue10mMonsterDataAsset`
  - 추가 근거 1건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/AI/BTTask_Rogue10mMonsterDecision.cpp|Enemy/AI/BTTask_Rogue10mMonsterDecision.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:10` — `UBTTask_Rogue10mMonsterDecision`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:46` — `UBTTask_Rogue10mMonsterDecision`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/AI/BTTask_Rogue10mMonsterDecision.h|Enemy/AI/BTTask_Rogue10mMonsterDecision.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:10` — `UBTTask_Rogue10mMonsterDecision`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:46` — `UBTTask_Rogue10mMonsterDecision`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/AI/Rogue10mMonsterAIController.cpp|Enemy/AI/Rogue10mMonsterAIController.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:9` — `ARogue10mMonsterAIController`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:37` — `Rogue10mMonsterAIController`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:40` — `ARogue10mMonsterAIController`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/AI/Rogue10mMonsterAIController.h|Enemy/AI/Rogue10mMonsterAIController.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:9` — `ARogue10mMonsterAIController`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:37` — `Rogue10mMonsterAIController`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:40` — `ARogue10mMonsterAIController`
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/Rogue10mBasicMonster.cpp|Enemy/Rogue10mBasicMonster.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:5` — `ARogue10mBasicMonster`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:14` — `ARogue10mBasicMonster`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:10` — `ARogue10mBasicMonster`
  - 추가 근거 10건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/Enemy/Rogue10mBasicMonster.h|Enemy/Rogue10mBasicMonster.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:5` — `ARogue10mBasicMonster`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-behavior-tree-ai.md:14` — `ARogue10mBasicMonster`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:10` — `ARogue10mBasicMonster`
  - 추가 근거 10건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Rogue10mCharacterCustomizationPreviewActor.cpp|UI/Rogue10mCharacterCustomizationPreviewActor.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:49` — `Rogue10mCharacterCustomizationPreviewActor.cpp`
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Rogue10mEquipmentPreviewActor.cpp|UI/Rogue10mEquipmentPreviewActor.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:49` — `Rogue10mEquipmentPreviewActor.cpp`
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mCharacterLobbyWidget.cpp|UI/Widgets/Rogue10mCharacterLobbyWidget.cpp]]
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:70` — `CharacterLobbyWidget`
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mCharacterLobbyWidget.h|UI/Widgets/Rogue10mCharacterLobbyWidget.h]]
  - 근거 (설계): `Feature/architect/2026-07-28_monster-behavior-tree-ai.md:70` — `CharacterLobbyWidget`
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mDamageIndicatorWidget.cpp|UI/Widgets/Rogue10mDamageIndicatorWidget.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:26` — `URogue10mDamageIndicatorWidget`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:7` — `URogue10mDamageIndicatorWidget`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:34` — `DamageIndicatorWidget`
  - 추가 근거 3건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mDamageIndicatorWidget.h|UI/Widgets/Rogue10mDamageIndicatorWidget.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:26` — `URogue10mDamageIndicatorWidget`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:7` — `URogue10mDamageIndicatorWidget`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_monster-damage-indicator.md:34` — `DamageIndicatorWidget`
  - 추가 근거 3건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mHudWidgetParts.cpp|UI/Widgets/Rogue10mHudWidgetParts.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-08-12_gothic-monster-info-frame.md:5` — `MonsterInfoWidget`
- [[Docs/Obsidian/ArchitectureMetrics/Files/UI/Widgets/Rogue10mHudWidgetParts.h|UI/Widgets/Rogue10mHudWidgetParts.h]]
  - 근거 (결과 문서): `Feature/doc/2026-08-12_gothic-monster-info-frame.md:5` — `MonsterInfoWidget`
- [[Docs/Obsidian/ArchitectureMetrics/Files/World/Rogue10mBreakableActor.cpp|World/Rogue10mBreakableActor.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:10` — `ARogue10mBreakableActor`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:25` — `ARogue10mBreakableActor`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:14` — `Rogue10mBreakableActor.*`
  - 추가 근거 2건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/World/Rogue10mBreakableActor.h|World/Rogue10mBreakableActor.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:10` — `ARogue10mBreakableActor`
  - 근거 (결과 문서): `Feature/doc/2026-07-11_multihit-damageable-targets.md:25` — `ARogue10mBreakableActor`
  - 근거 (설계): `Feature/architect/2026-07-11_multihit-damageable-targets.md:14` — `Rogue10mBreakableActor.*`
  - 추가 근거 2건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/World/Rogue10mMonsterSpawner.cpp|World/Rogue10mMonsterSpawner.cpp]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:5` — `ARogue10mMonsterSpawner`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:23` — `Rogue10mMonsterSpawner`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:43` — `Rogue10mMonsterSpawner.cpp`
  - 추가 근거 2건은 functions.json에 보관.
- [[Docs/Obsidian/ArchitectureMetrics/Files/World/Rogue10mMonsterSpawner.h|World/Rogue10mMonsterSpawner.h]]
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:5` — `ARogue10mMonsterSpawner`
  - 근거 (결과 문서): `Feature/doc/2026-07-28_monster-area-spawner.md:23` — `Rogue10mMonsterSpawner`
  - 근거 (설계): `Feature/architect/2026-07-28_monster-area-spawner.md:5` — `ARogue10mMonsterSpawner`
  - 추가 근거 1건은 functions.json에 보관.
