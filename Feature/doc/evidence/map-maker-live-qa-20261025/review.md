# Independent review — 2026-10-25

- Scope: the three changed editor C++ files, final build/report, retained GUI screenshots/readbacks, map hashes and cleanup. Review agents did not operate the shared editor or edit assets.
- Source: no blocker found. Grid width/scrolling and default window registration/teardown are consistent. Focus is restricted to the current world's active level viewport, rejects locked/piloted cameras, retains finite yaw, and leaves orthographic orientation to native framing.
- Camera: the first -70° correction failed at the southeast edge. The final -80° geometry clears the boundary's Z=31,000cm top; final camp/edge screenshots show terrain and readbacks both report -80°.
- Build: final three C++ changes precede the successful 115.11s full Editor build. Final GUI editor log ends normally.
- Regression: all three required tests have state Success and errors=0. Report totals are two succeeded plus one succeededWithWarnings; the integration test's four warnings (one reference-gather and three fixture map/HLOD registry warnings) are retained.
- Live operations: one Cube at (12800,-64000,3609.155) with yaw45 and scale20; actual keyboard Undo removes it, Redo restores the same actor, and the panel moves it to X38400 while preserving Z/rotation/scale.
- Preservation: independent before/after/current hashes match for all 1,260 original map files. Content Git status is empty and no fixture files remain. Final loaded actor count is 1,243, QA actors/selection/dirty map/content counts are zero.
- Limits: screenshot inspection and live readbacks substantiate the performed panel operations. Full runtime balance, long sessions, Shipping/cook, terrain sculpt/paint mutation, orthographic/piloted-camera GUI scenarios are outside this QA run. Changed-map saving/reload was tested in the isolated automation fixture.

## Existing startup Smoke errors

- The unmodified initial GUI log and final GUI/automation logs each contain 15 actual `LogAutomationTest: Error: Condition failed` entries at frame 0 before engine initialization. These are separate from the three selected map-maker tests' report entries (all Success, errors=0). Do not describe the entire raw log as error-free.
- Installed engine source evidence: `Runtime/Launch/Private/LaunchEngineLoop.cpp:4376` calls `RunSmokeTests()` during startup; `Runtime/Core/Private/Misc/AutomationTest.cpp:531-590` runs SmokeFilter tests and dumps their results; the same file at lines1243-1267 emits error entries at Error verbosity.
- `Runtime/Core/Private/Tests/UnifiedErrorTests.cpp:973-997` matches the five nearby UnifiedError diagnostic log lines. Those five lines themselves are ordinary Log verbosity despite containing the word Error. `Runtime/Core/Public/Misc/LowLevelTestAdapter.h:130` supplies the generic `Condition failed` CHECK message.
- The individual tests behind all 15 failed assertions and their precise cause are not identified by these logs. A locale-related cause was considered but is unconfirmed. No engine change was made as part of this map-maker QA.
