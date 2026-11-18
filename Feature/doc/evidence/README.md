# Feature 검증 자료 보존 목록

2026-11-18 조사 시점의 사진 910장·JSON 6,055개 중 **사진 621장·JSON 3,293개**, 총 3,914개(810,622,927바이트)를 정리했다. 원래 조사 대상에서 **사진 289장·JSON 2,762개**를 남겼다. 새 정리 manifest와 이 목록은 원래 조사 수에 포함하지 않는다.

최종 결과, 원인이 다른 중요한 실패, 실제 스크립트 입력, 소스/참조 이미지, 대표 동작의 의미 있는 단계와 손·무기 궤적을 보존한다. 같은 결과를 반복한 중간 RPC·세션·촬영 자료를 줄였다. 기존 결과의 수치나 실패 상태는 수정하지 않았다.

진행 중인 `landscape-natural-boundary20261117`·`caves20261118`은 폴더 전체를 보호했다. 활성 작업은 이후 자료가 추가되거나 변경될 수 있다. 로그·영상·패치·소스·Unreal 자산·실제 세이브와 generated/reparse 경로는 정리 대상이 아니다.

Git 추적 파일 2,004개 삭제를 이번 커밋에 반영한다. 기존 미추적 보조 파일 1,910개도 로컬에서 삭제했다. 남겨 둔 미추적/비공개 자료 본문을 일괄 추가하지 않는다. 아래 링크는 기존 Git 추적 보존 파일만 가리키며, 로컬 보존 파일은 CSV에서 `tracked_before=False`로 구분한다.

[파일별 보존/삭제 manifest](evidence-curation-20261118/curation-manifest.csv) · [검증 결과](evidence-curation-20261118/verification.json) · [문서 링크 정리](evidence-curation-20261118/reference-updates.csv)

과거 문서·JSON에 적힌 촬영 수, 파일명과 해시는 당시 실행 기록이다. 삭제된 사진을 다른 성공 화면으로 바꾸지 않았으며, 보존은 해당 기능의 새 검증 통과를 뜻하지 않는다. 이미 형식이 잘못된 `boxing-direct-head-20260922/validation-final.json`, `split-hud-20261014/apply-remote02.json` 두 파일도 기존 실패/제약 기록으로 원본을 남겼다.

2026-11-19 게시 준비 중 중복·중간 JSON 5개를 추가로 정리했다. 위 집계는 11월 18일 기준이며, 삭제 경로와 대응 보관본은 [레거시 정리 결과](../2026-11-19_legacy-cleanup.md#json-정리-목록)에 기록했다.

같은 날 이전 맵의 중복 복구 사본과 중간 검증 자료 중 미추적 파일 4,584개를 로컬에서 정리했다. 과거 파일 경로와 복구에 사용할 동일 사본을 [이전 맵 자료 정리 기록](../2026-11-19_older-map-evidence-cleanup.md)에 남겼다.

| 자료 폴더 | 보존 사진 / JSON | 삭제 | Git 보존 자료 예시 |
| --- | ---: | ---: | --- |
| `alt-dodge-regression-20261101` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `animation-stability-20260928` | 5 / 21 | 0 | [retarget-ab-summary.json](animation-stability-20260928/retarget-ab-summary.json) · [boxing-stability-summary.json](animation-stability-20260928/boxing-stability-summary.json) |
| `appearance-head-20260926` | 1 / 2 | 3 | [source-review-hashes.json](appearance-head-20260926/source-review-hashes.json) · [boxing-assets.json](appearance-head-20260926/boxing-assets.json) |
| `arin-apollo-20261005` | 12 / 38 | 58 | [source-quality-final.json](arin-apollo-20261005/source-quality-final.json) · [create.json](arin-apollo-20261005/create.json) |
| `arin-blender-20261007` | 4 / 10 | 16 | [validation.json](arin-blender-20261007/roundtrip-first-failure/validation.json) · [list_tools.json](arin-blender-20261007/list_tools.json) |
| `arin-npc-fbx-20261005` | 1 / 1 | 0 | [preparation-status.json](arin-npc-fbx-20261005/preparation-status.json) · [arin-tpose-reference-v1.png](arin-npc-fbx-20261005/arin-tpose-reference-v1.png) |
| `arin-quality-20261007` | 19 / 49 | 297 | [final-handoff.json](arin-quality-20261007/final-handoff.json) · [body-review.json](arin-quality-20261007/body-review.json) |
| `arin-reference-refinement-20261009` | 34 / 66 | 222 | [final-script-ast.json](arin-reference-refinement-20261009/final-script-ast.json) · [native-blender-final.json](arin-reference-refinement-20261009/native-blender-final.json) |
| `arin-walk-talk-20261009` | 12 / 37 | 111 | [visual-review.json](arin-walk-talk-20261009/preview/visual-review.json) · [render-manifest.json](arin-walk-talk-20261009/preview/render-manifest.json) |
| `arms-only-20260912` | 0 / 1 | 0 | [asset-hashes.json](arms-only-20260912/asset-hashes.json) |
| `azathor-boss-patterns-20261011` | 3 / 13 | 16 | [visual-preview.json](azathor-boss-patterns-20261011/visual-preview.json) · [inspection-final.json](azathor-boss-patterns-20261011/inspection-final.json) |
| `basic-brawler-20260910` | 0 / 1 | 0 | [existing-asset-hashes.json](basic-brawler-20260910/existing-asset-hashes.json) |
| `basic-play-regression-20261101` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `basic-run-regression-20261101` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `boss-readability-20261103` | 0 / 3 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `boss-run-completion-20261020` | 4 / 3 | 6 | [content-freeze.json](boss-run-completion-20261020/content-freeze.json) · [local-integration.json](boss-run-completion-20261020/local-integration.json) |
| `boss-save-v3-integration-20261020` | 0 / 4 | 0 | [public-push-result.json](boss-save-v3-integration-20261020/public-push-result.json) · [validation-results.json](boss-save-v3-integration-20261020/validation-results.json) |
| `boss-south-approach-20261101` | 0 / 1 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `boxing-basic-20260923` | 0 / 1 | 1 | [asset-validation.json](boxing-basic-20260923/asset-validation.json) |
| `boxing-combos-20260912` | 0 / 2 | 0 | [asset-hashes.json](boxing-combos-20260912/asset-hashes.json) · [actual-hand-peaks.json](boxing-combos-20260912/actual-hand-peaks.json) |
| `boxing-direct-head-20260922` | 3 / 5 | 6 | [validation-final.json](boxing-direct-head-20260922/validation-final.json) · [import-report.json](boxing-direct-head-20260922/import-report.json) |
| `boxing-reference-20260922` | 3 / 4 | 2 | [actual-hand-peaks.json](boxing-reference-20260922/actual-hand-peaks.json) · [shared-asset-hashes.json](boxing-reference-20260922/shared-asset-hashes.json) |
| `camera-forward-attacks-20261009` | 4 / 11 | 17 | [review.json](camera-forward-attacks-20261009/review.json) · [audit.json](camera-forward-attacks-20261009/audit.json) |
| `catalog-consumption-20261110` | 0 / 28 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `caves20261118` | 0 / 18 | 0 | 진행 중 보호 |
| `close-combat-preparation-20261002` | 2 / 8 | 0 | [editor-summary.json](close-combat-preparation-20261002/editor-summary.json) · [readiness-summary.json](close-combat-preparation-20261002/readiness-summary.json) |
| `combat-readability-20261103` | 0 / 20 | 0 | [summary.json](combat-readability-20261103/final/loot/summary.json) · [summary.json](combat-readability-20261103/final/native-automation/summary.json) |
| `compact-hud-20261005` | 3 / 3 | 4 | [preservation.json](compact-hud-20261005/preservation.json) · [before-assets.json](compact-hud-20261005/before-assets.json) |
| `desktop-adoption-20261027` | 0 / 10 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `devlog-photo-curation-20261117` | 0 / 3 | 0 | [deletion-result.json](devlog-photo-curation-20261117/deletion-result.json) · [verification.json](devlog-photo-curation-20261117/verification.json) |
| `devlog-record-curation-20261118` | 0 / 4 | 0 | [deletion-result.json](devlog-record-curation-20261118/deletion-result.json) · [verification.json](devlog-record-curation-20261118/verification.json) |
| `fab-owned-assets-20261019` | 0 / 2 | 0 | [validation.json](fab-owned-assets-20261019/validation.json) · [launcher-observations.json](fab-owned-assets-20261019/launcher-observations.json) |
| `fighter-animation-binding-20261002` | 3 / 17 | 1 | [outcome.json](fighter-animation-binding-20261002/outcome.json) · [bound-data.json](fighter-animation-binding-20261002/bound-data.json) |
| `fighter-arm-swing-20261005` | 0 / 12 | 1 | [baseline-summary.json](fighter-arm-swing-20261005/baseline-summary.json) · [independent-runtime-review.json](fighter-arm-swing-20261005/independent-runtime-review.json) |
| `fighter-combat-stance-20261005` | 3 / 8 | 1 | [independent-code-review.json](fighter-combat-stance-20261005/independent-code-review.json) · [independent-motion-review.json](fighter-combat-stance-20261005/independent-motion-review.json) |
| `fighter-skill-tree-20260930` | 1 / 2 | 0 | [combat-analysis.json](fighter-skill-tree-20260930/combat-analysis.json) · [asset-preservation.json](fighter-skill-tree-20260930/asset-preservation.json) |
| `finish-unused-work-audit-20261020` | 0 / 16 | 0 | [final-checks.json](finish-unused-work-audit-20261020/final-checks.json) · [preview-receipt.json](finish-unused-work-audit-20261020/preview-receipt.json) |
| `first-attack-frames-20261029` | 0 / 5 | 4 | 로컬 보존 또는 비대상 자료만 유지 |
| `fist-combat-20260929` | 2 / 8 | 1 | [runtime-final-review.json](fist-combat-20260929/runtime-final-review.json) · [baseline-assets.json](fist-combat-20260929/baseline-assets.json) |
| `five-village-graybox-20261110` | 5 / 54 | 3 | 로컬 보존 또는 비대상 자료만 유지 |
| `five-village-quests-20261111` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `five-village-route-spec-20261110` | 0 / 2 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `five-village-runtime-20261111` | 0 / 16 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `five-village-travel-20261111` | 0 / 20 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `full-body-first-person-20260922` | 2 / 2 | 1 | [shared-asset-hashes.json](full-body-first-person-20260922/shared-asset-hashes.json) · [guard-reach-analysis.json](full-body-first-person-20260922/guard-reach-analysis.json) |
| `full-body-shadow-20260928` | 1 / 2 | 2 | [pixel-and-state-review.json](full-body-shadow-20260928/pixel-and-state-review.json) · [source-sha256.json](full-body-shadow-20260928/source-sha256.json) |
| `gameplay-integration-20261111` | 0 / 79 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `git-checkpoint-20261117` | 0 / 12 | 0 | [snapshot-summary.json](git-checkpoint-20261117/snapshot-summary.json) · [publication-summary.json](git-checkpoint-20261117/publication-summary.json) |
| `head-attached-camera-20260922` | 1 / 3 | 1 | [head-excursion-summary.json](head-attached-camera-20260922/head-excursion-summary.json) · [retarget-head-poses.json](head-attached-camera-20260922/retarget-head-poses.json) |
| `head-framing-20260922` | 1 / 1 | 3 | [asset-hashes.json](head-framing-20260922/asset-hashes.json) · [visual-view-comparison.jpg](head-framing-20260922/visual-view-comparison.jpg) |
| `hub-compact-entry20261113A` | 0 / 27 | 13 | 로컬 보존 또는 비대상 자료만 유지 |
| `hub-entry-windows20261114A` | 0 / 34 | 3 | 로컬 보존 또는 비대상 자료만 유지 |
| `humanoid-boss-npc-20261110` | 0 / 41 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `hybrid-skill-tree-20261018` | 5 / 0 | 0 | [reference.png](hybrid-skill-tree-20261018/reference.png) · [diagram-736.png](hybrid-skill-tree-20261018/diagram-736.png) |
| `hybrid-skill-tree-runtime-20261020` | 2 / 5 | 3 | [automation-summary.json](hybrid-skill-tree-runtime-20261020/automation-summary.json) · [asset-authoring.json](hybrid-skill-tree-runtime-20261020/asset-authoring.json) |
| `hybrid-tree-input-polish-20261020` | 3 / 10 | 32 | [final-checks.json](hybrid-tree-input-polish-20261020/final-checks.json) · [public-push-result.json](hybrid-tree-input-polish-20261020/public-push-result.json) |
| `identity-images-20261014` | 0 / 1 | 0 | [file-qa.json](identity-images-20261014/file-qa.json) |
| `impact-reference-20260912` | 4 / 0 | 0 | [overview_0.jpg](impact-reference-20260912/overview_0.jpg) · [overview_1.jpg](impact-reference-20260912/overview_1.jpg) |
| `input-frame-validation-20261029` | 0 / 1 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `landscape-natural-boundary20261117` | 8 / 251 | 0 | 진행 중 보호 |
| `landscape-production-20261116` | 3 / 126 | 18 | 로컬 보존 또는 비대상 자료만 유지 |
| `landscape-resume-20261117` | 9 / 107 | 58 | 로컬 보존 또는 비대상 자료만 유지 |
| `landscape-road-surface20261117` | 0 / 15 | 3 | 로컬 보존 또는 비대상 자료만 유지 |
| `landscape-world-blockout-20261112` | 3 / 48 | 37 | 로컬 보존 또는 비대상 자료만 유지 |
| `lobby-trait-display-20261101` | 0 / 1 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `look-dash-regression-20261029` | 0 / 3 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `loot-qa-resume-20261111` | 0 / 102 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `magic-command-buffer-20261110` | 0 / 1 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `magic-particle-candidates-20261110` | 9 / 73 | 419 | 로컬 보존 또는 비대상 자료만 유지 |
| `magic-skill-vfx-binding-20261111` | 0 / 21 | 357 | 로컬 보존 또는 비대상 자료만 유지 |
| `main-checkpoint-20261020` | 0 / 1 | 0 | [public-push-result.json](main-checkpoint-20261020/public-push-result.json) |
| `map-maker-live-qa-20261025` | 7 / 16 | 49 | [live-final.json](map-maker-live-qa-20261025/live-final.json) · [final-checks.json](map-maker-live-qa-20261025/final-checks.json) |
| `map-resume-20261113` | 0 / 138 | 30 | 로컬 보존 또는 비대상 자료만 유지 |
| `martelo-first-person-20260922` | 0 / 5 | 2 | [preview-cleanup.json](martelo-first-person-20260922/preview-cleanup.json) · [retarget-poses.json](martelo-first-person-20260922/retarget-poses.json) |
| `martelo-preview-20260922` | 0 / 1 | 0 | [motion.json](martelo-preview-20260922/motion.json) |
| `melee-skill-content-20261111` | 0 / 2 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `menu-abandon-regression-20261029` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `monster-auto-loot-20261111` | 0 / 10 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `monster-effects-chase-20261011` | 18 / 6 | 192 | [exit-gate.json](monster-effects-chase-20261011/exit-gate.json) · [application.json](monster-effects-chase-20261011/application.json) |
| `monster-loot-20261103` | 0 / 8 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `monster-loot-pickup-retry-20261110` | 0 / 23 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `monster-test-ui-20261011` | 1 / 2 | 7 | [exit-gate.json](monster-test-ui-20261011/exit-gate.json) · [pie-smoke.json](monster-test-ui-20261011/pie-smoke.json) |
| `mouse-head-stable-locomotion-20261013` | 0 / 21 | 5 | [final_preservation.json](mouse-head-stable-locomotion-20261013/final_preservation.json) · [safe-motion-failure.json](mouse-head-stable-locomotion-20261013/safe-motion-failure.json) |
| `moving-upper-body-attack-20261009` | 3 / 8 | 5 | [graph-apply.json](moving-upper-body-attack-20261009/graph-apply.json) · [graph-reload.json](moving-upper-body-attack-20261009/graph-reload.json) |
| `native-landscape-crop20261114A` | 0 / 9 | 3 | 로컬 보존 또는 비대상 자료만 유지 |
| `natural-brawler-20260910` | 0 / 1 | 0 | [asset-hashes.json](natural-brawler-20260910/asset-hashes.json) |
| `next-run-validation-20261103` | 0 / 8 | 0 | [NativeIO-result.json](next-run-validation-20261103/NativeIO-result.json) · [P-milestones.json](next-run-validation-20261103/P-milestones.json) |
| `normal-growth-boss-20261101` | 0 / 2 | 0 | [l-verification-summary.json](normal-growth-boss-20261101/l-verification-summary.json) · [m-verification-summary.json](normal-growth-boss-20261101/m-verification-summary.json) |
| `normal-ui-roundtrip-20261101` | 0 / 7 | 0 | [k-verification-summary.json](normal-ui-roundtrip-20261101/k-verification-summary.json) |
| `npc-combat-credit-20261113A` | 0 / 24 | 2 | 로컬 보존 또는 비대상 자료만 유지 |
| `npc-dialogue-20261110` | 0 / 3 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `npc-interaction-20261029` | 0 / 13 | 6 | 로컬 보존 또는 비대상 자료만 유지 |
| `npc-intro-compact20261113B` | 4 / 100 | 11 | 로컬 보존 또는 비대상 자료만 유지 |
| `open-world-20261005` | 2 / 209 | 672 | [final-save.json](open-world-20261005/final-save.json) · [independent-review.json](open-world-20261005/independent-review.json) |
| `open-world-map-20261005` | 1 / 3 | 0 | [independent-review.json](open-world-map-20261005/independent-review.json) · [map-data.json](open-world-map-20261005/map-data.json) |
| `open-world-monsters-20261005` | 2 / 18 | 91 | [final-check-summary.json](open-world-monsters-20261005/final-check-summary.json) · [guide-key-final.json](open-world-monsters-20261005/guide-key-final.json) |
| `output-log-errors-20261116` | 0 / 19 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `owner-framing-regression-20261031` | 0 / 12 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `owner-view-regression-20261029` | 0 / 2 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `owner-view-resume-regression-20261031` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `personal-studio-20260928` | 2 / 0 | 0 | [mobile.png](personal-studio-20260928/mobile.png) · [desktop.png](personal-studio-20260928/desktop.png) |
| `quick-melee-20260909` | 0 / 1 | 0 | [source-finger-angles.json](quick-melee-20260909/source-finger-angles.json) |
| `quit-menu-20261111` | 9 / 15 | 3 | 로컬 보존 또는 비대상 자료만 유지 |
| `recorded-reference-20260912` | 4 / 0 | 0 | [move_sheet.jpg](recorded-reference-20260912/move_sheet.jpg) · [attack_sheet.jpg](recorded-reference-20260912/attack_sheet.jpg) |
| `reference-scanner-20261116` | 0 / 28 | 4 | 로컬 보존 또는 비대상 자료만 유지 |
| `relaxed-idle-20260922` | 1 / 2 | 4 | [final-source-hashes.json](relaxed-idle-20260922/final-source-hashes.json) · [first-run-motion-analysis.json](relaxed-idle-20260922/first-run-motion-analysis.json) |
| `repository-file-audit-20261117` | 0 / 1 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `run-lobby-regression-20261029` | 0 / 10 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `run-result-ui-regression-20261029` | 0 / 12 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `safe-menu-pause-20261103` | 0 / 3 | 0 | [VERIFICATION.json](safe-menu-pause-20261103/VERIFICATION.json) |
| `safe-menu-pause-regression-20261103` | 0 / 6 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `split-hud-20261014` | 3 / 13 | 21 | [dependent-preview-reload.json](split-hud-20261014/dependent-preview-reload.json) · [editor-before-final-reload.json](split-hud-20261014/editor-before-final-reload.json) |
| `stable-mouse-head-20261013` | 0 / 1 | 0 | [asset-motion-audit.json](stable-mouse-head-20261013/asset-motion-audit.json) |
| `staff-animation-stage-20261110` | 0 / 18 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `staff-equipment-20261110` | 2 / 42 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `staff-vfx-runtime-20261111` | 11 / 4 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `start-hub-tutorial-npc-image-20261005` | 1 / 1 | 0 | [validation.json](start-hub-tutorial-npc-image-20261005/validation.json) · [arin-concept-v1.png](start-hub-tutorial-npc-image-20261005/arin-concept-v1.png) |
| `start-village-20261005` | 2 / 15 | 47 | [inputs.json](start-village-20261005/inputs.json) · [runtime.json](start-village-20261005/runtime.json) |
| `storage-cleanup-20261110` | 0 / 2 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `studio-agent-workflow-20260928` | 3 / 2 | 4 | [real-agent-blocked-result.json](studio-agent-workflow-20260928/real-agent-blocked-result.json) · [real-agent-blocked-evidence.json](studio-agent-workflow-20260928/real-agent-blocked-evidence.json) |
| `studio-fist-completion-20260929` | 3 / 1 | 1 | [fresh-analysis.json](studio-fist-completion-20260929/fresh-analysis.json) · [knuckle-frame.png](studio-fist-completion-20260929/knuckle-frame.png) |
| `studio-task-execution-20260928` | 2 / 0 | 2 | [actual-result.png](studio-task-execution-20260928/actual-result.png) · [actual-schedule.png](studio-task-execution-20260928/actual-schedule.png) |
| `sword-knuckle-20261110` | 0 / 227 | 897 | 로컬 보존 또는 비대상 자료만 유지 |
| `third-person-20261110` | 0 / 15 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `third-person-inspection-20260928` | 1 / 2 | 1 | [final-source-hashes.json](third-person-inspection-20260928/final-source-hashes.json) · [source-review-hashes.json](third-person-inspection-20260928/source-review-hashes.json) |
| `town-services-persistence-20261011` | 1 / 1 | 2 | [source-integration.json](town-services-persistence-20261011/source-integration.json) · [town-services.png](town-services-persistence-20261011/Rogue10m_Town_QA_finalvisual1080/town-services.png) |
| `trait16-runtime-20261011` | 3 / 5 | 8 | [verification-summary.json](trait16-runtime-20261011/verification-summary.json) · [editor-final-reload.json](trait16-runtime-20261011/editor-final-reload.json) |
| `unreal-plugin-setup-20261019` | 0 / 5 | 32 | [connection-final.json](unreal-plugin-setup-20261019/connection-final.json) · [validation.json](unreal-plugin-setup-20261019/validation.json) |
| `unreal-skill-sync-20261019` | 0 / 5 | 0 | [final-audit.json](unreal-skill-sync-20261019/final-audit.json) · [validation.json](unreal-skill-sync-20261019/validation.json) |
| `unused-file-cleanup-20260928` | 0 / 6 | 0 | [summary.json](unused-file-cleanup-20260928/summary.json) · [candidate-review.json](unused-file-cleanup-20260928/candidate-review.json) |
| `unused-file-optimization-20261020` | 0 / 10 | 1 | [final-checks.json](unused-file-optimization-20261020/final-checks.json) · [preview-receipt.json](unused-file-optimization-20261020/preview-receipt.json) |
| `view-aligned-upper-body-20261011` | 0 / 6 | 7 | [verification-summary.json](view-aligned-upper-body-20261011/verification-summary.json) · [editor-pie-final.json](view-aligned-upper-body-20261011/editor-pie-final.json) |
| `village-ground-20261005` | 2 / 8 | 67 | [viewpoints.json](village-ground-20261005/viewpoints.json) · [application.json](village-ground-20261005/application.json) |
| `walking-palm-reference-20261009` | 24 / 17 | 12 | [review.json](walking-palm-reference-20261009/review.json) · [runtime-review.json](walking-palm-reference-20261009/reference/runtime-review.json) |
| `weapon-identity-20261110` | 0 / 3 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `weapon-presentation` | 0 / 21 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `weapon-skill-loadouts-20261110` | 0 / 17 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `weighted-first-person-regression-20261029` | 0 / 3 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `windows-executable-package-20261111` | 2 / 21 | 2 | 로컬 보존 또는 비대상 자료만 유지 |
| `world-map-maker-20261024` | 0 / 8 | 4 | [final-checks.json](world-map-maker-20261024/final-checks.json) · [result.json](world-map-maker-20261024/tests-final/result.json) |
| `world-map-production-20261112` | 0 / 2 | 0 | 로컬 보존 또는 비대상 자료만 유지 |
| `world-map-production-reference-20261112` | 0 / 1 | 0 | [validation.json](world-map-production-reference-20261112/validation.json) |
