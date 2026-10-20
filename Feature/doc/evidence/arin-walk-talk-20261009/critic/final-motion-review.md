# Final independent walk/talk motion review

**Approved for the requested basic walk and conversational body-gesture scope in the reviewed source, fresh export and actual engine critical poses.** This approval does not claim facial lip sync, finger articulation, continuous self-collision freedom, terrain foot IK or pathfinding. Build/runtime regression exit gates belong to the integrator and are not inferred from screenshots.

The frozen source is `Arin_Motions.blend` SHA256 `379e07e07388f8095cfce59093deba2de4534bf028c706440674bcf55bae5528`. Walk FBX SHA256 is `cfb6f7d3dda9c3f3fe39da05f529da91d7f243bd78b9f3c92a9573d9af54c2b3`; Talk is `172601a141d9e41983ac949e2e00db1de9adf539f81466fc5b2f7ff0fa76a82e`. The accepted model's source, topology, UV, materials and weights remain the unchanged v3 model; only new actions were authored.

## Independent fresh-export checks

Both animation-only FBX files were imported in distinct factory-startup Blender processes. The unchanged `ReferenceRefined/SK_Arin.fbx` supplied the actual bind skeleton and source mesh for evaluated skinning. All 37 Walk frames and all 91 Talk frames were checked, not only the rendered samples.

| Check | Walk | Talk |
|---|---:|---:|
| Clip duration at 30 fps | 1.2 s | 3.0 s |
| Native imported frame range | 2..38 | 2..92 |
| Root matrix deviation | 0 | 0 |
| Endpoint vertex displacement | 0 | 0 |
| Non-finite samples | 0 | 0 |
| Edges above 4x rest length | 0 | 0 |
| Lowest boot sole Z | left 1.000 mm / right 0.997 mm | left approximately 0 / right 0.177 mm |
| Highest lowest-sole Z | approximately 116 mm each foot | unchanged |
| Source vs fresh absolute bone-matrix max component difference | 0.000018165 | 0.000012405 |

The Walk ankle target has a constant 1.10 m/s stance path, matching the 110 cm/s reference speed. Boot sole centroid velocity changes slightly during heel/toe rocking, and the simple contact metric also captures low swing transitions. It is not a proof of zero continuous world-space foot sliding on all surfaces.

The `fresh-walk-motion-final.json` and `fresh-talk-motion-final.json` reports include exact file hashes, actual semantic bones, all samples, and eight actual native-export render records. `fresh-*-absolute-pose-compare-unconnected.json` compares every exported bone pose to the frozen Blender source after the documented metadata correction below.

## Blender reader caveat and diagnostic history

An animation-only FBX has no mesh bind cluster. Blender's reader reconstructs a rest skeleton from its first animated pose and guesses coincident parent/child joints as connected. The accepted authored rig has `use_connect=False` for every bone. Rebinding the source mesh directly to that reconstructed first-pose skeleton produces misleading skin deformation, while auto-connecting the pelvis to the root suppresses its animated translation.

The fresh QA preserved the actual source bind skeleton, restored the known unconnected flags only in an unsaved scene, and applied the freshly imported absolute bone pose matrices by semantic name. No input/export/model file was saved or repaired by the reviewer. This revealed the exported motion's agreement with its source across every frame. Initial `fresh-*-motion.json`, `fresh-*-motion-preserved-bind.json` and the first raw `fresh-walk-absolute-pose-compare.json` are diagnostic reader-misinterpretation records, not final defect or approval evidence.

Both native clips have the generic take name `Arin_Rig|Scene`; their explicit filenames and hashes distinguish them, and the engine creates explicitly named Walk/Talk assets. Blender also adds one to the imported frame numbers by its default animation offset; duration and inclusive loop endpoints remain unchanged. Import onto the existing v3 Skeleton, preserving its reference/bind pose, rather than replacing it with a clip's reconstructed rest skeleton.

## Actual images opened

- `walk-critical-frame-board.png`: all 16 authored images covering eight contact/passing/swing times from oblique/profile views.
- `talk-critical-frame-board.png`: all 12 authored images covering six conversation times from front/oblique views.
- `fresh-native-renders/`: all eight independent native-export images: Walk frames 2 and 11, oblique/profile; Talk frames 25 and 70, front/oblique.
- Actual Epic Unreal MCP engine images in `../ue-motion/`: Walk frames 1/10/19/28 and Talk frames 24/68. The manifest identifies the actual imported animation, stopped clip position, LOD0 and source mesh, with component/camera restoration results.

Walk reads as an alternating forward gait with opposite arm swing, knee flexion, foot support and clearance. The apron/coat follow the legs without the original large knee/ankle spikes or visible tears in these views. Talk presents alternating palm-up conversational gestures, small body/head changes and stable boots. The raised arm stays visibly clear of the torso and satchel in the inspected maximum-gesture images. The engine's four gait samples and two gesture samples reproduce those behaviors, with readable clothing, wrists and boots.

The static spread fingers and unmoving textured mouth remain visible. Cloth, satchel and braid have no secondary simulation; the source outfit's deformation is stiff. The engine retains the guide actor's existing Z=2 cm placement; screenshots are not an exact terrain-contact measurement or a slope-foot-IK test. Abrupt single-node state swaps can snap limbs at a transition; the basic scope does not establish cinematic blend quality.

Sparse critical-cycle GIFs are included for quick comparison, preserving the true elapsed time between selected frames. They contain eight Walk samples and six Talk samples, not a smooth 30-fps rendered movie. Their manifest is `review-boards-manifest.json`. The actual authored and exported clips remain 30 fps.

## Source C++ review

The scoped review is in `cpp-review.md` with current source hashes in `cpp-review-provenance.json`. The initial overly narrow walk-rate clamp was corrected, and the parent fixed low-FPS timer catch-up with maximum-one-callback-per-frame plus zero-elapsed sampling protection. Direct local UE5.8 source inspection supports those semantics. Existing NPC reward/detection/dialogue content logic is preserved by the scoped diff. Actual low/reference/high speeds, Talk open/close/range release, clip-time advancement and the low-FPS test must be assessed from the parent's real runtime report.
