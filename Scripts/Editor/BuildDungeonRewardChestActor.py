"""Create the reusable dungeon reward chest Actor Blueprint."""

import unreal


ASSET_FOLDER = "/Game/World/Rewards"
ASSET_NAME = "BP_DungeonRewardChest"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube"
CYLINDER_MESH_PATH = "/Engine/BasicShapes/Cylinder"
SPHERE_MESH_PATH = "/Engine/BasicShapes/Sphere"
WOOD_MATERIAL_PATH = (
    "/Game/Stylized_Village/Assets/Props/Furniture/"
    "_Materials/MI_Furniture_Crate"
)
GOLD_MATERIAL_PATH = "/Game/StarterContent/Materials/M_Metal_Gold"
STEEL_MATERIAL_PATH = (
    "/Game/StarterContent/Materials/M_Metal_Burnished_Steel"
)


def log(message):
    unreal.log(f"[Rogue10mDungeonRewardChest] {message}")


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset could not be loaded: {path}")
    return asset


def normalized_name(obj):
    return obj.get_name().removesuffix("_GEN_VARIABLE")


def get_blueprint_class():
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSET_PATH)
    if not generated_class:
        raise RuntimeError(f"Blueprint GeneratedClass could not be loaded: {ASSET_PATH}")
    return generated_class


def get_subobject_entries(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if not subsystem:
        raise RuntimeError("SubobjectDataSubsystem is unavailable")

    entries = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        entries.append((handle, data, obj))
    return subsystem, entries


def find_handle(entries, component_name):
    for handle, _, obj in entries:
        if obj and normalized_name(obj) == component_name:
            return handle
    return None


def ensure_component(
    blueprint,
    component_class,
    component_name,
    parent_name=None,
):
    subsystem, entries = get_subobject_entries(blueprint)
    for _, _, obj in entries:
        if obj and normalized_name(obj) == component_name:
            if not isinstance(obj, component_class):
                raise RuntimeError(
                    f"Component type mismatch for {component_name}: {obj.get_class()}"
                )
            return obj

    if parent_name:
        parent_handle = find_handle(entries, parent_name)
        if not parent_handle:
            raise RuntimeError(f"Parent component was not found: {parent_name}")
    else:
        parent_handle = None
        for handle, data, _ in entries:
            if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data):
                parent_handle = handle
                break

    if not parent_handle:
        actor_handle = None
        for handle, data, _ in entries:
            if unreal.SubobjectDataBlueprintFunctionLibrary.is_actor(data):
                actor_handle = handle
                break
        if not actor_handle:
            raise RuntimeError("Blueprint actor handle was not found")
        parent_handle, failure_reason = subsystem.add_new_subobject(
            unreal.AddNewSubobjectParams(
                parent_handle=actor_handle,
                new_class=unreal.SceneComponent.static_class(),
                blueprint_context=blueprint,
            )
        )
        if not failure_reason.is_empty():
            raise RuntimeError(
                f"Could not add SceneRoot: {failure_reason.to_string()}"
            )
        subsystem.rename_subobject(
            handle=parent_handle,
            new_name=unreal.Text("SceneRoot"),
        )
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    handle, failure_reason = subsystem.add_new_subobject(
        unreal.AddNewSubobjectParams(
            parent_handle=parent_handle,
            new_class=component_class.static_class(),
            blueprint_context=blueprint,
        )
    )
    if not failure_reason.is_empty():
        raise RuntimeError(
            f"Could not add {component_name}: {failure_reason.to_string()}"
        )
    subsystem.rename_subobject(
        handle=handle,
        new_name=unreal.Text(component_name),
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    component = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
    if not component:
        raise RuntimeError(f"Created component could not be resolved: {component_name}")
    return component


def set_transform(component, location, scale, rotation=None):
    component.set_editor_property("relative_location", unreal.Vector(*location))
    component.set_editor_property("relative_scale3d", unreal.Vector(*scale))
    if rotation is not None:
        component.set_editor_property(
            "relative_rotation",
            unreal.Rotator(
                roll=rotation[0],
                pitch=rotation[1],
                yaw=rotation[2],
            ),
        )


def configure_mesh_component(
    component,
    mesh,
    material,
    location,
    scale,
    rotation=None,
    collision_profile="NoCollision",
):
    component.set_editor_property("static_mesh", mesh)
    component.set_material(0, material)
    component.set_collision_profile_name(collision_profile)
    component.set_editor_property("generate_overlap_events", False)
    component.set_editor_property("cast_shadow", True)
    set_transform(component, location, scale, rotation)


def create_or_load_blueprint():
    blueprint = unreal.load_asset(ASSET_PATH)
    if blueprint:
        expected_parent = unreal.Actor.static_class()
        actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
            blueprint
        )
        if actual_parent != expected_parent:
            raise RuntimeError(
                f"Existing Blueprint has an unexpected parent: {actual_parent}"
            )
        return blueprint

    unreal.EditorAssetLibrary.make_directory(ASSET_FOLDER)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor.static_class())
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_FOLDER,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"Could not create Blueprint: {ASSET_PATH}")
    return blueprint


def configure_actor_defaults():
    generated_class = get_blueprint_class()
    cdo = unreal.get_default_object(generated_class)
    cdo.set_editor_property(
        "tags",
        [
            unreal.Name("DungeonRewardChest"),
            unreal.Name("RewardContainer"),
        ],
    )
    cdo.set_actor_tick_enabled(False)


def main():
    blueprint = create_or_load_blueprint()
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cube_mesh = require_asset(CUBE_MESH_PATH)
    cylinder_mesh = require_asset(CYLINDER_MESH_PATH)
    sphere_mesh = require_asset(SPHERE_MESH_PATH)
    wood_material = require_asset(WOOD_MATERIAL_PATH)
    gold_material = require_asset(GOLD_MATERIAL_PATH)
    steel_material = require_asset(STEEL_MATERIAL_PATH)

    base = ensure_component(
        blueprint, unreal.StaticMeshComponent, "ChestBase"
    )
    configure_mesh_component(
        base,
        cube_mesh,
        wood_material,
        (0.0, 0.0, 24.0),
        (1.20, 0.72, 0.48),
        collision_profile="BlockAllDynamic",
    )

    base_foot = ensure_component(
        blueprint, unreal.StaticMeshComponent, "ChestBaseFoot"
    )
    configure_mesh_component(
        base_foot,
        cube_mesh,
        steel_material,
        (0.0, 0.0, 4.0),
        (1.28, 0.78, 0.08),
    )

    base_rim = ensure_component(
        blueprint, unreal.StaticMeshComponent, "ChestBaseRim"
    )
    configure_mesh_component(
        base_rim,
        cube_mesh,
        gold_material,
        (0.0, 0.0, 48.0),
        (1.28, 0.78, 0.08),
    )

    for component_name, x_location in (
        ("ChestBaseBandLeft", -40.0),
        ("ChestBaseBandRight", 40.0),
    ):
        band = ensure_component(
            blueprint, unreal.StaticMeshComponent, component_name
        )
        configure_mesh_component(
            band,
            cube_mesh,
            gold_material,
            (x_location, -37.5, 27.0),
            (0.09, 0.04, 0.48),
        )

    lid_pivot = ensure_component(
        blueprint, unreal.SceneComponent, "LidPivot"
    )
    lid_pivot.set_editor_property(
        "relative_location", unreal.Vector(0.0, 34.0, 48.0)
    )

    lid_crown = ensure_component(
        blueprint,
        unreal.StaticMeshComponent,
        "ChestLidCrown",
        parent_name="LidPivot",
    )
    configure_mesh_component(
        lid_crown,
        cylinder_mesh,
        wood_material,
        (0.0, -34.0, 3.0),
        (1.20, 0.60, 0.72),
        (90.0, 0.0, 0.0),
        collision_profile="BlockAllDynamic",
    )

    lid_front = ensure_component(
        blueprint,
        unreal.StaticMeshComponent,
        "ChestLidFront",
        parent_name="LidPivot",
    )
    configure_mesh_component(
        lid_front,
        cube_mesh,
        wood_material,
        (0.0, -70.0, 12.0),
        (1.18, 0.05, 0.28),
    )

    for component_name, x_location in (
        ("ChestLidBandLeft", -40.0),
        ("ChestLidBandRight", 40.0),
    ):
        band = ensure_component(
            blueprint,
            unreal.StaticMeshComponent,
            component_name,
            parent_name="LidPivot",
        )
        configure_mesh_component(
            band,
            cube_mesh,
            gold_material,
            (x_location, -73.0, 15.0),
            (0.10, 0.04, 0.32),
        )

    latch = ensure_component(
        blueprint, unreal.StaticMeshComponent, "ChestLatch"
    )
    configure_mesh_component(
        latch,
        cube_mesh,
        gold_material,
        (0.0, -40.0, 48.0),
        (0.24, 0.05, 0.28),
    )

    lock_core = ensure_component(
        blueprint, unreal.StaticMeshComponent, "ChestLockCore"
    )
    configure_mesh_component(
        lock_core,
        sphere_mesh,
        steel_material,
        (0.0, -44.0, 42.0),
        (0.12, 0.06, 0.14),
    )

    interaction_zone = ensure_component(
        blueprint, unreal.BoxComponent, "RewardInteractionZone"
    )
    interaction_zone.set_box_extent(unreal.Vector(95.0, 95.0, 75.0), False)
    interaction_zone.set_editor_property(
        "relative_location", unreal.Vector(0.0, 0.0, 45.0)
    )
    interaction_zone.set_collision_profile_name("OverlapAllDynamic")
    interaction_zone.set_editor_property("generate_overlap_events", True)

    reward_glow = ensure_component(
        blueprint, unreal.PointLightComponent, "RewardGlow"
    )
    reward_glow.set_editor_property(
        "relative_location", unreal.Vector(0.0, 0.0, 88.0)
    )
    reward_glow.set_editor_property("intensity", 700.0)
    reward_glow.set_editor_property("attenuation_radius", 220.0)
    reward_glow.set_editor_property(
        "light_color", unreal.Color(255, 184, 72, 255)
    )
    reward_glow.set_editor_property("cast_shadows", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    configure_actor_defaults()
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Could not save Blueprint: {ASSET_PATH}")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"Created and saved {ASSET_PATH}")


if __name__ == "__main__":
    main()
