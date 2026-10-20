# Rogue10m Codex Harness

This file defines the working rules for Codex in this Unreal Engine 5.8 project.
It adapts the harness engineering ideas from `BaeJongHo/Unreal_Harness` to this repository.

## Project Facts

- Project name: `Rogue10m`
- Unreal version: UE 5.8
- Engine root: `D:\Program Files\UE_5.8`
- Project root: `D:\Project\Rogue10m`
- Project file: `D:\Project\Rogue10m\Rogue10m.uproject`
- Runtime module: `Source/Rogue10m`
- Module API macro: `ROGUE10M_API`
- Primary branch flow: `Sprint#<sprint>-<work>-<short-description>` -> `develop` -> `test` -> `main`
- Sprint numbering rule: a Sprint closes and the Sprint number increments only after a successful merge into `main`. Work branches created before that merge keep the same Sprint number and increment only `<work>`; the first work after the merge starts at `<next-sprint>-1`.

## Harness Pipeline

Use this pipeline for meaningful gameplay, tool, UI, AI, content, level, or balance work:

The high-level game development pipeline is:

1. Pre-production: define the idea, scope, constraints, and design plan.
2. Production: implement features and run in-development testing.
3. Testing: run QA-style verification on integrated work.
4. Distribution: prepare final builds and release candidates.
5. Post-Production: track DLC, patches, and live fixes.

Codex feature work maps onto that production flow:

1. Architect: inspect code and write a plan in `Feature/architect/` (Pre-production).
2. Implementer: make scoped code/config/doc changes (Production).
3. Builder: run the relevant build command and fix local compile issues only (In-development testing).
4. Reviewer: review the diff for Unreal-specific risks (Testing / QA).
5. Doc Writer: write the feature result in `Feature/doc/` and update `DevLog/YYYYMMDD.txt`.

Small typo fixes and tiny mechanical edits can skip the full pipeline, but still follow the Unreal rules below.

## Lazy Codex Overlay

Use the Lazy Codex overlay in `Docs/LazyCodexHarness.md` for large, vague, or multi-system work.
In this repository, Lazy Codex means a local harness behavior, not a required external tool install.

- Break large requests into Ultrawork Packets before implementation.
- Give each packet a goal, touched area, completion condition, validation command, and rollback boundary.
- Run the ULW loop: Scope Gate -> Breakdown -> Architect -> Implementer -> Builder -> Reviewer -> Doc Writer -> Exit Gate.
- Route work by role instead of blending design, implementation, build fixes, review, and documentation in one pass.
- If the same failure repeats twice, stop repeating the same implementation and return to Architect or Reviewer mode.
- Use cost guards: inspect narrowly first, build only when relevant, and avoid binary asset churn.

## Required Per-Command Checklist

Check this list on every user command before finishing:

- Confirm whether the request changes code, config, docs, branch state, build settings, assets, or project structure.
- If anything changed, update the daily development log in `DevLog/YYYYMMDD.txt`.
- If the daily log file does not exist, create `DevLog/` and the date file first.
- Append to the existing daily file instead of replacing prior entries.
- Write DevLog entries in a Tistory-friendly format.
- When a feature is developed, add a clearly marked Notion summary candidate inside the DevLog.
- When a Sprint work item is completed or materially changed, update Docs/SprintChangeLog.md with its goal, major changes, validation, status, and related documents.
- Before any GitHub commit or push summary, list the changed files and a short explanation.
- Standing user authorization (2026-10-20): commit and push the current checkpoint, and after each future feature is completed and validated, commit it and promote it through `develop` -> `test` -> `main`, then push the work and integration branches. This instruction is explicit confirmation; do not ask again for routine feature completion.
- Public publishing authorization (2026-10-20): the user explicitly approved publishing the current code, Unreal assets, source artwork, and validation evidence to the public GitHub repository `https://github.com/chickenbuger/Rogue10m`, and publishing future completed and validated features to that same public repository. This resolves the earlier public-destination approval block; do not request the same approval again for routine completion.
- Keep feature development on a work branch. Before promotion, fetch and inspect remote changes, run relevant checks, review the diff, and update the feature documents, Sprint changelog, and daily DevLog. Prefer fast-forward promotion when possible; never force-push or discard unrelated work under this authorization. If validation fails or integration is blocked, record the reason instead of claiming completion.
- Write every new DevLog entry in Korean.
- Always switch to an appropriate work branch before feature development.
- For large or unclear work, apply `Docs/LazyCodexHarness.md` and record the selected Ultrawork Packets.

## Unreal Engineering Rules

- Gameplay logic belongs in C++; Blueprint children should configure data and assets.
- Expose tuning data with `UPROPERTY(EditAnywhere/EditDefaultsOnly, BlueprintReadOnly, Category="...")`.
- All UObject member references that must be GC-tracked use `UPROPERTY()` and `TObjectPtr<T>`.
- Use `TWeakObjectPtr<T>` for non-owning cached references.
- Prefer Enhanced Input, Subsystems, StateTree, Niagara, MetaSounds, Gameplay Tags, and Data Assets when appropriate.
- Prefer `PlayerState` for persistent player-facing character state, and split growing feature logic into dedicated `ActorComponent` classes.
- Prefer `Data Asset` definitions for future gameplay data such as weapons, skills, items, monsters, balance values, and NPC setup.
- For Unreal-specific guidance, prefer the Epic community assistant URL: `https://dev.epicgames.com/community/assistant/unreal-engine/conversation`.
- Keep Tick disabled by default. Prefer events, delegates, timers, and cached references.
- Do not call `GetWorld()`, player lookups, or subsystem access from constructors.
- Dynamic delegate callbacks must be declared with `UFUNCTION()`.
- Use IWYU includes. Header `*.generated.h` includes must be last.
- Use `UE_LOG(LogRogue10m, ...)`; avoid `LogTemp` for project code.
- Never edit generated or cached outputs: `Intermediate/`, `Binaries/`, `Saved/`, `DerivedDataCache/`, `*.generated.h`, `*.gen.cpp`.
- Do not edit `.uasset` or `.umap` binaries directly. Use Unreal Editor and then commit the resulting assets.

## Fab Owned Asset Reuse

- Prefer reusing suitable assets already in the project and the user's Fab My Library before creating new art or sourcing additional assets. Honor an explicit user request for a particular asset or new creation.
- Before content work, check `Docs/FabAssetReuseGuide.md` and inspect the relevant owned packs. Record the chosen assets and why they fit in the feature plan; refresh ownership and compatibility for the assets actually needed.
- Distinguish Fab library confirmation, local availability, historical use, and verified current runtime use. A project folder or a prior document alone does not establish current account ownership or integration.
- Select the meshes, materials, animation clips, Niagara effects, and sounds needed for the feature. Preserve vendor originals and adapt through project-owned assets, Blueprint children, and Data Assets using the existing project layout.
- For basic animation and VFX, default to editing suitable owned assets into project-owned derivatives. Create a new base only when the available assets cannot meet the requirement through reasonable editing, or the user explicitly requests new creation; record the reason in the feature plan.
- For animation, adapt retargeting, clip ranges, playback rate, blending, poses, montages, and notifies as needed while preserving camera, root-motion, and hit-timing contracts. For VFX, adapt the chosen effect's parameters, materials, color, scale, lifetime, emission, timing, and attachment to fit the action and view. Validate the edited result in its actual gameplay context.
- Keep the current art direction and C++ gameplay, save, progression, camera, and input contracts. Evaluate framework/template packs against those contracts before integrating their systems.
- Before import or activation, check the chosen product's usage terms, UE version/format, skeleton/root motion or Niagara dependencies, and relevant performance cost. Import and validate a scoped candidate before wider use.
- Use the Unreal Editor for binary assets, record the source and changed assets, and run validation appropriate to the actual change. This reuse preference does not authorize purchases or unrelated project changes.

## Build Commands

Editor target build:

```powershell
.\Scripts\BuildEditor.ps1
```

Project file generation after module or `.Build.cs` changes:

```powershell
& "D:\Program Files\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="D:\Project\Rogue10m\Rogue10m.uproject" -game -engine
```

Before committing, run:

```powershell
.\Scripts\CheckGeneratedChanges.ps1
```

## Feature Artifact Locations

- Architecture plans: `Feature/architect/YYYY-MM-DD_feature-name.md`
- Development summaries: `Feature/doc/YYYY-MM-DD_feature-name.md`
- Harness documentation: `Docs/HarnessPipeline.md`, `Docs/UnrealEngineeringRules.md`, `Docs/LazyCodexHarness.md`

Create a paired feature document set:

```powershell
.\Scripts\NewHarnessFeature.ps1 -Name "player combat"
```
