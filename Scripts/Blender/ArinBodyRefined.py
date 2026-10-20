"""Arin v2: fitted authored body/costume with anatomical and drape refinement.

Blender entry point: ``build_body(materials, collection=None)``.
Coordinates are metres, Z up, front -Y; the adult character is 1.70 m tall.
This module deliberately owns no scene, rig, camera, export, or head creation.
All returned objects are meshes; anatomical pieces carry ``arin_region`` and
accessories carry ``arin_bone`` for the caller's skinning pass.
"""

import math

import bpy
from mathutils import Vector


class _BodyBuilder:
    def __init__(self, materials, collection):
        self.materials = materials
        self.collection = collection or bpy.context.scene.collection
        self.objects = []

    def mesh(self, name, vertices, faces, material, region=None, bone=None):
        data = bpy.data.meshes.new(name + "_Mesh")
        data.from_pydata(vertices, [], faces)
        data.update()
        obj = bpy.data.objects.new(name, data)
        self.collection.objects.link(obj)
        chosen_material = self.materials.get(material)
        if chosen_material is not None:
            data.materials.append(chosen_material)
        for polygon in data.polygons:
            polygon.use_smooth = True
        if region:
            obj["arin_region"] = region
        if bone:
            obj["arin_bone"] = bone
        obj["arin_geometry"] = "authored_volume"
        self.objects.append(obj)
        return obj

    def loft(self, name, rings, material, region=None, bone=None, sides=24, axis=None, warp=None, frame_reference=None):
        """Closed elliptical loft: (centre_xyz, radius_u, radius_v) per ring."""
        vertices, faces = [], []
        centres = [Vector(ring[0]) for ring in rings]
        for index, (centre, radius_u, radius_v) in enumerate(rings):
            previous = centres[max(0, index - 1)]
            following = centres[min(len(centres) - 1, index + 1)]
            tangent = Vector(axis).normalized() if axis is not None else (following - previous).normalized()
            # Select a perpendicular basis even for brooch/rivet lofts on Y.
            if frame_reference is not None:
                reference = Vector(frame_reference)
            elif abs(tangent.z) > 0.7:
                reference = Vector((1, 0, 0))
            elif abs(tangent.x) > 0.7:
                reference = Vector((0, 1, 0))
            else:
                reference = Vector((1, 0, 0))
            axis_u = (reference - tangent * reference.dot(tangent)).normalized()
            axis_v = tangent.cross(axis_u).normalized()
            for side in range(sides):
                theta = 2 * math.pi * side / sides
                point = Vector(centre) + axis_u * (radius_u * math.cos(theta)) + axis_v * (radius_v * math.sin(theta))
                if warp:
                    point = warp(point, theta, index)
                vertices.append(tuple(point))
        for ring_index in range(len(rings) - 1):
            for side in range(sides):
                nxt = (side + 1) % sides
                a = ring_index * sides + side
                b = ring_index * sides + nxt
                faces.append((a, b, b + sides, a + sides))
        # Fan caps are confined to concealed ends, with no deforming n-gons.
        vertices.extend([tuple(centres[0]), tuple(centres[-1])])
        bottom_centre, top_centre = len(vertices) - 2, len(vertices) - 1
        top_ring = (len(rings) - 1) * sides
        for side in range(sides):
            nxt = (side + 1) % sides
            faces.append((bottom_centre, nxt, side))
            faces.append((top_centre, top_ring + side, top_ring + nxt))
        return self.mesh(name, vertices, faces, material, region, bone)

    def tube(self, name, points, radius, material, region=None, bone=None, sides=8):
        if isinstance(radius, (int, float)):
            radii = [radius] * len(points)
        else:
            radii = radius
        return self.loft(name, [(point, value, value) for point, value in zip(points, radii)], material, region, bone, sides)

    def shell(self, name, grid, material, thickness=0.004, region=None, bone=None):
        """Thick, closed cloth patch from a rectangular surface grid."""
        rows, cols = len(grid), len(grid[0])
        vertices = [tuple(point) for row in grid for point in row]
        for row_index in range(rows):
            for col_index in range(cols):
                point = Vector(grid[row_index][col_index])
                across = Vector(grid[row_index][min(cols - 1, col_index + 1)]) - Vector(grid[row_index][max(0, col_index - 1)])
                along = Vector(grid[min(rows - 1, row_index + 1)][col_index]) - Vector(grid[max(0, row_index - 1)][col_index])
                normal = across.cross(along).normalized()
                vertices.append(tuple(point - normal * thickness))
        count = rows * cols
        faces = []
        for row_index in range(rows - 1):
            for col_index in range(cols - 1):
                a = row_index * cols + col_index
                quad = (a, a + 1, a + 1 + cols, a + cols)
                faces.append(quad)
                faces.append(tuple(value + count for value in reversed(quad)))
        boundary = list(range(cols))
        boundary += [row_index * cols + cols - 1 for row_index in range(1, rows)]
        boundary += [(rows - 1) * cols + col_index for col_index in range(cols - 2, -1, -1)]
        boundary += [row_index * cols for row_index in range(rows - 2, 0, -1)]
        for index, a in enumerate(boundary):
            b = boundary[(index + 1) % len(boundary)]
            faces.append((a, a + count, b + count, b))
        return self.mesh(name, vertices, faces, material, region, bone)

    def arc_cloth(self, name, sections, angle_start, angle_end, material, region="torso", columns=25, thickness=0.004, armhole_drop=0.0):
        grid = []
        for z, radius_x, radius_y, centre_y in sections:
            row = []
            for index in range(columns):
                theta = angle_start + (angle_end - angle_start) * index / (columns - 1)
                hanging = max(0.0, min(1.0, (1.04 - z) / 0.28))
                fold = 0.0038 * math.sin(theta * 4.0 + z * 6.0) * hanging
                fold += 0.0016 * math.sin(theta * 7.0 + z * 14.0)
                hem_shift = -0.009 * hanging ** 4 * (0.5 + 0.5 * math.sin(theta * 3 + 0.8))
                # Only the waistcoat uses this fitted, curved arm opening. The
                # top side edge stays clear of the moving proximal sleeve.
                shoulder_blend = max(0.0, min(1.0, (z - 1.231) / 0.108))
                opening = armhole_drop * abs(math.cos(theta)) ** 8 * shoulder_blend ** 2
                fit_z = z - opening
                fit_rx, fit_ry, fit_y = radius_x, radius_y, centre_y
                if opening > 0:
                    # Lowered armhole points must follow the wider torso at
                    # their new height instead of sinking through the shirt.
                    for a, b in zip(sections, sections[1:]):
                        if a[0] <= fit_z <= b[0]:
                            t = (fit_z - a[0]) / (b[0] - a[0])
                            fit_rx, fit_ry, fit_y = [a[i] * (1 - t) + b[i] * t for i in (1, 2, 3)]
                            break
                row.append(((fit_rx + fold) * math.cos(theta), fit_y + (fit_ry + fold) * math.sin(theta), fit_z + hem_shift))
            grid.append(row)
        return self.shell(name, grid, material, thickness, region=region)

    def leaf(self, name, centre, direction, length=0.014, width=0.004, bone="spine_03", region=None):
        """Small raised metal/embroidered leaf in the XZ plane."""
        centre = Vector(centre)
        axis = Vector(direction).normalized()
        across = Vector((axis.z, 0, -axis.x)).normalized()
        normal = Vector((0, -1, 0))
        outline = [centre - axis * length * 0.5,
                   centre - axis * length * 0.15 + across * width,
                   centre + axis * length * 0.30 + across * width * 0.55,
                   centre + axis * length * 0.5,
                   centre + axis * length * 0.30 - across * width * 0.55,
                   centre - axis * length * 0.15 - across * width]
        vertices = [tuple(point) for point in outline]
        vertices += [tuple(point + normal * 0.0012) for point in outline]
        vertices.append(tuple(centre + normal * 0.0024))
        faces = [(0, 2, 1), (0, 3, 2), (0, 4, 3), (0, 5, 4)]
        for index in range(6):
            nxt = (index + 1) % 6
            faces.append((index, nxt, nxt + 6, index + 6))
            faces.append((index + 6, nxt + 6, 12))
        return self.mesh(name, vertices, faces, "metal_gold", region=region, bone=None if region else bone)


def _vertical_sections(sections, x=0.0):
    return [((x, centre_y, z), radius_x, radius_y) for z, radius_x, radius_y, centre_y in sections]


def _rounded_rectangle(builder, name, centre, width, height, radius, material, bone, front_y=None):
    cx, cy, cz = centre
    cy = cy if front_y is None else front_y
    points = []
    for corner_x, corner_z, start in [(cx + width / 2 - radius, cz + height / 2 - radius, 0),
                                      (cx - width / 2 + radius, cz + height / 2 - radius, 90),
                                      (cx - width / 2 + radius, cz - height / 2 + radius, 180),
                                      (cx + width / 2 - radius, cz - height / 2 + radius, 270)]:
        for step in range(5):
            angle = math.radians(start + 90 * step / 4)
            points.append((corner_x + radius * math.cos(angle), cy, corner_z + radius * math.sin(angle)))
    points.append(points[0])
    return builder.tube(name, points, 0.0023, material, bone=bone, sides=8)


def _build_torso(builder):
    # Adult proportions and separate chest/waist/hip profiles under the costume.
    # Concealed shirt hip fabric tucks inside the split coat instead of
    # protruding as pale patches below the belt. The visible centre stays.
    torso = [(0.940, 0.120, 0.056, 0.003), (0.969, 0.124, 0.058, 0.003),
             (1.004, 0.118, 0.064, 0.001), (1.043, 0.114, 0.064, -0.002),
             (1.078, 0.109, 0.067, -0.003), (1.110, 0.105, 0.065, -0.004),
             (1.140, 0.112, 0.069, -0.005), (1.171, 0.123, 0.075, -0.006),
             (1.203, 0.135, 0.079, -0.008), (1.231, 0.141, 0.084, -0.008),
             (1.260, 0.143, 0.085, -0.007), (1.286, 0.148, 0.079, -0.004),
             (1.309, 0.151, 0.074, 0.000), (1.328, 0.146, 0.071, 0.003),
             (1.339, 0.137, 0.069, 0.005), (1.346, 0.121, 0.064, 0.007)]
    def concealed_rear_tuck(point, theta, ring_index):
        # Only the hidden rear shirt hem changes. Its former upper corners
        # showed as two ivory points in the actual rear slit below the belt.
        depth = (0.011, 0.010, 0.014)[ring_index] if ring_index < 3 else 0.0
        rear = max(0.0, math.sin(theta))
        return Vector((point.x, point.y - depth * rear * rear, point.z))
    builder.loft("Arin_IvoryBlouse", _vertical_sections(torso), "cloth_ivory", region="torso", sides=32,
                 axis=(0, 0, 1), warp=concealed_rear_tuck)
    green_sections = [(z, rx + 0.003, ry + 0.003, cy) for z, rx, ry, cy in torso[3:-1]]
    builder.arc_cloth("Arin_FittedGreenWaistcoat", green_sections, -math.pi / 2 + 0.34,
                      3 * math.pi / 2 - 0.34, "cloth_green", columns=30, armhole_drop=0.052)
    # A true open front reveals the ivory blouse. Raised borders follow the volume.
    for sign in (-1, 1):
        theta = -math.pi / 2 + sign * 0.34
        points = [((rx + 0.0045) * math.cos(theta), cy + (ry + 0.0055) * math.sin(theta), z)
                  for z, rx, ry, cy in green_sections]
        builder.tube("Arin_VestPiping_" + str(sign), points, 0.0016, "cloth_green_dark", region="torso")
    # The throat and neckline are volumes; no hidden billboard anatomy.
    neck = [(1.340, 0.047, 0.044, 0.011), (1.355, 0.045, 0.043, 0.010),
            (1.378, 0.041, 0.039, 0.010), (1.405, 0.037, 0.036, 0.008),
            (1.432, 0.037, 0.035, 0.007), (1.445, 0.034, 0.030, 0.009),
            (1.455, 0.030, 0.025, 0.010)]
    builder.loft("Arin_Neck", _vertical_sections(neck), "skin", region="neck", sides=28, axis=(0, 0, 1))
    # Two small blouse collar folds frame the neck instead of a flat painted collar.
    for sign in (-1, 1):
        grid = []
        for row in range(5):
            v = row / 4
            grid.append([(sign * (0.040 + 0.030 * u / 5 + 0.006 * v),
                          -0.041 - 0.036 * v - 0.007 * u / 5,
                          1.361 - 0.043 * v - 0.015 * u / 5)
                         for u in range(6)])
        builder.shell("Arin_BlouseCollar_" + str(sign), grid, "cloth_ivory", region="torso")
    # Three laced pairs and a split opening are modeled on the visible front.
    for index in range(3):
        z = 1.302 - index * 0.019
        builder.tube("Arin_BlouseLace_%02d" % index,
                     [(-0.013, -0.085, z + 0.008), (0.013, -0.091, z - 0.008)],
                     0.0015, "leather_dark", region="torso", sides=6)
        builder.tube("Arin_BlouseLaceBack_%02d" % index,
                     [(0.013, -0.085, z + 0.008), (-0.013, -0.091, z - 0.008)],
                     0.0015, "leather_dark", region="torso", sides=6)

    # Separate coat skirts leave leg clearance, a front opening, and a rear split.
    tails = [(0.765, 0.170, 0.098, 0.010), (0.799, 0.165, 0.096, 0.009),
             (0.842, 0.158, 0.093, 0.008), (0.892, 0.151, 0.090, 0.007),
             (0.946, 0.144, 0.085, 0.006), (0.996, 0.137, 0.080, 0.003),
             (1.037, 0.125, 0.076, -0.001)]
    for suffix, start, end in [("L", -1.18, 1.31), ("R", 1.83, 4.32)]:
        tail_start = len(builder.objects)
        builder.arc_cloth("Arin_GreenSplitCoatTail_" + suffix, tails, start, end,
                          "cloth_green", columns=18, thickness=0.005)
        theta = start if suffix == "L" else end
        builder.tube("Arin_TailFrontBorder_" + suffix,
                     [((rx + 0.002) * math.cos(theta), cy + (ry + 0.003) * math.sin(theta), z)
                      for z, rx, ry, cy in tails], 0.0017, "metal_gold", region="torso")
        z, rx, ry, cy = tails[0]
        builder.tube("Arin_TailHem_" + suffix,
                     [(rx * math.cos(start + (end - start) * t / 20),
                       cy + ry * math.sin(start + (end - start) * t / 20), z + 0.005)
                      for t in range(21)], 0.0015, "metal_gold", region="torso")
        # Keep each skirt panel and its sewn trim on the same leg even at the
        # inner edge. The caller applies ArinCoatSkin after the common bind.
        for obj in builder.objects[tail_start:]:
            obj["arin_coat_side"] = suffix.lower()
    apron_start = len(builder.objects)
    apron_grid = []
    for row in range(9):
        v = row / 8
        z = 0.759 + v * 0.281
        width = 0.067 - 0.012 * v
        apron_grid.append([((u / 8 * 2 - 1) * width,
                            -0.103 + 0.022 * v + 0.006 * (u / 8 * 2 - 1) ** 2,
                            z + 0.004 * math.cos(u / 8 * math.pi * 2)) for u in range(9)])
    builder.shell("Arin_IvoryFrontApron", apron_grid, "cloth_ivory", thickness=0.004, region="torso")
    builder.tube("Arin_ApronEmbroideredHem", [(x, -0.105, 0.775) for x in [-0.059, -0.03, 0, 0.03, 0.059]],
                 0.0012, "cloth_green", region="torso", sides=6)
    for sign in (-1, 1):
        builder.tube("Arin_ApronVine_" + str(sign),
                     [(sign * 0.010, -0.106, 0.777), (sign * 0.027, -0.106, 0.790),
                      (sign * 0.040, -0.104, 0.816)], 0.001, "cloth_green", region="torso", sides=6)
        for index in range(3):
            builder.leaf("Arin_ApronLeaf_%s_%s" % (sign, index),
                         (sign * (0.020 + index * 0.010), -0.108, 0.787 + index * 0.013),
                         (sign * (0.75 if index % 2 == 0 else -0.45), 0, 0.8),
                         length=0.012, width=0.003, region="torso")
    # The central panel stays continuous; the caller blends the left/right
    # thighs across X rather than splitting this fabric into capped halves.
    for obj in builder.objects[apron_start:]:
        obj["arin_coat_center"] = True


def _build_fingernail(builder, name, sample, centre_t, length_fraction, width, region):
    """A rounded nail follows the actual fingertip section, with its base buried."""
    def nail_point(longitudinal, across, buried=False):
        t = centre_t + longitudinal * length_fraction * 0.5
        centre, radius = sample(t)
        previous, _ = sample(max(0, t - 0.0001))
        following, _ = sample(min(1, t + 0.0001))
        tangent = (following - previous).normalized()
        reference = Vector((0, 1, 0))
        axis_u = (reference - tangent * reference.dot(tangent)).normalized()
        axis_v = tangent.cross(axis_u).normalized()
        transverse = across * width * 0.5
        radial_depth = math.sqrt(max(radius ** 2 - transverse ** 2, radius ** 2 * 0.25))
        # 0.12mm proud at the edge, with a shallow centre crown; the underside
        # is 0.18mm inside skin, eliminating the old raised rectangular block.
        rise = -0.00018 if buried else 0.00012 + 0.00008 * (1 - across ** 2)
        return tuple(centre - axis_u * (radial_depth + rise) + axis_v * transverse)
    front = [nail_point(0, 0)]
    for scale in (0.55, 1.0):
        front.extend(nail_point(scale * math.cos(math.tau * i / 8),
                                scale * math.sin(math.tau * i / 8)) for i in range(8))
    vertices = front + [nail_point(0, 0, True)]
    for scale in (0.55, 1.0):
        vertices.extend(nail_point(scale * math.cos(math.tau * i / 8),
                                   scale * math.sin(math.tau * i / 8), True) for i in range(8))
    faces = []
    for i in range(8):
        nxt = (i + 1) % 8
        faces.extend([(0, 1 + i, 1 + nxt),
                      (1 + i, 9 + i, 9 + nxt, 1 + nxt),
                      (17, 18 + nxt, 18 + i),
                      (18 + i, 18 + nxt, 26 + nxt, 26 + i),
                      (9 + i, 26 + i, 26 + nxt, 9 + nxt)])
    nail = builder.mesh(name, vertices, faces, "skin", region=region)
    nail["arin_surface"] = "fingernail_skin_atlas_region"
    nail["arin_motion_scope"] = "hand_bone_only_no_individual_finger_gesture"


def _build_arms_and_hands(builder):
    for sign, suffix in [(1, "l"), (-1, "r")]:
        region, hand_region = "arm_" + suffix, "hand_" + suffix
        skin_sections = [(0.150, 0.032, 0.036), (0.195, 0.039, 0.040),
                         (0.255, 0.035, 0.035), (0.325, 0.029, 0.030),
                         (0.398, 0.025, 0.026), (0.430, 0.025, 0.026),
                         (0.472, 0.028, 0.029), (0.510, 0.027, 0.029),
                         (0.556, 0.024, 0.025), (0.604, 0.020, 0.021),
                         (0.651, 0.016, 0.018)]
        builder.loft("Arin_AnatomicalArm_" + suffix,
                     [((sign * x, 0.003, 1.318), ry, rz) for x, ry, rz in skin_sections],
                     "skin", region=region, sides=20)
        # The concealed sleeve head tapers into the shoulder seam. It remains
        # outside the anatomical arm, without an inflated cap under the cape.
        sleeve = [(0.153, 0.036, 0.040), (0.174, 0.041, 0.044),
                  (0.200, 0.046, 0.048), (0.229, 0.049, 0.053),
                  (0.263, 0.046, 0.050), (0.297, 0.043, 0.048),
                  (0.331, 0.041, 0.046), (0.366, 0.040, 0.044),
                  (0.389, 0.041, 0.044), (0.406, 0.044, 0.046),
                  (0.418, 0.038, 0.041), (0.431, 0.043, 0.045),
                  (0.443, 0.036, 0.040), (0.454, 0.039, 0.043),
                  (0.466, 0.036, 0.038)]
        def sleeve_fold(point, theta, ring_index):
            x = abs(point.x)
            elbow = math.exp(-((x - 0.418) / 0.078) ** 2)
            shoulder = math.exp(-((x - 0.207) / 0.057) ** 2)
            fold = 0.0035 * elbow * math.sin(theta * 3.0 + x * 58)
            fold += 0.0024 * shoulder * math.sin(theta * 4.0 - x * 33)
            gravity = max(0, -math.sin(theta))
            return point + Vector((0, fold * math.cos(theta), fold * math.sin(theta) - 0.004 * gravity * elbow))
        builder.loft("Arin_IvorySleeve_" + suffix,
                     [((sign * x, 0.002 + 0.002 * math.sin(x * 55), 1.318 - 0.002 * math.sin(x * 44)),
                       ry, rz) for x, ry, rz in sleeve], "cloth_ivory", region=region, sides=24,
                     axis=(sign, 0, 0), warp=sleeve_fold)
        for cuff_index, centre_x in enumerate([0.451, 0.469]):
            cuff_rings = [((sign * (centre_x + offset), 0.002, 1.318), radius, radius * 1.02)
                          for offset, radius in [(-0.008, 0.036), (-0.005, 0.042),
                                                 (0.000, 0.043), (0.005, 0.042), (0.008, 0.036)]]
            builder.loft("Arin_RolledCuff_%s_%d" % (suffix, cuff_index), cuff_rings,
                         "cloth_ivory", region=region, sides=24)
        # Bracers sit outside the forearm and use modeled seams, straps and rivets.
        bracer = [(0.523, 0.029, 0.031), (0.529, 0.030, 0.032),
                  (0.567, 0.028, 0.030), (0.602, 0.025, 0.027),
                  (0.625, 0.023, 0.025), (0.631, 0.023, 0.024)]
        builder.loft("Arin_LeatherBracer_" + suffix,
                     [((sign * x, 0.002, 1.318), ry, rz) for x, ry, rz in bracer],
                     "leather", region=region, sides=24)
        builder.tube("Arin_BracerFrontSeam_" + suffix,
                     [(sign * x, -ry - 0.001, 1.326) for x, ry, rz in bracer],
                     0.0014, "leather_dark", region=region)
        for strap_index, (centre_x, ry, rz) in enumerate([(0.535, 0.031, 0.033), (0.615, 0.026, 0.028)]):
            builder.loft("Arin_BracerStrap_%s_%d" % (suffix, strap_index),
                         [((sign * (centre_x + delta), 0.002, 1.318), ry, rz)
                          for delta in [-0.005, 0.005]], "leather_dark", region=region, sides=24)
            builder.loft("Arin_BracerRivet_%s_%d" % (suffix, strap_index),
                         [((sign * centre_x, y, 1.327), radius, radius)
                          for y, radius in [(-ry - 0.002, 0.003), (-ry - 0.005, 0.0035), (-ry - 0.006, 0.001)]],
                         "metal_gold", region=region, sides=10)

        # Palm faces front (-Y); finger spread is in Z, with four digits and thumb.
        palm = [((sign * 0.645, 0.001, 1.318), 0.0140, 0.0175),
                ((sign * 0.657, -0.001, 1.318), 0.0140, 0.0200),
                ((sign * 0.669, -0.002, 1.319), 0.0145, 0.0250),
                ((sign * 0.680, -0.003, 1.319), 0.0155, 0.0280),
                ((sign * 0.692, -0.002, 1.319), 0.0145, 0.0290),
                ((sign * 0.701, -0.001, 1.318), 0.0130, 0.0290),
                ((sign * 0.710, 0.000, 1.317), 0.0110, 0.0270),
                ((sign * 0.718, 0.001, 1.317), 0.0060, 0.0245)]
        def palm_planes(point, theta, ring_index):
            # A rounded flat palm with a dorsal plane and thenar volume.
            y = point.y
            if y > 0:
                y *= 0.78
            if abs(point.x) < 0.689 and point.z > 1.328:
                y -= 0.0014 * math.sin(theta) ** 2
            return Vector((point.x, y, point.z))
        builder.loft("Arin_AnatomicalPalm_" + suffix, palm, "skin", region=hand_region,
                     sides=24, axis=(sign, 0, 0), warp=palm_planes)
        # Values are authored joint centres, rather than extruded flat hand outlines.
        digits = [("index", 1.340, 0.784, 0.0072),
                  ("middle", 1.323, 0.793, 0.0075),
                  ("ring", 1.306, 0.783, 0.0069),
                  ("little", 1.290, 0.766, 0.0060)]
        for digit, root_z, tip_x, radius in digits:
            outward = (root_z - 1.318) * 0.28
            length = tip_x - 0.695
            def sample_finger(t):
                ease = t * t * (3 - 2 * t)
                base_z = 1.318 + (root_z - 1.318) * 0.76
                end_z = root_z + outward - 0.002
                centre = Vector((sign * (0.695 + length * t),
                                 -0.0005 - 0.004 * t * t + 0.001 * math.sin(math.pi * t),
                                 base_z * (1 - ease) + end_z * ease))
                taper = 1.03 - 0.19 * t - 0.21 * t * t
                taper += 0.025 * math.exp(-((t - 0.22) / 0.16) ** 2)
                cap_start = 1 - radius * 0.85 / length
                cap = math.sqrt(max(0.075 ** 2, 1 - max(0, (t - cap_start) / (1 - cap_start)) ** 2))
                return centre, radius * taper * cap
            parameters = [i * 0.89 / 12 for i in range(13)] + [0.94, 0.98, 1.0]
            rings = [(tuple(sample_finger(t)[0]), sample_finger(t)[1], sample_finger(t)[1])
                     for t in parameters]
            finger = builder.loft("Arin_Finger_%s_%s" % (suffix, digit), rings,
                                  "skin", region=hand_region, sides=10, frame_reference=(0, 1, 0))
            finger["arin_refinement"] = "single_continuous_smooth_taper_with_rounded_tip"
            _build_fingernail(builder, "Arin_Nail_%s_%s" % (suffix, digit),
                              sample_finger, 0.88, 0.075, radius * 0.75, hand_region)
        thumb_controls = [Vector((sign * x, y, z)) for x, y, z in
                          [(0.674, -0.001, 1.333), (0.688, -0.020, 1.354),
                           (0.716, -0.015, 1.372), (0.735, -0.012, 1.370)]]
        def sample_thumb(t):
            a, b, c, d = thumb_controls
            centre = a * (1 - t) ** 3 + b * (3 * (1 - t) ** 2 * t) + c * (3 * (1 - t) * t * t) + d * t ** 3
            cap = math.sqrt(max(0.075 ** 2, 1 - max(0, (t - 0.91) / 0.09) ** 2))
            return centre, 0.0105 * (0.97 - 0.32 * t - 0.12 * t * t) * cap
        thumb_rings = [(tuple(sample_thumb(t)[0]), sample_thumb(t)[1], sample_thumb(t)[1])
                       for t in parameters]
        thumb = builder.loft("Arin_Finger_%s_thumb" % suffix, thumb_rings,
                             "skin", region=hand_region, sides=10, frame_reference=(0, 1, 0))
        thumb["arin_refinement"] = "continuous_opposed_bezier_thumb_with_rounded_tip"
        _build_fingernail(builder, "Arin_Nail_%s_thumb" % suffix,
                          sample_thumb, 0.82, 0.085, 0.0050, hand_region)


def _build_continuous_trousers(builder):
    """Two leg loops -> a sewn crotch saddle -> one connected waist loop.

    Each thigh's inner semicircle joins a five-column saddle. Its outer
    semicircle plus the saddle's front/back edges form a single 32-edge waist
    boundary. There are no overlapping pelvis/leg caps or hidden centre caps.
    """
    sections = [(0.089, 0.023, 0.024, 0.002, 0.085),
                    (0.190, 0.026, 0.026, 0.006, 0.086),
                    (0.292, 0.032, 0.031, 0.010, 0.087),
                    (0.335, 0.037, 0.036, 0.012, 0.088),
                    (0.377, 0.041, 0.040, 0.015, 0.089),
                    (0.423, 0.043, 0.042, 0.018, 0.091),
                    (0.470, 0.039, 0.038, 0.014, 0.091),
                    (0.507, 0.036, 0.035, 0.004, 0.091),
                    (0.527, 0.037, 0.037, -0.001, 0.092),
                    (0.546, 0.037, 0.038, -0.003, 0.092),
                    (0.560, 0.038, 0.039, -0.004, 0.092),
                    (0.574, 0.039, 0.039, -0.005, 0.092),
                    (0.596, 0.041, 0.041, -0.002, 0.091),
                    (0.617, 0.042, 0.043, 0.000, 0.091),
                    (0.670, 0.047, 0.049, 0.004, 0.090),
                    (0.727, 0.053, 0.055, 0.008, 0.087),
                    (0.785, 0.059, 0.058, 0.009, 0.084),
                    (0.844, 0.064, 0.061, 0.008, 0.081),
                    (0.889, 0.060, 0.060, 0.005, 0.078)]
    vertices, faces, tops = [], [], {}
    sides = 24
    for sign in (1, -1):
        first = len(vertices)
        for z, rx, ry, cy, cx in sections:
            for side in range(sides):
                theta = math.tau * side / sides
                knee = math.exp(-((z - 0.550 - 0.007 * math.cos(theta)) / 0.056) ** 2)
                shin = math.exp(-((z - 0.338) / 0.035) ** 2)
                crease = 0.0029 * knee * math.sin(theta * 2 + z * 91)
                crease += 0.0022 * shin * math.sin(theta * 3 - z * 64)
                vertices.append((sign * cx + (rx + crease) * math.cos(theta),
                                 cy + (ry + crease) * math.sin(theta), z))
        top = []
        for side in range(sides):
            theta = math.tau * side / sides
            top.append(len(vertices))
            # Concealed upper seat stays inside the coat's real folded inner
            # surface, with no brown patches through either back panel.
            vertices.append((sign * 0.074 + 0.055 * math.cos(theta),
                             0.005 + 0.058 * math.sin(theta),
                             0.940 + 0.025 * sign * math.cos(theta)))
        tops[sign] = top
        for ring_index in range(len(sections)):
            for side in range(sides):
                nxt = (side + 1) % sides
                a, b = first + ring_index * sides + side, first + ring_index * sides + nxt
                faces.append((a, b, b + sides, a + sides))
        vertices.append((sign * sections[0][4], sections[0][3], sections[0][0]))
        cap = len(vertices) - 1
        for side in range(sides):
            faces.append((cap, first + (side + 1) % sides, first + side))
    saddle = []
    for row in range(13):
        left, right = tops[1][6 + row], tops[-1][(6 - row) % sides]
        a, b = Vector(vertices[left]), Vector(vertices[right])
        line = [left]
        for column in (1, 2, 3):
            t = column / 4
            point = a * (1 - t) + b * t
            point += Vector((0, 0, -0.012 * (1 - (2 * t - 1) ** 2) * math.sin(row * math.pi / 12) ** 2))
            line.append(len(vertices))
            vertices.append(tuple(point))
        line.append(right)
        saddle.append(line)
    for row in range(12):
        for column in range(4):
            faces.append((saddle[row][column], saddle[row + 1][column],
                          saddle[row + 1][column + 1], saddle[row][column + 1]))
    boundary = [tops[1][index % sides] for index in range(18, 31)]
    boundary += saddle[0][1:4]
    boundary += [tops[-1][index] for index in range(6, 19)]
    boundary += list(reversed(saddle[-1][1:4]))
    if len(boundary) != 32:
        raise RuntimeError("Continuous trouser waist has the wrong boundary size")
    angles = [math.atan2(vertices[index][1] - 0.005, vertices[index][0]) for index in boundary]
    previous = boundary
    for z, rx, ry, cy in [(0.982, 0.129, 0.065, 0.005),
                           (1.009, 0.121, 0.063, 0.003), (1.042, 0.116, 0.062, 0.000)]:
        row = []
        for theta in angles:
            row.append(len(vertices))
            vertices.append((rx * math.cos(theta), cy + ry * math.sin(theta), z))
        for index, a in enumerate(previous):
            nxt = (index + 1) % len(row)
            faces.append((a, previous[nxt], row[nxt], row[index]))
        previous = row
    vertices.append((0, 0, 1.042))
    cap = len(vertices) - 1
    for index, a in enumerate(previous):
        faces.append((cap, a, previous[(index + 1) % len(previous)]))
    trousers = builder.mesh("Arin_ContinuousSeatCrotchTrousers", vertices, faces,
                            "leather_dark", region="leg_l")
    trousers["arin_bilateral_legs"] = True
    trousers["arin_topology"] = "two_leg_loops_sewn_saddle_single_waist_boundary_no_centre_caps"
    trousers["arin_surface"] = "woven_trouser_fabric"
    return sections


def _build_legs_and_boots(builder):
    sections = _build_continuous_trousers(builder)
    for sign, suffix in [(1, "l"), (-1, "r")]:
        region = "leg_" + suffix
        builder.tube("Arin_TrouserOuterSeam_" + suffix,
                     [(sign * (cx + rx * 0.73), cy - ry * 0.70 - 0.001, z)
                      for z, rx, ry, cy, cx in sections[3:-2]], 0.0012, "leather", region=region, sides=6)
        # Rounded foot shape: real heel, instep and forward toe, on a thick sole.
        foot = [(0.019, 0.040, 0.092, -0.047), (0.028, 0.043, 0.099, -0.049),
                (0.043, 0.044, 0.098, -0.049), (0.058, 0.042, 0.094, -0.046),
                (0.073, 0.039, 0.083, -0.039), (0.088, 0.036, 0.065, -0.022),
                (0.109, 0.032, 0.042, -0.004), (0.135, 0.029, 0.034, 0.004),
                (0.148, 0.031, 0.035, 0.005), (0.158, 0.028, 0.032, 0.006),
                (0.169, 0.030, 0.033, 0.007), (0.207, 0.032, 0.034, 0.009),
                (0.244, 0.035, 0.036, 0.011), (0.277, 0.038, 0.039, 0.012),
                (0.298, 0.039, 0.040, 0.013), (0.309, 0.037, 0.038, 0.013)]
        def boot_form(point, theta, ring_index):
            x = point.x - sign * 0.085
            heel = max(0.0, math.sin(theta))
            toe = max(0.0, -math.sin(theta))
            low = max(0, min(1, (0.13 - point.z) / 0.08))
            x *= 1 - 0.22 * heel * low
            x -= sign * 0.0015 * toe * low
            ankle = math.exp(-((point.z - 0.150) / 0.054) ** 2)
            crease = 0.0023 * ankle * math.sin(theta * 2 + point.z * 98)
            return Vector((sign * 0.085 + x + crease * math.cos(theta),
                           point.y + crease * math.sin(theta), point.z))
        boot = builder.loft("Arin_AnatomicalLeatherBoot_" + suffix,
                     [((sign * 0.085, cy, z), rx, ry) for z, rx, ry, cy in foot],
                     "leather", region=region, sides=28, axis=(0, 0, 1), warp=boot_form)
        boot["arin_refinement"] = "narrow_heel_tapered_toe_sloping_instep_ankle_compression_folds"
        sole = [(0.000, 0.040, 0.094, -0.048), (0.004, 0.045, 0.101, -0.050),
                (0.015, 0.046, 0.102, -0.050), (0.021, 0.043, 0.100, -0.050)]
        builder.loft("Arin_BootSole_" + suffix,
                     [((sign * 0.085, cy, z), rx, ry) for z, rx, ry, cy in sole],
                     "leather_dark", region=region, sides=32, axis=(0, 0, 1), warp=boot_form)
        boot_cuff = [(0.275, 0.039, 0.040, 0.013), (0.282, 0.044, 0.045, 0.013),
                     (0.307, 0.045, 0.046, 0.013), (0.316, 0.042, 0.043, 0.013)]
        builder.loft("Arin_BootFoldedCuff_" + suffix,
                     [((sign * 0.085, cy, z), rx, ry) for z, rx, ry, cy in boot_cuff],
                     "leather", region=region, sides=24, axis=(0, 0, 1))
        builder.tube("Arin_BootFrontSeam_" + suffix,
                     [tuple(boot_form(Vector((sign * 0.085, cy - ry - 0.001, z)),
                                      3 * math.pi / 2, 0)) for z, rx, ry, cy in foot[7:]],
                     0.0013, "leather_dark", region=region, sides=6)
        builder.loft("Arin_BootAnkleStrap_" + suffix,
                     [((sign * 0.085, 0.002, z), 0.034, 0.040) for z in [0.105, 0.117]],
                     "leather_dark", region=region, sides=24)
        # Ankle buckle is rigid to the foot, unlike the long boot shaft.
        _rounded_rectangle(builder, "Arin_BootBuckle_" + suffix,
                           (sign * 0.111, -0.028, 0.112), 0.013, 0.018, 0.002,
                           "metal_gold", "foot_" + suffix)


def _build_cape(builder):
    # A single sewn mantle wraps neck/shoulders with a deep curved back drape.
    # Its X extent never reaches the elbow; the sleeves remain separate.
    start, end = -math.pi / 2 + 0.14, 3 * math.pi / 2 - 0.14
    def mantle_point(v, theta):
        sine = math.sin(theta)
        front, back = max(0.0, -sine), max(0.0, sine)
        rx = 0.052 + 0.153 * v
        ry = 0.048 + (0.075 * front * math.sqrt(v) + 0.107 * back * v ** 0.8
                      + 0.028 * (1 - abs(sine)) * v)
        inner_z = 1.386 + 0.012 * sine
        # Gradual shoulder turnover prevents the front/back slope from cutting
        # through the sleeve head. The back remains a short hanging cape.
        outer_z = 1.382 - 0.086 * front ** 2 - 0.221 * back ** 2.5
        fold = 0.0030 * math.sin(theta * 5 + 0.6) * v * v
        return ((rx + fold) * math.cos(theta),
                0.012 + (ry + fold + 0.013 * back * math.sin(math.pi * v)) * sine,
                inner_z * (1 - v) + outer_z * v + 0.004 * math.sin(theta * 4) * v * v)
    grid = [[mantle_point(row / 9, start + (end - start) * col / 28)
             for col in range(29)] for row in range(10)]
    cape = builder.shell("Arin_SoftContinuousShoulderMantle", grid, "cloth_green", thickness=0.0045, bone="spine_03")
    cape["arin_cape"] = "continuous_neck_shoulders_back_no_arm_web"
    cape["arin_refinement"] = "curved_neck_opening_soft_contour_weighted_back_drape"
    cape["arin_clearance_revision"] = "tucked_collar_sleeve_head_curved_armhole_local_shoulders"
    builder.tube("Arin_CapeSoftGoldHem", grid[-1], 0.00145, "metal_gold", bone="spine_03", sides=6)
    builder.tube("Arin_CapeCollarPiping", grid[0], 0.0011, "cloth_green_dark", bone="spine_03", sides=6)
    for sign in (-1, 1):
        for index, theta in enumerate([-1.08, -0.86, -0.64]):
            x, y, z = mantle_point(0.91, theta)
            builder.leaf("Arin_CapeLeaf_%s_%s" % (sign, index),
                         (sign * abs(x), y - 0.002, z + 0.009),
                         (sign * 0.55, 0, 0.84), length=0.014, width=0.0034)
    # Circular brooch, a raised leaf and a pin are independent solid geometry.
    builder.loft("Arin_CapePinnedBrooch",
                 [((0.0, y, 1.347), radius, radius)
                  for y, radius in [(-0.084, 0.014), (-0.090, 0.017), (-0.096, 0.017), (-0.098, 0.014)]],
                 "metal_gold", bone="spine_03", sides=24)
    builder.loft("Arin_BroochGreenInset",
                 [((0.0, y, 1.347), radius, radius) for y, radius in [(-0.099, 0.0135), (-0.101, 0.0130)]],
                 "cloth_green_dark", bone="spine_03", sides=24)
    builder.tube("Arin_BroochLeafStem", [(0, -0.103, 1.337), (0.001, -0.103, 1.352), (0.005, -0.103, 1.359)],
                 0.001, "metal_gold", bone="spine_03", sides=6)
    for index, sign in enumerate([-1, 1, -1]):
        builder.leaf("Arin_BroochLeaf_%d" % index, (sign * 0.004, -0.103, 1.342 + index * 0.005),
                     (sign * 0.65, 0, 0.75), length=0.007, width=0.0018)


def _build_belt_satchel_potions(builder):
    # Belt is a slim solid ring over the tailored waist.
    builder.loft("Arin_WaistLeatherBelt", _vertical_sections([
        (1.025, 0.130, 0.081, -0.002), (1.030, 0.131, 0.082, -0.002),
        (1.061, 0.124, 0.078, -0.002), (1.067, 0.121, 0.076, -0.002)]),
        "leather", bone="pelvis", sides=40, axis=(0, 0, 1))
    _rounded_rectangle(builder, "Arin_BeltGoldBuckle", (0.003, -0.088, 1.045),
                       0.036, 0.035, 0.004, "metal_gold", "pelvis")
    builder.tube("Arin_BeltBuckleTongue", [(0.003, -0.091, 1.061), (0.004, -0.094, 1.029)],
                 0.0016, "metal_gold", bone="pelvis", sides=8)
    for index in range(4):
        x = -0.038 - 0.018 * index
        y = -0.004 - 0.082 * math.sqrt(max(0, 1 - (x / 0.131) ** 2))
        builder.loft("Arin_BeltStud_%d" % index,
                     [((x, y + delta, 1.047), radius, radius)
                      for delta, radius in [(0.0, 0.002), (-0.002, 0.0025), (-0.003, 0.0015)]],
                     "metal_gold", bone="pelvis", sides=8)

    # The satchel is a closed rounded volume at the outer left hip, with a flap.
    bag_sections = [(0.761, 0.026, 0.021, -0.018), (0.771, 0.040, 0.031, -0.019),
                    (0.791, 0.053, 0.043, -0.020), (0.824, 0.057, 0.048, -0.018),
                    (0.863, 0.055, 0.049, -0.015), (0.900, 0.052, 0.045, -0.012),
                    (0.940, 0.048, 0.040, -0.011), (0.974, 0.043, 0.034, -0.012),
                    (0.983, 0.037, 0.029, -0.012)]
    def bag_form(point, theta, ring_index):
        rx, ry = bag_sections[ring_index][1:3]
        cy = bag_sections[ring_index][3]
        cosine, sine = math.cos(theta), math.sin(theta)
        x = 0.210 + rx * math.copysign(abs(cosine) ** 0.78, cosine)
        y = cy + ry * math.copysign(abs(sine) ** 0.78, sine)
        y += 0.0017 * math.sin(theta * 3 + point.z * 22)
        return Vector((x, y, point.z))
    bag = builder.loft("Arin_StructuredLeatherSatchel", _vertical_sections(bag_sections, x=0.210),
                       "leather", bone="pelvis", sides=28, axis=(0, 0, 1), warp=bag_form)
    bag["arin_refinement"] = "rounded_panel_cross_section_fitted_side_gusset_folded_flap"
    gusset_grid = []
    for row_index, (z, rx, ry, cy) in enumerate(bag_sections):
        row = []
        for column in range(6):
            theta = -0.37 + 0.74 * column / 5
            point = bag_form(Vector((0.210 + rx * math.cos(theta), cy + ry * math.sin(theta), z)), theta, row_index)
            point += Vector((0.0007, 0, 0))
            row.append(tuple(point))
        gusset_grid.append(row)
    builder.shell("Arin_SatchelSewnSideGusset", gusset_grid, "leather_dark", thickness=0.0015, bone="pelvis")
    for column in (0, 5):
        builder.tube("Arin_SatchelGussetSeam_%d" % column,
                     [row[column] for row in gusset_grid], 0.0008, "leather", bone="pelvis", sides=6)
    flap_grid = []
    for row in range(7):
        v = row / 6
        flap_grid.append([(0.210 + (col / 8 * 2 - 1) * (0.039 + 0.005 * math.sin(v * math.pi)),
                           -0.047 - 0.025 * math.sin(v * math.pi / 2)
                           - 0.0018 * math.sin(col / 8 * math.pi) * math.sin(v * math.pi),
                           0.981 - 0.108 * v + 0.022 * (col / 8 * 2 - 1) ** 2 * v)
                          for col in range(9)])
    builder.shell("Arin_SatchelRoundedFrontFlap", flap_grid, "leather", thickness=0.005, bone="pelvis")
    builder.tube("Arin_SatchelFlapStitching",
                 [(0.171, -0.054, 0.974), (0.168, -0.066, 0.933), (0.172, -0.074, 0.896),
                  (0.193, -0.074, 0.876), (0.225, -0.074, 0.876), (0.250, -0.074, 0.896),
                  (0.253, -0.066, 0.933), (0.250, -0.054, 0.974)],
                 0.0009, "leather_dark", bone="pelvis", sides=6)
    _build_bag_stitches(builder)
    builder.loft("Arin_SatchelLatchStrap",
                 [((0.215, y, z), rx, ry) for z, y, rx, ry in
                  [(0.846, -0.072, 0.008, 0.004), (0.855, -0.076, 0.010, 0.004),
                   (0.883, -0.077, 0.009, 0.004), (0.914, -0.074, 0.009, 0.004),
                   (0.934, -0.070, 0.009, 0.004)]], "leather_dark", bone="pelvis", sides=12)
    _rounded_rectangle(builder, "Arin_SatchelGoldBuckle", (0.215, -0.083, 0.886),
                       0.019, 0.025, 0.003, "metal_gold", "pelvis")
    builder.tube("Arin_SatchelBuckleTongue", [(0.215, -0.087, 0.895), (0.215, -0.087, 0.875)],
                 0.0014, "metal_gold", bone="pelvis", sides=6)
    # Ribbon follows the front of the torso, well clear of the cloth envelope.
    strap_centres = [(-0.123, -0.098, 1.322), (-0.100, -0.105, 1.291),
                     (-0.073, -0.112, 1.252), (-0.044, -0.110, 1.211),
                     (-0.014, -0.101, 1.166), (0.021, -0.096, 1.118),
                     (0.058, -0.095, 1.070), (0.095, -0.098, 1.025),
                     (0.132, -0.093, 0.982), (0.170, -0.079, 0.944),
                     (0.196, -0.064, 0.922)]
    strap_grid = []
    for index, centre in enumerate(strap_centres):
        before = Vector(strap_centres[max(0, index - 1)])
        after = Vector(strap_centres[min(len(strap_centres) - 1, index + 1)])
        tangent = (after - before).normalized()
        across = Vector((tangent.z, 0, -tangent.x)).normalized()
        strap_grid.append([tuple(Vector(centre) + across * amount) for amount in [-0.013, -0.010, 0.010, 0.013]])
    builder.shell("Arin_DiagonalChestSatchelStrap", strap_grid, "leather", thickness=0.004, region="torso")
    for sign in (-1, 1):
        points = []
        for index, centre in enumerate(strap_centres):
            before = Vector(strap_centres[max(0, index - 1)])
            after = Vector(strap_centres[min(len(strap_centres) - 1, index + 1)])
            tangent = (after - before).normalized()
            across = Vector((tangent.z, 0, -tangent.x)).normalized()
            points.append(tuple(Vector(centre) + across * (sign * 0.009) + Vector((0, -0.0015, 0))))
        builder.tube("Arin_SatchelStrapEdge_%d" % sign, points, 0.0008, "leather_dark", region="torso", sides=6)
    _rounded_rectangle(builder, "Arin_ChestStrapAdjuster", (0.061, -0.102, 1.070),
                       0.036, 0.023, 0.003, "metal_gold", "pelvis")
    # Belt potion holster is a small open-topped leather cup.
    builder.loft("Arin_BeltPotionLeatherHolster",
                 [((-0.157, -0.064, z), rx, ry) for z, rx, ry in
                  [(0.938, 0.014, 0.012), (0.945, 0.024, 0.020),
                   (0.972, 0.027, 0.022), (0.994, 0.027, 0.022)]],
                 "leather", bone="pelvis", sides=20)
    for label, x, y, base_z in [("Belt", -0.157, -0.069, 0.963),
                               ("Satchel", 0.211, -0.014, 0.965)]:
        _build_potion(builder, label, (x, y, base_z))


def _build_bag_stitches(builder):
    # Twelve sewn dashes in one low-cost mesh, instead of separate gold objects.
    vertices, faces = [], []
    for sign in (-1, 1):
        for index in range(6):
            v, u = 0.23 + index * 0.10, sign * 0.91
            z = 0.981 - 0.108 * v + 0.022 * u * u * v
            x = 0.210 + u * (0.039 + 0.005 * math.sin(v * math.pi))
            y = (-0.047 - 0.025 * math.sin(v * math.pi / 2)
                 - 0.0018 * math.sin((u + 1) * math.pi / 2) * math.sin(v * math.pi) - 0.0005)
            offset = len(vertices)
            for dz, dy, dx in [(-0.0014, -0.0006, -0.00055), (-0.0014, -0.0006, 0.00055),
                               (0.0014, -0.0006, 0.00055), (0.0014, -0.0006, -0.00055),
                               (-0.0014, 0.0001, -0.00055), (-0.0014, 0.0001, 0.00055),
                               (0.0014, 0.0001, 0.00055), (0.0014, 0.0001, -0.00055)]:
                vertices.append((x + dx, y + dy, z + dz))
            faces += [tuple(offset + value for value in face) for face in
                      [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1),
                       (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]]
    builder.mesh("Arin_SatchelIvorySewnStitches", vertices, faces, "cloth_ivory", bone="pelvis")


def _build_potion(builder, label, position):
    x, y, base = position
    outer = [(0.000, 0.010), (0.005, 0.019), (0.016, 0.025), (0.032, 0.026),
             (0.046, 0.022), (0.057, 0.013), (0.063, 0.010), (0.077, 0.010),
             (0.080, 0.012), (0.084, 0.012)]
    builder.loft("Arin_%sPotionGlassBottle" % label,
                 [((x, y, base + z), radius, radius) for z, radius in outer],
                 "glass", bone="pelvis", sides=24)
    inner = [(0.005, 0.009), (0.010, 0.017), (0.020, 0.022),
             (0.034, 0.023), (0.043, 0.020), (0.045, 0.019)]
    builder.loft("Arin_%sPotionRedLiquid" % label,
                 [((x, y, base + z), radius, radius) for z, radius in inner],
                 "potion_red", bone="pelvis", sides=24)
    builder.loft("Arin_%sPotionCork" % label,
                 [((x, y, base + z), radius, radius) for z, radius in
                  [(0.079, 0.009), (0.091, 0.0105), (0.096, 0.010), (0.097, 0.0085)]],
                 "leather", bone="pelvis", sides=16)
    builder.loft("Arin_%sPotionGoldNeckRing" % label,
                 [((x, y, base + z), 0.0112, 0.0112) for z in [0.073, 0.076]],
                 "metal_gold", bone="pelvis", sides=20)


def build_body(materials, collection=None):
    """Create the v2 body/costume and return its mesh objects.

    Required material keys: skin, cloth_ivory, cloth_green, cloth_green_dark,
    leather, leather_dark, metal_gold, potion_red, glass. Existing scene content
    is preserved. Region names match the root builder's smooth-weight contract.
    """
    builder = _BodyBuilder(materials, collection)
    _build_torso(builder)
    _build_arms_and_hands(builder)
    _build_legs_and_boots(builder)
    _build_cape(builder)
    _build_belt_satchel_potions(builder)
    for obj in builder.objects:
        if obj.type != "MESH":
            raise TypeError("Arin body builder returned a non-mesh object: " + obj.name)
        obj["arin_authored_scale_metres"] = 1.0
        obj["arin_refinement_version"] = 2
    triangle_count = sum(sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons)
                         for obj in builder.objects)
    if triangle_count > 30000:
        raise RuntimeError("Arin v2 body exceeds its 30,000-triangle budget")
    return builder.objects
