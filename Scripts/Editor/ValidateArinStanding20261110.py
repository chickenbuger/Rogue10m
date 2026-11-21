"""Disposable StartHub PIE: real native dialogue switches configured Arin Talk/Idle."""
import unreal,time,json,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'DevLog/20261110_HumanoidBossNpc/NPC/runtime.json'
STATE={'started':time.monotonic(),'phase':'wait','checks':[],'samples':[],'physical_input':False,'save_mutations_requested':False}
IDLE='/Game/Rogue10m/Characters/Arin/Standing20261110/A_Arin_BreathingIdle'
def check(name,value):
    STATE['checks'].append({'name':name,'passed':bool(value)})
    if not value:raise RuntimeError(name)
def sample(npc,label):
    mesh=npc.get_editor_property('guide_mesh');anim=npc.get_editor_property('guide_animation')
    instance = mesh.get_anim_instance()
    selected = anim.get_current_animation()
    check('native_playback_' + label, isinstance(instance, unreal.Rogue10mTutorialGuideAnimInstance)
          and instance.get_playback_sequence() == selected)
    STATE['samples'].append({'label':label,'animation':selected.get_path_name(),
        'state':str(anim.get_animation_state()),'position':instance.get_playback_position(),
        'bones':{n:list(mesh.get_socket_location(n).to_tuple()) for n in ['root','head','hand_l','hand_r','foot_l','foot_r']}})
def finish(error=None):
    if STATE.get('finished'):return
    STATE['finished']=True;STATE['status']='failed' if error else 'passed'
    if error:STATE['error']=str(error);STATE['traceback']=traceback.format_exc()
    unreal.unregister_slate_post_tick_callback(STATE['handle'])
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    public={k:v for k,v in STATE.items() if k not in ['handle','npc','pawn']}
    OUT.write_text(json.dumps(public,indent=2))
def tick(delta):
    try:
        if time.monotonic()-STATE['started']>75:raise RuntimeError('Bounded 75s watchdog')
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        if STATE['phase']=='wait':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            npcs=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Rogue10mTutorialNPC)
            if not pawn or not npcs:return
            npc=npcs[0];STATE.update(npc=npc,pawn=pawn,phase='idle',at=time.monotonic())
            pawn.get_editor_property('character_movement').stop_movement_immediately()
            at=npc.get_actor_location();pawn.set_actor_location(unreal.Vector(at.x+140,at.y,at.z+100),False,True)
            check('real_configured_Arin',npc.get_editor_property('guide_mesh').get_skeletal_mesh_asset().get_path_name().startswith('/Game/Rogue10m/Characters/Arin/RefinedV3/'))
        elif STATE['phase']=='idle' and time.monotonic()-STATE['at']>3:
            npc=STATE['npc'];anim=npc.get_editor_property('guide_animation')
            check('new_idle_active',anim.get_current_animation().get_path_name().split('.')[0]==IDLE);sample(npc,'idle_before_dialogue')
            check('real_begin_dialogue',npc.begin_dialogue(STATE['pawn']));STATE.update(phase='talk',at=time.monotonic())
        elif STATE['phase']=='talk' and time.monotonic()-STATE['at']>1:
            npc=STATE['npc'];anim=npc.get_editor_property('guide_animation')
            check('dialogue_open',npc.is_dialogue_open());check('talk_selected','Talk' in anim.get_current_animation().get_name());sample(npc,'talk')
            npc.close_dialogue();STATE.update(phase='return',at=time.monotonic())
        elif STATE['phase']=='return' and time.monotonic()-STATE['at']>2:
            npc=STATE['npc'];check('dialogue_closed',not npc.is_dialogue_open())
            check('returned_new_idle',npc.get_editor_property('guide_animation').get_current_animation().get_path_name().split('.')[0]==IDLE);sample(npc,'idle_after_dialogue')
            a,b=STATE['samples'][0],STATE['samples'][-1]
            for n in ['root','foot_l','foot_r']:check('planted_'+n,max(abs(x-y) for x,y in zip(a['bones'][n],b['bones'][n]))<.05)
            finish()
    except Exception as exc:finish(exc)
def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():raise RuntimeError('Existing PIE must be preserved')
    if OUT.exists():raise RuntimeError('Preserve prior runtime evidence')
    STATE['handle']=unreal.register_slate_post_tick_callback(tick)
    setattr(unreal,'_arin_standing_runtime_20261110',STATE)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
if __name__=='__main__':main()
