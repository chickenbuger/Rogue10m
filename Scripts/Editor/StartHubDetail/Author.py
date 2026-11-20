"""Apply the StartHub art set to the existing editor map and save only its packages."""

from collections import Counter
from math import cos, radians, sin
from pathlib import Path
import json
import os
import sys

import unreal

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import Finishes
import Landmarks
import Perimeter
import Services

HUB = "/Game/Rogue10m/Maps/StartHub"
HUB_EXTERNAL = (
    "/Game/__ExternalActors__/Rogue10m/Maps/StartHub/",
    "/Game/__ExternalObjects__/Rogue10m/Maps/StartHub/",
)
TAG = "Rogue10m.StartHubDetail"
MATERIAL_ROOT = "/Game/Rogue10m/World/StartVillage/Materials/"
MATERIAL_NAMES = {
    "timber": "M_StartVillage_DarkTimber",
    "stone": "M_StartVillage_WarmStone",
    "ochre": "M_StartVillage_Banner_Ochre",
    "blue": "M_StartVillage_Banner_Blue",
    "burgundy": "M_StartVillage_Banner_Burgundy",
    "teal": "M_StartVillage_Banner_Teal",
    "glow": "M_StartVillage_LanternGlow",
    "grass": "MI_StartVillage_Grass",
    "cobble": "MI_StartVillage_Cobblestone",
    "earth": "MI_StartVillage_EarthPath",
}
PRIMITIVES = {
    "cube": "/Engine/BasicShapes/Cube",
    "cylinder": "/Engine/BasicShapes/Cylinder",
    "plane": "/Engine/BasicShapes/Plane",
}
ORIGINAL_ANCHORS = {
    "SV_PlayerStart_SouthApproach": (0, -2040, 112),
    "SV_TutorialGuide_Arin": (440, -1510, 2),
    "SV_RunPortal_ToOpenWorld": (0, 2135, 200),
}
ROLE_PLACEHOLDERS = {
    f"SV_{role}_{suffix}"
    for role in ("Equipment", "Traits", "Difficulty", "Story")
    for suffix in ("Banner", "BannerPole", "RoleSign", "FutureService", "SignBoard")
}
VISUAL_PADS = {
    "SV_PortalStonePad",
    *(f"SV_{role}_ReservedNPCPad" for role in ("Equipment", "Traits", "Difficulty", "Story")),
}


def dirty_packages():
    utils = unreal.EditorLoadingAndSavingUtils
    packages = list(utils.get_dirty_map_packages()) + list(utils.get_dirty_content_packages())
    return {package.get_name(): package for package in packages}


def xyz(vector):
    return [vector.x, vector.y, vector.z]


def rotate(point, pitch, yaw, roll):
    pitch, yaw, roll = radians(pitch), radians(yaw), radians(roll)
    x, y, z = point
    y, z = cos(roll) * y + sin(roll) * z, -sin(roll) * y + cos(roll) * z
    projected = cos(pitch) * x - sin(pitch) * z
    return (
        cos(yaw) * projected - sin(yaw) * y,
        sin(yaw) * projected + cos(yaw) * y,
        sin(pitch) * x + cos(pitch) * z,
    )


class DetailPlan:
    def __init__(self):
        self.meshes = []
        self.lights = []

    def mesh(self, group, name, asset, x, y, z, sx, sy, sz, yaw=0,
             material=None, blocking=False, roll=0):
        self.meshes.append({
            "group": group, "name": name, "asset": PRIMITIVES.get(asset, asset),
            "center": (x, y, z + sz / 2), "dimensions": (sx, sy, sz), "yaw": yaw, "roll": roll,
            "material": MATERIAL_ROOT + MATERIAL_NAMES[material] if material in MATERIAL_NAMES else material,
            "blocking": blocking,
        })

    def light(self, group, name, x, y, z, intensity, radius, color=(255, 185, 111)):
        self.lights.append({
            "group": group, "name": name, "location": (x, y, z),
            "intensity": intensity, "radius": radius, "color": color,
        })


def label(row):
    return f"StartHubDetail_{row['group']}_{row['name']}"


def prepare_mesh(row, assets):
    dimensions = list(row["dimensions"])
    if min(dimensions) <= 0:
        raise ValueError(f"Non-positive dimensions for {label(row)}")
    mesh = assets[row["asset"]]
    bounds = mesh.get_bounds()
    extent = xyz(bounds.box_extent)
    yaw = row["yaw"]
    pitch = 0
    if "/Wood/Pieces/SM_Beam_" in row["asset"] and extent[0] == max(extent):
        # Keep the vendor grain and bevels along the target's longest axis.
        if dimensions[2] > max(dimensions[:2]):
            dimensions[0], dimensions[2] = dimensions[2], dimensions[0]
            pitch = 90
        elif dimensions[1] > dimensions[0]:
            dimensions[0], dimensions[1] = dimensions[1], dimensions[0]
            yaw += 90
    scale = [size / (2 * axis) if axis > 0.01 else 1 for size, axis in zip(dimensions, extent)]
    roll = row["roll"]
    offset = rotate([coord * factor for coord, factor in zip(xyz(bounds.origin), scale)], pitch, yaw, roll)
    location = tuple(axis - delta for axis, delta in zip(row["center"], offset))
    return mesh, location, scale, pitch, yaw, roll


def tag_actor(actor, row):
    if not actor:
        raise RuntimeError(f"Failed to spawn {label(row)}")
    actor.set_actor_label(label(row))
    actor.set_folder_path(f"StartVillage/Detail/{row['group']}")
    actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name(f"{TAG}.{row['group']}")])
    actor.set_editor_property("is_spatially_loaded", False)
    package = actor.get_package().get_name()
    if not package.startswith(HUB_EXTERNAL[0]):
        raise RuntimeError(f"Expected StartHub external actor packaging, got {package}")
    return package


def spawn_mesh(actors, row, prepared, materials):
    mesh, location, scale, pitch, yaw, roll = prepared
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location),
                                          unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
    package = tag_actor(actor, row)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component.set_static_mesh(mesh):
        raise RuntimeError(f"Failed to assign {row['asset']} to {label(row)}")
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name("BlockAll" if row["blocking"] else "NoCollision")
    component.set_editor_property("generate_overlap_events", False)
    component.set_editor_property("can_ever_affect_navigation", row["blocking"])
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if row["material"]:
        for slot in range(component.get_num_materials()):
            component.set_material(slot, materials[row["material"]])
    return package


def spawn_light(actors, row):
    actor = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*row["location"]))
    package = tag_actor(actor, row)
    configure_light(actor.get_component_by_class(unreal.PointLightComponent), row)
    return package


def configure_light(component, row):
    component.modify()
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("cast_shadows", False)
    component.set_intensity(row["intensity"] * 0.24)
    component.set_attenuation_radius(row["radius"])
    red, green, blue = row["color"]
    component.set_editor_property("light_color", unreal.Color(r=red, g=green, b=blue, a=255))


def inspect_baseline(world, actors):
    if not world or world.get_path_name() != HUB + ".StartHub":
        raise RuntimeError("StartHub must be the active editor world")
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem.get_game_world():
        raise RuntimeError("StartHub authoring requires an editor world outside PIE")
    partition = unreal.WorldPartitionBlueprintLibrary
    descriptors = partition.get_actor_descs()
    partition.load_actors([desc.get_editor_property("guid") for desc in descriptors])
    by_label = {actor.get_actor_label(): actor for actor in actors.get_all_level_actors()}
    if any(row.startswith("StartHubDetail_") for row in by_label):
        raise RuntimeError("StartHub details are already present. Review the existing art set before reauthoring.")
    for name, expected in ORIGINAL_ANCHORS.items():
        actor = by_label[name]
        observed = xyz(actor.get_actor_location())
        if max(abs(a - b) for a, b in zip(observed, expected)) > 0.25:
            raise RuntimeError(f"Unexpected {name} anchor: {observed}")
    portal = by_label["SV_RunPortal_ToOpenWorld"]
    if (not isinstance(portal, unreal.Rogue10mRunPortal)
            or sum(isinstance(actor, unreal.Rogue10mRunPortal) for actor in by_label.values()) != 1
            or str(portal.get_editor_property("target_level_name")) != "/Game/Rogue10m/Maps/OpenRunMap"
            or not portal.get_editor_property("start_run_timer_on_travel")):
        raise RuntimeError("StartHub departure contract differs from the art baseline")
    missing = (ROLE_PLACEHOLDERS | VISUAL_PADS) - by_label.keys()
    if missing:
        raise RuntimeError(f"Missing original preparation props: {sorted(missing)}")
    if dirty_packages():
        raise RuntimeError(f"Unexpected dirty packages during baseline inspection: {list(dirty_packages())}")
    return by_label


def load_assets(paths, expected_class):
    loaded = {}
    for path in sorted(paths):
        asset = unreal.load_asset(path)
        if not isinstance(asset, expected_class):
            raise RuntimeError(f"Expected {expected_class.__name__}: {path}")
        loaded[path] = asset
    return loaded


def hide_old_prop(actor):
    actor.modify()
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        component.modify()
        component.set_visibility(False, True)
        component.set_hidden_in_game(True, True)
        component.set_collision_profile_name("NoCollision")
    return actor.get_package().get_name()


def run(apply):
    if dirty_packages():
        raise RuntimeError(f"Save or close existing edits before authoring StartHub: {list(dirty_packages())}")
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_editor_world()
    if not world or world.get_path_name() != HUB + ".StartHub":
        world = unreal.EditorLoadingAndSavingUtils.load_map(HUB)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    baseline = inspect_baseline(world, actors)
    plan = DetailPlan()
    Services.author(plan)
    Landmarks.author(plan)
    Perimeter.author(plan)
    Finishes.author(plan)

    labels = [label(row) for row in plan.meshes + plan.lights]
    if len(labels) != len(set(labels)):
        raise RuntimeError("StartHub detail labels must be unique")
    assets = load_assets({row["asset"] for row in plan.meshes}, unreal.StaticMesh)
    materials = load_assets({row["material"] for row in plan.meshes if row["material"]}, unreal.MaterialInterface)
    prepared = [prepare_mesh(row, assets) for row in plan.meshes]
    group_counts = Counter(row["group"] for row in plan.meshes + plan.lights)
    report = {
        "map": HUB, "baseline_actor_count": len(baseline), "groups": dict(sorted(group_counts.items())),
        "mesh_count": len(plan.meshes), "light_count": len(plan.lights),
        "mesh_assets": sorted(assets), "material_assets": sorted(materials), "applied": apply,
    }
    if apply:
        changed = {spawn_mesh(actors, row, fit, materials) for row, fit in zip(plan.meshes, prepared)}
        changed.update(spawn_light(actors, row) for row in plan.lights)
        changed.update(hide_old_prop(baseline[name]) for name in sorted(ROLE_PLACEHOLDERS | VISUAL_PADS))
        packages = dirty_packages()
        unexpected = sorted(name for name in packages.keys() - changed - {HUB}
                            if not name.startswith(HUB_EXTERNAL[1]))
        if unexpected:
            raise RuntimeError(f"Unexpected packages during StartHub save: {unexpected}")
        if not changed.issubset(packages):
            raise RuntimeError(f"Expected dirty actor packages: {sorted(changed - packages.keys())}")
        if not unreal.EditorLoadingAndSavingUtils.save_packages(list(packages.values()), True):
            raise RuntimeError("StartHub package save failed")
        if dirty_packages():
            raise RuntimeError(f"Found dirty packages after StartHub save: {list(dirty_packages())}")
        report["saved_package_count"] = len(packages)
        report["hidden_placeholders"] = sorted(ROLE_PLACEHOLDERS)
        report["replaced_floor_actors"] = sorted(VISUAL_PADS)
    output = os.environ.get("ROGUE_START_HUB_DETAIL_REPORT")
    if output:
        with Path(output).open("x", encoding="utf-8", newline="\n") as handle:
            json.dump(report, handle, indent=2, ensure_ascii=False)
    unreal.log(f"START_HUB_DETAIL {json.dumps(report, sort_keys=True)}")


if __name__ == "__main__":
    command_line = unreal.SystemLibrary.get_command_line().casefold()
    if "-roguestarthubdetailapply" not in command_line and "-roguestarthubdetailplan" not in command_line:
        raise RuntimeError("Use -RogueStartHubDetailPlan or -RogueStartHubDetailApply")
    run("-roguestarthubdetailapply" in command_line)
