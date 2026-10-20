"""Arin v2 continuous face and sculpted hair, editable Blender 4.5 meshes.
Metres, Z up, front -Y; height crown1.700/chin1.430. Body owns neck.
No operators, scene reset, rendering, rigging or export are performed here.
Call build_head(materials, collection=None); every mesh has arin_bone hints.
This is a static refined face, not a full facial animation or lip-sync rig.
"""


import math

import bpy
from mathutils import Vector

TAU = math.tau


def _mesh(name, vertices, faces, material, collection, bone="head"):
    # Mirrored face/ear paths reverse winding. Correct closed solids by signed
    # volume in a local coordinate frame, without selection or mesh operators.
    reference = Vector(vertices[0])
    volume = 0.0
    for face in faces:
        p0 = Vector(vertices[face[0]]) - reference
        for i in range(1, len(face) - 1):
            p1 = Vector(vertices[face[i]]) - reference
            p2 = Vector(vertices[face[i + 1]]) - reference
            volume += p0.dot(p1.cross(p2))
    if volume < 0:
        faces = [tuple(reversed(face)) for face in faces]
    data = bpy.data.meshes.new(name + "_Mesh")
    data.from_pydata(vertices, [], faces)
    data.update(calc_edges=True)
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    data.materials.append(material)
    for polygon in data.polygons:
        polygon.use_smooth = True
    obj["arin_bone"] = bone
    obj["arin_part"] = "head_and_hair"
    return obj


def _gaussian(value, centre, spread):
    return math.exp(-((value - centre) / spread) ** 2)


def _tube(name, points, widths, depths, material, collection, sides=10,
          bone="head", front_frame=False):
    """Swept elliptical solid; local frames avoid Blender context operators."""
    path = [Vector(p) for p in points]
    if len(path) < 2:
        raise ValueError("A swept mesh requires at least two path points")
    seam = path[-1] - path[0]
    closed_loop = seam.dot(seam) < 1e-16
    if closed_loop:
        path.pop()
        if not isinstance(widths, (int, float)):
            widths = widths[:-1]
        if not isinstance(depths, (int, float)):
            depths = depths[:-1]
    if isinstance(widths, (int, float)):
        widths = [float(widths)] * len(path)
    if isinstance(depths, (int, float)):
        depths = [float(depths)] * len(path)
    if len(widths) != len(path) or len(depths) != len(path):
        raise ValueError("Sweep dimensions must match path points")
    vertices, faces = [], []
    previous_u = None
    for i, point in enumerate(path):
        if closed_loop:
            tangent = path[(i + 1) % len(path)] - path[(i - 1) % len(path)]
        else:
            tangent = path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]
        tangent.normalize()
        reference = Vector((0, -1, 0)) if front_frame else Vector((1, 0, 0))
        if abs(tangent.dot(reference)) > 0.96:
            reference = Vector((0, 0, 1))
        u = tangent.cross(reference).normalized()
        if previous_u is not None and u.dot(previous_u) < 0:
            u.negate()
        v = tangent.cross(u).normalized()
        previous_u = u
        for j in range(sides):
            angle = TAU * j / sides
            vertex = point + u * (widths[i] * math.cos(angle))
            vertex += v * (depths[i] * math.sin(angle))
            vertices.append(tuple(vertex))
    for i in range(len(path) if closed_loop else len(path) - 1):
        a, b = i * sides, ((i + 1) % len(path)) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, a + n, b + n, b + j))
    if not closed_loop:
        # Tangent points make rounded, tapering closed tips without duplicate rings.
        start = len(vertices)
        vertices.append(tuple(path[0]))
        end = len(vertices)
        vertices.append(tuple(path[-1]))
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((start, n, j))
            offset = (len(path) - 1) * sides
            faces.append((end, offset + j, offset + n))
    return _mesh(name, vertices, faces, material, collection, bone)


def _catmull(points, steps=5):
    """Centrally interpolated smooth lock path through editable control points."""
    controls = [Vector(p) for p in points]
    result = []
    for i in range(len(controls) - 1):
        p0 = controls[max(0, i - 1)]
        p1, p2 = controls[i], controls[i + 1]
        p3 = controls[min(i + 2, len(controls) - 1)]
        for step in range(steps):
            t = step / steps
            value = (2 * p1 + (p2 - p0) * t
                     + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                     + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t) * 0.5
            result.append(tuple(value))
    result.append(tuple(controls[-1]))
    return result


def _lock(name, controls, width, depth, material, collection, bone="head",
          steps=3, sides=10):
    points = _catmull(controls, steps)
    count = len(points)
    widths, depths = [], []
    for i in range(count):
        t = i / (count - 1)
        # Grow from an embedded root; pointed tips avoid a bundle of sausages.
        shape = max(0.045, math.sin(math.pi * (0.12 + 0.86 * t)) ** 0.6)
        if t > 0.83:
            shape *= max(0.05, (1 - t) / 0.17)
        widths.append(width * shape)
        depths.append(depth * shape)
    return _tube(name, points, widths, depths, material, collection, sides,
                 bone, front_frame=True)


def _disc_dome(name, centre, rx, rz, projection, material, collection,
               rings=3, sides=24, bone="head"):
    """Small closed corneal/feature dome, facing -Y."""
    cx, cy, cz = centre
    vertices = [(cx, cy - projection, cz)]
    faces = []
    for ring in range(1, rings + 1):
        radius = ring / rings
        for j in range(sides):
            angle = TAU * j / sides
            vertices.append((cx + rx * radius * math.cos(angle),
                             cy - projection * math.sqrt(max(0, 1 - radius * radius)),
                             cz + rz * radius * math.sin(angle)))
    for j in range(sides):
        faces.append((0, 1 + j, 1 + (j + 1) % sides))
    for ring in range(rings - 1):
        a, b = 1 + ring * sides, 1 + (ring + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, b + j, b + n, a + n))
    back = len(vertices)
    vertices.append((cx, cy + .0007, cz))
    a = 1 + (rings - 1) * sides
    for j in range(sides):
        faces.append((back, a + (j + 1) % sides, a + j))
    return _mesh(name, vertices, faces, material, collection, bone)


def _ear(side, material, collection):
    # Sculpted bean-shaped auricle with a raised helix and recessed concha.
    cx, cy, cz = side * .075, .002, 1.548
    sides, rings = 32, 5
    vertices = [(cx + side * .003, cy - .002, cz)]
    for ring in range(1, rings + 1):
        r = ring / rings
        for j in range(sides):
            a = TAU * j / sides
            width = .0105 * (1 + .17 * math.sin(a))
            px = cx + side * (.004 + width * r * math.cos(a))
            pz = cz + .023 * r * math.sin(a)
            py = cy - .008 * r * math.cos(a) - .003 * math.sin(a)
            py -= .0045 * math.exp(-((r - .78) / .20) ** 2)
            vertices.append((px, py, pz))
    faces = [(0, 1 + j, 1 + (j + 1) % sides) for j in range(sides)]
    for ring in range(rings - 1):
        a, b = 1 + ring * sides, 1 + (ring + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, b + j, b + n, a + n))
    back = len(vertices)
    vertices.append((cx - side * .004, cy + .006, cz))
    start = 1 + (rings - 1) * sides
    for j in range(sides):
        faces.append((back, start + (j + 1) % sides, start + j))
    return _mesh("Arin_Ear_" + str(side), vertices, faces, material, collection)


# Form controls are denser on the visible face and retain the v1 neck contract.
_PROFILE = [
    (1.430, .018, .041, .025, -.006), (1.442, .036, .052, .034, -.001),
    (1.455, .049, .060, .043, .001), (1.472, .061, .067, .052, .002),
    (1.490, .071, .073, .062, .003), (1.510, .079, .079, .071, .005),
    (1.530, .083, .084, .079, .006), (1.550, .083, .085, .084, .007),
    (1.570, .081, .084, .088, .008), (1.590, .083, .084, .090, .009),
    (1.610, .084, .086, .091, .010), (1.630, .082, .085, .091, .012),
    (1.650, .075, .080, .087, .014), (1.670, .060, .066, .075, .017),
    (1.684, .043, .047, .054, .018), (1.692, .022, .025, .028, .019),
    (1.695, .000, .000, .000, .019),
]

# A shared ocular contract keeps the complete eyeballs within the cranium.
# The prior spheres were centred 8.5mm too far forward and broke through the
# forehead above the actual apertures. The whole sphere, lid contacts, iris and
# pupils now use the same recessed centre and reduced extents.
_EYE_CY = -.0588
_EYE_RX = .0184
_EYE_RY = .0142
_EYE_RZ = .0162
_EYE_CZ = 1.567
_EYE_OPEN_RX = .0160


def _profile(z):
    if z <= _PROFILE[0][0]:
        return _PROFILE[0][1:]
    if z >= _PROFILE[-1][0]:
        return _PROFILE[-1][1:]
    for low, high in zip(_PROFILE, _PROFILE[1:]):
        if low[0] <= z <= high[0]:
            t = (z - low[0]) / (high[0] - low[0])
            t = t * t * (3 - 2 * t)
            return tuple(low[j] + (high[j] - low[j]) * t for j in range(1, 5))
    raise AssertionError("Cranium profile interpolation")


def _face_field(x, z):
    # Cheek/orbit/muzzle transitions form one continuous skin surface.
    y = 0.0
    for s in (-1, 1):
        y += .0017 * _gaussian(x, s * .033, .019) * _gaussian(z, 1.568, .012)
        y -= .0077 * _gaussian(x, s * .039, .025) * _gaussian(z, 1.547, .025)
        y -= .0037 * _gaussian(x, s * .033, .025) * _gaussian(z, 1.588, .013)
        y -= .0023 * _gaussian(x, s * .026, .022) * _gaussian(z, 1.512, .020)
    # Gradual bridge, rounded tip, distinct alae and a small columella.
    y -= .0105 * _gaussian(x, 0, .0105) * _gaussian(z, 1.563, .027)
    y -= .0220 * _gaussian(x, 0, .0125) * _gaussian(z, 1.540, .011)
    y -= .0060 * (_gaussian(x, -.012, .0058) + _gaussian(x, .012, .0058)) * _gaussian(z, 1.534, .006)
    y -= .0043 * _gaussian(x, 0, .0045) * _gaussian(z, 1.531, .007)
    # Nostril recesses are negative skin form, not dots floating on the nose.
    y += .0025 * (_gaussian(x, -.0088, .0026) + _gaussian(x, .0088, .0026)) * _gaussian(z, 1.531, .0023)
    y -= .0030 * _gaussian(x, 0, .023) * _gaussian(z, 1.502, .020)
    y -= .0020 * _gaussian(x, 0, .018) * _gaussian(z, 1.457, .012)
    return y


def _skin_y(x, z):
    width, front, _, shift = _profile(z)
    cy = math.sqrt(max(.001, 1 - (x / max(.001, width)) ** 2))
    return shift - front * cy ** .43 + _face_field(x, z) * cy ** 1.2


def _skin_tint(obj):
    """Multiplicative, coherent skin warmth for root UV/PBR baking."""
    data = obj.data
    if not hasattr(data, "color_attributes"):
        return  # Allows context-free construction checks with a minimal mock.
    attribute = data.color_attributes.get("ArinSkinTint")
    if attribute is None:
        attribute = data.color_attributes.new(name="ArinSkinTint", type="FLOAT_COLOR", domain="POINT")
    for vertex, colour in zip(data.vertices, attribute.data):
        x, y, z = vertex.co
        front = math.exp(-((y + .078) / .045) ** 2)
        cheek = (_gaussian(x, -.043, .025) + _gaussian(x, .043, .025)) * _gaussian(z, 1.539, .022) * front
        nose = _gaussian(x, 0, .016) * _gaussian(z, 1.535, .017) * front
        warmth = min(1.0, .80 * cheek + .55 * nose)
        colour.color = (1 + .028 * warmth, 1 - .063 * warmth,
                        1 - .070 * warmth, 1)
    obj["arin_skin_tint"] = "POINT FLOAT_COLOR; multiplicative RGB .930..1.028"


def _eye_rim_point(cx, angle):
    cz = _EYE_CZ
    x = cx + _EYE_OPEN_RX * math.cos(angle)
    sine = math.sin(angle)
    z = cz + (.0068 if sine >= 0 else .0040) * sine
    z += .037 * (x - cx) * (1 if cx > 0 else -1)
    nx, nz = (x - cx) / _EYE_RX, (z - cz) / _EYE_RZ
    y = _EYE_CY - _EYE_RY * math.sqrt(max(.08, 1 - nx * nx - nz * nz)) - .00025
    return (x, y, z)


def _rectangle_boundary(row_lo, row_hi, col_lo, col_hi, sides):
    coordinates = [(row_lo, j) for j in range(col_lo, col_hi + 1)]
    coordinates += [(i, col_hi) for i in range(row_lo + 1, row_hi + 1)]
    coordinates += [(row_hi, j) for j in range(col_hi - 1, col_lo - 1, -1)]
    coordinates += [(i, col_lo) for i in range(row_hi - 1, row_lo, -1)]
    ids = [i * sides + j % sides for i, j in coordinates]
    angles = [math.atan2((i - (row_lo + row_hi) / 2) / ((row_hi - row_lo) / 2),
                         (j - (col_lo + col_hi) / 2) / ((col_hi - col_lo) / 2))
              for i, j in coordinates]
    return ids, angles


def _bridge(faces, material_ids, outer, inner, material=0):
    for j in range(len(outer)):
        n = (j + 1) % len(outer)
        faces.append((outer[j], outer[n], inner[n], inner[j]))
        material_ids.append(material)


def _continuous_face(materials, collection):
    levels = [1.430, 1.435, 1.442, 1.448, 1.455, 1.463, 1.472, 1.480,
              1.485, 1.490, 1.494, 1.498, 1.502, 1.506, 1.510, 1.516,
              1.522, 1.528, 1.531, 1.534, 1.540, 1.546, 1.551, 1.555,
              1.559, 1.563, 1.567, 1.571, 1.575, 1.579, 1.583, 1.587,
              1.591, 1.595, 1.602, 1.610, 1.620, 1.630, 1.640, 1.650,
              1.660, 1.670, 1.679, 1.687, 1.692]
    sides = 80
    vertices, faces, material_ids = [], [], []
    for z in levels:
        width, front, back, shift = _profile(z)
        for j in range(sides):
            angle = TAU * j / sides
            x, cy = width * math.sin(angle), math.cos(angle)
            if cy >= 0:
                y = shift - front * cy ** .43 + _face_field(x, z) * cy ** 1.2
            else:
                y = shift - back * cy
            vertices.append((x, y, z))
    eye_lo, eye_hi = levels.index(1.551), levels.index(1.583)
    mouth_lo, mouth_hi = levels.index(1.485), levels.index(1.510)
    eye_rects = [(eye_lo, eye_hi, 2, 10, .0335),
                 (eye_lo, eye_hi, sides - 10, sides - 2, -.0335)]
    mouth_rect = (mouth_lo, mouth_hi, -5, 5)
    for i in range(len(levels) - 1):
        for j in range(sides):
            eye_hole = any(a <= i < b and c <= j < d for a, b, c, d, _ in eye_rects)
            mouth_column = j if j <= sides / 2 else j - sides
            mouth_hole = mouth_lo <= i < mouth_hi and -5 <= mouth_column < 5
            if eye_hole or mouth_hole:
                continue
            a, b = i * sides, (i + 1) * sides
            faces.append((a + j, a + (j + 1) % sides, b + (j + 1) % sides, b + j))
            material_ids.append(0)
    # Three concentric lid/socket loops share the surrounding face's vertices.
    for low, high, left, right, cx in eye_rects:
        outer, angles = _rectangle_boundary(low, high, left, right, sides)
        origins = [vertices[index] for index in outer]
        previous = outer
        for fraction in (.34, .68, 1.0):
            loop = []
            for origin, angle in zip(origins, angles):
                target = _eye_rim_point(cx, angle)
                point = tuple(origin[k] * (1 - fraction) + target[k] * fraction for k in range(3))
                # Smooth, restrained upper lid fold; no freestanding skin tube.
                point = (point[0], point[1] - .00055 * math.sin(math.pi * fraction), point[2])
                loop.append(len(vertices)); vertices.append(point)
            _bridge(faces, material_ids, previous, loop)
            previous = loop
        # Close the head's socket behind the eyeball. The visible opening is
        # intentional, but the skin mesh itself remains a closed concave solid.
        back_loop = []
        for index in previous:
            x, _, z = vertices[index]
            back_loop.append(len(vertices))
            vertices.append((cx + (x - cx) * 1.08, -.044, 1.567 + (z - 1.567) * 1.18))
        _bridge(faces, material_ids, previous, back_loop)
        centre = len(vertices); vertices.append((cx, -.043, 1.567))
        for j in range(len(back_loop)):
            faces.append((back_loop[j], back_loop[(j + 1) % len(back_loop)], centre))
            material_ids.append(0)
    # Mouth contours are part of the same skin mesh, with lip material regions.
    outer, angles = _rectangle_boundary(*mouth_rect, sides)
    previous = outer
    for stage in range(4):
        loop = []
        for angle in angles:
            fraction = (math.cos(angle) + 1) / 2
            x = .024 * math.cos(angle)
            lift = .0028 * abs(x / .024) ** 1.6
            seam = 1.499 + lift
            sine = math.sin(angle)
            if stage == 0:
                h = .0060 if sine >= 0 else .0064
                z = seam + h * sine
                y = _skin_y(x, z) - .0003
            elif stage == 1:
                x *= .95
                h = .0040 if sine >= 0 else .0050
                z = seam + h * sine
                z += .0008 * _gaussian(abs(x), .005, .004) * max(0, sine)
                y = _skin_y(x, z) - .0020 * math.sin(math.pi * fraction)
            elif stage == 2:
                x *= .90
                z = seam + (.0018 if sine >= 0 else .0022) * sine
                y = _skin_y(x, z) - .0031 * math.sin(math.pi * fraction)
            else:
                x *= .88
                z = seam + .00040 * sine
                y = _skin_y(x, z) - .0019 * math.sin(math.pi * fraction)
            loop.append(len(vertices)); vertices.append((x, y, z))
        _bridge(faces, material_ids, previous, loop, 0 if stage == 0 else 1)
        previous = loop
    mouth_back = []
    for index in previous:
        x, y, z = vertices[index]
        mouth_back.append(len(vertices)); vertices.append((x, y + .005, z))
    _bridge(faces, material_ids, previous, mouth_back, 2)
    centre = len(vertices); vertices.append((0, -.066, 1.499))
    for j in range(len(mouth_back)):
        faces.append((mouth_back[j], mouth_back[(j + 1) % len(mouth_back)], centre))
        material_ids.append(2)
    # Back/hidden crown and underside caps; poles stay away from moving corners.
    bottom = len(vertices); vertices.append((0, -.004, 1.429))
    top = len(vertices); vertices.append((0, .019, 1.695))
    for j in range(sides):
        n = (j + 1) % sides
        faces.append((bottom, n, j)); material_ids.append(0)
        a = (len(levels) - 1) * sides
        faces.append((top, a + j, a + n)); material_ids.append(0)
    # Remove the original grid points inside the cut apertures; retaining them
    # would export isolated, unused vertices even though every face is closed.
    used = sorted({index for face in faces for index in face})
    remap = {old: new for new, old in enumerate(used)}
    vertices = [vertices[index] for index in used]
    faces = [tuple(remap[index] for index in face) for face in faces]
    obj = _mesh("Arin_Head_ContinuousFace", vertices, faces, materials["skin"], collection)
    obj.data.materials.append(materials["lip"])
    obj.data.materials.append(materials["hair"])
    for polygon, material in zip(obj.data.polygons, material_ids):
        polygon.material_index = material
    obj["arin_topology"] = "continuous quad eye-socket/lid and mouth contour patches, closed rear cavities; static facial mesh"
    obj["arin_face_loops"] = "3 eye rings; 4 mouth contours; no lip sync or expression controls claimed"
    _skin_tint(obj)
    return obj


def _ellipsoid_eye(cx, material, collection):
    sides, rows = 32, 12
    centre = (cx, _EYE_CY, _EYE_CZ)
    vertices = [(centre[0], centre[1], centre[2] + _EYE_RZ)]
    for i in range(1, rows):
        phi = math.pi * i / rows
        for j in range(sides):
            angle = TAU * j / sides
            vertices.append((cx + _EYE_RX * math.sin(phi) * math.cos(angle),
                             centre[1] + _EYE_RY * math.sin(phi) * math.sin(angle),
                             centre[2] + _EYE_RZ * math.cos(phi)))
    bottom = len(vertices); vertices.append((cx, centre[1], centre[2] - _EYE_RZ))
    faces = []
    for j in range(sides):
        faces.append((0, 1 + j, 1 + (j + 1) % sides))
    for i in range(rows - 2):
        a, b = 1 + i * sides, 1 + (i + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, b + j, b + n, a + n))
    a = 1 + (rows - 2) * sides
    for j in range(sides):
        faces.append((bottom, a + (j + 1) % sides, a + j))
    return _mesh("Arin_EyeSphere_" + str(-1 if cx < 0 else 1), vertices, faces, material, collection)


def _refined_brow(side, material, collection):
    points, widths, depths = [], [], []
    for i in range(18):
        t = i / 17
        x = side * (.0165 + .041 * t)
        z = 1.589 + .0053 * math.sin(math.pi * (.08 + .85 * t)) + .0007 * t
        points.append((x, _skin_y(x, z) - .00035, z))
        width = .0028 * (.55 + .45 * math.sin(math.pi * t)) * (1 - .74 * t)
        if t > .86:
            width *= max(.08, (1 - t) / .14)
        widths.append(max(.00013, width)); depths.append(.00035)
    return _tube("Arin_BrowTaper_" + str(side), points, widths, depths,
                 material, collection, sides=6, front_frame=True)


def _eye_patch(name, cx, cz, radius, offset, material, collection,
               rings=3, sides=28):
    """Iris/pupil geometry conforms to the real spherical eye, not a flat disk."""
    def point(x, z):
        nx, nz = (x - cx) / _EYE_RX, (z - _EYE_CZ) / _EYE_RZ
        y = _EYE_CY - _EYE_RY * math.sqrt(max(.08, 1 - nx * nx - nz * nz)) - offset
        return (x, y, z)
    vertices = [point(cx, cz)]
    faces = []
    for i in range(1, rings + 1):
        r = radius * i / rings
        for j in range(sides):
            angle = TAU * j / sides
            vertices.append(point(cx + r * math.cos(angle), cz + r * math.sin(angle)))
    for j in range(sides):
        faces.append((0, 1 + j, 1 + (j + 1) % sides))
    for i in range(rings - 1):
        a, b = 1 + i * sides, 1 + (i + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, b + j, b + n, a + n))
    back = len(vertices)
    centre = point(cx, cz)
    vertices.append((cx, centre[1] + .0015, cz))
    a = 1 + (rings - 1) * sides
    for j in range(sides):
        faces.append((back, a + (j + 1) % sides, a + j))
    return _mesh(name, vertices, faces, material, collection)


def _refined_scalp(material, collection):
    sides, rows = 80, 14
    vertices = [(0, .017, 1.700)]
    faces = []
    for row in range(1, rows + 1):
        t = row / rows
        for j in range(sides):
            theta = TAU * j / sides
            front = max(0, math.cos(theta))
            boundary = 2.13 - 1.15 * front ** 1.60 - .105 * math.sin(theta) * front
            phi = t * boundary
            z = 1.557 + .143 * math.cos(phi)
            width, fd, bd, shift = _profile(z - .004)
            # Relief follows swept flow across part/crown/nape, as part of the
            # solid shell rather than bright freestanding wire outlines.
            flow = 11 * theta + 3.1 * phi + .7 * math.sin(theta)
            relief = .00075 * math.cos(flow) * math.sin(phi) ** .7
            ripple = 1 + .006 * math.sin(5 * theta + phi)
            x = (width + .0048 + relief) * math.sin(theta) * ripple
            cy = math.cos(theta)
            if cy >= 0:
                y = shift - (fd + .0048 + relief) * cy ** .43 * ripple
            else:
                y = shift - (bd + .0058 + relief) * cy * ripple
            vertices.append((x, y, min(z, 1.700)))
    for j in range(sides):
        faces.append((0, 1 + (j + 1) % sides, 1 + j))
    for i in range(rows - 1):
        a, b = 1 + i * sides, 1 + (i + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, a + n, b + n, b + j))
    cap = len(vertices); vertices.append((0, .019, 1.559))
    a = 1 + (rows - 1) * sides
    for j in range(sides):
        faces.append((cap, a + j, a + (j + 1) % sides))
    obj = _mesh("Arin_Hair_DirectedScalp", vertices, faces, material, collection)
    obj["arin_hair_form"] = "fitted continuous cranial coverage with authored directional relief"
    return obj


def _refined_hair(materials, collection):
    objects = [_refined_scalp(materials["hair"], collection)]
    locks = [
        [( .038, -.022, 1.683), ( .010, -.066, 1.672), (-.025, -.087, 1.651), (-.056, -.082, 1.623), (-.080, -.044, 1.578), (-.080, -.010, 1.542)],
        [( .020, .003, 1.690), (-.023, -.052, 1.676), (-.065, -.060, 1.645), (-.086, -.023, 1.600), (-.082, .018, 1.547), (-.074, .017, 1.502)],
        [( .005, .028, 1.690), (-.040, -.006, 1.681), (-.080, .013, 1.642), (-.089, .043, 1.592), (-.079, .052, 1.536), (-.071, .027, 1.482)],
        [( .039, -.012, 1.682), ( .062, -.045, 1.658), ( .080, -.052, 1.622), ( .087, -.018, 1.578), ( .080, -.008, 1.539), ( .075, -.025, 1.516)],
        [( .034, .010, 1.686), ( .072, -.009, 1.655), ( .092, .023, 1.608), ( .086, .059, 1.553), ( .064, .061, 1.508), ( .026, .059, 1.484)],
        [( .023, .035, 1.687), ( .055, .070, 1.659), ( .066, .100, 1.614), ( .042, .104, 1.553), (-.014, .078, 1.508), (-.067, .029, 1.468)],
        [(-.005, .041, 1.689), (-.040, .087, 1.659), (-.064, .101, 1.606), (-.073, .083, 1.547), (-.078, .044, 1.493), (-.081, .008, 1.453)],
    ]
    for i, path in enumerate(locks):
        objects.append(_lock("Arin_Hair_Mass_%02d" % i, path,
                            .0158 if i < 3 else .0130, .0054,
                            materials["hair"], collection, steps=4, sides=10))
    # Fine secondary strands are low-contrast, flattened, and surface-fitted.
    for index in (0, 2, 3, 5):
        path = [(x, y - .0031 if y < .035 else y + .0031, min(z + .001, 1.697))
                for x, y, z in locks[index]]
        objects.append(_lock("Arin_Hair_Secondary_%d" % index, path,
                            .0021, .0007, materials["hair_light"], collection,
                            steps=3, sides=6))
    for i, path in enumerate([
        [(-.071, -.053, 1.615), (-.089, -.056, 1.579), (-.091, -.025, 1.543), (-.081, -.037, 1.516)],
        [( .081, -.037, 1.605), ( .090, -.045, 1.571), ( .087, -.021, 1.537), ( .072, -.033, 1.517)],
    ]):
        objects.append(_lock("Arin_Hair_LooseTemple_%d" % i, path,
                            .0033, .0018, materials["hair"], collection,
                            steps=4, sides=8))
    # Four overlapping, flattened gather bundles replace the isolated bean.
    for i in range(4):
        d = (i - 1.5) * .006
        path = [(-.067 + d, .032 + abs(d) * .4, 1.493 + d),
                (-.080 + d * .65, .005 - i * .003, 1.455),
                (-.095 + d * .3, -.041 - i * .002, 1.419),
                (-.103 + d * .18, -.078 - i * .001, 1.389)]
        objects.append(_lock("Arin_Braid_GatherBundle_%d" % i, path,
                            .0120, .0068, materials["hair"], collection,
                            steps=4, sides=8))
    # Compact interlace: broad lobe sections overlap at contacts rather than
    # making three open helices. The lower braid is attached to upper chest.
    segments = 42
    for strand in range(3):
        points, widths, depths = [], [], []
        for i in range(segments + 1):
            t = i / segments
            phase = strand * TAU / 3 + t * TAU * 3.2
            x = -.103 + .004 * t + .0090 * (1 - .25 * t) * math.cos(phase)
            y = -.081 - .045 * math.sin(t * math.pi / 2) + .0048 * math.sin(phase)
            z = 1.398 - .233 * t
            points.append((x, y, z))
            widths.append(.0109 * (1 - .24 * t))
            depths.append(.0063 * (1 - .25 * t))
        objects.append(_tube("Arin_Braid_CompactLobe_%d" % strand, points,
                            widths, depths, materials["hair"], collection,
                            sides=10, bone="spine_03", front_frame=True))
    ring = [(-.099 + .0145 * math.cos(TAU * i / 24),
             -.126 + .0090 * math.sin(TAU * i / 24), 1.165) for i in range(25)]
    objects.append(_tube("Arin_Braid_Binding", ring, .0018, .0018,
                         materials["metal_gold"], collection, sides=8, bone="spine_03"))
    for i in range(6):
        d = (i - 2.5) * .004
        objects.append(_lock("Arin_Braid_EndFlow_%d" % i,
            [(-.099 + d * .6, -.126, 1.166), (-.099 + d, -.133, 1.142),
             (-.096 + d * 1.15, -.135, 1.120), (-.100 + d * 1.45, -.131, 1.103 + abs(d) * .6)],
            .0048, .0028, materials["hair"], collection, "spine_03", steps=3, sides=8))
    return objects


def build_head(materials, collection=None):
    """Build the refined static adult head, continuous face and coherent hair.

    Uses existing material keys only. ArinSkinTint is an optional multiplicative
    POINT/FLOAT_COLOR skin attribute for the root's shared UV/PBR bake pipeline.
    Eye and mouth cavities are closed behind openings; all returned parts are
    mesh objects. No facial controls or lip sync are claimed.
    """
    required = ("skin", "hair", "hair_light", "eye_white", "iris", "pupil", "lip", "metal_gold")
    missing = [key for key in required if key not in materials]
    if missing:
        raise ValueError("Arin refined head materials missing: " + ", ".join(missing))
    collection = collection if collection is not None else bpy.context.collection
    objects = [_continuous_face(materials, collection)]
    for side in (-1, 1):
        ear = _ear(side, materials["skin"], collection)
        _skin_tint(ear)
        objects.append(ear)
        cx = side * .0335
        objects.append(_ellipsoid_eye(cx, materials["eye_white"], collection))
        front = _EYE_CY - _EYE_RY
        objects.append(_eye_patch("Arin_IrisRefined_" + str(side),
                       cx, 1.5677, .0060, .00010,
                       materials["iris"], collection, rings=3, sides=28))
        objects.append(_eye_patch("Arin_PupilRefined_" + str(side),
                       cx, 1.5677, .0025, .00022,
                       materials["pupil"], collection, rings=2, sides=20))
        objects.append(_disc_dome("Arin_CornealGlint_" + str(side),
                       (cx - .0017, front - .00105, 1.5692), .00045, .00045, .0001,
                       materials["eye_white"], collection, rings=1, sides=8))
        objects.append(_refined_brow(side, materials["hair"], collection))
        # Small embedded warm-dark recess on the sculpted alar underside.
        nx, nz = side * .0088, 1.531
        objects.append(_disc_dome("Arin_NostrilRecess_" + str(side),
                       (nx, _skin_y(nx, nz) - .00015, nz), .0018, .00075, .00015,
                       materials["hair"], collection, rings=1, sides=12))
    objects.extend(_refined_hair(materials, collection))
    return objects
