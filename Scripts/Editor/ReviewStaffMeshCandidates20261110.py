"""Read-only owned Staff mesh gallery. Explicit lease/fresh output; no asset/map save.
Three unrotated meshes normalized by longest bounds dimension to 180 cm for comparison.
The normalized gallery is NOT actual gameplay scale, grip approval, or asset redistribution.
"""
from __future__ import annotations
import argparse,hashlib,json,math,time
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
BASE='/Game/BattleWizardPolyart'
MESHES=[BASE+'/Meshes/MagicStaffs/Staff%02dSM'%i for i in (1,2,3)]
PACKAGES=set(MESHES+[BASE+'/Materials/StandardPolyartMat',BASE+'/Textures/StandardBaseColorTex'])
STATE={}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def vec(v):return [v.x,v.y,v.z]
def write(): (STATE['out']/'gallery.json').write_text(json.dumps(STATE['report'],indent=2),encoding='utf-8')
def load(path):
    obj=unreal.load_asset(path)
    if not obj:raise RuntimeError('Missing asset '+path)
    return obj

def audit_registry():
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(['/Game'],False)
    options=unreal.AssetRegistryDependencyOptions()
    for key in ('include_soft_package_references','include_hard_package_references','include_game_package_references','include_editor_only_package_references','include_searchable_names','include_soft_management_references','include_hard_management_references'):options.set_editor_property(key,True)
    queue=list(PACKAGES);seen=set();edges={};outside=set()
    while queue:
        package=queue.pop()
        if package in seen:continue
        seen.add(package)
        if not registry.get_assets_by_package_name(package,True):raise RuntimeError('Registry package missing '+package)
        dependencies=registry.get_dependencies(package,options)
        if dependencies is None:raise RuntimeError('Registry dependencies unavailable '+package)
        edges[package]=sorted(str(d) for d in dependencies)
        for dependency in edges[package]:
            if dependency.startswith('/Game/'):
                if dependency not in PACKAGES:outside.add(dependency)
                elif dependency not in seen:queue.append(dependency)
    STATE['report']['dependency_audit']={'scope':'recursive /Game dependencies; hard/soft/searchable/management enabled; engine/script mount dependencies recorded but not part of vendor five','edges':edges,'visited':sorted(seen),'outside_installed_five':sorted(outside),'passed':not outside and seen==PACKAGES}
    if outside or seen!=PACKAGES:raise RuntimeError('Game dependency closure exceeds five installed packages: '+str(sorted(outside)))

def spawn(cls,location=None,rotation=None):
    actor=STATE['actors'].spawn_actor_from_class(cls,location or unreal.Vector(),rotation or unreal.Rotator(),transient=True)
    if not actor:raise RuntimeError('Transient actor creation failed')
    STATE['owned'].append(actor);return actor

def setup_scene():
    s=STATE;unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    s['actors']=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    floor=spawn(unreal.StaticMeshActor,unreal.Vector(0,0,-7));floor.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(13,13,.1))
    light=spawn(unreal.DirectionalLight,unreal.Vector(0,0,400),unreal.Rotator(pitch=-40,yaw=-35,roll=0));light.light_component.set_editor_property('intensity',3.)
    fill=spawn(unreal.PointLight,unreal.Vector(250,-250,250));fill.light_component.set_editor_property('intensity',5000.);fill.light_component.set_editor_property('attenuation_radius',1400.)
    pp=spawn(unreal.PostProcessVolume);pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings')
    for key,value in {'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,'auto_exposure_min_brightness':1.,'auto_exposure_max_brightness':1.,'override_auto_exposure_bias':True,'auto_exposure_bias':-.5,'override_bloom_intensity':True,'bloom_intensity':.1}.items():settings.set_editor_property(key,value)
    pp.set_editor_property('settings',settings)
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for index,path in enumerate(MESHES):
        mesh=load(path)
        if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Expected StaticMesh '+path)
        bounds=mesh.get_bounds();origin=bounds.origin;extent=bounds.box_extent;dimensions=[2*v for v in vec(extent)]
        if not all(math.isfinite(v) and v>=0 for v in dimensions) or max(dimensions)<=0:raise RuntimeError('Invalid mesh bounds')
        scale=180./max(dimensions);lod_count=mesh.get_num_lods()
        lods=[{'lod':lod,'triangles':mesh.get_num_triangles(lod),'vertices':subsystem.get_number_verts(mesh,lod)} for lod in range(lod_count)]
        if not lods or any(v['triangles']<=0 or v['vertices']<=0 for v in lods):raise RuntimeError('Empty mesh LOD')
        materials=[]
        for slot in mesh.get_editor_property('static_materials'):
            material=slot.get_editor_property('material_interface')
            materials.append({'slot':str(slot.get_editor_property('material_slot_name')),'material':material.get_path_name() if material else None})
        if not materials or any(not v['material'] or v['material'].split('.')[0] not in PACKAGES for v in materials):raise RuntimeError('Unresolved/out-of-scope material')
        actor=spawn(unreal.StaticMeshActor);actor.set_actor_label('QA_StaffGallery_'+str(index+1),False)
        comp=actor.static_mesh_component;comp.set_static_mesh(mesh);comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        # Preserve original XYZ orientation. Center native bounds and place bottom at floor height.
        offset=(index-1)*155.;location=unreal.Vector(offset-origin.x*scale,offset-origin.y*scale,extent.z*scale-origin.z*scale)
        actor.set_actor_scale3d(unreal.Vector(scale,scale,scale));actor.set_actor_location(location,False,False)
        s['report']['meshes'].append({'asset':path,'native_bounds_origin_cm':vec(origin),'native_bounds_extent_cm':vec(extent),'native_dimensions_cm':dimensions,'native_sphere_radius_cm':bounds.sphere_radius,'lod_count':lod_count,'lods':lods,'materials':materials,'preview_uniform_scale':scale,'preview_normalized_longest_dimension_cm':180.,'preview_rotation_degrees':[0,0,0],'preview_location_cm':vec(location),'gallery_index':index+1,'actual_gameplay_scale':False})
    camera=unreal.Vector(580,-580,300);look=unreal.Vector(0,0,90)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,unreal.MathLibrary.find_look_at_rotation(camera,look))
    s['report']['camera']={'position':vec(camera),'look_at':vec(look),'settings_saved':False}
    s['actors'].clear_actor_selection_set();s['started']=time.monotonic();s['phase']='settle'

def finish(error=None):
    s=STATE
    if s.get('callback'):unreal.unregister_slate_post_tick_callback(s.pop('callback'))
    after={p:sha(Path(p)) if Path(p).is_file() else None for p in s['protected']}
    s['report'].update(status='failed' if error else 'gallery_captured_visual_review_pending',error=str(error) if error else None,protected_sha256_after=after,protected_files_unchanged=after==s['protected'])
    if after!=s['protected']:s['report'].update(status='failed',error='Protected file changed')
    pending=bool(s.get('task') and not s['task'].is_task_done())
    s['report']['pending_screenshot_scene_retained']=pending
    # Keep scene/editor available for coordinator inspection and the next Staff preview.
    s['report']['keep_alive_requested']=s['keep_alive'];write()
    unreal.EditorPythonScripting.set_keep_python_script_alive(s['keep_alive'] or pending)

def step(dt):
    s=STATE
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
        if s['phase']=='settle' and time.monotonic()-s['started']>=2.:
            s['image']=s['out']/'staff-gallery.png';s['task']=unreal.AutomationLibrary.take_high_res_screenshot(1600,1000,str(s['image']))
            if not s['task'] or not s['task'].is_valid_task():raise RuntimeError('Invalid ScreenshotTask')
            s['phase']='capture';s['capture_started']=time.monotonic()
        elif s['phase']=='capture':
            if time.monotonic()-s['capture_started']>90:raise RuntimeError('Screenshot timeout; scene retained')
            if s['task'].is_task_done():
                if not s['image'].is_file() or s['image'].stat().st_size<=1024:raise RuntimeError('Empty/missing PNG')
                s['report']['capture']={'image':s['image'].name,'bytes':s['image'].stat().st_size,'sha256':sha(s['image']),'screenshot_task_completed':True,'source':'actual Unreal viewport; no image editing','visual_review':'pending'}
                finish()
    except Exception as exc:finish(exc)

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--install-manifest',required=True);p.add_argument('--output',required=True);p.add_argument('--lease-granted',action='store_true');p.add_argument('--allow-transient-map',action='store_true');p.add_argument('--keep-alive',action='store_true');a=p.parse_args(argv)
    if not a.lease_granted or not a.allow_transient_map:raise RuntimeError('Exclusive lease/transient map authorization required')
    if STATE:raise RuntimeError('One gallery per script session')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():raise RuntimeError('Dirty editor; do not discard or save automatically')
    mp=Path(a.install_manifest).resolve();m=json.loads(mp.read_text(encoding='utf-8-sig'));out=Path(a.output).resolve()
    if out.exists():raise RuntimeError('Fresh output required')
    if not m.get('current_owned_confirmed') or len(m['files'])!=5:raise RuntimeError('Owned exact-five install evidence required')
    protected={};packages=set()
    for row in m['files']:
        relative=Path(row['relative']);file=(ROOT/'Content/BattleWizardPolyart'/relative).resolve()
        if not file.is_relative_to((ROOT/'Content/BattleWizardPolyart').resolve()) or file.suffix!='.uasset':raise RuntimeError('Unsafe installed path')
        if not row['source_preserved'] or row['source_sha256']!=row['installed_sha256'] or sha(file)!=row['installed_sha256']:raise RuntimeError('Vendor bytes changed')
        protected[str(file)]=sha(file);packages.add('/Game/'+file.relative_to(ROOT/'Content').with_suffix('').as_posix())
    if packages!=PACKAGES:raise RuntimeError('Unexpected installed five-package scope')
    out.mkdir(parents=True,exist_ok=False)
    STATE.update(out=out,protected=protected,keep_alive=a.keep_alive,task=None,owned=[],report={'status':'started','install_manifest':str(mp),'install_manifest_sha256':sha(mp),'protected_sha256_before':protected,'meshes':[],'saved_assets_or_maps':False,'runtime_selected':False,'normalized_preview_not_gameplay_scale':True,'native_axes_preserved':True,'source_assets_published':False,'visual_review':'pending'})
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    try:audit_registry();setup_scene();STATE['callback']=unreal.register_slate_post_tick_callback(step);write()
    except Exception as exc:finish(exc);raise

if __name__=='__main__':main()
