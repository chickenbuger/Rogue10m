"""Create an isolated 120 m village from the existing first-person template.

Run inside the editor via official Python Remote Execution; Unreal MCP inspects,
captures and saves the live editor. This script saves only StartHub and assets
under /Game/Rogue10m/World/StartVillage. Purchased assets are references.
Preparation areas are scenery reserved for future services, not implemented NPCs.
"""

import datetime
import json
import math
from pathlib import Path
import random
import traceback

import unreal


MAP_PATH = "/Game/Rogue10m/Maps/StartHub"
TEMPLATE_PATH = "/Game/FirstPerson/Lvl_FirstPerson"
ASSET_ROOT = "/Game/Rogue10m/World/StartVillage"
MATERIAL_ROOT = ASSET_ROOT + "/Materials"
TAG = "Rogue10m.StartVillage"
TEMPLATE_CLEANED_TAG = "Rogue10m.StartVillage.TemplateCleaned"
PACK = "/Game/Stylized_Village/Assets"
EVIDENCE_RELATIVE = "Feature/doc/evidence/start-village-20261005/creation.json"
MESH_PATHS = {
    "cube": "/Engine/BasicShapes/Cube",
    "cylinder": "/Engine/BasicShapes/Cylinder",
    "sphere": "/Engine/BasicShapes/Sphere",
    "plaster": PACK + "/Architecture/Plaster/SM_Plaster_FullXFull_Plaster_FullXFull",
    "beam": PACK + "/Architecture/Wood/Pieces/SM_Beam_Long_1",
    "gable": PACK + "/Architecture/Wood/Roof/SM_Wood_Wall_RoofFront_Large_LP",
    "roof": PACK + "/Architecture/RoofTiles/Round/SM_RoofTiles_Round_FullXFull",
    "door": PACK + "/Architecture/Doors/SM_Door_01",
    "window": PACK + "/Architecture/Windows/SM_Window_FrenchPanes_01_LP",
    "chimney": PACK + "/Architecture/Plaster/SM_Plaster_Chimney_Thick",
    "stone_tile": PACK + "/Architecture/StoneFloor/SM_StoneFloor_01_Full",
    "tree_medium": PACK + "/Foliage/Trees/Tree_Medium_Green",
    "tree_large": PACK + "/Foliage/Trees/Tree_Large_Green",
    "bush": PACK + "/Foliage/Bushes/SM_Bush_01",
    "rock": PACK + "/Stones/Boulder/SM_Boulder_Medium_01",
    "barrel": PACK + "/Props/Furniture/SM_SM_Barrel_01",
    "crate": PACK + "/Props/Furniture/SM_Crate_01",
    "lantern": PACK + "/Props/Lanterns/SM_Lantern_Box",
    "pot": PACK + "/Props/Pots/SM_ClayPot_WideSmall",
}
EXISTING_GROUND_MATERIALS = {
    "grass": "/Game/StarterContent/Materials/M_Ground_Grass",
    "dirt": "/Game/StarterContent/Materials/M_Ground_Gravel",
}
GROUND_DETAIL_MASTER = MATERIAL_ROOT + "/M_StartVillage_GroundDetail"
GROUND_DETAIL_PROFILES = {
    "grass": ("T_Ground_Grass", 180.0, (0.72, 0.86, 0.65), 0.9),
    "dirt": ("T_Ground_Gravel", 90.0, (0.86, 0.78, 0.65), 0.88),
}


def vec(values):
    return unreal.Vector(*[float(value) for value in values])


def values(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def require_asset(path, asset_type=None):
    asset = unreal.load_asset(path)
    if not asset or (asset_type and not isinstance(asset, asset_type)):
        raise RuntimeError("Missing or unexpected asset: " + path)
    return asset


def ground_detail_parameter_names(parent):
    """Inspect the owned master without creating, compiling, or saving assets."""
    if not isinstance(parent, unreal.Material):
        raise RuntimeError("Unexpected owned ground detail master: " + GROUND_DETAIL_MASTER)
    library = unreal.MaterialEditingLibrary
    scalar_names = [str(value) for value in library.get_scalar_parameter_names(parent)]
    vector_names = [str(value) for value in library.get_vector_parameter_names(parent)]
    texture_names = [str(value) for value in library.get_texture_parameter_names(parent)]
    if (not {"TileSizeCm", "Roughness"}.issubset(scalar_names)
            or "ColorTint" not in vector_names
            or not {"BaseColorTexture", "NormalTexture"}.issubset(texture_names)):
        raise RuntimeError("Incomplete owned ground detail master: " + GROUND_DETAIL_MASTER)
    return scalar_names, vector_names, texture_names


def inspect_ground_detail_instance(instance, parent, path):
    """Validate the existing physical profile without resetting artist tuning."""
    if not isinstance(instance, unreal.MaterialInstanceConstant) or instance.get_editor_property("parent") != parent:
        raise RuntimeError("Unexpected owned ground detail instance/parent: " + path)
    library = unreal.MaterialEditingLibrary
    tile_size = library.get_material_instance_scalar_parameter_value(instance, "TileSizeCm")
    if not math.isfinite(tile_size) or tile_size <= 0.0:
        raise RuntimeError("Invalid owned ground physical tile size: " + path)
    textures = {}
    for parameter in ("BaseColorTexture", "NormalTexture"):
        texture = library.get_material_instance_texture_parameter_value(instance, parameter)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("Missing owned ground detail texture: " + path + " " + parameter)
        textures[parameter] = texture.get_path_name()
    return {"instance": path, "tile_size_cm": tile_size, "textures": textures}


def require_world(levels):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_path_name().split(".")[0] != MAP_PATH:
        raise RuntimeError("Refusing to modify any world except " + MAP_PATH)
    return world


def rotate(point, pitch=0.0, yaw=0.0, roll=0.0):
    """Match Unreal FRotator axes for mesh-bounds origin compensation."""
    p, y, r = [math.radians(value) for value in (pitch, yaw, roll)]
    cp, sp, cy, sy, cr, sr = math.cos(p), math.sin(p), math.cos(y), math.sin(y), math.cos(r), math.sin(r)
    x, yy, z = point
    return (
        cp * cy * x + (sr * sp * cy - cr * sy) * yy + (-cr * sp * cy - sr * sy) * z,
        cp * sy * x + (sr * sp * sy + cr * cy) * yy + (-cr * sp * sy + sr * cy) * z,
        sp * x - sr * cp * yy + cr * cp * z,
    )


class VillageBuilder:
    def __init__(self, actors, levels, report):
        self.actors = actors
        self.levels = levels
        self.report = report
        self.meshes = {}
        self.bounds = {}
        self.materials = {}
        self.saved_assets = []
        self.spawned = []
        self.report["saved_owned_assets"] = self.saved_assets
        self.report["created_actor_labels"] = []
        self.rng = random.Random(1052026)

    def preload(self):
        for key, path in MESH_PATHS.items():
            mesh = require_asset(path, unreal.StaticMesh)
            bound = mesh.get_bounds()
            extent = values(bound.box_extent)
            if min(extent) <= 0.0001:
                raise RuntimeError("Degenerate mesh bounds: " + path)
            self.meshes[key] = mesh
            self.bounds[key] = {"origin": values(bound.origin), "size": [2.0 * n for n in extent]}
        self.report["mesh_bounds"] = {key: {"asset": MESH_PATHS[key], **bound} for key, bound in self.bounds.items()}
        for path in EXISTING_GROUND_MATERIALS.values():
            require_asset(path, unreal.MaterialInterface)
        self.preflight_ground_materials()
        portal_class = unreal.load_class(None, "/Script/Rogue10m.Rogue10mRunPortal")
        if not portal_class:
            raise RuntimeError("Native Rogue10mRunPortal is unavailable; build/restart first.")
        self.portal_class = portal_class

    def preflight_ground_materials(self):
        """Fail before clean_new_world can remove and checkpoint village actors."""
        detail_parent = unreal.load_asset(GROUND_DETAIL_MASTER)
        source_textures = []
        if detail_parent:
            ground_detail_parameter_names(detail_parent)
            for prefix, _, _, _ in GROUND_DETAIL_PROFILES.values():
                for suffix in ("_D", "_N"):
                    texture = require_asset("/Game/StarterContent/Textures/" + prefix + suffix, unreal.Texture2D)
                    source_textures.append(texture.get_path_name())
            macro = require_asset("/Game/StarterContent/Textures/T_MacroVariation", unreal.Texture2D)
            source_textures.append(macro.get_path_name())
        instances = {}
        for key, name in (("grass", "MI_StartVillage_Grass"), ("dirt", "MI_StartVillage_EarthPath")):
            path = MATERIAL_ROOT + "/" + name
            instance = unreal.load_asset(path)
            if not instance:
                continue
            legacy_parent = require_asset(EXISTING_GROUND_MATERIALS[key], unreal.MaterialInterface)
            if not isinstance(instance, unreal.MaterialInstanceConstant):
                raise RuntimeError("Unexpected owned ground instance type: " + path)
            parent = instance.get_editor_property("parent")
            if parent != legacy_parent and (not detail_parent or parent != detail_parent):
                raise RuntimeError("Unexpected village material instance parent: " + path)
            if detail_parent and parent == detail_parent:
                instances[key] = inspect_ground_detail_instance(instance, detail_parent, path)
            else:
                instances[key] = {"instance": path, "parent": legacy_parent.get_path_name(), "legacy_profile": True}
        stone_path = MATERIAL_ROOT + "/MI_StartVillage_Cobblestone"
        stone = unreal.load_asset(stone_path)
        if stone:
            if not detail_parent:
                raise RuntimeError("Owned cobblestone instance requires the owned ground detail master")
            instances["stone"] = inspect_ground_detail_instance(stone, detail_parent, stone_path)
            for suffix in ("_D", "_N"):
                texture = require_asset("/Game/StarterContent/Textures/T_CobbleStone_Rough" + suffix, unreal.Texture2D)
                source_textures.append(texture.get_path_name())
        self.report["ground_material_preflight"] = {
            "passed": True, "read_only": True,
            "detail_master": detail_parent.get_path_name() if detail_parent else None,
            "existing_instances": instances, "required_source_textures": source_textures}

    def owned(self, actor, label, folder):
        require_world(self.levels)
        actor.set_actor_label(label)
        actor.set_folder_path("StartVillage/" + folder)
        tags = list(actor.get_editor_property("tags"))
        if TAG not in [str(tag) for tag in tags]:
            tags.append(unreal.Name(TAG))
        actor.set_editor_property("tags", tags)
        # This compact hub stays loaded together, avoiding holes from unloaded
        # floor/decoration actors when inspecting the World Partition duplicate.
        try:
            actor.set_editor_property("is_spatially_loaded", False)
        except Exception:
            pass
        self.spawned.append(actor)
        self.report["created_actor_labels"].append(label)
        self.report["created_actor_count"] = len(self.spawned)
        return actor

    def mesh(self, key, center, dimensions, label, folder="Buildings", yaw=0.0,
             pitch=0.0, roll=0.0, material=None, collision=True, align_width=False):
        require_world(self.levels)
        native = self.bounds[key]
        target = list(dimensions)
        if key == "beam":
            # The measured wooden beam is 302 cm long along native X. Rotate
            # that axis for posts and cross rails instead of stretching its
            # 16 cm cross-section into a post, keeping the timber grain useful.
            long_axis = max(range(3), key=lambda axis: target[axis])
            if long_axis == 2:
                target[0], target[2] = target[2], target[0]
                pitch += 90.0
            elif long_axis == 1:
                target[0], target[1] = target[1], target[0]
                yaw += 90.0
        # Wall/window modules can face native X or native Y. Orient their long
        # horizontal dimension along canonical X before the house rotation.
        if align_width and native["size"][1] > native["size"][0]:
            target[0], target[1] = target[1], target[0]
            yaw -= 90.0
        scale = [target[i] / native["size"][i] for i in range(3)]
        shifted_origin = rotate([native["origin"][i] * scale[i] for i in range(3)], pitch, yaw, roll)
        location = [center[i] - shifted_origin[i] for i in range(3)]
        actor = self.actors.spawn_actor_from_class(unreal.StaticMeshActor, vec(location), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
        if not actor:
            raise RuntimeError("Could not spawn " + label)
        self.owned(actor, label, folder)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not component.set_static_mesh(self.meshes[key]):
            raise RuntimeError("Could not assign mesh to " + label)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("BlockAll" if collision else "NoCollision")
        component.set_editor_property("generate_overlap_events", False)
        actor.set_actor_scale3d(vec(scale))
        if material:
            for slot in range(max(1, component.get_num_materials())):
                component.set_material(slot, material)
        return actor

    def uniform_mesh(self, key, bottom, height, label, folder="Nature", yaw=0.0, collision=False):
        native_size = self.bounds[key]["size"]
        scale = height / native_size[2]
        dims = [size * scale for size in native_size]
        center = (bottom[0], bottom[1], bottom[2] + height * 0.5)
        return self.mesh(key, center, dims, label, folder, yaw=yaw, collision=collision)

    def material(self, name, color, emission=False, opacity=None):
        path = MATERIAL_ROOT + "/" + name
        asset = unreal.load_asset(path)
        if asset:
            if not isinstance(asset, unreal.Material):
                raise RuntimeError("Unexpected existing village material: " + path)
            # Existing generated materials are reused without graph churn.
            self.materials[name] = asset
            return asset
        unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew())
        if not asset:
            raise RuntimeError("Could not create " + path)
        library = unreal.MaterialEditingLibrary
        color_node = library.create_material_expression(asset, unreal.MaterialExpressionVectorParameter)
        color_node.set_editor_property("parameter_name", "Color")
        color_node.set_editor_property("default_value", unreal.LinearColor(*color, 1.0))
        library.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR if emission else unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = library.create_material_expression(asset, unreal.MaterialExpressionScalarParameter)
        roughness.set_editor_property("parameter_name", "Roughness")
        roughness.set_editor_property("default_value", 0.92)
        library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        if emission:
            asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        if opacity is not None:
            asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
            asset.set_editor_property("two_sided", True)
            node = library.create_material_expression(asset, unreal.MaterialExpressionScalarParameter)
            node.set_editor_property("parameter_name", "Opacity")
            node.set_editor_property("default_value", opacity)
            library.connect_material_property(node, "", unreal.MaterialProperty.MP_OPACITY)
        library.recompile_material(asset)
        self.save_asset(asset)
        self.materials[name] = asset
        return asset

    def detail_ground_material_instance(self, key, name, parent):
        """Preserve the owned world-mapped master and any existing tuning."""
        library = unreal.MaterialEditingLibrary
        scalar_names, vector_names, texture_names = ground_detail_parameter_names(parent)
        path = MATERIAL_ROOT + "/" + name
        instance = unreal.load_asset(path)
        created = not instance
        if created:
            unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, MATERIAL_ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("Could not load/create owned ground detail instance: " + path)
        existing_parent = instance.get_editor_property("parent")
        if not created and existing_parent not in (parent, require_asset(EXISTING_GROUND_MATERIALS[key], unreal.MaterialInterface)):
            raise RuntimeError("Unexpected village material instance parent: " + path)
        initialize = created or existing_parent != parent
        instance.modify()
        if initialize:
            prefix, tile_size, tint, roughness = GROUND_DETAIL_PROFILES[key]
            library.set_material_instance_parent(instance, parent)
            if instance.get_editor_property("parent") != parent:
                raise RuntimeError("Cannot initialize owned ground detail parent: " + path)
            for parameter, suffix in (("BaseColorTexture", "_D"), ("NormalTexture", "_N")):
                texture = require_asset("/Game/StarterContent/Textures/" + prefix + suffix, unreal.Texture2D)
                library.set_material_instance_texture_parameter_value(instance, parameter, texture)
                if library.get_material_instance_texture_parameter_value(instance, parameter) != texture:
                    raise RuntimeError("Cannot initialize owned ground texture: " + path + " " + parameter)
            library.set_material_instance_scalar_parameter_value(instance, "TileSizeCm", tile_size)
            library.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
            library.set_material_instance_vector_parameter_value(instance, "ColorTint", unreal.LinearColor(*tint, 1.0))
            actual_tint = library.get_material_instance_vector_parameter_value(instance, "ColorTint")
            if (abs(library.get_material_instance_scalar_parameter_value(instance, "TileSizeCm") - tile_size) > 0.0001
                    or abs(library.get_material_instance_scalar_parameter_value(instance, "Roughness") - roughness) > 0.0001
                    or any(abs(getattr(actual_tint, component) - expected) > 0.0001
                           for component, expected in zip(("r", "g", "b", "a"), (*tint, 1.0)))):
                raise RuntimeError("Cannot initialize owned ground detail profile: " + path)
        # Reusing an instance must not overwrite artist-tuned physical tiling.
        tile_size = library.get_material_instance_scalar_parameter_value(instance, "TileSizeCm")
        if not math.isfinite(tile_size) or tile_size <= 0.0:
            raise RuntimeError("Invalid owned ground physical tile size: " + path)
        library.set_material_instance_scalar_parameter_value(instance, "TileSizeCm", tile_size)
        library.update_material_instance(instance)
        if abs(library.get_material_instance_scalar_parameter_value(instance, "TileSizeCm") - tile_size) > 0.0001:
            raise RuntimeError("Cannot preserve owned ground physical tile size: " + path)
        textures = {}
        for parameter in ("BaseColorTexture", "NormalTexture"):
            texture = library.get_material_instance_texture_parameter_value(instance, parameter)
            if not isinstance(texture, unreal.Texture2D):
                raise RuntimeError("Missing owned ground detail texture: " + path + " " + parameter)
            textures[parameter] = texture.get_path_name()
        self.save_asset(instance)
        tint = library.get_material_instance_vector_parameter_value(instance, "ColorTint")
        self.report.setdefault("ground_materials", {})[key] = {
            "instance": path, "existing_parent": parent.get_path_name(),
            "scalar_parameters": scalar_names, "vector_parameters": vector_names,
            "texture_parameters": textures, "tile_size_cm": tile_size,
            "color_tint": [getattr(tint, component) for component in ("r", "g", "b", "a")],
            "roughness": library.get_material_instance_scalar_parameter_value(instance, "Roughness"),
            "uv_mapping": "absolute world XY / TileSizeCm",
            "applied_parameters": {"TileSizeCm": tile_size},
            "existing_profile_preserved": not initialize, "original_material_modified": False,
            "surface": "grass" if key == "grass" else "earth and fine gravel approach"}
        return instance

    def ground_material_instance(self, key, name, repeats):
        """Prefer owned world UVs; retain the StarterContent fallback."""
        detail_parent = unreal.load_asset(GROUND_DETAIL_MASTER)
        if detail_parent:
            return self.detail_ground_material_instance(key, name, detail_parent)
        parent = require_asset(EXISTING_GROUND_MATERIALS[key], unreal.MaterialInterface)
        path = MATERIAL_ROOT + "/" + name
        instance = unreal.load_asset(path)
        if not instance:
            unreal.EditorAssetLibrary.make_directory(MATERIAL_ROOT)
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, MATERIAL_ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            if not instance:
                raise RuntimeError("Could not create owned ground material instance: " + path)
            unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        if not isinstance(instance, unreal.MaterialInstanceConstant) or instance.get_editor_property("parent") != parent:
            raise RuntimeError("Unexpected village material instance parent: " + path)
        library = unreal.MaterialEditingLibrary
        scalar_names = [str(value) for value in library.get_scalar_parameter_names(parent)]
        vector_names = [str(value) for value in library.get_vector_parameter_names(parent)]
        applied = {}
        for parameter in scalar_names:
            if parameter.casefold() in ("tiling", "texture tiling", "uv tiling", "large scale tiling"):
                # UE 5.8 setters return false even after applying the value.
                # Validate the stored parameter instead of their return flag.
                library.set_material_instance_scalar_parameter_value(instance, unreal.Name(parameter), repeats)
                if abs(library.get_material_instance_scalar_parameter_value(instance, unreal.Name(parameter)) - repeats) > 0.0001:
                    raise RuntimeError("Cannot adjust owned material tiling: " + path)
                applied[parameter] = repeats
        tiling_applied = bool(applied)
        warning = None
        if not tiling_applied:
            warning = ("StarterContent parent exposes no supported tiling parameter; requested "
                       + str(repeats) + " repeats were not applied. Scaled floor mesh UVs may stretch. "
                       + "Create the owned world-mapped ground detail master to repair texel density.")
            unreal.log_warning("[Rogue10mStartVillage] " + path + ": " + warning)
        if key == "grass" and "Base Color tint" in vector_names:
            tint = unreal.LinearColor(0.72, 0.86, 0.65, 1.0)
            library.set_material_instance_vector_parameter_value(instance, "Base Color tint", tint)
            actual = library.get_material_instance_vector_parameter_value(instance, "Base Color tint")
            if any(abs(getattr(actual, component) - getattr(tint, component)) > 0.0001 for component in ("r", "g", "b", "a")):
                raise RuntimeError("Cannot adjust owned grass tint: " + path)
            applied["Base Color tint"] = [0.72, 0.86, 0.65, 1.0]
        library.update_material_instance(instance)
        self.save_asset(instance)
        self.report.setdefault("ground_materials", {})[key] = {
            "instance": path, "existing_parent": parent.get_path_name(),
            "scalar_parameters": scalar_names, "vector_parameters": vector_names,
            "applied_parameters": applied, "original_material_modified": False,
            "requested_repeats": repeats, "tiling_applied": tiling_applied, "warning": warning,
            "surface": "grass" if key == "grass" else "earth and fine gravel approach"}
        return instance

    def save_asset(self, asset):
        path = asset.get_path_name().split(".")[0]
        if not path.startswith(ASSET_ROOT + "/"):
            raise RuntimeError("Refusing to save source/purchased asset: " + path)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("Failed to save owned asset: " + path)
        self.saved_assets.append(path)

    def make_materials(self):
        self.grass = self.ground_material_instance("grass", "MI_StartVillage_Grass", 65.0)
        self.dirt = self.ground_material_instance("dirt", "MI_StartVillage_EarthPath", 6.0)
        self.stone = self.material("M_StartVillage_WarmStone", (0.34, 0.32, 0.27))
        self.ground_stone = unreal.load_asset(MATERIAL_ROOT + "/MI_StartVillage_Cobblestone")
        if self.ground_stone and not isinstance(self.ground_stone, unreal.MaterialInstanceConstant):
            raise RuntimeError("Unexpected owned cobblestone material instance")
        if self.ground_stone:
            library = unreal.MaterialEditingLibrary
            if "TileSizeCm" not in [str(value) for value in library.get_scalar_parameter_names(self.ground_stone)]:
                raise RuntimeError("Owned cobblestone instance has no physical tile-size parameter")
            tile_size = library.get_material_instance_scalar_parameter_value(self.ground_stone, "TileSizeCm")
            if not math.isfinite(tile_size) or tile_size <= 0.0:
                raise RuntimeError("Invalid owned cobblestone physical tile size")
            self.ground_stone.modify()
            library.set_material_instance_scalar_parameter_value(self.ground_stone, "TileSizeCm", tile_size)
            library.update_material_instance(self.ground_stone)
            if abs(library.get_material_instance_scalar_parameter_value(self.ground_stone, "TileSizeCm") - tile_size) > 0.0001:
                raise RuntimeError("Cannot preserve owned cobblestone physical tile size")
            self.save_asset(self.ground_stone)
            self.report.setdefault("ground_materials", {})["stone"] = {
                "instance": self.ground_stone.get_path_name(), "tile_size_cm": tile_size,
                "existing_profile_preserved": True, "surface": "cobblestone plaza and service/portal pads"}
        else:
            self.ground_stone = self.stone
        self.report["ground_stone_material"] = self.ground_stone.get_path_name()
        self.wood = self.material("M_StartVillage_DarkTimber", (0.10, 0.055, 0.025))
        self.water = self.material("M_StartVillage_WellWater", (0.035, 0.25, 0.27))
        self.cyan = self.material("M_StartVillage_PortalGlow", (0.15, 2.5, 3.4), emission=True)
        self.portal_surface = self.material("M_StartVillage_PortalSurface", (0.08, 0.9, 1.3), emission=True, opacity=0.34)
        self.gold = self.material("M_StartVillage_LanternGlow", (4.0, 2.0, 0.4), emission=True)
        self.colors = [self.material("M_StartVillage_Banner_" + name, color) for name, color in (
            ("Teal", (0.035, 0.28, 0.25)), ("Ochre", (0.55, 0.30, 0.04)),
            ("Burgundy", (0.32, 0.055, 0.07)), ("Blue", (0.065, 0.14, 0.32)))]

    def ground_and_paths(self):
        self.mesh("cube", (0, 0, -60), (12000, 12000, 120), "SV_WalkableGrass_120m", "Ground", material=self.grass)
        self.mesh("cylinder", (0, 0, 6), (2600, 2600, 12), "SV_CentralStoneSquare", "Ground", material=self.ground_stone)
        # Four generous paths keep the well and service placeholders readable.
        for label, center, dimensions in (
            ("South", (0, -2850, 3), (640, 3700, 6)),
            ("North", (0, 2850, 3), (640, 3700, 6)),
            ("West", (-2850, 0, 3), (3700, 560, 6)),
            ("East", (2850, 0, 3), (3700, 560, 6)),
        ):
            self.mesh("cube", center, dimensions, "SV_DirtPath_" + label, "Ground", material=self.dirt)
        for i in range(32):
            angle = i * 2.0 * math.pi / 32.0
            self.mesh("stone_tile", (1280 * math.cos(angle), 1280 * math.sin(angle), 15), (210, 85, 25),
                      "SV_PlazaBorder_%02d" % i, "Ground", yaw=math.degrees(angle) + 90)

    def boundary(self):
        # Continuous hidden collision prevents leaving the finite floor even
        # through the visible low fence. These are owned actors in this map.
        for name, center, dimensions in (
            ("North", (0, 5860, 350), (11900, 80, 700)),
            ("South", (0, -5860, 350), (11900, 80, 700)),
            ("West", (-5860, 0, 350), (80, 11900, 700)),
            ("East", (5860, 0, 350), (80, 11900, 700)),
        ):
            wall = self.mesh("cube", center, dimensions, "SV_BoundaryBlocking_" + name, "Gameplay/Boundary", material=self.wood)
            wall.set_actor_hidden_in_game(True)
            wall.get_component_by_class(unreal.StaticMeshComponent).set_visibility(False)
        for side in (-1, 1):
            for z in (70, 135):
                self.mesh("beam", (0, side * 5790, z), (11600, 22, 18), "SV_FenceY_%s_%s" % (side, z), "Nature/Fence")
                self.mesh("beam", (side * 5790, 0, z), (22, 11600, 18), "SV_FenceX_%s_%s" % (side, z), "Nature/Fence")
            for i in range(25):
                offset = -5760 + i * 480
                self.mesh("beam", (offset, side * 5790, 80), (32, 32, 160), "SV_FencePostY_%s_%s" % (side, i), "Nature/Fence")
                self.mesh("beam", (side * 5790, offset, 80), (32, 32, 160), "SV_FencePostX_%s_%s" % (side, i), "Nature/Fence")
        self.report["boundary"] = {"blocking_walls": 4, "inside_limit_cm": 5820, "wall_height_cm": 700,
                                   "visual": "continuous low timber fence", "prevents_floor_walkout": True}

    def well(self):
        for i in range(14):
            angle = i * 2.0 * math.pi / 14.0
            self.mesh("cube", (155 * math.cos(angle), 155 * math.sin(angle), 72), (83, 52, 120),
                      "SV_Well_Rim_%02d" % i, "Landmarks/Well", yaw=math.degrees(angle) + 90, material=self.stone)
        self.mesh("cylinder", (0, 0, 28), (250, 250, 8), "SV_Well_Water", "Landmarks/Well", material=self.water, collision=False)
        for side in (-1, 1):
            self.mesh("beam", (side * 210, 0, 195), (28, 28, 370), "SV_Well_Post_" + str(side), "Landmarks/Well")
            self.mesh("roof", (0, side * 90, 412), (540, 224, 16), "SV_Well_Roof_" + str(side),
                      "Landmarks/Well", roll=side * 32.0, collision=False)
        self.mesh("beam", (0, 0, 470), (555, 25, 25), "SV_Well_Ridge", "Landmarks/Well")

    def house(self, index, x, y, width, depth, height, yaw):
        name = "SV_House_%02d" % index
        folder = "Buildings/House_%02d" % index

        def part(key, local, dims, suffix, extra_yaw=0.0, **kwargs):
            offset = rotate(local, yaw=yaw)
            return self.mesh(key, (x + offset[0], y + offset[1], offset[2]), dims,
                             name + "_" + suffix, folder, yaw=yaw + extra_yaw, **kwargs)

        bottom = 24.0
        part("cube", (0, 0, 12), (width + 70, depth + 70, 24), "Foundation", material=self.stone)
        for side in (-1, 1):
            part("plaster", (0, side * depth / 2, bottom + height / 2), (width, 24, height), "WallY_" + str(side), align_width=True)
            part("plaster", (side * width / 2, 0, bottom + height / 2), (depth, 24, height), "WallX_" + str(side), extra_yaw=90, align_width=True)
            for xx in (-width / 2, 0, width / 2):
                part("beam", (xx, side * (depth / 2 + 14), bottom + height / 2), (26, 32, height + 20), "FrontTimber_%s_%s" % (side, xx))
            part("beam", (0, side * (depth / 2 + 14), bottom + height - 8), (width + 55, 30, 28), "TopBeamY_" + str(side))
            part("beam", (0, side * (depth / 2 + 14), bottom + 30), (width + 55, 30, 26), "BaseBeamY_" + str(side))
        roof_angle = 33.0
        rise = (depth / 2 + 55) * math.tan(math.radians(roof_angle))
        slope = (depth / 2 + 65) / math.cos(math.radians(roof_angle))
        for side in (-1, 1):
            part("roof", (0, side * (depth / 4 + 28), bottom + height + rise / 2),
                 (width + 120, slope, 24), "Roof_" + str(side), roll=side * roof_angle, collision=False)
            part("gable", (side * (width / 2 + 8), 0, bottom + height + rise / 2),
                 (depth + 15, 24, rise), "Gable_" + str(side), extra_yaw=90, align_width=True, collision=False)
        part("beam", (0, 0, bottom + height + rise), (width + 145, 30, 30), "RoofRidge", collision=False)
        part("door", (0, -depth / 2 - 20, bottom + 114), (118, 28, 228), "Door", align_width=True)
        for side in (-1, 1):
            part("window", (side * width * 0.29, -depth / 2 - 25, bottom + height * 0.60),
                 (42, 12, 140), "FrontWindow_" + str(side), align_width=True, collision=False)
            part("window", (side * (width / 2 + 24), 0, bottom + height * 0.60),
                 (42, 12, 140), "SideWindow_" + str(side), extra_yaw=90, align_width=True, collision=False)
        part("chimney", (width * 0.25, depth * 0.23, bottom + height + rise * 0.65),
             (75, 78, 240), "Chimney", collision=False)
        part("stone_tile", (0, -depth / 2 - 100, 22), (205, 150, 25), "Doorstep")
        part("barrel", (width / 2 + 85, -depth / 2 + 100, 65), (90, 90, 130), "Barrel")
        part("crate", (-width / 2 - 90, -depth / 2 + 140, 47), (94, 94, 94), "Crate")
        part("pot", (155, -depth / 2 - 45, 32), (62, 62, 64), "DoorPot", collision=False)
        part("lantern", (85, -depth / 2 - 58, 240), (30, 30, 55), "DoorLantern", collision=False)
        part("sphere", (85, -depth / 2 - 60, 238), (15, 15, 23), "LanternGlow", material=self.gold, collision=False)
        # Blend each doorstep into its approach with a shallow dirt pad.
        approach = rotate((0, -depth / 2 - 255, 3), yaw=yaw)
        self.mesh("cube", (x + approach[0], y + approach[1], 3), (330, 470, 6), name + "_Approach", "Ground", yaw=yaw, material=self.dirt)
        self.report.setdefault("houses", []).append({"id": index, "center": [x, y], "size_cm": [width, depth, height], "yaw": yaw, "exterior_only": True})

    def buildings(self):
        layout = [
            (-2650, -2350, 620, 640, 365, 180), (2600, -2500, 660, 690, 400, 180),
            (-3450, -850, 610, 720, 400, 90), (3500, -800, 700, 780, 430, -90),
            (-3150, 1050, 740, 800, 455, 90), (3200, 1100, 620, 650, 375, -90),
            (-2450, 2850, 650, 690, 410, 0), (2550, 2800, 660, 740, 420, 0),
            (-4200, 3050, 540, 590, 340, 0), (4150, -2850, 560, 620, 355, 180),
        ]
        for index, data in enumerate(layout, 1):
            self.house(index, *data)

    def text(self, label, text, location, size=48, yaw=-90.0, color=(242, 224, 171, 255)):
        actor = self.actors.spawn_actor_from_class(unreal.TextRenderActor, vec(location), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
        self.owned(actor, label, "Preparation/Signs")
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        component.set_editor_property("text", unreal.Text(text))
        component.set_editor_property("world_size", float(size))
        component.set_editor_property("text_render_color", unreal.Color(*color))
        component.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
        return actor

    def preparation_areas(self):
        roles = [
            ("Equipment", "EQUIPMENT", -1420, -790),
            ("Traits", "TRAITS", 1420, -790),
            ("Difficulty", "DIFFICULTY", -1420, 1150),
            ("Story", "STORY", 1420, 1150),
        ]
        for index, (role, title, x, y) in enumerate(roles):
            folder = "Preparation/" + role
            self.mesh("cylinder", (x, y, 16), (410, 410, 32), "SV_" + role + "_ReservedNPCPad", folder, material=self.ground_stone)
            self.mesh("beam", (x - 215, y + 90, 168), (25, 25, 336), "SV_" + role + "_BannerPole", folder)
            self.mesh("cube", (x - 132, y + 92, 264), (146, 9, 160), "SV_" + role + "_Banner", folder, material=self.colors[index], collision=False)
            self.mesh("cube", (x, y - 235, 188), (460, 22, 100), "SV_" + role + "_SignBoard", folder, material=self.wood)
            self.text("SV_" + role + "_RoleSign", title, (x, y - 250, 213), 39)
            self.text("SV_" + role + "_FutureService", {"Equipment": "READY YOUR GEAR", "Traits": "FIND YOUR PATH", "Difficulty": "CHOOSE YOUR TRIAL", "Story": "HEAR THE TALE"}[role], (x, y - 252, 172), 20)
            self.report.setdefault("preparation_areas", []).append({"role": role, "location": [x, y, 32], "status": "reserved scenery; NPC/service functionality not implemented"})

    def nature(self):
        for i in range(42):
            angle = i * 2.0 * math.pi / 42.0 + self.rng.uniform(-0.07, 0.07)
            radius = self.rng.uniform(4700, 5500)
            x, y = radius * math.cos(angle), radius * math.sin(angle)
            # Leave the north portal vista and south entry lane visible.
            if abs(x) < 650 and abs(y) > 4300:
                continue
            self.uniform_mesh("tree_large" if i % 3 == 0 else "tree_medium", (x, y, 0),
                              self.rng.uniform(850, 1300), "SV_PerimeterTree_%02d" % i, yaw=self.rng.uniform(0, 360))
        for i in range(38):
            angle = self.rng.uniform(0, 2 * math.pi)
            radius = self.rng.uniform(1900, 5350)
            x, y = radius * math.cos(angle), radius * math.sin(angle)
            if abs(x) < 620 or abs(y) < 460:
                continue
            key = "rock" if i % 3 == 0 else "bush"
            self.uniform_mesh(key, (x, y, 0), self.rng.uniform(70, 165),
                              "SV_Garden_%s_%02d" % (key, i), yaw=self.rng.uniform(0, 360), collision=key == "rock")
        for i in range(16):
            angle = i * 2.0 * math.pi / 16.0
            x, y = 5600 * math.cos(angle), 5600 * math.sin(angle)
            if abs(x) < 800:
                continue
            self.mesh("rock", (x, y, 125), (780, 520, 360), "SV_EdgeBoulder_%02d" % i, "Nature/Boundary", yaw=math.degrees(angle))
        for x in (-670, 670):
            for y in (-1750, 1750, 3150):
                self.mesh("beam", (x, y, 165), (25, 25, 330), "SV_PathLanternPost_%s_%s" % (x, y), "Props/Lanterns")
                self.mesh("lantern", (x, y, 314), (48, 48, 85), "SV_PathLantern_%s_%s" % (x, y), "Props/Lanterns", collision=False)
                self.mesh("sphere", (x, y, 311), (26, 26, 38), "SV_PathLanternGlow_%s_%s" % (x, y), "Props/Lanterns", material=self.gold, collision=False)

    def portal_and_spawn(self):
        y = 4270.0
        self.mesh("cylinder", (0, y, 15), (830, 720, 30), "SV_PortalStonePad", "Landmarks/Portal", material=self.ground_stone)
        portal = self.actors.spawn_actor_from_class(self.portal_class, vec((0, y, 200)), unreal.Rotator())
        self.owned(portal, "SV_RunPortal_ToPrototypeCombat", "Landmarks/Portal")
        portal.set_editor_property("target_level_name", unreal.Name(TEMPLATE_PATH))
        portal.set_editor_property("start_run_timer_on_travel", True)
        portal.set_editor_property("additional_travel_options", "")
        # A hollow stone arch surrounds the native trigger. Decoration has no
        # collision so only the intentionally approached portal initiates travel.
        for x in (-205, 205):
            self.mesh("cube", (x, y, 230), (90, 110, 430), "SV_PortalPillar_" + str(x), "Landmarks/Portal", material=self.stone)
        for i in range(9):
            angle = i * math.pi / 8.0
            self.mesh("cube", (205 * math.cos(angle), y, 430 + 185 * math.sin(angle)),
                      (93, 115, 82), "SV_PortalArch_%02d" % i, "Landmarks/Portal", pitch=math.degrees(angle) - 90, material=self.stone)
        # Elliptical emissive ring made of short pieces, leaving the center open.
        for i in range(24):
            angle = i * 2.0 * math.pi / 24.0
            self.mesh("sphere", (150 * math.cos(angle), y - 62, 305 + 240 * math.sin(angle)),
                      (31, 20, 39), "SV_PortalRune_%02d" % i, "Landmarks/Portal", material=self.cyan, collision=False)
        self.mesh("sphere", (0, y - 12, 295), (282, 12, 445), "SV_PortalEnergySurface", "Landmarks/Portal", material=self.portal_surface, collision=False)
        self.text("SV_PortalDestinationSign", "COMBAT PORTAL", (0, y - 120, 675), 46, color=(104, 238, 245, 255))
        self.text("SV_PortalPrototypeSign", "ENTER TO BEGIN A RUN", (0, y - 121, 622), 26, color=(193, 233, 236, 255))
        point = self.actors.spawn_actor_from_class(unreal.PlayerStart, vec((0, -2240, 112)), unreal.Rotator(pitch=0, yaw=90, roll=0))
        self.owned(point, "SV_PlayerStart_SouthApproach", "Gameplay")
        point.set_editor_property("player_start_tag", unreal.Name("StartVillage"))
        self.report["portal"] = {"actor": portal.get_path_name(), "target_level": TEMPLATE_PATH,
                                 "start_run_timer_on_travel": True, "location": [0, y, 200]}
        self.report["player_start"] = {"location": [0, -2240, 112], "rotation": [0, 90, 0],
                                       "distance_to_portal_cm": y + 2240, "spawn_outside_portal_overlap": True}


def configure_game_mode(world, builder):
    settings = world.get_world_settings()
    parent = unreal.EditorAssetLibrary.load_blueprint_class("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode")
    if not parent:
        raise RuntimeError("Missing existing first-person GameMode class.")
    parent_cdo = unreal.get_default_object(parent)
    auto_run = bool(parent_cdo.get_editor_property("start_run_on_start_play"))
    prototype_duration = bool(parent_cdo.get_editor_property("use_prototype_run_duration"))
    mode = parent
    if auto_run or prototype_duration:
        path = ASSET_ROOT + "/BP_StartVillageGameMode"
        bp = unreal.load_asset(path)
        if not bp:
            unreal.EditorAssetLibrary.make_directory(ASSET_ROOT)
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_StartVillageGameMode", ASSET_ROOT, unreal.Blueprint, factory)
        if not bp or unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) != parent:
            raise RuntimeError("Unexpected village GameMode parent.")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        mode = bp.generated_class()
        unreal.get_default_object(mode).set_editor_property("start_run_on_start_play", False)
        unreal.get_default_object(mode).set_editor_property("use_prototype_run_duration", False)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        mode = bp.generated_class()
        builder.save_asset(bp)
    mode_cdo = unreal.get_default_object(mode)
    if mode_cdo.get_editor_property("start_run_on_start_play") or mode_cdo.get_editor_property("use_prototype_run_duration"):
        raise RuntimeError("Hub GameMode would auto-start a run or use the prototype duration.")
    settings.set_editor_property("default_game_mode", mode)
    builder.report["game_mode"] = {"class": mode.get_path_name(), "source_class": parent.get_path_name(),
                                   "source_auto_run": auto_run, "hub_auto_run": False,
                                   "source_prototype_duration": prototype_duration, "hub_prototype_duration": False,
                                   "entry_requires_no_StartRun_travel_option": True,
                                   "original_blueprint_modified": False}


def describe_actor(actor):
    return {"label": actor.get_actor_label(), "class": actor.get_class().get_path_name(),
            "path": actor.get_path_name(), "location": values(actor.get_actor_location())}


def clean_new_world(builder, fresh):
    existing = list(builder.actors.get_all_level_actors())
    cleanup_confirmed = any(TEMPLATE_CLEANED_TAG in [str(tag) for tag in actor.get_editor_property("tags")] for actor in existing)
    if not fresh and not cleanup_confirmed:
        builder.report["initial_actor_count"] = len(existing)
        builder.report["incomplete_template_actors"] = [describe_actor(actor) for actor in existing]
        raise RuntimeError("StartHub exists without its TemplateCleaned checkpoint. Inspect incomplete template duplication before retry; unknown untagged actors are preserved.")
    inventory = []
    builder.report["initial_actor_count"] = len(existing)
    builder.report["cleanup_decisions"] = inventory
    for actor in existing:
        row = describe_actor(actor)
        class_name = actor.get_class().get_name().lower()
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        remove = TAG in tags
        reason = "owned village actor from previous execution" if remove else "preserved template environment/system actor"
        if fresh:
            template_fixture = isinstance(actor, (unreal.StaticMeshActor, unreal.PlayerStart, unreal.Pawn))
            environmental = any(part in (class_name + " " + row["label"].lower()) for part in ("sky", "atmosphere", "fog", "cloud", "light"))
            template_fixture = template_fixture and not environmental
            gameplay_fixture = any(part in class_name for part in ("monster", "spawner", "runportal", "wobbletarget", "jumppad", "rewardchest", "heavylootbag"))
            remove = remove or template_fixture or gameplay_fixture
            if remove:
                reason = "template test geometry/player spawn/gameplay fixture removed only from new map"
        row.update({"decision": "remove" if remove else "preserve", "reason": reason})
        inventory.append(row)
        if remove:
            if not builder.actors.destroy_actor(actor):
                raise RuntimeError("Failed to remove new-map actor: " + row["label"])
    builder.report["initial_actor_count"] = len(existing)
    builder.report["cleanup_decisions"] = inventory
    builder.report["removed_actor_count"] = sum(row["decision"] == "remove" for row in inventory)
    builder.report["preserved_actor_count"] = sum(row["decision"] == "preserve" for row in inventory)
    marker = builder.actors.spawn_actor_from_class(unreal.Actor, vec((0, 0, 0)), unreal.Rotator())
    builder.owned(marker, "SV_TemplateCleanupCheckpoint", "Gameplay")
    marker.set_editor_property("tags", [unreal.Name(TAG), unreal.Name(TEMPLATE_CLEANED_TAG)])
    require_world(builder.levels)
    if not builder.levels.save_current_level():
        raise RuntimeError("Could not save new-map template cleanup checkpoint.")
    builder.report["template_cleanup_checkpoint"] = {"tag": TEMPLATE_CLEANED_TAG, "saved": True,
                                                       "source_map_saved": False, "unknown_actors_preserved_on_rerun": True}


def main():
    project = Path(unreal.Paths.project_dir()).resolve()
    if project != Path("D:/Project/Rogue10m").resolve():
        raise RuntimeError("Refusing to create assets or write evidence outside D:/Project/Rogue10m.")
    report_path = project / EVIDENCE_RELATIVE
    report = {"status": "started", "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "map": MAP_PATH, "template_map": TEMPLATE_PATH, "tag": TAG,
              "target_size_m": [120, 120], "source_assets_modified": False}
    try:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if levels.is_in_play_in_editor():
            raise RuntimeError("End PIE before creating or rebuilding StartHub.")
        fresh = not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH)
        builder = VillageBuilder(actors, levels, report)
        builder.preload()
        if fresh:
            if not levels.new_level_from_template(MAP_PATH, TEMPLATE_PATH):
                raise RuntimeError("Cannot duplicate first-person template into StartHub.")
        world = require_world(levels)
        report["new_map_from_template"] = fresh
        clean_new_world(builder, fresh)
        configure_game_mode(world, builder)
        builder.make_materials()
        builder.ground_and_paths()
        builder.boundary()
        builder.well()
        builder.buildings()
        builder.preparation_areas()
        builder.nature()
        builder.portal_and_spawn()
        require_world(levels)
        if not levels.save_current_level():
            raise RuntimeError("Failed to save the new StartHub map.")
        overview = {"location": [-6550, -6900, 5350], "rotation": [-29, 48, 0], "purpose": "southwest overview of village and north portal"}
        player_view = {"location": [0, -2240, 175], "rotation": [0, 90, 0], "purpose": "south approach player eye toward central square"}
        report["viewpoints"] = {"overview": overview, "player_start": player_view}
        try:
            levels.set_level_viewport_camera_info(vec(overview["location"]), unreal.Rotator(pitch=overview["rotation"][0], yaw=overview["rotation"][1], roll=overview["rotation"][2]), levels.get_active_viewport_config_key())
        except Exception as error:
            report["viewport_note"] = str(error)
        report["saved_owned_assets"] = sorted(set(builder.saved_assets))
        report["created_actor_count"] = len(builder.spawned)
        report["created_actor_labels"] = [actor.get_actor_label() for actor in builder.spawned]
        report["status"] = "created"
        report["validation_scope"] = "editor creation and save; PIE movement/portal/visual verification still required"
        unreal.log("[Rogue10mStartVillage] CREATED map=%s houses=%d actors=%d" % (MAP_PATH, len(report["houses"]), len(builder.spawned)))
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        raise
    finally:
        report["completed_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
