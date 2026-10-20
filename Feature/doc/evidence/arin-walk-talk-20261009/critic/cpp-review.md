# Scoped tutorial animation C++ review

Reviewed the two new TutorialAnimationComponent files plus TutorialNPC `.h/.cpp` and TutorialDataAsset `.h` against the preserved files in `../baseline`. No code, art asset or live editor asset was modified by this reviewer. This is a source review, not a compilation or runtime approval.

## Finding resolved during review

The initial `RefreshAnimation` walk-rate clamp was `0.5..1.5` while the minimum walk speed was 10 cm/s and the reference speed 110 cm/s. At 11 cm/s it selected Walk but played at an effective 55 cm/s, causing substantial virtual ground sliding. The parent changed the clamp to `0.01..10.0`; direct reread confirmed the update. Check a speed just above the threshold, the reference speed, and a speed above the previous upper clamp in PIE. Motion export's actual virtual ground velocity must agree with the final DataAsset reference speed.

## Reviewed strengths

- The new component disables Tick and owns a 0.1-second native timer. EndPlay marks it stopping before clearing the timer and dropping cached references.
- After the parent's real low-frame-rate probe exposed catch-up callbacks, the component uses `FTimerManagerTimerParameters.bMaxOncePerFrame=true` and skips repeated samples with the same world time. Direct inspection of the installed UE5.8 `TimerManager.h` and `TimerManager.cpp` confirms this parameter breaks the delegate catch-up loop after the first invocation while the next expiration advances by the full elapsed `CallCount * Rate`. The zero-elapsed early return preserves the previous speed/state and clip time instead of resetting it. This revision is structurally consistent with irregular frame times; the parent's forced-5-FPS runtime result remains separate evidence.
- Mesh/DataAsset/fallback animation are non-owning weak caches; the current animation is a GC-tracked `UPROPERTY(Transient) TObjectPtr` and the owning NPC/DataAsset holds configured asset references.
- Animation candidates require the actual mesh Skeleton object and positive clip length. Missing/incompatible Walk or Talk falls back to a compatible Idle for the normal configured guide.
- Dialogue begin, explicit close and nearby-player release all notify the component. Talk has priority and is selected immediately; walking derives from sampled actor translation with threshold hysteresis. A large jump resets its movement sample.
- Current sequence identity is checked before `PlayAnimation`, so the repeating movement timer does not restart the same clip every 0.1 seconds.
- Constructor work creates default subobjects and static asset finders without world/player/subsystem queries. Tuning assets and speeds are exposed through Blueprint-read-only properties. Generated includes remain last and project logging uses `LogRogue10m`.
- The preserved NPC reward/detection/input/dialogue text logic has no new behavioral changes in the scoped diff; the added statements configure the animation component and signal dialogue state.

## Practical limits requiring final evidence

Single-node `PlayAnimation` swaps clips without a blended AnimGraph. A contact-pose-to-Idle transition or raised talking arm can therefore snap; actual engine captures/runtime review must decide whether this meets the requested basic motion. The component detects horizontal movement but does not create movement, pathfinding, facing direction, turn animation or obstacle avoidance. Forward gait also assumes an appropriate actor-facing/movement relationship. Conversation gesture priority does not stop externally driven movement; any future moving NPC controller should stop or otherwise coordinate movement during dialogue.

No new blocking source-level timer/delegate/GC defect was found in the reviewed normal guide configuration. Build success, saved asset references, exported motion fidelity and real state transitions remain the parent integrator's exit gates.

## Related actual motion defect

The first walk probe had 1,238 edges above 4x length and a 9.119x maximum. Independent diagnosis localized these to knees/ankles and measured disconnected pose joints. `Bone.vector` is parent-local, while `matrix_local` and solved targets were in armature space. Using `(tail_local - head_local)` for the segment rotation in an unsaved QA process reduced the maximum to 3.2049x and edges above 4x to 0 across all 37 frames. Joint gaps fell below 0.0076 mm. This is a confirmed authoring-math repair, not a request to alter the accepted source mesh/weights. See `walk-diagnosis-first.json` and `walk-segment-fix-verification.json`. The final delivered clip must be rebuilt and independently checked after the durable authoring script repair.
