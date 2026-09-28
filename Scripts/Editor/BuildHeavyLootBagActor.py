"""Create the reusable heavy loot bag Actor Blueprint."""

import unreal


ASSET_FOLDER = "/Game/World/Loot"
ASSET_NAME = "BP_HeavyLootBag"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"

SPHERE_MESH_PATH = "/Engine/BasicShapes/Sphere"
CYLINDER_MESH_PATH = "/Engine/BasicShapes/Cylinder"
BAG_MATERIAL_PATH = (
    "/Game/Stylized_Village/Assets/Props/Tarps/_Materials/MI_Tarps_Dark"
)
ROPE_MATERIAL_PATH = (
    "/Game/Stylized_Village/Assets/Props/Furniture/"
    "_Materials/MI_Furniture_Crate"
)


def log(message):
    unreal.log(f"[Rogue10mHeavyLootBag] {message}")


def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset could not be loaded: {path}")
    return asset


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


def ensure_component(blueprint, component_class, component_name):
    subsystem, entries = get_subobject_entries(blueprint)
    for _, _, obj in entries:
        if obj and obj.get_name() == component_name:
            if not isinstance(obj, component_class):
                raise RuntimeError(
                    f"Component type mismatch for {component_name}: {obj.get_class()}"
                )
            return obj

    root_handle = None
    for handle, data, _ in entries:
        if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data):
            root_handle = handle
            break
    if not root_handle:
        raise RuntimeError("Blueprint root component handle was not found")

    handle, failure_reason = subsystem.add_new_subobject(
        unreal.AddNewSubobjectParams(
            parent_handle=root_handle,
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
    component.set_editor_property(
        "generate_overlap_events", collision_profile == "OverlapAllDynamic"
    )
    component.set_editor_property("cast_shadow", True)
    set_transform(component, location, scale, rotation)


def create_or_load_blueprint():
    blueprint = unreal.load_asset(ASSET_PATH)
    if blueprint:
        expected_parent = unreal.Rogue10mDroppedItem.static_class()
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
    factory.set_editor_property(
        "parent_class", unreal.Rogue10mDroppedItem.static_class()
    )
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_FOLDER,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"Could not create Blueprint: {ASSET_PATH}")
    return blueprint


def configure_inherited_components():
    generated_class = get_blueprint_class()
    cdo = unreal.get_default_object(generated_class)

    item_mesh = cdo.get_editor_property("item_mesh")
    item_mesh.set_editor_property("visible", False)
    item_mesh.set_editor_property("hidden_in_game", True)
    item_mesh.set_collision_profile_name("NoCollision")
    item_mesh.set_editor_property("generate_overlap_events", False)

    item_name_text = cdo.get_editor_property("item_name_text")
    item_name_text.set_editor_property(
        "relative_location", unreal.Vector(0.0, 0.0, 92.0)
    )
    item_name_text.set_editor_property("world_size", 15.0)
    item_name_text.set_editor_property(
        "text_render_color", unreal.Color(246, 210, 112, 255)
    )
    item_name_text.set_editor_property("text", unreal.Text("전리품 주머니"))


def main():
    blueprint = create_or_load_blueprint()
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    sphere_mesh = require_asset(SPHERE_MESH_PATH)
    cylinder_mesh = require_asset(CYLINDER_MESH_PATH)
    bag_material = require_asset(BAG_MATERIAL_PATH)
    rope_material = require_asset(ROPE_MATERIAL_PATH)

    body = ensure_component(
        blueprint, unreal.StaticMeshComponent, "LootBagBody"
    )
    configure_mesh_component(
        body,
        sphere_mesh,
        bag_material,
        (0.0, 0.0, 22.0),
        (0.48, 0.42, 0.40),
        collision_profile="OverlapAllDynamic",
    )

    neck = ensure_component(
        blueprint, unreal.StaticMeshComponent, "LootBagNeck"
    )
    configure_mesh_component(
        neck,
        cylinder_mesh,
        bag_material,
        (0.0, 0.0, 45.0),
        (0.20, 0.18, 0.14),
    )

    knot = ensure_component(
        blueprint, unreal.StaticMeshComponent, "LootBagKnot"
    )
    configure_mesh_component(
        knot,
        sphere_mesh,
        rope_material,
        (0.0, 0.0, 54.0),
        (0.13, 0.11, 0.10),
    )

    left_tie = ensure_component(
        blueprint, unreal.StaticMeshComponent, "LootBagTieLeft"
    )
    configure_mesh_component(
        left_tie,
        cylinder_mesh,
        rope_material,
        (0.0, -9.0, 49.0),
        (0.035, 0.035, 0.18),
        (34.0, 0.0, 0.0),
    )

    right_tie = ensure_component(
        blueprint, unreal.StaticMeshComponent, "LootBagTieRight"
    )
    configure_mesh_component(
        right_tie,
        cylinder_mesh,
        rope_material,
        (0.0, 9.0, 49.0),
        (0.035, 0.035, 0.18),
        (-34.0, 0.0, 0.0),
    )

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    configure_inherited_components()
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError(f"Could not save Blueprint: {ASSET_PATH}")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"Created and saved {ASSET_PATH}")


if __name__ == "__main__":
    main()
