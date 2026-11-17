"""Guarded first environment pass for the isolated 2026-11-12 WP candidate.

Import is inert: no Unreal import, editor calls, asset saves or file writes.
The editor owner calls preflight(), then author_scene(allow_editor_mutation=True,
editor_handoff='...', game_mode_verified=True, ...).  Native Landscape creation,
StartHub edits, functional NPC/quest/skill/AI work and runtime validation belong
to other packets.  Markers here are editor-only planning targets, not gameplay.

No old creation/cleanup script is imported.  No source pack/material is edited.
An existing scene tag rejects reruns; failed partial scenes require inspection.
"""
from __future__ import annotations

import datetime
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import random
import re


PROJECT = Path(__file__).resolve().parents[2]
MAP = '/Game/Rogue10m/Maps/Production/WorldLayout20261112'
OWN = '/Game/Rogue10m/World/Production/WorldLayout20261112'
TAG = 'Rogue10m.WorldLayout20261112'
SCENE_TAG = TAG + '.Scene'
PACK = '/Game/Stylized_Village/Assets'
TERRAIN_MODULE = PROJECT / 'Scripts/WorldLayoutTerrain20261112.py'
WORLD_SPEC = PROJECT / 'Docs/References/WorldMap/world-layout-20261112.json'
ACQUISITION_SPEC = PROJECT / 'Docs/References/WorldMap/acquisition-catalog-20261112.json'
BOSS_BP = '/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_FinalBoss'
BOSS_DATA = '/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_AbyssOverlordAzathor'
BOSS_MESH = '/Game/Polyart/ModularStylizedChars/Meshes/SkeletalMeshes/Characters/SK_Garret_FullBody'
BOSS_PROFILE = '/Game/Rogue10m/Characters/Boss/AzathorHuman20261110/DA_HumanBoss_AttackProfile'
SEED = 20261112
ROAD_HALF_WIDTH_CM = 400.0
ROAD_VISUAL_WIDTH_CM = 760.0
MAX_ACTORS = 2600

MESHES = {
    'cube': '/Engine/BasicShapes/Cube',
    'cylinder': '/Engine/BasicShapes/Cylinder',
    'sphere': '/Engine/BasicShapes/Sphere',
    'plaster': PACK + '/Architecture/Plaster/SM_Plaster_FullXFull_Plaster_FullXFull',
    'beam': PACK + '/Architecture/Wood/Pieces/SM_Beam_Long_1',
    'gable': PACK + '/Architecture/Wood/Roof/SM_Wood_Wall_RoofFront_Large_LP',
    'roof': PACK + '/Architecture/RoofTiles/Round/SM_RoofTiles_Round_FullXFull',
    'door': PACK + '/Architecture/Doors/SM_Door_01',
    'window': PACK + '/Architecture/Windows/SM_Window_FrenchPanes_01_LP',
    'chimney': PACK + '/Architecture/Plaster/SM_Plaster_Chimney_Thick',
    'stone_tile': PACK + '/Architecture/StoneFloor/SM_StoneFloor_01_Full',
    'wood_floor': PACK + '/Architecture/Wood/Floors/SM_Wood_Floor_FullxFull_LP',
    'stone_wall': PACK + '/Architecture/Flagstone/Walls/SM_Flagstone_FullXFull',
    'pillar': PACK + '/Architecture/Flagstone/Structure/SM_FlagStone_Pillar_Full',
    'arch': PACK + '/Architecture/Flagstone/Trims/SM_FlagStone_Trim_Arch',
    'steps': PACK + '/Architecture/Flagstone/Structure/SM_FlagStone_Steps_01',
    'windmill': PACK + '/Proxy/SM_PROXY_WindMill',
    'tarp': PACK + '/Props/Tarps/SM_Tarp_BigSquare',
    'barrel': PACK + '/Props/Furniture/SM_SM_Barrel_01',
    'crate': PACK + '/Props/Furniture/SM_Crate_01',
    'lantern': PACK + '/Props/Lanterns/SM_Lantern_Box',
    'pot': PACK + '/Props/Pots/SM_ClayPot_WideSmall',
    'tree_medium': PACK + '/Foliage/Trees/Tree_Medium_Green',
    'tree_large': PACK + '/Foliage/Trees/Tree_Large_Green',
    'bush': PACK + '/Foliage/Bushes/SM_Bush_01',
    'rock_medium': PACK + '/Stones/Boulder/SM_Boulder_Medium_01',
    'rock_large': PACK + '/Stones/Boulder/SM_Boulder_Large_01',
}
MATERIALS = {
    'grass': '/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_Grass',
    'dirt': '/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath',
    'stone': '/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_Cobblestone',
    'warm_stone': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_WarmStone',
    'wood': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_DarkTimber',
    'teal': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_Banner_Teal',
    'ochre': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_Banner_Ochre',
    'blue': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_Banner_Blue',
    'burgundy': '/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_Banner_Burgundy',
}


def _load_terrain():
    spec = importlib.util.spec_from_file_location('world_layout_terrain_scene_20261112', TERRAIN_MODULE)
    if not spec or not spec.loader:
        raise RuntimeError('Pure terrain module is unavailable: ' + str(TERRAIN_MODULE))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for name in ('height_at', 'nodes_world', 'roadsegments'):
        if not callable(getattr(module, name, None)):
            raise RuntimeError('Terrain module API missing: ' + name)
    return module


def _read_specs():
    return (json.loads(WORLD_SPEC.read_text(encoding='utf-8-sig')),
            json.loads(ACQUISITION_SPEC.read_text(encoding='utf-8-sig')))


def _distance_to_segment(x, y, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    den = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((x - a[0]) * dx + (y - a[1]) * dy) / den)) if den else 0.0
    return math.hypot(x - a[0] - t * dx, y - a[1] - t * dy)


def _rotate(point, pitch=0.0, yaw=0.0, roll=0.0):
    # UE FRotator basis; also compensates noncentral imported mesh pivots.
    p, y, r = map(math.radians, (pitch, yaw, roll))
    cp, sp, cy, sy, cr, sr = math.cos(p), math.sin(p), math.cos(y), math.sin(y), math.cos(r), math.sin(r)
    x, yy, z = point
    return (cp * cy * x + (sr * sp * cy - cr * sy) * yy + (-cr * sp * cy - sr * sy) * z,
            cp * sy * x + (sr * sp * sy + cr * cy) * yy + (-cr * sp * sy + sr * cy) * z,
            sp * x - sr * cp * yy + cr * cp * z)


class _Plan:
    """Pure deterministic geometry plan; no Unreal objects or filesystem writes."""

    def __init__(self, terrain, world_spec, acquisition):
        self.terrain, self.spec, self.acquisition = terrain, world_spec, acquisition
        self.nodes = terrain.nodes_world()
        self.roads = tuple(terrain.roadsegments())
        if len(self.roads) != 29 or not {'S', 'B', 'H', 'M', 'T', 'Q', 'G', 'O', 'F', 'P', 'W', 'A', 'R', 'Z'} <= set(self.nodes):
            raise RuntimeError('Unexpected plan node/edge schema')
        for key, expected in (('S', (15000.0, 32550.0)), ('B', (15000.0, 62550.0))):
            node = self.nodes[key]
            if math.hypot(node['x'] - expected[0], node['y'] - expected[1]) > 0.01:
                raise RuntimeError('Terrain anchor contract drift: ' + key)
        if abs(terrain.height_at(15000.0, 62550.0) - 9600.0) > 0.1:
            raise RuntimeError('Boss entrance grade must remain Z9600')
        self.items, self.sites, self.site_records = [], [], []
        self.labels = set()
        self.rng = random.Random(SEED)
        self.marker_records = []
        self.hunting_centers = {}

    def height(self, x, y):
        result = float(self.terrain.height_at(float(x), float(y)))
        if not math.isfinite(result):
            raise RuntimeError('Nonfinite terrain sample')
        return result

    def road_distance(self, x, y):
        return min(_distance_to_segment(x, y, edge['a'], edge['b']) for edge in self.roads)

    def clear(self, x, y, radius, *, exclude_centers=True):
        if not (-88000 < x - radius and x + radius < 88000 and -88000 < y - radius and y + radius < 88000):
            return False
        if self.road_distance(x, y) < ROAD_HALF_WIDTH_CM + radius + 40:
            return False
        if exclude_centers and any(math.hypot(x - n['x'], y - n['y']) < self.core_radius(n) + radius for n in self.nodes.values()):
            return False
        if any(math.hypot(x - xx, y - yy) < radius + rr + 120 for xx, yy, rr in self.sites):
            return False
        # Preserve the explicit 110m boss floor/arena and southern entrance.
        if abs(x - 15000) < 5600 + radius and abs(y - 68000) < 5600 + radius:
            return False
        return True

    @staticmethod
    def core_radius(node):
        return 450.0 if node['kind'] == 'outpost' else 900.0

    def site(self, node_id, preferred, radius, name, *, east_only=False):
        node = self.nodes[node_id]
        length = max(1600.0, math.hypot(*preferred))
        base = math.atan2(preferred[1], preferred[0])
        pad_radius = {'H': 3500, 'M': 3500, 'T': 3400, 'Q': 1800, 'G': 1800,
                      'O': 1800, 'F': 1800, 'P': 1800}.get(node_id)
        # Architecture is kept inside the frozen terrain's genuinely flat pad;
        # combat/nature reservations retain their deliberately exterior offsets.
        architecture = not name.endswith('_dressed_reservation')
        if pad_radius and architecture:
            maximum = pad_radius - radius - 65
            minimum = self.core_radius(node) + radius + 20
            lengths = []
            distance = maximum
            while distance >= minimum:
                lengths.append(distance)
                distance -= 90
            if maximum < minimum:
                raise RuntimeError('Assembly exceeds available flat pad: ' + name)
        else:
            lengths = [length * scale for scale in (1.0, 1.2, 1.45, 1.7)]
        angles = (0, 30, -30, 60, -60, 90, -90, 120, -120, 150, -150, 180,
                  15, -15, 45, -45, 75, -75, 105, -105, 135, -135, 165, -165)
        for distance in lengths:
            for delta in angles:
                angle = base + math.radians(delta)
                x, y = node['x'] + distance * math.cos(angle), node['y'] + distance * math.sin(angle)
                # T's whole 34m pad is already east of the central axis. Do not
                # confuse that contract with forbidding the pad's western half.
                if east_only and x < self.nodes['S']['x'] + 1200:
                    continue
                if self.clear(x, y, radius):
                    self.sites.append((x, y, radius))
                    row = {'name': name, 'node': node_id, 'center': [x, y, self.height(x, y)], 'radius_cm': radius,
                           'inside_flat_pad': bool(pad_radius and architecture), 'flat_pad_radius_cm': pad_radius}
                    self.site_records.append(row)
                    return x, y, row['center'][2]
        raise RuntimeError('No road-clear assembly site for ' + name)

    def add(self, row):
        label = row['label']
        if label in self.labels:
            raise RuntimeError('Duplicate planned label: ' + label)
        self.labels.add(label)
        self.items.append(row)
        if len(self.items) > MAX_ACTORS:
            raise RuntimeError('Scene actor budget exceeded before editor mutation')

    def mesh(self, key, center, size, label, folder, *, yaw=0.0, pitch=0.0, roll=0.0,
             collision=False, material=None, align_width=False, persistent=False, hidden=False):
        if key not in MESHES or material and material not in MATERIALS:
            raise RuntimeError('Unlisted mesh/material')
        values = list(center) + list(size) + [yaw, pitch, roll]
        if not all(math.isfinite(float(v)) for v in values) or min(size) <= 0:
            raise RuntimeError('Invalid planned transform')
        self.add({'kind': 'mesh', 'mesh': key, 'center': list(center), 'size': list(size), 'label': label,
                  'folder': folder, 'yaw': yaw, 'pitch': pitch, 'roll': roll, 'collision': collision,
                  'material': material, 'align_width': align_width, 'persistent': persistent, 'hidden': hidden})

    def marker(self, marker_kind, marker_id, name, node_id, index, detail=None):
        node = self.nodes[node_id]
        origin = self.hunting_centers.get(node_id) if marker_kind == 'AI' else None
        origin_x, origin_y = (origin[0], origin[1]) if origin else (node['x'], node['y'])
        # The artifact specifies relative subsites. These are editor-only first
        # placement suggestions, explicitly not functional pickups/spawners.
        for step in range(40):
            angle = (index * 137.507764 + step * 47.0) * math.pi / 180
            radius = 1250 + 100 * step
            x, y = origin_x + radius * math.cos(angle), origin_y + radius * math.sin(angle)
            if self.road_distance(x, y) >= 550:
                break
        else:
            raise RuntimeError('No marker offset outside road: ' + marker_id)
        label = 'WL26_PENDING_' + marker_kind + '_' + re.sub('[^A-Za-z0-9_]+', '_', marker_id)
        row = {'kind': 'marker', 'marker_kind': marker_kind, 'marker_id': marker_id, 'name': name,
               'label': label, 'folder': 'Pending/' + marker_kind, 'center': [x, y, self.height(x, y) + 110],
               'node': node_id, 'detail': detail, 'functional': False, 'placement_status': 'approximate_editor_marker_only'}
        self.add(row)
        self.marker_records.append(row.copy())

    def roads_visual(self):
        for edge in self.roads:
            a, b = edge['a'], edge['b']
            dx, dy = b[0] - a[0], b[1] - a[1]
            length = math.hypot(dx, dy)
            count = max(1, math.ceil(length / 500.0))
            yaw = math.degrees(math.atan2(dy, dx))
            for i in range(count):
                t0, t1 = i / count, (i + 1) / count
                x0, y0 = a[0] + t0 * dx, a[1] + t0 * dy
                x1, y1 = a[0] + t1 * dx, a[1] + t1 * dy
                z0, z1 = self.height(x0, y0), self.height(x1, y1)
                self.mesh('cube', ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2 + 2.5),
                          (math.hypot(length / count, z1 - z0) + 4, ROAD_VISUAL_WIDTH_CM, 2),
                          'WL26_Road_%s_%03d' % (edge['id'], i), 'Roads', yaw=yaw,
                          pitch=math.degrees(math.atan2(z1 - z0, length / count)), material='dirt')

    def house(self, name, site, *, width=760, depth=700, height=470, yaw=0.0, stone=False):
        x, y, z = site
        folder = 'Architecture/' + name
        def part(key, offset, size, suffix, **kwargs):
            xx, yy, zz = _rotate(offset, yaw=yaw)
            self.mesh(key, (x + xx, y + yy, z + zz), size, name + '_' + suffix, folder,
                      yaw=yaw + kwargs.pop('extra_yaw', 0), **kwargs)
        part('cube', (0, 0, 4), (width + 75, depth + 75, 8), 'Foundation', collision=True, material='stone')
        wall = 'stone_wall' if stone else 'plaster'
        for side in (-1, 1):
            part(wall, (0, side * depth / 2, height / 2 + 8), (width, 30, height), 'WallY%d' % side, collision=True, align_width=True)
            part(wall, (side * width / 2, 0, height / 2 + 8), (depth, 30, height), 'WallX%d' % side,
                 collision=True, align_width=True, extra_yaw=90)
            for xx in (-width / 2, 0, width / 2):
                part('beam', (xx, side * (depth / 2 + 20), height / 2 + 8), (25, 30, height + 16), 'Post%d_%d' % (side, xx))
            part('beam', (0, side * (depth / 2 + 20), height - 2), (width + 65, 30, 26), 'Top%d' % side)
            part('beam', (0, side * (depth / 2 + 20), 42), (width + 65, 30, 26), 'Bottom%d' % side)
        rise = (depth / 2 + 55) * math.tan(math.radians(32))
        slope = (depth / 2 + 65) / math.cos(math.radians(32))
        for side in (-1, 1):
            part('roof', (0, side * (depth / 4 + 28), height + 8 + rise / 2),
                 (width + 120, slope, 24), 'Roof%d' % side, roll=side * 32)
            part('gable', (side * (width / 2 + 8), 0, height + 8 + rise / 2),
                 (depth + 15, 24, rise), 'Gable%d' % side, extra_yaw=90, align_width=True)
        part('beam', (0, 0, height + 8 + rise), (width + 140, 30, 30), 'Ridge')
        part('door', (0, -depth / 2 - 23, 122), (125, 26, 228), 'Door', align_width=True)
        for side in (-1, 1):
            part('window', (side * width * .29, -depth / 2 - 27, height * .60),
                 (70, 12, 145), 'Window%d' % side, align_width=True)
        part('chimney', (width * .25, depth * .22, height + rise * .65), (90, 90, 270), 'Chimney')
        part('stone_tile', (0, -depth / 2 - 95, 15), (200, 135, 20), 'Doorstep', collision=True)
        part('lantern', (90, -depth / 2 - 60, 255), (36, 36, 65), 'Lantern')
        for i, (key, xx) in enumerate((('barrel', width / 2 + 90), ('crate', -width / 2 - 90))):
            part(key, (xx, -depth / 2 + 110, 65), (105, 105, 130), 'Supply%d' % i)
        # Exterior only: the door is scenery, not a claimed enterable interior.

    def props(self, name, site, count=8, radius=350):
        x, y, _ = site
        for i in range(count):
            angle = self.rng.uniform(0, math.tau)
            distance = self.rng.uniform(min(100, radius * .35), radius)
            xx, yy = x + distance * math.cos(angle), y + distance * math.sin(angle)
            key = ('barrel', 'crate', 'pot')[i % 3]
            height = (110, 90, 65)[i % 3]
            self.mesh(key, (xx, yy, self.height(xx, yy) + height / 2), (90, 90, height),
                      name + '_Supply%02d' % i, 'Props/' + name, yaw=self.rng.uniform(-180, 180))

    def pavilion(self, name, site, *, stone=False, size=1000, height=650, color='teal'):
        x, y, z = site
        folder = 'Architecture/' + name
        self.mesh('cube', (x, y, z + 4), (size + 80, size + 80, 8), name + '_Floor', folder, collision=True, material='stone')
        for ix in (-1, 1):
            for iy in (-1, 1):
                self.mesh('pillar' if stone else 'beam', (x + ix * size / 2, y + iy * size / 2, z + height / 2),
                          (65, 65, height), name + '_Post%d%d' % (ix, iy), folder, collision=True)
        for side in (-1, 1):
            self.mesh('roof', (x, y + side * size / 4, z + height + 130),
                      (size + 190, size * .65, 26), name + '_Roof%d' % side, folder, roll=side * 27)
        self.mesh('cube', (x + size / 2 + 10, y, z + height * .7), (8, 180, 260), name + '_Banner', folder, material=color)
        self.props(name, (x, y, z), 5, radius=min(350, size * .3))

    def watchtower(self, name, site):
        x, y, z = site
        folder = 'Architecture/' + name
        for ix in (-1, 1):
            for iy in (-1, 1):
                self.mesh('beam', (x + ix * 230, y + iy * 230, z + 650), (60, 60, 1300),
                          name + '_Post%d%d' % (ix, iy), folder, collision=True)
        self.mesh('wood_floor', (x, y, z + 970), (620, 620, 25), name + '_Platform', folder, collision=True)
        for side in (-1, 1):
            self.mesh('roof', (x, y + side * 175, z + 1410), (850, 470, 24), name + '_Roof%d' % side, folder, roll=side * 29)
            self.mesh('beam', (x, y + side * 290, z + 1130), (620, 30, 30), name + '_Rail%d' % side, folder)
        # Decorative lookout only; no ladder/accessibility claim.

    def stone_tower(self, name, site):
        x, y, z = site
        folder = 'Architecture/' + name
        self.mesh('stone_tile', (x, y, z + 10), (1450, 1450, 20), name + '_Base', folder, collision=True)
        for level in range(3):
            width, h = 1200 - level * 180, 560
            zz = z + 20 + level * h
            for side in (-1, 1):
                self.mesh('stone_wall', (x, y + side * width / 2, zz + h / 2), (width, 90, h),
                          name + '_WallY%d_%d' % (level, side), folder, collision=True, align_width=True)
                self.mesh('stone_wall', (x + side * width / 2, y, zz + h / 2), (width, 90, h),
                          name + '_WallX%d_%d' % (level, side), folder, yaw=90, collision=True, align_width=True)
            self.mesh('stone_tile', (x, y, zz + h + 10), (width + 140, width + 140, 35),
                      name + '_Cornice%d' % level, folder)
            for ix in (-1, 1):
                for iy in (-1, 1):
                    self.mesh('pillar', (x + ix * width / 2, y + iy * width / 2, zz + h / 2),
                              (90, 90, h + 30), name + '_Pillar%d_%d%d' % (level, ix, iy), folder)
        for side in (-1, 1):
            self.mesh('roof', (x, y + side * 215, z + 1890), (1100, 600, 30), name + '_Roof%d' % side, folder, roll=side * 38)
        self.mesh('arch', (x, y - 665, z + 340), (500, 70, 580), name + '_DoorFrame', folder)
        self.mesh('door', (x, y - 690, z + 160), (220, 28, 300), name + '_ExteriorDoor', folder, align_width=True)

    def hub_scenes(self):
        for i, preferred in enumerate(((-2500, -2200), (2500, -2300), (-2600, 2300))):
            hall = i == 0
            self.house('WL26_H_HunterHall' if hall else 'WL26_H_HunterHouse%02d' % i,
                       self.site('H', preferred, 900 if hall else 650, 'H_house%d' % i),
                       width=920 if hall else 660, depth=820 if hall else 600,
                       height=620 if hall else 430, yaw=i * 72)
        # Frozen 35m pad: reserve the defining landmark before small commerce
        # modules, instead of pushing large exterior houses onto sloping land.
        wind = self.site('M', (4200, 1000), 900, 'M_windmill')
        self.mesh('windmill', (wind[0], wind[1], wind[2] + 1200), (1200, 1200, 2400), 'WL26_M_WindmillStatic', 'Architecture/M_Windmill')
        for i, preferred in enumerate(((-2500, -2300),)):
            self.house('WL26_M_MarketHouse%02d' % i, self.site('M', preferred, 650, 'M_house%d' % i), width=640, depth=600, height=430, yaw=180 if i < 2 else 0)
        for i, preferred in enumerate(((-1800, 3500), (1800, -3900), (0, 3400))):
            self.pavilion('WL26_M_Stall%d' % i, self.site('M', preferred, 450, 'M_stall%d' % i), size=470, height=330, color='ochre' if i % 2 else 'burgundy')
        self.stone_tower('WL26_T_ExteriorTower', self.site('T', (4700, 2600), 1100, 'T_exterior_tower', east_only=True))
        self.pavilion('WL26_T_OpenLibrary', self.site('T', (2900, -4100), 750, 'T_open_library', east_only=True), stone=True, size=900, height=680, color='blue')

    def outpost_scenes(self):
        for node_id in ('Q', 'G', 'O', 'F', 'P'):
            cabin = self.site(node_id, (-2100, -1700), 520, node_id + '_cabin')
            self.house('WL26_%s_OutpostCabin' % node_id, cabin, width=560, depth=500, height=380, stone=node_id in ('G', 'O'), yaw=45)
            # One compact cabin per 18m pad. Its flag/props stay in that same
            # reserved footprint instead of forcing a second large structure.
            color = {'Q': 'ochre', 'G': 'burgundy', 'O': 'blue', 'F': 'burgundy', 'P': 'teal'}[node_id]
            self.mesh('beam', (cabin[0] + 380, cabin[1] - 100, cabin[2] + 250), (24, 24, 500),
                      'WL26_%s_BannerPost' % node_id, 'Architecture/' + node_id)
            self.mesh('cube', (cabin[0] + 350, cabin[1] - 100, cabin[2] + 390), (130, 8, 170),
                      'WL26_%s_Banner' % node_id, 'Architecture/' + node_id, material=color)

    def hunting_scenes(self):
        # Quarry combat reservation deliberately sits far outside service Q.
        regions = (('W', 'Forest', (-2000, 2300)), ('A', 'Transport', (2200, -2300)),
                   ('Q', 'Quarry', (-7800, 3300)), ('R', 'Ruins', (2500, 2300)), ('Z', 'Furnace', (2800, -1800)))
        for node_id, name, offset in regions:
            center = self.site(node_id, offset, 1500, name + '_dressed_reservation')
            self.hunting_centers[node_id] = center
            x, y, z = center
            if name in ('Transport', 'Ruins', 'Furnace'):
                self.props('WL26_%s_AbandonedSupplies' % name, center, 9)
            if name == 'Transport':
                self.mesh('wood_floor', (x, y, z + 80), (400, 230, 25), 'WL26_A_OverturnedCartBed', 'Hunting/Transport', roll=22)
                for side in (-1, 1):
                    self.mesh('beam', (x, y + side * 110, z + 150), (420, 25, 35), 'WL26_A_CartRail%d' % side, 'Hunting/Transport', roll=22)
            if name == 'Ruins':
                for i in range(4):
                    px, py = x + (i % 2 * 2 - 1) * 650, y + (i // 2 * 2 - 1) * 650
                    ph = 680 if i != 3 else 340
                    self.mesh('pillar', (px, py, self.height(px, py) + ph / 2),
                              (150, 150, ph), 'WL26_R_RuinPillar%d' % i, 'Hunting/Ruins', collision=True)
                self.mesh('arch', (x, y + 650, self.height(x, y + 650) + 240), (1300, 130, 480), 'WL26_R_RuinArch', 'Hunting/Ruins')
            for i in range(12):
                angle = i / 12 * math.tau + .13
                radius = 1100 + self.rng.uniform(150, 650)
                xx, yy = x + radius * math.cos(angle), y + radius * math.sin(angle)
                if self.road_distance(xx, yy) < 900:
                    continue
                key = 'tree_medium' if name == 'Forest' and i % 3 else 'rock_medium'
                height = self.rng.uniform(550, 1100) if key.startswith('tree') else self.rng.uniform(180, 430)
                self.mesh(key, (xx, yy, self.height(xx, yy) + height / 2), (height * .65, height * .65, height),
                          'WL26_%s_RingProp%02d' % (name, i), 'Hunting/' + name, yaw=self.rng.uniform(-180, 180))

    def nature(self):
        # Sparse dressed edges; this pass does not replace the later PCG forest.
        for node_id in ('H', 'P', 'W', 'Q', 'M', 'G', 'T', 'O', 'F', 'R', 'Z'):
            node = self.nodes[node_id]
            placed = 0
            for attempt in range(110):
                if placed >= 16:
                    break
                angle, radius = self.rng.uniform(0, math.tau), self.rng.uniform(4000, 7600)
                x, y = node['x'] + radius * math.cos(angle), node['y'] + radius * math.sin(angle)
                key = ('tree_large', 'tree_medium', 'bush', 'rock_medium')[placed % 4]
                height = {'tree_large': 1700, 'tree_medium': 1050, 'bush': 210, 'rock_medium': 370}[key]
                if not self.clear(x, y, height * .6 / math.sqrt(2)):
                    continue
                self.mesh(key, (x, y, self.height(x, y) + height / 2), (height * .6, height * .6, height),
                          'WL26_%s_Nature%02d' % (node_id, placed), 'Nature/' + node_id, yaw=self.rng.uniform(-180, 180))
                placed += 1

    def markers(self):
        for i, species in enumerate(self.spec['ordinarySpecies']):
            self.marker('AI', species['monsterId'], species['label'], species['areaAnchorNodeId'], i,
                        {'status': 'future_behavior_and_spawner_not_implemented', 'source': species['canonicalAssetPath']})
        fallback = {'stageplan.npc.seoyul': 'M', 'stageplan.npc.soha': 'P', 'stageplan.npc.taegeon': 'Q', 'stageplan.npc.roan': 'S'}
        for i, person in enumerate(self.acquisition['npcs']):
            node = person['location']['parentMapRef'] or fallback.get(person['catalogId'])
            if node not in self.nodes:
                raise RuntimeError('Unresolved NPC marker parent: ' + person['catalogId'])
            self.marker('NPC', person['catalogId'], person['nameKorean'], node, i,
                        {'status': 'no_functional_npc_or_dialogue_spawned', 'phase': person['phase'], 'subsite': person['location']['siteKorean']})
        for i, book in enumerate(self.acquisition['books']):
            self.marker('BOOK', book['catalogId'], book['techniqueNameKorean'], book['source']['location']['parentMapRef'], i,
                        {'status': 'no_item_or_skill_or_reward_implemented', 'subsite': book['source']['location']['siteKorean']})
        for i, weapon in enumerate(self.acquisition['weapons']):
            self.marker('WEAPON', weapon['catalogId'], weapon['proposedItemNameKorean'], weapon['location']['parentMapRef'], i,
                        {'status': 'no_pickup_or_equipment_implemented', 'subsite': weapon['location']['siteKorean']})

    def build(self):
        self.roads_visual()
        self.hub_scenes()
        self.outpost_scenes()
        self.hunting_scenes()
        self.nature()
        self.markers()
        self.mesh('cube', (15000, 68000, 9550), (11000, 11000, 100), 'WL26_PersistentBossGround',
                  'Gameplay/Support', collision=True, persistent=True, hidden=True)
        return self


def build_placement_plan():
    """Pure plan inspection/testing; does not import Unreal or create artifacts."""
    world_spec, acquisition = _read_specs()
    plan = _Plan(_load_terrain(), world_spec, acquisition).build()
    return {'map': MAP, 'tag': TAG, 'scene_tag': SCENE_TAG, 'seed': SEED, 'actor_count': len(plan.items), 'placements': plan.items,
            'sites': plan.site_records, 'markers': plan.marker_records, 'road_edge_count': len(plan.roads),
            'road_clear_width_cm': ROAD_HALF_WIDTH_CM * 2, 'terrain_module': str(TERRAIN_MODULE)}


def _package_allowed(name):
    name = str(name).split('.')[0]
    tail = MAP.removeprefix('/Game/')
    return (name == MAP or name.startswith(OWN + '/')
            or name.startswith('/Game/__ExternalActors__/' + tail + '/')
            or name.startswith('/Game/__ExternalObjects__/' + tail + '/'))


def _dirty_packages(unreal):
    helper = unreal.EditorLoadingAndSavingUtils
    packages = list(helper.get_dirty_map_packages()) + list(helper.get_dirty_content_packages())
    return {p.get_path_name(): p for p in packages}


def _class(unreal, value):
    if isinstance(value, unreal.Class):
        return value
    if isinstance(value, str):
        result = unreal.load_class(None, value)
        if not result and value.startswith('/Game/'):
            result = unreal.EditorAssetLibrary.load_blueprint_class(value.split('.')[0].removesuffix('_C'))
        if not result:
            raise RuntimeError('GameMode class unavailable: ' + value)
        return result
    if isinstance(value, type) and hasattr(value, 'static_class'):
        return value.static_class()
    return value


def _prepare(*, game_mode_class=None, game_mode_verified=False, place_boss=False,
             boss_preflight_record=None, reuse_existing_player_start=False):
    import unreal
    if Path(unreal.Paths.project_dir()).resolve() != PROJECT.resolve():
        raise RuntimeError('Wrong project; no editor mutation performed')
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if levels.is_in_play_in_editor():
        raise RuntimeError('PIE must be idle')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_path_name().split('.')[0] != MAP:
        raise RuntimeError('Load the exact WP candidate first; this script never switches maps: ' + MAP)
    settings = world.get_world_settings()
    if settings.get_editor_property('world_partition') is None:
        raise RuntimeError('Candidate must be World Partition')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    current = list(actors.get_all_level_actors())
    if not any(isinstance(a, unreal.Landscape) for a in current):
        raise RuntimeError('Native Landscape must already exist; scene author does not create terrain')
    if any(SCENE_TAG in map(str, a.get_editor_property('tags')) for a in current):
        raise RuntimeError('Scene tag already exists. Inspect prior/partial pass; no cleanup or rerun is automatic')
    dirty = _dirty_packages(unreal)
    unrelated = [name for name in dirty if not _package_allowed(name)]
    if unrelated:
        raise RuntimeError('Unrelated dirty packages block scene author: ' + json.dumps(unrelated))
    mode = _class(unreal, game_mode_class) if game_mode_class is not None else settings.get_editor_property('default_game_mode')
    if not game_mode_verified or mode is None:
        raise RuntimeError('Editor owner must pass/confirm the inspected GameMode class with game_mode_verified=True')
    cdo = unreal.get_default_object(mode)
    if not isinstance(cdo, unreal.GameModeBase):
        raise RuntimeError('Supplied class is not GameModeBase')
    starts = [a for a in current if isinstance(a, unreal.PlayerStart)]
    if len(starts) > 1 or starts and not reuse_existing_player_start:
        raise RuntimeError('Existing PlayerStart requires explicit reuse_existing_player_start=True and must be unique')
    placement = build_placement_plan()
    used_meshes = {r['mesh'] for r in placement['placements'] if r['kind'] == 'mesh'}
    used_materials = {r['material'] for r in placement['placements'] if r['kind'] == 'mesh' and r['material']}
    mesh_objects, bounds = {}, {}
    for key in sorted(used_meshes):
        path = MESHES[key]
        value = unreal.load_asset(path)
        if not isinstance(value, unreal.StaticMesh):
            raise RuntimeError('Required StaticMesh missing: ' + path)
        b = value.get_bounds()
        size = [float(b.box_extent.x) * 2, float(b.box_extent.y) * 2, float(b.box_extent.z) * 2]
        origin = [float(b.origin.x), float(b.origin.y), float(b.origin.z)]
        if not all(math.isfinite(v) for v in size + origin) or min(size) <= .0001:
            raise RuntimeError('Invalid mesh bounds: ' + path)
        mesh_objects[key], bounds[key] = value, {'size': size, 'origin': origin}
    materials = {}
    for key in sorted(used_materials):
        path = MATERIALS[key]
        value = unreal.load_asset(path)
        if not isinstance(value, unreal.MaterialInterface):
            raise RuntimeError('Required existing MaterialInterface missing: ' + path)
        materials[key] = value
    boss_class = None
    if place_boss:
        record = boss_preflight_record or {}
        expected = {'blueprint': BOSS_BP, 'monster_data': BOSS_DATA, 'skeletal_mesh': BOSS_MESH, 'attack_profile': BOSS_PROFILE}
        if record.get('verified') is not True or not record.get('snapshot_id') or any(record.get(k) != v for k, v in expected.items()):
            raise RuntimeError('Explicit, fresh root boss preflight record required: verified/snapshot_id/' + '/'.join(expected))
        boss_class = unreal.EditorAssetLibrary.load_blueprint_class(BOSS_BP)
        data = unreal.load_asset(BOSS_DATA)
        if not boss_class or not data:
            raise RuntimeError('Boss Blueprint/data unavailable')
        boss_default = unreal.get_default_object(boss_class)
        if boss_default.get_editor_property('monster_data') != data:
            raise RuntimeError('Boss Blueprint CDO is not bound to inspected DA_OW data')
        for field, expected_path in (('skeletal_mesh', BOSS_MESH), ('attack_pattern_profile', BOSS_PROFILE)):
            value = data.get_editor_property(field)
            if not value or value.get_path_name().split('.')[0] != expected_path:
                raise RuntimeError('Boss human presentation readback differs: ' + field)
        if any('monster' in a.get_class().get_name().lower() for a in current):
            raise RuntimeError('Candidate already contains a monster; inspect before placing boss')
    return {'unreal': unreal, 'world': world, 'settings': settings, 'levels': levels, 'actors': actors,
            'mode': mode, 'starts': starts, 'mesh_objects': mesh_objects, 'bounds': bounds, 'materials': materials,
            'boss_class': boss_class, 'placement': placement, 'initial_dirty': list(dirty),
            'initial_actor_paths': [a.get_path_name() for a in current]}


def preflight(**kwargs):
    """Read-only editor/asset/placement inspection. Never saves or switches maps."""
    prepared = _prepare(**kwargs)
    plan = prepared['placement']
    return {'status': 'ready_for_owner_review', 'map': MAP, 'tag': TAG, 'scene_tag': SCENE_TAG, 'planned_scene_actors': plan['actor_count'],
            'planned_player_start': 'reuse' if prepared['starts'] else 'new', 'road_edges': 29,
            'marker_counts': {kind: sum(r['marker_kind'] == kind for r in plan['markers']) for kind in ('AI', 'NPC', 'BOOK', 'WEAPON')},
            'mesh_paths': {k: MESHES[k] for k in prepared['mesh_objects']},
            'material_paths': {k: MATERIALS[k] for k in prepared['materials']}, 'mesh_bounds': prepared['bounds'],
            'initial_dirty_candidate_packages': prepared['initial_dirty'], 'game_mode': prepared['mode'].get_path_name(),
            'place_boss': prepared['boss_class'] is not None, 'runtime_gameplay_validation': 'not_performed'}


def author_scene(*, allow_editor_mutation=False, editor_handoff='', run_label='SceneA', report_path=None,
                 save_scene=True, game_mode_class=None, game_mode_verified=False, place_boss=False,
                 boss_preflight_record=None, reuse_existing_player_start=False):
    """One explicit pass. On failure leave evidence/partial actors for inspection.

    place_boss defaults False. Its root record must contain verified=True,
    snapshot_id and exact blueprint/monster_data/skeletal_mesh/attack_profile.
    No mutation is retried and no broad rollback/SaveAll is performed.
    """
    if allow_editor_mutation is not True or not isinstance(editor_handoff, str) or not editor_handoff.strip():
        raise RuntimeError('Explicit mutation opt-in and editor-owner handoff are required')
    if not re.fullmatch('[A-Za-z0-9_-]{1,48}', run_label):
        raise ValueError('Use a fresh safe evidence label')
    target = Path(report_path) if report_path else PROJECT / 'Feature/doc/evidence/world-map-production-20261112' / run_label / 'scene-author.json'
    target = target.resolve()
    allowed_evidence = (PROJECT / 'Feature/doc/evidence').resolve()
    if not target.is_relative_to(allowed_evidence) or target.suffix.lower() != '.json' or target.exists():
        raise RuntimeError('Evidence must be a fresh JSON under Feature/doc/evidence')
    prepared = _prepare(game_mode_class=game_mode_class, game_mode_verified=game_mode_verified,
                        place_boss=place_boss, boss_preflight_record=boss_preflight_record,
                        reuse_existing_player_start=reuse_existing_player_start)
    unreal, world, actors = prepared['unreal'], prepared['world'], prepared['actors']
    plan = prepared['placement']
    report = {'status': 'started', 'map': MAP, 'own_asset_namespace': OWN, 'tag': TAG, 'scene_tag': SCENE_TAG, 'seed': SEED,
              'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'editor_handoff': editor_handoff,
              'report_path': str(target), 'scene_plan_sha256': hashlib.sha256(json.dumps(plan, sort_keys=True).encode('utf-8')).hexdigest(),
              'mesh_paths': {k: MESHES[k] for k in prepared['mesh_objects']},
              'material_paths': {k: MATERIALS[k] for k in prepared['materials']}, 'initial_actor_paths': prepared['initial_actor_paths'],
              'initial_dirty_packages': prepared['initial_dirty'], 'sites': plan['sites'], 'markers': plan['markers'],
              'spawned': [], 'saved_packages': [], 'source_assets_saved': [], 'game_mode': prepared['mode'].get_path_name(),
              'limitations': ['First environment pass only', 'House/tower exteriors are not accessible interior gameplay',
                              'Windmill is a static proxy visual, not animated gameplay', 'TargetPoint markers are editor-only and approximate',
                              'NPC/dialogue/quest/book/weapon pickups/15 special AI behaviors are not implemented',
                              'No runtime collision/navigation/60-second route/playthrough/cook certification']}
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')

    def flush():
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def guard():
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if current != world or current.get_path_name().split('.')[0] != MAP or prepared['levels'].is_in_play_in_editor():
            raise RuntimeError('Editor context changed during pass; stop without retry')
        unrelated = [p for p in _dirty_packages(unreal) if not _package_allowed(p)]
        if unrelated:
            raise RuntimeError('Unrelated packages became dirty: ' + json.dumps(unrelated))

    def own(actor, row):
        if not actor:
            raise RuntimeError('Actor spawn failed: ' + row['label'])
        report['spawned'].append({'label': row['label'], 'actor_path': actor.get_path_name(), 'kind': row['kind']})
        actor.set_actor_label(row['label'])
        actor.set_folder_path('WorldLayout20261112/' + row.get('folder', 'Gameplay'))
        actor.set_editor_property('tags', list(actor.get_editor_property('tags')) + [unreal.Name(TAG), unreal.Name(SCENE_TAG)])
        actor.set_editor_property('is_spatially_loaded', not row.get('persistent', False))
        return actor

    try:
        guard()
        report['game_mode_before'] = str(prepared['settings'].get_editor_property('default_game_mode'))
        prepared['settings'].set_editor_property('default_game_mode', prepared['mode'])
        for index, row in enumerate(plan['placements']):
            if index % 100 == 0:
                guard()
                report['completed_placements'] = index
                flush()
                unreal.log('[WorldLayoutScene] %d/%d' % (index, plan['actor_count']))
            if row['kind'] == 'marker':
                actor = own(actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*row['center']), unreal.Rotator()), row)
                actor.set_actor_hidden_in_game(True)
                actor.set_editor_property('is_editor_only_actor', True)
                actor.set_editor_property('tags', list(actor.get_editor_property('tags')) + [unreal.Name(TAG + '.Pending.' + row['marker_kind'])])
                continue
            size = list(row['size'])
            pitch, yaw, roll = row['pitch'], row['yaw'], row['roll']
            native = prepared['bounds'][row['mesh']]
            if row['mesh'] == 'beam':
                axis = max(range(3), key=lambda i: size[i])
                if axis == 2:
                    size[0], size[2] = size[2], size[0]
                    pitch += 90
                elif axis == 1:
                    size[0], size[1] = size[1], size[0]
                    yaw += 90
            if row['align_width'] and native['size'][1] > native['size'][0]:
                size[0], size[1] = size[1], size[0]
                yaw -= 90
            scale = [size[i] / native['size'][i] for i in range(3)]
            shift = _rotate([native['origin'][i] * scale[i] for i in range(3)], pitch, yaw, roll)
            center = [row['center'][i] - shift[i] for i in range(3)]
            actor = own(actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center),
                        unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll)), row)
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if not component.set_static_mesh(prepared['mesh_objects'][row['mesh']]):
                raise RuntimeError('Mesh assignment failed: ' + row['label'])
            component.set_mobility(unreal.ComponentMobility.STATIC)
            component.set_collision_profile_name('BlockAll' if row['collision'] else 'NoCollision')
            component.set_editor_property('generate_overlap_events', False)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            if row['material']:
                for slot in range(max(1, component.get_num_materials())):
                    component.set_material(slot, prepared['materials'][row['material']])
            if row['hidden']:
                actor.set_actor_hidden_in_game(True)
        guard()
        start_node = _load_terrain().nodes_world()['S']
        start_location = unreal.Vector(start_node['x'], start_node['y'], start_node['z'] + 112)
        if prepared['starts']:
            start = prepared['starts'][0]
            old = start.get_actor_location()
            report['reused_player_start_before'] = {'path': start.get_path_name(), 'location': [old.x, old.y, old.z], 'rotation': str(start.get_actor_rotation())}
            start.set_actor_location(start_location, False, False)
            start.set_actor_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0), False)
            own(start, {'label': 'WL26_PlayerStart_S', 'kind': 'player_start', 'folder': 'Gameplay', 'persistent': True})
        else:
            own(actors.spawn_actor_from_class(unreal.PlayerStart, start_location, unreal.Rotator(pitch=0, yaw=90, roll=0)),
                {'label': 'WL26_PlayerStart_S', 'kind': 'player_start', 'folder': 'Gameplay', 'persistent': True})
        if prepared['boss_class'] is not None:
            own(actors.spawn_actor_from_class(prepared['boss_class'], unreal.Vector(15000, 69800, 9780),
                unreal.Rotator(pitch=0, yaw=-90, roll=0)), {'label': 'WL26_HumanAzathor', 'kind': 'boss', 'folder': 'Gameplay', 'persistent': True})
            report['boss_preflight_record'] = boss_preflight_record
        report['completed_placements'] = plan['actor_count']
        report['spawned_count'] = len(report['spawned'])
        report['marker_counts'] = {kind: sum(r['marker_kind'] == kind for r in plan['markers']) for kind in ('AI', 'NPC', 'BOOK', 'WEAPON')}
        guard()
        if save_scene:
            # FileHelpers.h: SaveMap(UWorld*, AssetPath), SavePackages(array,bool).
            # Only this map and its external packages/new OWN assets are allowed.
            if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
                raise RuntimeError('Scoped candidate SaveMap returned false; inspect before retry')
            dirty = _dirty_packages(unreal)
            if any(not _package_allowed(p) for p in dirty):
                raise RuntimeError('Unrelated dirty package after SaveMap; no SavePackages attempted')
            if dirty and not unreal.EditorLoadingAndSavingUtils.save_packages(list(dirty.values()), True):
                raise RuntimeError('Scoped candidate package save returned false; inspect before retry')
            report['saved_packages'] = [MAP] + list(dirty)
            remaining = _dirty_packages(unreal)
            if remaining:
                raise RuntimeError('Dirty packages remain after scoped save: ' + json.dumps(list(remaining)))
            report['status'] = 'saved_first_pass_pending_visual_and_runtime_validation'
        else:
            report['status'] = 'authored_unsaved_pending_owner_inspection'
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        return report
    except Exception as error:
        report['status'] = 'failed_partial_scene_requires_inspection_no_retry'
        report['error'] = repr(error)
        report['spawned_count'] = len(report['spawned'])
        report['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        flush()
        raise


if __name__ == '__main__':
    raise RuntimeError('Import this inert module and call preflight()/author_scene() explicitly from the editor owner.')
