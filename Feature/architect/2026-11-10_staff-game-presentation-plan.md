# Staff actual-game presentation fixture

## Scope and provenance

- Add only an editor-build native runtime test and PowerShell launcher. Do not change production defaults, combat input/damage, skill Data Assets, animation assets, camera defaults or first-person presentation.
- Reuse the owned Standard Wizard for Battle Polyart Staff01 derivative `/Game/Weapons/Staff20261110/SM_BattleWizardStaff01` and exact provisional `StaffMeshDerivativeA/grip-spec-a.json` transform. Vendor originals remain untouched. Ownership/UE 5.8 verification is recorded in the preceding installation evidence; public redistribution is not approved.
- Play the two project-owned `StaffResumeC/A_Staff_StaffStrike_Manny` and `A_Staff_StaffShot_Manny` sequences through the existing native source `ABP_Common_Unarmed` DefaultSlot. Require `UAnimSequence`, the exact project Manny skeleton, 89 source tracks, full 2.0/2.9-second lengths and no enabled root motion. HumanMale offline derivative sequences are not substituted for the native live-retarget path.

## Fixture contract

1. Launch the normal game map in a fresh isolated three-slot QA save namespace. Spawn the actual HumanMale BP at the existing player transform, possess it normally, apply a fresh valid profile and verify PlayerState/ASC registration. Preserve body transform and original control rotation.
2. Mark only the runtime native visual component transient and override its StaffMesh/StaffGrip. Restore Staff as presentation setup, not inventory equip proof. Require visible right mesh, exact rendered GetMesh parent, hand_r, NoCollision, hidden offhand, third-person and normal HumanMale retarget ABP.
3. Disable fixture movement and monster AI targeting. Check source/body hand movement, monotonically increasing active montage samples, unchanged actor position/orientation, and finite model-local Y bounds-centerline versus palm-midpoint geometry. The right-palm coarse proxy has a 15 cm guard, not a surface-contact criterion; left gap is evidence only.
4. Capture exactly eight unique PNGs with real screenshot-processed callbacks and non-empty files: Strike ready in default third person; Strike windup/action/recovery and Shot ready/windup/action/recovery in a fixed front-side fixture camera. Dynamic montages keep progressing naturally, without SetPosition, freezing or time-dilation tricks. Recovery waits for natural completion and blend-out.
5. Check return-to-Sword and return-to-Knuckle presentation without more images. Verify native defaults remain unchanged. Record request/processed time separately; captures are deferred and do not prove exact frame correspondence.

## Validation and rollback

- Parent owns the serialized UE/build lease. Worker does not build or launch UE.
- Build with `Scripts/BuildEditor.ps1`, then run `Scripts/VerifyStaffPresentation.ps1 -Label StaffGameReviewA` using a new label each run.
- Wrapper uses `-game -DisablePython -d3d11 -RenderOffScreen`, rejects NullRHI and shared project UE processes, has a bounded timeout and only terminates its own matched PID.
- Snapshot all existing `.sav` SHA256 values and 25 protected source/candidate packages before/after. Save and package preservation are mandatory on success and failure. No package save API is called.
- Static check, compilation, numerical/capture checks and actual eight-PNG visual review are separate gates. Native Staff default must remain null until the parent has reviewed the images and decided integration is safe.
- Rollback consists of removing the new test and wrapper only; no production or asset rollback is needed. Source pose exports remain private local evidence and are not copied or published.
