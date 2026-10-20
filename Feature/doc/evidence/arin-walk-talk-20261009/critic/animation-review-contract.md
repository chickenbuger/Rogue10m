# Arin walk/talk independent review contract

The baseline source is `Art/NPC/Arin/ReferenceRefined/Arin.blend`, SHA256 `56cc68fda9a32ec932a83d57a20dbb8897ef51933922c609027a542e2ad01f74`. Fresh read-only Blender inspection produced `rig-baseline.json`. The accepted model has 45,205 authored vertices, 90,214 triangles, 23 deform bones, 1.70 m rest height, and linear skinning. It lacks jaw, facial and finger bones. `root`, `ball_l`, and `ball_r` have zero weighted vertices. The baseline source and its reviewed material/UV/topology should remain unchanged while new motion deliverables are added.

## Walk scope

- A basic forward in-place walk on the existing custom skeleton, at 30 fps, with a seamless endpoint and stationary root. A roughly 1.2-second stride is a reasonable initial choice, subject to the actual reference motion.
- Feet alternate support and swing, with an observable swing clearance. Contact is judged from the rendered boot soles as well as numerical bounds; ankle bone placement alone cannot prove shoe contact.
- The expected actor movement speed should match the planted local foot's backward movement. Report foot sliding in virtual world space separately from the naturally moving foot in an in-place clip. Engine speed/play rate and turning must also be verified by the parent integrator.
- Use opposite, moderate arm swing and subtle torso/pelvis motion. Keep the right arm clear of the satchel and potion. Inspect coat/apron/crotch deformation at left and right contact, passing, and maximum swing frames.
- Foot bones can rotate the boots. The unweighted ball bones do not independently bend the toes. Do not describe unimplemented toe deformation or cloth simulation.

## Talking scope

- A stationary loop with readable head/neck nods, modest torso movement, and alternating or asymmetrical forearm/hand gestures suitable for a tutorial guide.
- Keep the lower body and boots stable. Review the maximum gesture and elbow bend from front, oblique and profile views for garment stretching, shoulder collapse and bag penetration.
- The baseline face is mostly texture-driven and has no facial controls. This delivery can demonstrate conversational body gestures; it cannot establish speech-synchronized mouth movement or finger articulation.

## Technical evidence

Inspect authored motion and at least one fresh native exported clip per requested action. Verify exact semantic bone hierarchy, rest joints, stationary root, finite coordinates/transforms, absence of unexpected bone scale, duration/fps, endpoint deformation continuity, and actual deformation across the cycle. Evaluate every integer frame for gross edge inflation and boot sole height. Numerical passes still require actual motion drawings/GIF and engine runtime state verification.

`InspectMotionReadOnly.py` in the owned helper directory supports full Blender scenes and animation-only FBX over an explicitly supplied unchanged mesh source. It does not save scenes or modify source files. Its edge and contact thresholds are review signals, not a promise of continuous collision-free motion or anatomically exact gait.

## Evidence classification

- `gate`: absent/invalid clip, wrong rig/hierarchy, non-finite motion, missing requested runtime state.
- `defect`: clear foot penetration/sliding, visible garment tear/spike or strong bag/arm clipping in the provided clip, broken loop or gross transition discontinuity.
- `warning`: static spread fingers, texture-driven face, minor fold artifacts, unsupported arbitrary slope or turning behavior.
- `observation`: actual measured duration, joint positions, sampled clearance and geometry preservation.

Final visual findings remain pending until the parent supplies the frozen motion source/export and actual rendered critical frames.
