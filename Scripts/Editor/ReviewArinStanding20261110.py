"""Temporary exact-pose viewport inspection of the actual StartHub Arin actor."""
import unreal
import ReviewArinMotion as motion
import ReviewReferenceArinLOD as review
ROOT='/Game/Rogue10m/Characters/Arin/'
CLIPS={'before':ROOT+'RefinedV3/ArinIdle','idle':ROOT+'Standing20261110/A_Arin_BreathingIdle','talk':ROOT+'RefinedV3/Animations/A_Arin_Talk'}
def begin():
    return motion.begin()
def pose(key,seconds):
    mesh=review.component();clip=unreal.load_asset(CLIPS[key])
    if clip is None:raise RuntimeError('Missing '+CLIPS[key])
    mesh.play_animation(clip,True);mesh.set_play_rate(0.);mesh.set_position(seconds,False)
    return motion.inspect()
def inspect():return motion.inspect()
def end():return motion.end()
