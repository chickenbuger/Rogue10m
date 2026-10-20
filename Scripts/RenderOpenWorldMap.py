"""Render an accurate Korean atlas from the saved OpenRunMap authoring data.

Requires Pillow and NumPy. No Unreal Editor calls or Content writes.
Terrain, roads, river, bridges and landmarks use authored coordinates;
canopy marks summarize the saved PCG tile distribution, not individual trees.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Scripts/Editor'))
import OpenWorldLayout as layout

SOURCE = ROOT / 'Feature/doc/evidence/open-world-20261005'
OUTPUT = ROOT / 'Feature/doc/evidence/open-world-map-20261005'
WIDTH, HEIGHT = 2200, 2550
LEFT, TOP, SIZE = 160, 250, 1880
PAPER = (239, 232, 211)
INK = (53, 65, 50)
SECONDARY = (92, 100, 79)
GOLD = (144, 101, 48)
RED = (147, 57, 48)
FONT = Path('C:/Windows/Fonts/malgun.ttf')
BOLD = Path('C:/Windows/Fonts/malgunbd.ttf')
NAMES = {'camp': '남쪽 야영지', 'forest': '서쪽 숲', 'crossroads': '강 교차로',
         'mill': '동쪽 폐풍차', 'highlands': '바위 고원', 'boss': '북쪽 보스 유적'}


def font(size, bold=False):
    return ImageFont.truetype(str(BOLD if bold else FONT), size)


def project(x_cm, y_cm):
    return (LEFT + (x_cm + layout.WORLD_HALF_CM) / layout.WORLD_SIZE_CM * SIZE,
            TOP + (layout.WORLD_HALF_CM - y_cm) / layout.WORLD_SIZE_CM * SIZE)


def pixels(cm):
    return cm / layout.WORLD_SIZE_CM * SIZE


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def terrain_image(raw):
    height = np.flipud((raw.astype(np.float32) - 32768.0) * layout.LANDSCAPE_Z_SCALE / 12800.0)
    breaks = [20, 40, 70, 100, 140]
    palette = np.array([[178, 196, 137], [197, 205, 152], [189, 183, 144],
                        [175, 164, 142], [210, 201, 181]], dtype=float)
    rgb = np.stack([np.interp(height, breaks, palette[:, c]) for c in range(3)], axis=-1)
    gy, gx = np.gradient(height, layout.HEIGHTMAP_SPACING_CM / 100.0)
    norm = np.sqrt(gx * gx + gy * gy + 1)
    illumination = (gx * 0.55 + gy * 0.55 + 0.63) / norm
    shade = np.clip(0.84 + illumination * 0.22, 0.68, 1.08)
    rng = np.random.default_rng(20261005)
    rgb = np.clip(rgb * shade[:, :, None] + rng.normal(0, 0.65, (*height.shape, 1)), 0, 255)
    image = Image.fromarray(rgb.astype(np.uint8)).convert('RGBA')
    # Thin 10m contours retain the actual terrain ridges and road grading.
    band = np.floor(height / 10).astype(int)
    edge = np.zeros_like(height, dtype=bool)
    edge[1:, :] |= band[1:, :] != band[:-1, :]
    edge[:, 1:] |= band[:, 1:] != band[:, :-1]
    contour = np.zeros((*height.shape, 4), dtype=np.uint8)
    contour[:, :, :3] = (81, 89, 66)
    contour[:, :, 3] = edge * 26
    image = Image.alpha_composite(image, Image.fromarray(contour))
    return image.resize((SIZE, SIZE), Image.Resampling.LANCZOS).convert('RGB'), height


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    creation = json.loads((SOURCE / 'creation.json').read_text(encoding='utf-8'))
    tiles = json.loads((SOURCE / 'pcg/tiles.json').read_text(encoding='utf-8'))
    assert creation['status'] == 'created' and creation['pois'] == layout.POIS
    assert creation['bridges'] == layout.BRIDGES
    assert tiles['status'] == 'generated' and len(tiles['tiles']) == 183
    assert all(t['generated'] and not t['messages'] for t in tiles['tiles'])
    assert FONT.exists() and BOLD.exists()
    raw = np.fromfile(SOURCE / 'terrain-height.r16', dtype='<u2')
    assert raw.size == layout.HEIGHTMAP_SIZE ** 2
    raw = raw.reshape((layout.HEIGHTMAP_SIZE, layout.HEIGHTMAP_SIZE))
    for row, col in [(0, 0), (1008, 1008), (1008, 0), (0, 1008), (504, 504)]:
        x = layout.LANDSCAPE_ORIGIN[0] + col * layout.HEIGHTMAP_SPACING_CM
        y = layout.LANDSCAPE_ORIGIN[1] + row * layout.HEIGHTMAP_SPACING_CM
        decoded_cm = (int(raw[row, col]) - 32768) * layout.LANDSCAPE_Z_SCALE / 128.0
        assert abs(decoded_cm - layout.height_at(x, y)) <= 1.001

    image = Image.new('RGB', (WIDTH, HEIGHT), PAPER)
    draw = ImageDraw.Draw(image)
    draw.rectangle((34, 34, WIDTH - 35, HEIGHT - 35), outline=(165, 151, 120), width=2)
    draw.rectangle((46, 46, WIDTH - 47, HEIGHT - 47), outline=(211, 200, 170), width=1)
    draw.text((LEFT, 80), 'ROGUE10M  /  OPEN WORLD', font=font(29, True), fill=GOLD)
    draw.text((LEFT - 3, 127), '오픈월드 탐험 지도', font=font(61, True), fill=INK)
    draw.text((WIDTH - LEFT, 173), '1.8 × 1.8 km', font=font(37), fill=SECONDARY, anchor='ra')
    terrain, heights = terrain_image(raw)
    image.paste(terrain, (LEFT, TOP))
    draw = ImageDraw.Draw(image)

    # Subtle cartographic grid at 300m; canopy is schematic within actual tiles.
    overlay = Image.new('RGBA', image.size)
    od = ImageDraw.Draw(overlay)
    for c in range(-60000, 60001, 30000):
        x, _ = project(c, 0)
        _, y = project(0, c)
        od.line((x, TOP, x, TOP + SIZE), fill=(74, 91, 62, 24), width=1)
        od.line((LEFT, y, LEFT + SIZE, y), fill=(74, 91, 62, 24), width=1)
    rng = random.Random(20261005)
    for tile in tiles['tiles']:
        cx, cy = tile['center']
        for _ in range(15):
            x, y = cx + rng.uniform(-4250, 4250), cy + rng.uniform(-4250, 4250)
            if not layout.legal_foliage(x, y):
                continue
            px, py = project(x, y)
            r = rng.uniform(5.5, 10.5)
            od.ellipse((px - r + 2, py - r + 4, px + r + 2, py + r + 4), fill=(52, 71, 43, 48))
            shade = rng.choice([(84, 119, 67, 142), (109, 132, 70, 155), (132, 145, 76, 148)])
            od.ellipse((px - r, py - r, px + r, py + r), fill=shade)
            od.arc((px - r + 2, py - r + 1, px + r - 1, py + r - 2), 180, 280, fill=(198, 211, 144, 90), width=2)
    image = Image.alpha_composite(image.convert('RGBA'), overlay).convert('RGB')
    draw = ImageDraw.Draw(image)

    # All actual road vertices; bridges replace the crossing surface.
    for route in layout.ROAD_ROUTES.values():
        points = [project(x, y) for x, y, _ in route]
        draw.line(points, fill=(132, 117, 78), width=round(pixels(layout.ROAD_VISIBLE_WIDTH_CM)) + 5, joint='curve')
        draw.line(points, fill=(225, 210, 163), width=round(pixels(layout.ROAD_VISIBLE_WIDTH_CM)), joint='curve')
    a = project(-layout.RIVER_HALF_LENGTH_CM, layout.RIVER_CENTER_Y)
    b = project(layout.RIVER_HALF_LENGTH_CM, layout.RIVER_CENTER_Y)
    river_width = round(pixels(layout.RIVER_HALF_WIDTH_CM * 2))
    draw.line((a, b), fill=(111, 146, 137), width=river_width + 10)
    draw.line((a, b), fill=(95, 150, 161), width=river_width)
    for point in (a, b):
        draw.ellipse((point[0] - river_width / 2, point[1] - river_width / 2,
                      point[0] + river_width / 2, point[1] + river_width / 2), fill=(95, 150, 161))
    for i in range(30):
        x, y = project(-71000 + i * 4750, layout.RIVER_CENTER_Y)
        draw.line((x - 12, y - 11 + (i % 3) * 9, x + 23, y - 11 + (i % 3) * 9), fill=(155, 190, 183), width=2)
    draw.text((project(-70500, layout.RIVER_CENTER_Y)[0], a[1] + river_width / 2 + 20),
              '강', font=font(27, True), fill=(53, 102, 119))
    for bridge in layout.BRIDGES:
        x, y = project(bridge['x'], bridge['y'])
        w, h = pixels(bridge['width']), pixels(bridge['length'])
        draw.rectangle((x - w / 2, y - h / 2, x + w / 2, y + h / 2), fill=(193, 167, 117), outline=(96, 86, 61), width=2)
        for yy in np.arange(y - h / 2 + 5, y + h / 2, 7):
            draw.line((x - w / 2 + 1, yy, x + w / 2 - 1, yy), fill=(143, 120, 82), width=1)
        draw.line((x - w / 2 - 4, y - h / 2, x - w / 2 - 4, y + h / 2), fill=(99, 87, 60), width=3)
        draw.line((x + w / 2 + 4, y - h / 2, x + w / 2 + 4, y + h / 2), fill=(99, 87, 60), width=3)

    # Camp safe area and actual three house footprints.
    camp = layout.POIS['camp']
    x, y = project(camp['x'], camp['y'])
    r = pixels(creation['safe_camp_radius_cm'])
    draw.ellipse((x - r, y - r, x + r, y + r), outline=(194, 152, 75), width=3)
    for dx, dy in [(-3500, 500), (3500, 500), (-3500, 4000)]:
        hx, hy = project(camp['x'] + dx, camp['y'] + dy)
        draw.rectangle((hx - 11, hy - 11, hx + 11, hy + 11), fill=(216, 200, 158), outline=(91, 90, 65), width=2)
        draw.polygon([(hx - 15, hy - 5), (hx, hy - 17), (hx + 15, hy - 5), (hx + 15, hy + 10), (hx - 15, hy + 10)], fill=(111, 125, 95), outline=(69, 84, 62))
    draw.polygon([(x, y - 16), (x - 12, y + 12), (x + 12, y + 12)], fill=GOLD)

    # Actual ruined arena footprint, open southern entrance and 55m tower site.
    bx, by = layout.POIS['boss']['x'], layout.POIS['boss']['y']
    walls = [[(bx - 5700, by - 5600), (bx - 5700, by + 5600), (bx + 5700, by + 5600), (bx + 5700, by - 5600)],
             [(bx - 5700, by - 5600), (bx - 1800, by - 5600)],
             [(bx + 1800, by - 5600), (bx + 5700, by - 5600)]]
    for wall in walls:
        draw.line([project(*p) for p in wall], fill=(109, 101, 81), width=7)
    tx, ty = project(bx + 3800, by + 3800)
    draw.rectangle((tx - 12, ty - 12, tx + 12, ty + 12), fill=(124, 116, 91), outline=INK, width=2)
    for angle in range(0, 360, 30):
        px, py = project(bx + 4300 * math.cos(math.radians(angle)), by + 4300 * math.sin(math.radians(angle)))
        draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=(129, 117, 89))
    # Windmill marker at its actual building coordinate, not just region center.
    mx, my = project(52500, -3200)
    draw.ellipse((mx - 10, my - 10, mx + 10, my + 10), fill=(200, 187, 151), outline=INK, width=2)
    draw.line((mx - 22, my - 22, mx + 22, my + 22), fill=(79, 84, 63), width=5)
    draw.line((mx - 22, my + 22, mx + 22, my - 22), fill=(79, 84, 63), width=5)

    def diamond(x, y, radius=9, boss=False):
        draw.polygon([(x, y - radius), (x + radius, y), (x, y + radius), (x - radius, y)], fill=RED, outline=PAPER)
        if boss:
            draw.ellipse((x - radius - 5, y - radius - 5, x + radius + 5, y + radius + 5), outline=RED, width=3)

    for encounter in creation['encounters']:
        ex, ey = project(*encounter['location'][:2])
        diamond(ex, ey, 12 if encounter['kind'] == 'ThornWarden' else 9)
    boss_point = project(15000, 69800)
    diamond(*boss_point, 14, boss=True)

    boxes = []

    def label(text, sub, anchor, offset, size=36):
        px, py = anchor
        lx, ly = px + offset[0], py + offset[1]
        f, sf = font(size, True), font(23)
        width = max(draw.textlength(text, font=f), draw.textlength(sub, font=sf)) + 36
        height = 98 if sub else 63
        box = (int(lx), int(ly), int(lx + width), int(ly + height))
        edge_x = min(max(px, box[0]), box[2])
        edge_y = min(max(py, box[1]), box[3])
        draw.line((px, py, edge_x, edge_y), fill=(109, 111, 84), width=2)
        draw.rounded_rectangle(box, radius=8, fill=(239, 232, 211), outline=(160, 151, 119), width=1)
        draw.text((lx + 18, ly + 5), text, font=f, fill=INK)
        if sub:
            draw.text((lx + 18, ly + 60), sub, font=sf, fill=SECONDARY)
        assert LEFT <= box[0] < box[2] <= LEFT + SIZE and TOP <= box[1] < box[3] <= TOP + SIZE
        boxes.append({'text': text, 'bounds': box, 'anchor': anchor})

    label(NAMES['camp'], '입장 · 안전 반경 80m', project(0, -65000), (105, 22))
    label(NAMES['forest'], '숲 전투 구역', project(-50000, -25000), (-295, 80))
    label(NAMES['crossroads'], '중앙 다리 · 북쪽 갈림길', project(0, 5000), (75, -145))
    label(NAMES['mill'], '풍차 전투 구역', project(50000, -5000), (90, 77))
    label(NAMES['highlands'], '고원 전투 · 중간 보스', project(-35000, 42000), (-375, -30))
    label(NAMES['boss'], '고정 보스 · 석조 유적', project(15000, 68000), (110, -6))
    label('서쪽 다리', '', project(-44000, 5000), (-205, -126), size=27)
    label('동쪽 다리', '', project(45000, 5000), (35, -127), size=27)
    # The crossroads label already identifies the central bridge.
    for i, a in enumerate(boxes):
        for b in boxes[i + 1:]:
            aa, bb = a['bounds'], b['bounds']
            assert not (max(aa[0], bb[0]) < min(aa[2], bb[2]) and max(aa[1], bb[1]) < min(aa[3], bb[3])), (a['text'], b['text'])

    # Compass and map frame, north = +Y as the built regions are named.
    nx, ny = LEFT + SIZE - 95, TOP + 110
    draw.ellipse((nx - 53, ny - 53, nx + 53, ny + 53), fill=PAPER, outline=(164, 153, 122), width=2)
    draw.polygon([(nx, ny - 39), (nx - 13, ny + 29), (nx, ny + 17), (nx + 13, ny + 29)], fill=INK)
    draw.text((nx, ny - 90), '북', font=font(29, True), fill=INK, anchor='ma')
    draw.text((nx, ny + 59), 'N', font=font(23), fill=INK, anchor='ma')
    draw.rectangle((LEFT, TOP, LEFT + SIZE, TOP + SIZE), outline=(101, 112, 81), width=3)
    draw.text((LEFT + SIZE / 2, TOP + SIZE + 14), '남', font=font(25), fill=SECONDARY, anchor='ma')
    draw.text((LEFT - 40, TOP + SIZE / 2), '서', font=font(25), fill=SECONDARY, anchor='mm')
    draw.text((LEFT + SIZE + 40, TOP + SIZE / 2), '동', font=font(25), fill=SECONDARY, anchor='mm')

    # Compact legend and a separate-map portal inset; never invent an in-world town.
    legend_y = 2230
    draw.line((LEFT, legend_y + 16, LEFT + 60, legend_y + 16), fill=(132, 117, 78), width=16)
    draw.line((LEFT, legend_y + 16, LEFT + 60, legend_y + 16), fill=(225, 210, 163), width=10)
    draw.text((LEFT + 78, legend_y - 5), '길', font=font(28), fill=INK)
    draw.line((LEFT + 270, legend_y + 16, LEFT + 330, legend_y + 16), fill=(95, 150, 161), width=18)
    draw.text((LEFT + 350, legend_y - 5), '강', font=font(28), fill=INK)
    diamond(LEFT + 565, legend_y + 16, 12)
    draw.text((LEFT + 590, legend_y - 5), '전투 구역', font=font(28), fill=INK)
    diamond(LEFT + 865, legend_y + 16, 12, boss=True)
    draw.text((LEFT + 895, legend_y - 5), '고정 보스', font=font(28), fill=INK)
    draw.ellipse((LEFT + 1200, legend_y + 4, LEFT + 1225, legend_y + 29), fill=(109, 132, 70))
    draw.text((LEFT + 1245, legend_y - 5), '수목 분포', font=font(28), fill=INK)

    draw.rounded_rectangle((LEFT, 2330, LEFT + 670, 2420), radius=12, outline=(167, 154, 122), width=2)
    draw.text((LEFT + 23, 2354), '시작마을', font=font(31, True), fill=INK)
    draw.text((LEFT + 180, 2361), '(별도 맵)', font=font(23), fill=SECONDARY)
    draw.line((LEFT + 308, 2375, LEFT + 390, 2375), fill=GOLD, width=3)
    draw.polygon([(LEFT + 390, 2375), (LEFT + 378, 2368), (LEFT + 378, 2382)], fill=GOLD)
    draw.text((LEFT + 409, 2354), '입장 야영지', font=font(31, True), fill=INK)
    draw.text((LEFT + 339, 2343), '포탈', font=font(18), fill=GOLD, anchor='ma')

    sx, sy = WIDTH - LEFT - pixels(50000), 2375
    length = pixels(50000)
    for i in range(4):
        draw.rectangle((sx + length * i / 4, sy, sx + length * (i + 1) / 4, sy + 12),
                       fill=INK if i % 2 == 0 else PAPER, outline=INK, width=1)
    for value in [0, 250, 500]:
        draw.text((sx + length * value / 500, sy + 22), str(value) + (' m' if value == 500 else ''),
                  font=font(23), fill=INK, anchor='ma')
    draw.text((LEFT, 2460), '지형 · 길 · 주요 위치는 제작 좌표 기준 / 수목과 건물은 지도 기호로 표현', font=font(23), fill=SECONDARY)

    image_path = OUTPUT / 'OpenRunMap_Map.png'
    image.save(image_path, optimize=True)
    inputs = [SOURCE / n for n in ['creation.json', 'terrain-height.r16', 'pcg/tiles.json']]
    report = {'status': 'passed', 'map': '/Game/Rogue10m/Maps/OpenRunMap', 'image': str(image_path),
              'image_size': [WIDTH, HEIGHT], 'orientation': {'north': '+Y', 'east': '+X'},
              'extent_cm': [-90000, -90000, 90000, 90000], 'map_frame_pixels': [LEFT, TOP, SIZE, SIZE],
              'regions': {key: {'label': NAMES[key], 'world_xy_cm': [p['x'], p['y']],
                                'pixel_xy': project(p['x'], p['y'])} for key, p in layout.POIS.items()},
              'roads': {key: {'world_vertices_cm': route, 'pixel_vertices': [project(x, y) for x, y, _ in route]}
                        for key, route in layout.ROAD_ROUTES.items()},
              'river': {'center_y_cm': 5000, 'centerline_x_cm': [-75000, 75000], 'width_cm': 6000,
                        'rounded_end_extent_x_cm': [-78000, 78000]},
              'bridges': layout.BRIDGES, 'boss_actual_xy_cm': [15000, 69800],
              'windmill_actual_xy_cm': [52500, -3200], 'pcg_tiles': len(tiles['tiles']),
              'source_sha256': {str(p.relative_to(ROOT)): digest(p) for p in inputs},
              'labels': boxes, 'height_range_m': [float(heights.min()), float(heights.max())],
              'checks': ['creation/layout POIs and bridges identical', 'PCG183 generated tiles',
                         'R16 five sample points match authored terrain within1cm',
                         'labels within map and no label rectangles overlap', 'Korean fonts present'],
              'limits': ['tree and landmark marks are symbols, not a per-instance orthographic editor capture',
                         'StartHub is a separate map shown only as a portal inset']}
    (OUTPUT / 'map-data.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'status': 'passed', 'image': str(image_path), 'regions': 6, 'bridges': 3,
                      'pcg_tiles': len(tiles['tiles']), 'image_size': [WIDTH, HEIGHT]}))


if __name__ == '__main__':
    main()
