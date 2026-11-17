"""Read-only actual Camera14 sphere sweep through the failed turn's full orbit."""
from pathlib import Path
import json
import math
import unreal as u
BASE=Path('D:/Project/Rogue10m/Feature/doc/evidence/caves20261118')
def audit(stage):
    assert stage in ('before','after')
    out=BASE/('camera-arc-'+stage+'.json');assert not out.exists()
    assert not u.get_editor_subsystem(u.LevelEditorSubsystem).is_in_play_in_editor()
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0]=='/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A'
    dirty=lambda:sorted({p.get_name() for p in list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())+list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})
    before=dirty()
    s=json.loads((BASE/'runtime-camera-final/result.json').read_text())['worst_camera_sample']
    p=s['pivot_cm'];d=[b-a for a,b in zip(p,s['unfixed_camera_cm'])]
    old_yaw=math.radians(s['camera_rotation_pitch_yaw_roll'][1]);c=math.cos(old_yaw);sn=math.sin(old_yaw)
    local=[c*d[0]+sn*d[1],-sn*d[0]+c*d[1],d[2]];length=math.sqrt(sum(x*x for x in d))
    rows=[]
    for i in range(1440):
        angle=i*.25;r=math.radians(angle);c=math.cos(r);sn=math.sin(r)
        end=[p[0]+c*local[0]-sn*local[1],p[1]+sn*local[0]+c*local[1],p[2]+local[2]]
        hit=u.SystemLibrary.sphere_trace_single(world,u.Vector(*p),u.Vector(*end),14.,
            u.TraceTypeQuery.TRACE_TYPE_QUERY2,False,[],u.DrawDebugTrace.NONE,True)
        f=hit.to_tuple() if hit is not None else None
        distance=float(f[3]) if f and f[0] else length
        rows.append({'yaw_degrees':angle,'distance_cm':distance,'pull_in_cm':length-distance,
            'actor':f[9].get_actor_label() if f and f[9] else None,
            'component':f[10].get_name() if f and f[10] else None,
            'item':int(f[13]) if f else None,'initial_overlap':bool(f[1]) if f else False})
    minimum=min(rows,key=lambda x:x['distance_cm']);bad=[x for x in rows if x['distance_cm']<140 or x['pull_in_cm']>180 or x['initial_overlap']]
    assert dirty()==before
    report={'schema':'Rogue10m.CaveActualCameraArc.20261118.v1','stage':stage,'read_only':True,
        'probe_radius_cm':14,'angles':1440,'angular_step_degrees':.25,'pivot_cm':p,
        'camera_default_offset_derived_from_recorded_actual_PCM':local,'unfixed_distance_cm':length,
        'minimum':minimum,'failed_angles':len(bad),'passed':len(bad)==0,
        'dirty_before':before,'dirty_after':dirty(),'cases':rows,
        'runtime_traversal_certified':False,'camera_or_pawn_setters':False,'assets_saved':False}
    with out.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
    print('CAVE_CAMERA_ARC_'+stage.upper()+' '+str(report['passed'])+' min='+str(minimum['distance_cm']))
    return report
if __name__=='__main__':audit('before')
