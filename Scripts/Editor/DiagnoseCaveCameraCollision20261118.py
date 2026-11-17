"""Read-only Camera-channel sphere trace and actual stopped PIE failure image."""
from pathlib import Path
import json
import time
import unreal as u
BASE=Path('D:/Project/Rogue10m/Feature/doc/evidence/caves20261118/runtime-camera-final')
assert not (BASE/'camera-collision-diagnosis.json').exists()
failure=json.loads((BASE/'result.json').read_text());assert failure['passed'] is False
sample=failure['worst_camera_sample']
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world();assert w
p=u.GameplayStatics.get_player_character(w,0);pc=u.GameplayStatics.get_player_controller(w,0)
arm=list(p.get_components_by_class(u.SpringArmComponent))[0]
assert arm.get_editor_property('probe_channel')==u.CollisionChannel.ECC_CAMERA
def pack(v):
    if hasattr(v,'get_path_name'):return {'path':v.get_path_name(),'class':v.get_class().get_path_name()}
    if all(hasattr(v,k) for k in ('x','y','z')):return [float(v.x),float(v.y),float(v.z)]
    if v is None or isinstance(v,(str,int,float,bool)):return v
    return str(v)
# CollisionProfile.cpp373 adds Visibility then Camera to TraceTypeMapping.
hit=u.SystemLibrary.sphere_trace_single(w,u.Vector(*sample['pivot_cm']),u.Vector(*sample['unfixed_camera_cm']),
    14.,u.TraceTypeQuery.TRACE_TYPE_QUERY2,False,[p],u.DrawDebugTrace.NONE,True)
assert hit is not None
fields=hit.to_tuple();assert len(fields)>=11 and fields[0]
component=fields[10]
report={'read_only':True,'assets_saved':False,'camera_channel_mapping':'Engine CollisionProfile.cpp373 Visibility query1, Camera query2',
    'recorded_failure_sample':sample,'sphere_radius_cm':14.,'hit_fields':[pack(x) for x in fields],
    'component':pack(component),'current_camera_cm':pack(pc.player_camera_manager.get_camera_location()),
    'current_camera_rotation':str(pc.player_camera_manager.get_camera_rotation()),'camera_or_pawn_setters':False}
with (BASE/'camera-collision-diagnosis.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
image=BASE/'captures/07_Q_camera_collision_failure.png';assert not image.exists()
state={'ticks':0,'task':None,'busy':False,'start':time.monotonic()}
def tick(_):
    if state['busy']:return
    state['busy']=True
    try:
        state['ticks']+=1
        if state['ticks']<3:return
        if state['task'] is None:
            state['task']=u.AutomationLibrary.take_high_res_screenshot(1280,720,str(image));return
        assert time.monotonic()-state['start']<90
        if not state['task'].is_task_done():return
        assert image.is_file()
        u.unregister_slate_post_tick_callback(state['handle'])
        print('CAVE_CAMERA_COLLISION_FAILURE_IMAGE_RETAINED')
    finally:state['busy']=False
state['handle']=u.register_slate_post_tick_callback(tick)
