"""Editable, material-driven Arin head for Blender 4.5+.

Coordinate contract: metres, Z up, character front -Y, adult head crown 1.700,
chin 1.430. The body builder owns the neck. Call build_head(materials,
collection=None); this module neither resets the scene nor uses operators.
Every returned object is a mesh with an ``arin_bone`` rigid skinning hint.
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


def _face_depth(x, z, base_y):
    """Sculpted front field: sockets, cheek pads, nasal bridge and muzzle."""
    y = base_y
    # Sunken sockets keep eye features seated within the skull.
    for side in (-1, 1):
        y += 0.0035 * _gaussian(x, side * 0.0335, 0.023)
        y_socket = _gaussian(z, 1.565, 0.014)
        # The previous field is restricted vertically to the orbit.
        y -= 0.0035 * _gaussian(x, side * 0.0335, 0.023) * (1 - y_socket)
        y -= 0.0070 * _gaussian(x, side * 0.042, 0.025) * _gaussian(z, 1.542, 0.025)
        y -= 0.0040 * _gaussian(x, side * 0.030, 0.025) * _gaussian(z, 1.590, 0.011)
    # Continuous nose geometry is part of the face surface, not a stuck-on cone.
    y -= 0.0120 * _gaussian(x, 0, 0.0090) * _gaussian(z, 1.561, 0.027)
    y -= 0.0270 * _gaussian(x, 0, 0.0115) * _gaussian(z, 1.535, 0.010)
    y -= 0.0060 * (_gaussian(x, -0.011, 0.006) + _gaussian(x, 0.011, 0.006)) * _gaussian(z, 1.530, 0.005)
    y -= 0.0035 * _gaussian(x, 0, 0.026) * _gaussian(z, 1.499, 0.018)
    y -= 0.0023 * _gaussian(x, 0, 0.018) * _gaussian(z, 1.455, 0.011)
    return y


def _head_surface(material, collection):
    # Z, half-width, front depth, back depth, Y shift. Narrow chin and full
    # cranium have deliberately different proportions from an oval primitive.
    profile = [
        (1.430, .013, .040, .026, -.007),
        (1.435, .023, .046, .030, -.004),
        (1.442, .031, .052, .035, -.001),
        (1.450, .040, .059, .043, .001),
        (1.461, .049, .065, .048, .002),
        (1.475, .058, .069, .053, .003),
        (1.488, .064, .071, .058, .004),
        (1.499, .068, .073, .063, .004),
        (1.510, .071, .074, .068, .005),
        (1.520, .074, .077, .074, .006),
        (1.527, .075, .078, .076, .006),
        (1.533, .077, .078, .078, .007),
        (1.540, .078, .080, .081, .007),
        (1.550, .078, .082, .082, .008),
        (1.557, .077, .083, .084, .008),
        (1.565, .076, .082, .086, .008),
        (1.574, .076, .082, .087, .008),
        (1.583, .077, .082, .088, .009),
        (1.595, .079, .082, .089, .010),
        (1.610, .080, .083, .090, .011),
        (1.628, .079, .083, .091, .012),
        (1.645, .076, .080, .089, .014),
        (1.660, .068, .073, .083, .016),
        (1.674, .055, .061, .070, .017),
        (1.684, .040, .045, .052, .018),
        (1.692, .021, .024, .028, .019),
    ]
    sides = 72
    vertices, faces = [], []
    for z, width, front, back, shift in profile:
        for j in range(sides):
            angle = TAU * j / sides
            sx, cy = math.sin(angle), math.cos(angle)
            x = width * sx
            if cy >= 0:
                # A soft squircle spreads features across a human facial plane.
                y = shift - front * (max(0, cy) ** 0.43)
                field = _face_depth(x, z, shift - front) - (shift - front)
                y += field * (max(0, cy) ** 1.2)
            else:
                y = shift - back * cy
            vertices.append((x, y, z))
    for i in range(len(profile) - 1):
        for j in range(sides):
            n = (j + 1) % sides
            a, b = i * sides, (i + 1) * sides
            faces.append((a + j, b + j, b + n, a + n))
    bottom = len(vertices)
    vertices.append((0, -.008, 1.428))
    top = len(vertices)
    vertices.append((0, .019, 1.695))
    for j in range(sides):
        n = (j + 1) % sides
        faces.append((bottom, j, n))
        a = (len(profile) - 1) * sides
        faces.append((top, a + n, a + j))
    return _mesh("Arin_Head_Sculpted", vertices, faces, material, collection)


def _eye_boundary(side, t):
    # t 0..1 goes from inner to outer corner; outer corner rises slightly.
    x = side * (0.0145 + 0.0365 * t)
    baseline = 1.5638 + 0.0016 * t
    upper = baseline + 0.0071 * (math.sin(math.pi * t) ** 0.82)
    lower = baseline - 0.0041 * (math.sin(math.pi * t) ** 0.82)
    return x, baseline, upper, lower


def _eye_front(x, z, side):
    # Spherical/ellipsoidal cornea-front depth, exposed through almond outline.
    nx = (x - side * .0328) / .0218
    nz = (z - 1.5650) / .017
    return -.0678 - .0138 * math.sqrt(max(.10, 1 - nx * nx - nz * nz))


def _almond_eye(side, material, collection):
    # Closed solid almond with a spherical front, buried rear face. The white
    # has no oversized exposed full-sphere side silhouette.
    segments, rings = 32, 3
    perimeter = []
    for j in range(segments):
        angle = TAU * j / segments
        t = (1 - math.cos(angle)) * .5
        x, baseline, upper, lower = _eye_boundary(side, t)
        z = upper if math.sin(angle) >= 0 else lower
        perimeter.append((x, z))
    centre = (side * .0328, 1.5650)
    vertices = [(centre[0], _eye_front(*centre, side), centre[1])]
    for ring in range(1, rings + 1):
        fraction = ring / rings
        for x, z in perimeter:
            px = centre[0] + (x - centre[0]) * fraction
            pz = centre[1] + (z - centre[1]) * fraction
            vertices.append((px, _eye_front(px, pz, side), pz))
    faces = []
    for j in range(segments):
        faces.append((0, 1 + j, 1 + (j + 1) % segments))
    for ring in range(rings - 1):
        a, b = 1 + ring * segments, 1 + (ring + 1) * segments
        for j in range(segments):
            n = (j + 1) % segments
            faces.append((a + j, b + j, b + n, a + n))
    back = len(vertices)
    vertices.append((centre[0], -.067, centre[1]))
    a = 1 + (rings - 1) * segments
    for j in range(segments):
        faces.append((back, a + (j + 1) % segments, a + j))
    return _mesh("Arin_EyeWhite_" + ("L" if side < 0 else "R"),
                 vertices, faces, material, collection)


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


def _eyelids(side, material, collection):
    upper, lower = [], []
    upper_widths, lower_widths = [], []
    for i in range(21):
        t = i / 20
        x, _, high, low = _eye_boundary(side, t)
        upper.append((x, _eye_front(x, high, side) - .0004, high))
        lower.append((x, _eye_front(x, low, side) - .0002, low))
        fade = .35 + .65 * math.sin(math.pi * t) ** .5
        upper_widths.append(.0017 * fade)
        lower_widths.append(.00125 * fade)
    return [
        _tube("Arin_UpperLid_" + str(side), upper, upper_widths,
              [.0011] * 21, material, collection, 6, front_frame=True),
        _tube("Arin_LowerLid_" + str(side), lower, lower_widths,
              [.0009] * 21, material, collection, 6, front_frame=True),
    ]


def _orbital_base_y(x, z):
    # Match the face ring surface at the orbital band's outer edge.
    rows = [(1.540, .078, .080, .007), (1.550, .078, .082, .008),
            (1.557, .077, .083, .008), (1.565, .076, .082, .008),
            (1.574, .076, .082, .008), (1.583, .077, .082, .009),
            (1.595, .079, .082, .010), (1.610, .080, .083, .011)]
    low, high = rows[0], rows[-1]
    for i in range(len(rows) - 1):
        if rows[i][0] <= z <= rows[i + 1][0]:
            low, high = rows[i], rows[i + 1]
            break
    t = min(1, max(0, (z - low[0]) / (high[0] - low[0])))
    width, front, shift = [low[i] + (high[i] - low[i]) * t for i in (1, 2, 3)]
    cy = math.sqrt(max(.001, 1 - (x / width) ** 2))
    baseline = shift - front * cy ** .43
    field = _face_depth(x, z, shift - front) - (shift - front)
    return baseline + field * cy ** 1.2


def _orbital_skin(side, material, collection):
    """Anatomical skin band seats the almond cornea inside the actual face."""
    sides = 32
    inner, outer = [], []
    for j in range(sides):
        angle = TAU * j / sides
        t = (1 - math.cos(angle)) * .5
        x, _, upper, lower = _eye_boundary(side, t)
        z = upper if math.sin(angle) >= 0 else lower
        inner.append((x, _eye_front(x, z, side) + .00025, z))
        centre_x, centre_z = side * .0328, 1.5650
        ox = centre_x + (x - centre_x) * 1.45
        oz = centre_z + (z - centre_z) * (1.85 if z > centre_z else 2.6)
        outer.append((ox, _orbital_base_y(ox, oz) + .0002, oz))
    vertices = inner + outer
    vertices += [(x, y + .0015, z) for x, y, z in inner + outer]
    faces = []
    for j in range(sides):
        n = (j + 1) % sides
        faces.append((j, n, sides + n, sides + j))
        faces.append((2 * sides + j, 3 * sides + j, 3 * sides + n, 2 * sides + n))
        faces.append((j, 2 * sides + j, 2 * sides + n, n))
        faces.append((sides + j, sides + n, 3 * sides + n, 3 * sides + j))
    return _mesh("Arin_OrbitalSkin_" + str(side), vertices, faces, material, collection)


def _brow(side, material, collection):
    points, widths, depths = [], [], []
    for i in range(16):
        t = i / 15
        x = side * (.017 + .039 * t)
        z = 1.584 + .0063 * math.sin(math.pi * (.06 + .88 * t)) + .001 * t
        base = .009 - .082 * ((max(0, 1 - (x / .077) ** 2)) ** .215)
        y = _face_depth(x, z, base) - .0012
        points.append((x, y, z))
        widths.append(.0023 * (1 - .68 * t) * (.7 + .3 * math.sin(math.pi * t)))
        depths.append(.0007)
    return _tube("Arin_Brow_" + str(side), points, widths, depths,
                 material, collection, 8, front_frame=True)


def _lip(name, upper, material, collection):
    count, across = 32, 5
    # Corner tips are single vertices; collapsed six-vertex columns would
    # create zero-area faces and disconnected solid caps at the mouth corners.
    vertices, faces = [(-.021, -.0712, 1.4968)], []
    for i in range(1, count):
        t = i / count
        x = -.021 + .042 * t
        fraction = x / .021
        taper = max(0, 1 - fraction * fraction) ** .65
        seam = 1.494 + .0028 * abs(fraction) ** 1.7
        if upper:
            # A modest Cupid's bow and upper lip, with friendly lifted corners.
            height = taper * (.0035 + .0014 * math.exp(-((abs(x) - .005) / .004) ** 2))
        else:
            height = -.0048 * taper
        for j in range(across + 1):
            v = j / across
            z = seam + height * v
            y = -.0712 - .0030 * taper - .0015 * math.sin(math.pi * v) * taper
            vertices.append((x, y, z))
    right = len(vertices)
    vertices.append((.021, -.0712, 1.4968))
    for j in range(across):
        faces.append((0, 1 + j, 1 + j + 1))
        a = 1 + (count - 2) * (across + 1)
        faces.append((a + j, right, a + j + 1))
    for i in range(count - 2):
        for j in range(across):
            a, b = 1 + i * (across + 1) + j, 1 + (i + 1) * (across + 1) + j
            faces.append((a, b, b + 1, a + 1))
    # Rear solid fan closes the outline behind the skin without a flat front.
    outline = [0] + [1 + i * (across + 1) for i in range(count - 1)] + [right]
    outline += [1 + i * (across + 1) + across for i in reversed(range(count - 1))]
    back = len(vertices)
    vertices.append((0, -.066, 1.496 if upper else 1.492))
    for i in range(len(outline)):
        faces.append((back, outline[(i + 1) % len(outline)], outline[i]))
    return _mesh(name, vertices, faces, material, collection)


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


def _scalp(material, collection):
    sides, rows = 56, 12
    vertices = [(0, .017, 1.700)]
    faces = []
    for row in range(1, rows + 1):
        t = row / rows
        for j in range(sides):
            theta = TAU * j / sides
            front = max(0, math.cos(theta))
            boundary = 2.12 - 1.19 * front ** 1.9
            # Asymmetric part and swept low edge at the viewer's left temple.
            boundary += .14 * max(0, -math.sin(theta)) * front
            phi = t * boundary
            ripple = 1 + .012 * math.sin(7 * theta + 2 * phi)
            z = 1.557 + .143 * math.cos(phi)
            # The face is a shaped cranium rather than an ellipsoid. Fit the
            # hair shell to those actual ring dimensions: the former spherical
            # cap sat inside the wide upper skull and exposed bald side patches.
            width, front_depth, back_depth, shift = _hair_cranium(z - .004)
            x = (width + .0045) * math.sin(theta) * ripple
            cy = math.cos(theta)
            if cy >= 0:
                y = shift - (front_depth + .0045) * cy ** .43 * ripple
            else:
                y = shift - (back_depth + .0060) * cy * ripple
            vertices.append((x, y, min(1.700, z)))
    for j in range(sides):
        faces.append((0, 1 + (j + 1) % sides, 1 + j))
    for row in range(rows - 1):
        a, b = 1 + row * sides, 1 + (row + 1) * sides
        for j in range(sides):
            n = (j + 1) % sides
            faces.append((a + j, a + n, b + n, b + j))
    # Hidden closing surface sits inside cranium; exterior scalp has real depth.
    back = len(vertices)
    vertices.append((0, .022, 1.565))
    a = 1 + (rows - 1) * sides
    for j in range(sides):
        faces.append((back, a + j, a + (j + 1) % sides))
    return _mesh("Arin_Hair_SculptedScalp", vertices, faces, material, collection)


def _hair_cranium(z):
    """Upper head dimensions for a continuous shell outside the skin surface."""
    profile = [
        (1.475, .058, .069, .053, .003),
        (1.488, .064, .071, .058, .004),
        (1.499, .068, .073, .063, .004),
        (1.510, .071, .074, .068, .005),
        (1.520, .074, .077, .074, .006),
        (1.533, .077, .078, .078, .007),
        (1.540, .078, .080, .081, .007),
        (1.550, .078, .082, .082, .008),
        (1.557, .077, .083, .084, .008),
        (1.565, .076, .082, .086, .008),
        (1.574, .076, .082, .087, .008),
        (1.583, .077, .082, .088, .009),
        (1.595, .079, .082, .089, .010),
        (1.610, .080, .083, .090, .011),
        (1.628, .079, .083, .091, .012),
        (1.645, .076, .080, .089, .014),
        (1.660, .068, .073, .083, .016),
        (1.674, .055, .061, .070, .017),
        (1.684, .040, .045, .052, .018),
        (1.692, .021, .024, .028, .019),
        (1.695, .000, .000, .000, .019),
    ]
    if z >= profile[-1][0]:
        return profile[-1][1:]
    if z <= profile[0][0]:
        return profile[0][1:]
    for i in range(len(profile) - 1):
        low, high = profile[i], profile[i + 1]
        if low[0] <= z <= high[0]:
            t = (z - low[0]) / (high[0] - low[0])
            return tuple(low[j] + (high[j] - low[j]) * t for j in range(1, 5))
    raise AssertionError("Hair profile interpolation missed a valid height")


def _hair(materials, collection):
    objects = [_scalp(materials["hair"], collection)]
    # Broad chestnut locks sweep from an off-centre part, each with elliptical
    # volume and an embedded root. Paths deliberately leave both eyes exposed.
    locks = [
        [( .026, -.008, 1.691), (-.010, -.065, 1.677), (-.045, -.082, 1.651), (-.071, -.058, 1.612), (-.079, -.015, 1.563), (-.070, -.012, 1.523)],
        [( .019, .004, 1.693), (-.024, -.046, 1.682), (-.065, -.052, 1.653), (-.080, -.023, 1.614), (-.077, .015, 1.551), (-.071, .010, 1.502)],
        [( .009, .025, 1.692), (-.045, -.010, 1.678), (-.076, .012, 1.645), (-.083, .041, 1.590), (-.071, .051, 1.522), (-.067, .027, 1.477)],
        [( .027, -.017, 1.684), ( .052, -.056, 1.664), ( .071, -.060, 1.633), ( .081, -.026, 1.585), ( .075, -.020, 1.540), ( .078, -.039, 1.515)],
        [( .038, .004, 1.690), ( .068, -.021, 1.662), ( .086, .009, 1.616), ( .084, .049, 1.565), ( .068, .054, 1.519), ( .053, .045, 1.486)],
        [( .030, .028, 1.692), ( .058, .067, 1.663), ( .066, .090, 1.619), ( .043, .100, 1.555), (-.016, .080, 1.502), (-.064, .027, 1.461)],
        [( .002, .042, 1.693), (-.030, .086, 1.660), (-.053, .102, 1.612), (-.063, .083, 1.547), (-.071, .049, 1.491), (-.077, .014, 1.451)],
    ]
    for index, path in enumerate(locks):
        path = [(x, y, z - .0015) for x, y, z in path]
        objects.append(_lock("Arin_Hair_Lock_%02d" % index, path,
                             .014 if index < 3 else .0115, .0058,
                             materials["hair"], collection, steps=3))
    # Narrow warm highlights follow surface flow rather than painted textures.
    for index in (0, 1, 3, 5, 6):
        path = [(x, y - .0028 if y < .035 else y + .0028, z + .002)
                for x, y, z in locks[index]]
        path[0] = (path[0][0], path[0][1], min(1.698, path[0][2]))
        objects.append(_lock("Arin_Hair_Highlight_%02d" % index, path,
                             .0014, .0009, materials["hair_light"], collection,
                             steps=3, sides=6))
    # Soft loose temple wisps, mostly side silhouette with fine tapering tips.
    wisps = [
        [(-.065, -.056, 1.615), (-.086, -.053, 1.575), (-.085, -.028, 1.538), (-.074, -.039, 1.502)],
        [( .077, -.034, 1.596), ( .087, -.050, 1.563), ( .083, -.029, 1.530), ( .065, -.041, 1.509)],
    ]
    for index, path in enumerate(wisps):
        objects.append(_lock("Arin_Hair_TempleWisp_%d" % index, path,
                             .005, .0028, materials["hair"], collection,
                             steps=4, sides=8))
    # Gathered upper plait is assigned to the head. Lower interlaced strands
    # follow the upper chest bone rather than stretching down from the skull.
    objects.append(_lock("Arin_Braid_Gather", [(-.071, .027, 1.490),
                        (-.080, -.005, 1.448), (-.094, -.042, 1.411),
                        (-.106, -.074, 1.391)], .019, .014,
                        materials["hair"], collection, steps=4, sides=12))
    length, segments = .224, 40
    for strand in range(3):
        points, widths, depths = [], [], []
        for i in range(segments + 1):
            t = i / segments
            phase = strand * TAU / 3 + t * TAU * 3.6
            centre_x = -.106 + .008 * t * t
            centre_y = -.072 - .053 * math.sin(t * math.pi / 2)
            z = 1.390 - length * t
            spread = .0132 * (1 - .34 * t)
            points.append((centre_x + spread * math.cos(phase),
                           centre_y + .0065 * math.sin(phase), z))
            width = .0086 * (1 - .27 * t)
            widths.append(width)
            depths.append(width * .69)
        objects.append(_tube("Arin_Braid_Interlace_%d" % strand, points,
                             widths, depths, materials["hair"], collection,
                             sides=8, bone="spine_03"))
        # A thin fibre along each interlaced lobe breaks uniform cylinder shading.
        highlight = [(x, y - .004, z) for x, y, z in points]
        objects.append(_tube("Arin_Braid_Fibre_%d" % strand, highlight,
                             .0010, .0007, materials["hair_light"], collection,
                             sides=5, bone="spine_03"))
    tie_points = []
    for i in range(25):
        angle = TAU * i / 24
        tie_points.append((-.098 + .014 * math.cos(angle),
                           -.125 + .009 * math.sin(angle), 1.165))
    # Closed-loop sweep welds the seam and omits overlapping end caps.
    objects.append(_tube("Arin_Braid_Tie", tie_points, .0021, .0021,
                         materials["metal_gold"], collection, sides=8,
                         bone="spine_03"))
    for index in range(5):
        delta = (index - 2) * .0045
        objects.append(_lock("Arin_Braid_Tassel_%d" % index,
                        [(-.098 + delta * .7, -.126, 1.166),
                         (-.099 + delta, -.133, 1.141),
                         (-.095 + delta * 1.25, -.134 + abs(delta) * .3, 1.116),
                         (-.100 + delta * 1.5, -.129, 1.1005 + abs(delta))],
                        .0054, .0030, materials["hair_light"] if index == 1
                        else materials["hair"], collection, "spine_03",
                        steps=3, sides=8))
    return objects


def build_head(materials, collection=None):
    """Return editable adult female head/face/hair meshes; no scene side effects.

    Required materials: skin, hair, hair_light, eye_white, iris, pupil, lip,
    metal_gold. Materials are reused and never modified. The caller owns scene
    lighting, rigging, export and the neck/body. Expected triangulated budget
    is 17,754 triangles, below the 18,000 head limit.
    """
    required = ("skin", "hair", "hair_light", "eye_white", "iris", "pupil",
                "lip", "metal_gold")
    missing = [key for key in required if key not in materials]
    if missing:
        raise ValueError("Arin head materials missing: " + ", ".join(missing))
    collection = collection if collection is not None else bpy.context.collection
    objects = [_head_surface(materials["skin"], collection)]
    for side in (-1, 1):
        objects.append(_ear(side, materials["skin"], collection))
        objects.append(_almond_eye(side, materials["eye_white"], collection))
        objects.append(_orbital_skin(side, materials["skin"], collection))
        objects.extend(_eyelids(side, materials["skin"], collection))
        cx, cz = side * .0330, 1.5652
        front = _eye_front(cx, cz, side)
        objects.append(_disc_dome("Arin_Iris_" + str(side),
                       (cx, front - .0002, cz), .0051, .0051, .00065,
                       materials["iris"], collection, rings=3, sides=24))
        objects.append(_disc_dome("Arin_Pupil_" + str(side),
                       (cx, front - .00090, cz), .0023, .0023, .0003,
                       materials["pupil"], collection, rings=2, sides=20))
        # Tiny corneal glint catches light even under plain viewport materials.
        objects.append(_disc_dome("Arin_EyeGlint_" + str(side),
                       (cx - .0015, front - .0013, cz + .0017),
                       .00065, .00065, .00018,
                       materials["eye_white"], collection, rings=1, sides=10))
        objects.append(_brow(side, materials["hair"], collection))
        objects.append(_disc_dome("Arin_Nostril_" + str(side),
                       (side * .0083, -.097, 1.5288), .0022, .0009, .0002,
                       materials["lip"], collection, rings=1, sides=16))
    objects.append(_lip("Arin_Lip_Upper", True, materials["lip"], collection))
    objects.append(_lip("Arin_Lip_Lower", False, materials["lip"], collection))
    objects.extend(_hair(materials, collection))
    return objects
