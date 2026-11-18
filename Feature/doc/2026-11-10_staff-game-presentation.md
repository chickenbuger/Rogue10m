# Staff game presentation review fixture

## Implemented

- `Source/Rogue10m/Tests/Rogue10mStaffPresentationRuntimeTest.cpp`: editor-build-only `Rogue10m.TestStaffPresentation` console fixture. Actual HumanMale game actor, normal possession/profile/ASC/appearance, transient instance-only Staff candidate, unchanged native defaults.
- `Scripts/VerifyStaffPresentation.ps1`: serialized render launcher, unique QA namespace, 8-phase/callback/file checks, continuous playback and movement/shaft-proxy verification, all original save hashes and 25 protected package hashes before/after.
- The exact provisional candidate grip is embedded from `StaffMeshDerivativeA/grip-spec-a.json`; no parsing dependency or asset edit was introduced.
- Two continuous native-source DefaultSlot montages retain the existing Common_Unarmed and HumanMale retarget graphs. The 89 count refers to sequence bone tracks, not an assumed global skeleton bone count.

## Evidence contract

- Outputs: `Feature/doc/evidence/staff-equipment-20261110/<fresh label>/` contains engine log, frames.csv, captures.csv, metrics.txt, owned-process.json, launcher.json and save/asset hash snapshots.
- Images: `DevLog/20261110_StaffPresentation/<fresh label>/`, exactly eight PNGs with unique run IDs.
- Order: Strike ready/default-third-person, Strike windup at >=0.20s, action at >=0.80s, post-completion recovery; Shot ready/fixed-front-side, windup at >=0.30s, action at >=1.20s, post-completion recovery.
- Timings are provisional review sampling points, not authored contact or damage times. Both requested and processed montage positions are recorded. No exact rendered-frame synchronization is claimed.
- `frames.csv` records rendered right hand, right palm midpoint, transformed model Y-axis endpoints, right/left palm gaps, component-local hand travel, montage activity/progression and actor movement guard. The 15 cm right-hand bound is a coarse attachment sanity check only.
- Sword/Knuckle restore checks establish presentation switching only. This fixture never sends a Combat request, triggers skill input, equips through inventory, or claims damage verification.

## Validation status

- PowerShell parser: passed on worker-written launcher.
- UE APIs used were read against the installed UE 5.8 headers. No engine launch or build was run by the implementation worker.
- Parent must compile, run with a fresh label and inspect all eight PNGs. Numeric success is not grip/appearance approval. The native Staff default is still null and no asset was saved or published.

## Run

`powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\VerifyStaffPresentation.ps1 -Label StaffGameReviewA -TimeoutSeconds 240`

Use another label if that evidence/image directory already exists. Preserve failed candidates for comparison.
