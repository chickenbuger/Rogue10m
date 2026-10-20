"""Scoped split-coat skin weights for Arin's existing 23-bone rig.

Call ``apply_coat_weights(objects, rig)`` after the common body binder and
before joining/baking authored parts. Only meshes explicitly tagged with
``arin_coat_side`` or ``arin_coat_center`` are changed; the existing skeleton
and rig are retained. The centre panel stays continuous across both thighs.
"""

import math

import BuildArin as common


def apply_coat_weights(objects, rig):
    targets = [obj for obj in objects if obj.get("arin_coat_side") is not None or obj.get("arin_coat_center")]
    required = {"pelvis", "thigh_l", "thigh_r"}
    missing = required - {bone.name for bone in rig.data.bones}
    if missing:
        raise ValueError("Arin coat bones missing: " + ", ".join(sorted(missing)))

    # Validate the whole scoped set before mutating any groups.
    for obj in targets:
        centre = bool(obj.get("arin_coat_center"))
        if obj.type != "MESH" or (not centre and obj.get("arin_coat_side") not in {"l", "r"}):
            raise ValueError("Invalid Arin coat side on " + obj.name)
        if centre and obj.get("arin_coat_side") is not None:
            raise ValueError("Coat panel has both centre and side tags: " + obj.name)
        if obj.get("arin_bone"):
            raise ValueError("Coat panel must not have a rigid bone tag: " + obj.name)
        if not any(mod.type == "ARMATURE" and mod.object == rig for mod in obj.modifiers):
            raise ValueError("Apply common body bind before coat weights: " + obj.name)
        if any(not math.isfinite((obj.matrix_world @ vertex.co).z)
               for vertex in obj.data.vertices):
            raise ValueError("Nonfinite Arin coat vertex on " + obj.name)

    report = {"objects": [], "vertices": 0, "side_counts": {"l": 0, "r": 0, "centre": 0},
              "weight_stops_metres": [[0.765, "thigh_side"], [0.87, "thigh_side"], [1.00, "pelvis"]],
              "part_side_used_at_centre": True, "central_blend_half_width_metres": 0.045,
              "maximum_influences": 3}
    for obj in targets:
        centre = bool(obj.get("arin_coat_center"))
        side = "centre" if centre else obj["arin_coat_side"]
        thigh = "thigh_l" if centre else "thigh_" + side
        vertex_indices = [vertex.index for vertex in obj.data.vertices]
        for group in obj.vertex_groups:
            group.remove(vertex_indices)
        bone_names = ("thigh_l", "thigh_r", "pelvis") if centre else (thigh, "pelvis")
        groups = {name: obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
                  for name in bone_names}
        for vertex in obj.data.vertices:
            p = obj.matrix_world @ vertex.co
            weights = common.blend_axis(p.z, [(0.765, thigh), (0.87, thigh), (1.00, "pelvis")])
            if centre:
                # Smooth, complementary side weights preserve the sewn centre
                # line while letting either leg carry its outer apron edge.
                t = max(0.0, min(1.0, (p.x + 0.045) / 0.09))
                left = t * t * (3 - 2 * t)
                moving = weights.get("thigh_l", 0.0)
                weights = {"thigh_l": moving * left,
                           "thigh_r": moving * (1 - left),
                           "pelvis": weights.get("pelvis", 0.0)}
            if abs(sum(weights.values()) - 1.0) > 1e-7:
                raise RuntimeError("Unnormalized Arin coat weights: " + obj.name)
            for bone, weight in weights.items():
                if weight > 1e-6:
                    groups[bone].add([vertex.index], weight, "REPLACE")
        obj["arin_coat_skin"] = "continuous_bilateral_thigh_pelvis_blend" if centre else "part_side_thigh_pelvis_z_blend"
        report["objects"].append(obj.name)
        report["vertices"] += len(obj.data.vertices)
        report["side_counts"][side] += 1
    return report
