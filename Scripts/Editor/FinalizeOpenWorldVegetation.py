"""Read back MCP-generated foliage, tag owned actors, and save owned packages."""
import json
from pathlib import Path
import sys
import unreal

ROOT=Path('D:/Project/Rogue10m')
sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import OpenWorldLayout as layout

def main():
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if levels.is_in_play_in_editor() or world.get_path_name().split('.')[0]!='/Game/Rogue10m/Maps/OpenRunMap':
        raise RuntimeError('Idle OpenRunMap is required')
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    volumes=[a for a in actors if a.get_actor_label().startswith('OW_ForestTile_')]
    if len(volumes)!=183:
        raise RuntimeError('Expected 183 authored forest tiles, got '+str(len(volumes)))
    report={'tiles':len(volumes),'instances':0,'ground_height_errors_cm':[],
            'excluded_placement_count':0,'meshes':{},'spatial_tiles':0,'visible_components':0}
    for actor in volumes:
        actor.set_editor_property('tags',[unreal.Name('Rogue10m.OpenWorld'),unreal.Name('Rogue10m.OpenWorld.Vegetation')])
        actor.set_folder_path('OpenWorld/PCG Woodland')
        actor.set_editor_property('is_spatially_loaded',True)
        # PCG volumes inherit AVolume's hidden actor default; generated trees must render in play.
        actor.set_actor_hidden_in_game(False)
        report['spatial_tiles']+=int(actor.get_editor_property('is_spatially_loaded'))
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            count=component.get_instance_count()
            if not count:
                continue
            component.set_hidden_in_game(False,False)
            component.set_visibility(True,False)
            component.set_cull_distances(45000,65000)
            report['visible_components']+=1
            path=component.static_mesh.get_path_name()
            report['meshes'][path]=report['meshes'].get(path,0)+count
            report['instances']+=count
            for i in range(count):
                location=component.get_instance_transform(i,True).translation
                report['ground_height_errors_cm'].append(abs(location.z-layout.height_at(location.x,location.y)))
                if not layout.legal_foliage(location.x,location.y,road_buffer=2000,clearing_margin=1200):
                    report['excluded_placement_count']+=1
    errors=report.pop('ground_height_errors_cm')
    report['maximum_ground_height_error_cm']=max(errors,default=0)
    report['mean_ground_height_error_cm']=sum(errors)/max(len(errors),1)
    report['status']='passed' if report['instances']>1500 and report['spatial_tiles']==183 and not report['excluded_placement_count'] and max(errors,default=0)<150 else 'failed'
    evidence=ROOT/'Feature/doc/evidence/open-world-20261005'
    (evidence/'vegetation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if report['status']!='passed':
        raise RuntimeError('Vegetation readback failed: '+json.dumps(report))
    # Tags propagate from only our new Blueprint classes to spawner-created monsters.
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Rogue10m/World/OpenWorld/Encounters',recursive=False):
        bp=unreal.load_asset(path)
        if isinstance(bp,unreal.Blueprint):
            unreal.get_default_object(bp.generated_class()).set_editor_property('tags',[unreal.Name('Rogue10m.OpenWorld')])
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp):
                raise RuntimeError('Owned encounter Blueprint save failed')
    if not levels.save_current_level():
        raise RuntimeError('Vegetation map save failed')
    unreal.log('[OpenWorldVegetation] '+json.dumps(report))

if __name__=='__main__':
    main()
