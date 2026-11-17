"""Read-only actual PIE exposure component diagnosis; no camera setters."""
from pathlib import Path
import json
import unreal as u
out=Path('D:/Project/Rogue10m/Feature/doc/evidence/caves20261118/runtime-camera-final/exposure-diagnosis.json')
assert not out.exists()
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert w
rows=[]
for a in u.GameplayStatics.get_all_actors_of_class(w,u.Actor):
    if 'CV18_' not in a.get_actor_label() and not isinstance(a,u.PostProcessVolume):continue
    comps=[]
    for c in a.get_components_by_class(u.SceneComponent):
        if not isinstance(c,(u.PostProcessComponent,u.BoxComponent,u.PointLightComponent)):continue
        d={'name':c.get_name(),'class':c.get_class().get_name(),'world_location':str(c.get_world_location()),'parent':str(c.get_attach_parent()),'editor_only':c.get_editor_property('is_editor_only')}
        if isinstance(c,u.PostProcessComponent):
            d.update({k:str(c.get_editor_property(k)) for k in ('enabled','priority','blend_weight','blend_radius','unbound','settings','visible','hidden_in_game')})
        if isinstance(c,u.BoxComponent):
            extent=c.get_unscaled_box_extent();center=c.get_world_location()
            inside=c.get_closest_point_on_collision(center)
            outside=c.get_closest_point_on_collision(center+u.Vector(extent.x+500,0,0))
            assert abs(float(inside[0]))<.01 and abs(float(outside[0])-500)<.1
            d.update(extent=str(extent),inside_distance_cm=float(inside[0]),outside_distance_cm=float(outside[0]),
                collision=str(c.get_collision_enabled()),
                responses={str(ch):str(c.get_collision_response_to_channel(ch)) for ch in (u.CollisionChannel.ECC_PAWN,u.CollisionChannel.ECC_CAMERA,u.CollisionChannel.ECC_VISIBILITY)})
        if isinstance(c,u.PointLightComponent):d.update(intensity=c.get_editor_property('intensity'),visible=c.get_editor_property('visible'))
        comps.append(d)
    d={'actor':a.get_path_name(),'label':a.get_actor_label(),'components':comps,'hidden':a.get_editor_property('hidden')}
    if isinstance(a,u.PostProcessVolume):d['settings']=a.get_editor_property('settings').export_text()
    rows.append(d)
p=u.GameplayStatics.get_player_character(w,0)
for c in p.get_components_by_class(u.CameraComponent):rows.append({'camera':c.get_name(),'settings':c.get_editor_property('post_process_settings').export_text()})
rows.append({'extend_luminance_range':u.SystemLibrary.get_console_variable_int_value('r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange')})
with out.open('x',encoding='utf-8') as f:json.dump(rows,f,ensure_ascii=False,indent=2)
print('CAVE_RUNTIME_EXPOSURE_READ_ONLY_DIAGNOSIS_WRITTEN')
