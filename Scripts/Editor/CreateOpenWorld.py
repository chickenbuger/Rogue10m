"""Author OpenRunMap through editor APIs, preserving template and pack assets.

Geometry lives in OpenWorldLayout; native authoring exposes Landscape import and
map-local navigation which are not available as ordinary Python editor APIs.
PCG vegetation is a separate MCP graph pass. Only new map-owned actors are reset.
"""
import datetime
import json
import math
from pathlib import Path
import random
import sys
import traceback
import unreal

ROOT = Path('D:/Project/Rogue10m')
sys.path.insert(0, str(ROOT/'Scripts/Editor'))
import OpenWorldLayout as layout
from CreateStartVillage import rotate

MAP = '/Game/Rogue10m/Maps/OpenRunMap'
TEMPLATE = '/Game/FirstPerson/Lvl_FirstPerson'
OWN = '/Game/Rogue10m/World/OpenWorld'
TAG = 'Rogue10m.OpenWorld'
CHECKPOINT = TAG+'.TemplateCleaned'
PACK = '/Game/Stylized_Village/Assets'
EVIDENCE = ROOT/'Feature/doc/evidence/open-world-20261005'
MESHES = {
    'cube': '/Engine/BasicShapes/Cube',
    'plane': '/Engine/BasicShapes/Plane',
    'beam': PACK+'/Architecture/Wood/Pieces/SM_Beam_Long_1',
    'floor': PACK+'/Architecture/Wood/Floors/SM_Wood_Floor_FullxFull_LP',
    'wall': PACK+'/Architecture/Flagstone/Walls/SM_Flagstone_FullXFull',
    'pillar': PACK+'/Architecture/Flagstone/Structure/SM_FlagStone_Pillar_Full',
    'arch': PACK+'/Architecture/Flagstone/Trims/SM_FlagStone_Trim_Arch',
    'rock': PACK+'/Stones/Boulder/SM_Boulder_Large_01',
    'plaster': PACK+'/Architecture/Plaster/SM_Plaster_FullXFull_Plaster_FullXFull',
    'roof': PACK+'/Architecture/RoofTiles/Round/SM_RoofTiles_Round_FullXFull',
    'barrel': PACK+'/Props/Furniture/SM_SM_Barrel_01',
    'crate': PACK+'/Props/Furniture/SM_Crate_01',
    'water': PACK+'/VFX/Water/Mesh/SM_WaterMesh',
}

def vec(values):
    return unreal.Vector(*map(float, values))

def asset(path):
    result = unreal.load_asset(path)
    if not result:
        raise RuntimeError('Missing required asset '+path)
    return result

def world():
    result = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not result or result.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Only OpenRunMap may be authored')
    return result

class Builder:
    def __init__(self, report):
        self.report = report
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.meshes = {k: asset(p) for k,p in MESHES.items()}
        self.count = 0
        self.rng = random.Random(51011)
        self.dirt = asset('/Game/StarterContent/Materials/M_Ground_Gravel')
        self.stone = asset('/Game/StarterContent/Materials/M_Concrete_Panels')
        self.water = asset(PACK+'/VFX/Water/Material/MI_Water')

    def owned(self, actor, label, folder='Scenery', spatial=True):
        if not actor:
            raise RuntimeError('Could not create '+label)
        world()
        actor.set_actor_label(label)
        actor.set_folder_path('OpenWorld/'+folder)
        actor.set_editor_property('tags', [unreal.Name(TAG)])
        try:
            actor.set_editor_property('is_spatially_loaded', spatial)
        except Exception as error:
            self.report.setdefault('spatial_flag_errors',[]).append({'label':label,'error':str(error)})
        self.count += 1
        return actor

    def mesh(self, key, center, size, label, folder='Scenery', yaw=0, pitch=0,
             collision=False, material=None, spatial=True):
        bounds = self.meshes[key].get_bounds()
        native = [2*bounds.box_extent.x, 2*bounds.box_extent.y, 2*bounds.box_extent.z]
        origin = [bounds.origin.x,bounds.origin.y,bounds.origin.z]
        size = list(size)
        if key == 'beam':
            axis = max(range(3), key=lambda i:size[i])
            if axis == 2:
                size[0],size[2] = size[2],size[0]
                pitch += 90
            elif axis == 1:
                size[0],size[1] = size[1],size[0]
                yaw += 90
        scale = [size[i]/max(native[i],0.01) for i in range(3)]
        shift = rotate([origin[i]*scale[i] for i in range(3)],pitch,yaw,0)
        location = [center[i]-shift[i] for i in range(3)]
        a = self.owned(self.actors.spawn_actor_from_class(unreal.StaticMeshActor,vec(location),
                       unreal.Rotator(pitch=pitch,yaw=yaw,roll=0)),label,folder,spatial)
        c = a.static_mesh_component
        c.set_static_mesh(self.meshes[key])
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
        c.set_editor_property('generate_overlap_events',False)
        a.set_actor_scale3d(vec(scale))
        if material:
            for slot in range(max(1,c.get_num_materials())):
                c.set_material(slot,material)
        return a

    def text(self, text, at, label, size=180, yaw=-90):
        a = self.owned(self.actors.spawn_actor_from_class(unreal.TextRenderActor,vec(at),
                       unreal.Rotator(pitch=0,yaw=yaw,roll=0)),label,'Wayfinding')
        c = a.get_component_by_class(unreal.TextRenderComponent)
        c.set_text(text)
        c.set_world_size(size)
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_text_render_color(unreal.Color(242,222,160,255))
        return a

    def checkpoint(self):
        if not self.levels.save_current_level():
            raise RuntimeError('Map save failed')

    def clean(self, fresh):
        actors = list(self.actors.get_all_level_actors())
        if not fresh and not any(CHECKPOINT in map(str,a.get_editor_property('tags')) for a in actors):
            raise RuntimeError('Existing map lacks cleanup checkpoint; inspect before retry')
        decisions = []
        for a in actors:
            cls = a.get_class().get_name().lower()
            tags = set(map(str,a.get_editor_property('tags')))
            is_terrain = isinstance(a,unreal.LandscapeProxy)
            remove = TAG in tags and not is_terrain
            if fresh:
                fixture = isinstance(a,(unreal.StaticMeshActor,unreal.PlayerStart,unreal.Pawn))
                environment = any(s in cls+' '+a.get_actor_label().lower() for s in ['sky','light','atmosphere','fog','cloud'])
                gameplay = any(s in cls for s in ['monster','spawner','runportal','wobbletarget','jumppad','rewardchest','heavylootbag'])
                remove = remove or (fixture and not environment) or gameplay
            decisions.append({'label':a.get_actor_label(),'class':cls,'removed':remove})
            if remove and not self.actors.destroy_actor(a):
                raise RuntimeError('Template fixture cleanup failed')
        self.report['cleanup'] = decisions
        marker = self.mesh('cube',(0,0,-10000),(1,1,1),'OW_TemplateCleanupCheckpoint',spatial=False)
        marker.set_actor_hidden_in_game(True)
        marker.set_editor_property('tags',[unreal.Name(TAG),unreal.Name(CHECKPOINT)])
        self.checkpoint()

    def terrain(self):
        existing = [a for a in self.actors.get_all_level_actors() if isinstance(a,unreal.Landscape)]
        if existing:
            result = json.loads(unreal.Rogue10mOpenWorldAuthoring.inspect_open_world_terrain())
        else:
            result = json.loads(unreal.Rogue10mOpenWorldAuthoring.create_landscape_from_heightmap(
                str(EVIDENCE/'terrain-height.r16'),asset(PACK+'/Terrain/Material_Landscape/MI_Terrain'),
                asset(PACK+'/Terrain/TerrainLayerWeights/AutoLayer_LayerInfo')))
        self.report['terrain'] = result
        if not result.get('success'):
            raise RuntimeError('Native Landscape authoring: '+json.dumps(result))
        self.checkpoint()

    def paths_and_river(self):
        index = 0
        for route in layout.ROAD_ROUTES.values():
            for a,b in zip(route,route[1:]):
                dx,dy = b[0]-a[0],b[1]-a[1]
                length = math.hypot(dx,dy)
                count = math.ceil(length/600)
                for i in range(count):
                    t=(i+0.5)/count
                    x,y=a[0]+t*dx,a[1]+t*dy
                    if abs(y-layout.RIVER_CENTER_Y) < 8300:
                        continue
                    z=layout.height_at(x,y)
                    z0=layout.height_at(x-dx/length*250,y-dy/length*250)
                    z1=layout.height_at(x+dx/length*250,y+dy/length*250)
                    self.mesh('cube',(x,y,z+7),(length/count+20,1200,8),'OW_Path_%04d'%index,
                              'Roads',yaw=math.degrees(math.atan2(dy,dx)),
                              pitch=math.degrees(math.atan2(z1-z0,500)),material=self.dirt)
                    index+=1
        self.report['road_patch_count']=index
        for i in range(15):
            self.mesh('water',(-70000+i*10000,5000,2600),(10050,6000,1),
                      'OW_River_%02d'%i,'River',material=self.water)
        for bridge in layout.BRIDGES:
            x,y,z=bridge['x'],bridge['y'],bridge['deck_z']
            # Continuous collision deck, with referenced wooden cladding above.
            self.mesh('cube',(x,y,z-55),(1800,12000,110),'OW_Bridge_'+bridge['id']+'_Deck',
                      'Bridges',collision=True,material=self.dirt)
            for j in range(24):
                self.mesh('floor',(x,y-5750+j*500,z-9),(1800,510,18),
                          'OW_Bridge_%s_Plank_%02d'%(bridge['id'],j),'Bridges')
            for side in (-1,1):
                for j in range(9):
                    yy=y-6000+j*1500
                    self.mesh('beam',(x+side*840,yy,z+125),(45,45,250),
                              'OW_Bridge_%s_Post_%d_%d'%(bridge['id'],side,j),'Bridges')
                self.mesh('beam',(x+side*840,y,z+150),(35,12000,35),
                          'OW_Bridge_%s_Rail_%d'%(bridge['id'],side),'Bridges')
                # Native cube X longitudinal, rotated onto Y. Top rises toward deck.
                ramp_y=y+side*7200
                self.mesh('cube',(x,ramp_y,3240),(2400,1800,20),
                          'OW_Bridge_%s_Ramp_%d'%(bridge['id'],side),'Bridges',yaw=90,
                          pitch=-side*math.degrees(math.atan2(100,2400)),collision=True,material=self.dirt)

    def house(self,x,y,z,index):
        for sx in (-1,1):
            self.mesh('plaster',(x+sx*650,y,z+500),(1300,55,1000),
                      'OW_Camp_House%d_Side%d'%(index,sx),'Camp',yaw=90,collision=True)
        for sy in (-1,1):
            self.mesh('plaster',(x,y+sy*650,z+500),(1300,55,1000),
                      'OW_Camp_House%d_End%d'%(index,sy),'Camp',collision=True)
        for side in (-1,1):
            self.mesh('roof',(x+side*355,y,z+1190),(830,1500,45),
                      'OW_Camp_House%d_Roof%d'%(index,side),'Camp',pitch=-side*26)
        for sx in (-1,1):
            for sy in (-1,1):
                self.mesh('beam',(x+sx*665,y+sy*665,z+515),(50,50,1050),
                          'OW_Camp_House%d_Frame%d%d'%(index,sx,sy),'Camp')

    def scenery(self):
        c=layout.POIS['camp']
        for i,(dx,dy) in enumerate([(-3500,500),(3500,500),(-3500,4000)]):
            self.house(c['x']+dx,c['y']+dy,c['grade_z'],i)
        for i in range(12):
            x=c['x']-2200+(i%4)*260
            y=c['y']+2200+(i//4)*220
            self.mesh('crate' if i%2 else 'barrel',(x,y,2900),(180,180,200),
                      'OW_Camp_Supplies%d'%i,'Camp')
        self.text('OPEN WORLD\nNorthern Ruins 1.36 km',(0,-63000,3400),'OW_Camp_Direction',140)
        for key,p in layout.POIS.items():
            z=p['placement_z']
            probe=self.mesh('cube',(p['x'],p['y'],z+30),(5,5,5),'OW_StreamProbe_'+key,'StreamingProbes')
            probe.set_actor_hidden_in_game(True)
            probe.set_editor_property('tags',[unreal.Name(TAG),unreal.Name(TAG+'.StreamProbe.'+key)])
            if key not in ('camp','crossroads'):
                self.text(p['name'],(p['x'],p['y']-1500,z+650),'OW_Sign_'+key,210)
        self.text('WEST: FOREST & HIGHLANDS\nNORTH: BOSS RUINS\nEAST: OLD MILL',
                  (0,14500,3950),'OW_Crossroads_Direction',160)
        mill=layout.POIS['mill']
        bp=asset(PACK+'/BP_Assets/BP_WindMill')
        a=self.owned(self.actors.spawn_actor_from_class(bp.generated_class(),
            vec((mill['x']+2500,mill['y']+1800,mill['grade_z'])),unreal.Rotator()),'OW_OldWindmill','Mill')
        a.set_actor_scale3d(vec((1.5,1.5,1.5)))
        # Northern ruin enclosure has a 36 m open southern entry.
        boss=layout.POIS['boss']
        x,y,z=boss['x'],boss['y'],boss['grade_z']
        for side in (-1,1):
            for j in range(9):
                self.mesh('wall',(x+side*5700,y-5000+j*1300,z+700),(1250,180,1400),
                          'OW_Ruin_Side%d_%d'%(side,j),'Ruins',yaw=90,collision=True)
            for j in range(4):
                self.mesh('wall',(x+side*(2400+j*1100),y-5600,z+500),(1050,180,1000),
                          'OW_Ruin_Entry%d_%d'%(side,j),'Ruins',collision=True)
        for j in range(9):
            self.mesh('wall',(x-5200+j*1300,y+5600,z+1100),(1250,250,2200),
                      'OW_Ruin_North%d'%j,'Ruins',collision=True)
        for j in range(12):
            angle=2*math.pi*j/12
            self.mesh('pillar',(x+4300*math.cos(angle),y+4300*math.sin(angle),z+900),
                      (240,240,1800),'OW_Ruin_Pillar%d'%j,'Ruins',collision=True)
        # Distinct 55 m tower silhouette, reachable arena remains ground level.
        for j in range(6):
            width=1600-j*120
            self.mesh('wall',(x+3800,y+3800,z+450+j*850),(width,650,880),
                      'OW_Landmark_Tower%d'%j,'Ruins',collision=True)
        self.mesh('arch',(x,y-5600,z+1400),(3600,180,1250),'OW_Ruin_EntranceArch','Ruins')
        for key,p in layout.POIS.items():
            if key not in ('forest','highlands','mill'):
                continue
            for i in range(18):
                angle=self.rng.uniform(0,2*math.pi)
                distance=self.rng.uniform(p['clearance_radius']+900,p['clearance_radius']+4000)
                xx,yy=p['x']+distance*math.cos(angle),p['y']+distance*math.sin(angle)
                if layout.distance_to_roads(xx,yy)<2300 or abs(yy-5000)<6500:
                    continue
                height=self.rng.uniform(600,1800)
                self.mesh('rock',(xx,yy,layout.height_at(xx,yy)+height*.4),
                          (height*1.2,height,height),'OW_%s_Boulder%d'%(key,i),'Rocks',
                          yaw=self.rng.uniform(0,360),collision=True)

    def monster_class(self,name,data_path):
        parent=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Monster/BP_BaseMonster')
        if not parent:
            raise RuntimeError('Missing monster parent Blueprint class')
        path=OWN+'/Encounters/'+name
        bp=unreal.load_asset(path)
        if not bp:
            factory=unreal.BlueprintFactory()
            factory.set_editor_property('parent_class',parent)
            bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,OWN+'/Encounters',unreal.Blueprint,factory)
        if not bp:
            raise RuntimeError('Cannot create encounter blueprint '+name)
        data=asset(data_path)
        unreal.get_default_object(bp.generated_class()).set_editor_property('monster_data',data)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not unreal.EditorAssetLibrary.save_loaded_asset(bp):
            raise RuntimeError('Encounter blueprint save failed')
        self.report.setdefault('owned_blueprints',[]).append(path)
        return bp.generated_class()

    def gameplay(self):
        settings=world().get_world_settings()
        settings.set_editor_property('default_game_mode',unreal.EditorAssetLibrary.load_blueprint_class(
            '/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode'))
        nav=[a for a in self.actors.get_all_level_actors() if isinstance(a,unreal.NavMeshBoundsVolume)]
        if not nav:
            raise RuntimeError('Template Navigation bounds missing')
        for a in nav:
            a.set_actor_location(vec((0,0,8000)),False,False)
            a.set_actor_scale3d(vec((1,1,1)))
            _,extent=a.get_actor_bounds(False)
            a.set_actor_scale3d(vec((90000/max(extent.x,1),90000/max(extent.y,1),9000/max(extent.z,1))))
            a.set_actor_label('OW_NavigationBounds')
            a.set_editor_property('is_spatially_loaded',False)
        result=json.loads(unreal.Rogue10mOpenWorldBootstrap.configure_open_world_navigation())
        self.report['navigation']=result
        if not result.get('success'):
            raise RuntimeError('Map navigation configuration failed: '+json.dumps(result))
        bootstrap=self.owned(self.actors.spawn_actor_from_class(unreal.Rogue10mOpenWorldBootstrap,
            vec((0,-65000,2800)),unreal.Rotator()),'OW_RuntimeBootstrap','Gameplay',False)
        start=self.owned(self.actors.spawn_actor_from_class(unreal.PlayerStart,vec((0,-65000,2950)),
            unreal.Rotator(pitch=0,yaw=90,roll=0)),'OW_PlayerStart','Gameplay',False)
        defs=[('forest','VerdantSlime','Normal',3),('forest','Sporeling','Normal',3),
              ('mill','CrimsonSlime','Normal',4),('highlands','IronShell','Normal',4),
              ('highlands','ThornWarden','MidBoss',1)]
        spawner_class=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Monster/BP_MonsterSpawner')
        if not spawner_class:
            raise RuntimeError('Missing spawner Blueprint class')
        encounters=[]
        for i,(key,kind,rank,count) in enumerate(defs):
            p=layout.POIS[key]
            cls=self.monster_class('BP_OW_'+kind,'/Game/DataAsset/Monster/%s/DA_Monster_%s_%s'%(rank,rank,kind))
            x=p['x']+(-1 if i%2 else 1)*2200
            y=p['y']+2000
            a=self.owned(self.actors.spawn_actor_from_class(spawner_class,
                vec((x,y,layout.height_at(x,y)+120)),unreal.Rotator()),'OW_Encounter_%d_%s'%(i,kind),'Encounters')
            a.set_editor_property('monster_class',cls)
            a.set_editor_property('max_alive_monsters',count)
            a.set_editor_property('spawn_interval_seconds',12)
            a.set_editor_property('maintain_population',True)
            a.set_editor_property('ground_trace_distance',2500)
            area=a.get_editor_property('spawn_area')
            area.set_box_extent(vec((1500,1500,100)),False)
            encounters.append({'poi':key,'kind':kind,'count':count,'location':[x,y,layout.height_at(x,y)],'spatial':True})
        p=layout.POIS['boss']
        cls=self.monster_class('BP_OW_FinalBoss','/Game/DataAsset/Monster/FinalBoss/DA_Monster_FinalBoss_AbyssOverlordAzathor')
        boss=self.owned(self.actors.spawn_actor_from_class(cls,vec((p['x'],p['y']+1800,p['grade_z']+180)),
            unreal.Rotator(pitch=0,yaw=-90,roll=0)),'OW_NorthernBoss','Encounters',False)
        self.report['encounters']=encounters
        self.report['safe_camp_radius_cm']=8000
        self.report['boss_limit']='Existing monster combat/data only; new boss patterns and run victory are not part of map authoring.'
        # Invisible always-loaded barriers around the terrain prevent falling off its outer edge.
        for side in (-1,1):
            for axis in (0,1):
                center=[0,0,11000]
                center[axis]=side*89500
                size=[180000,180000,40000]
                size[axis]=200
                a=self.mesh('cube',center,size,'OW_Boundary_%d_%d'%(axis,side),'Gameplay',collision=True,spatial=False)
                a.set_actor_hidden_in_game(True)

def main():
    if Path(unreal.Paths.project_dir()).resolve()!=ROOT.resolve():
        raise RuntimeError('Unexpected project')
    report={'status':'started','map':MAP,'template':TEMPLATE,'extent_m':[1800,1800],
            'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    try:
        b=Builder(report)
        if b.levels.is_in_play_in_editor():
            raise RuntimeError('PIE must be idle')
        fresh=not unreal.EditorAssetLibrary.does_asset_exist(MAP)
        if fresh:
            if not b.levels.new_level_from_template(MAP,TEMPLATE):
                raise RuntimeError('Map template duplication failed')
        elif not b.levels.load_level(MAP):
            raise RuntimeError('Map load failed')
        world()
        region=json.loads(unreal.Rogue10mOpenWorldAuthoring.load_open_world_editor_region())
        report['editor_region']=region
        if not region.get('editor_region_loaded'):
            raise RuntimeError('Editor region was not loaded: '+json.dumps(region))
        b.clean(fresh)
        b.terrain()
        b.paths_and_river()
        b.scenery()
        b.gameplay()
        b.checkpoint()
        report.update(status='created',created_actor_count=b.count,pois=layout.POIS,bridges=layout.BRIDGES,
                      vegetation_pass_pending=True)
        unreal.log('[OpenWorld] Saved actual terrain, roads, scenery and encounters: '+MAP)
    except Exception as error:
        report.update(status='failed',error=str(error),traceback=traceback.format_exc())
        raise
    finally:
        (EVIDENCE/'creation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':
    main()
