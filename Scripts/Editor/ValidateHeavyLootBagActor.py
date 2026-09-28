"""Validate the heavy loot bag Blueprint in a fresh Unreal process."""

import unreal


ASSET_PATH = "/Game/World/Loot/BP_HeavyLootBag"
EXPECTED_COMPONENTS = {
    "LootBagBody": "/Engine/BasicShapes/Sphere.Sphere",
    "LootBagNeck": "/Engine/BasicShapes/Cylinder.Cylinder",
    "LootBagKnot": "/Engine/BasicShapes/Sphere.Sphere",
    "LootBagTieLeft": "/Engine/BasicShapes/Cylinder.Cylinder",
    "LootBagTieRight": "/Engine/BasicShapes/Cylinder.Cylinder",
}


def fail(message):
    unreal.log_error(f"[Rogue10mHeavyLootBagValidator] {message}")
    raise RuntimeError(message)


def main():
    blueprint = unreal.load_asset(ASSET_PATH)
    if not blueprint:
        fail(f"Blueprint could not be loaded: {ASSET_PATH}")

    expected_parent = unreal.Rogue10mDroppedItem.static_class()
    actual_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(
        blueprint
    )
    if actual_parent != expected_parent:
        fail(f"Unexpected parent class: {actual_parent}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSET_PATH)
    if not generated_class:
        fail("GeneratedClass could not be loaded")
    cdo = unreal.get_default_object(generated_class)

    item_mesh = cdo.get_editor_property("item_mesh")
    if item_mesh.get_editor_property("visible"):
        fail("Inherited ItemMesh must remain hidden")
    if not item_mesh.get_editor_property("hidden_in_game"):
        fail("Inherited ItemMesh must be hidden in game")

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if not subsystem:
        fail("SubobjectDataSubsystem is unavailable")

    components = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        component = (
            unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(
                data
            )
        )
        if isinstance(component, unreal.StaticMeshComponent):
            component_name = component.get_name().removesuffix("_GEN_VARIABLE")
            components[component_name] = component

    for name, expected_mesh_path in EXPECTED_COMPONENTS.items():
        component = components.get(name)
        if not component:
            fail(f"Missing component: {name}")
        mesh = component.get_editor_property("static_mesh")
        if not mesh or mesh.get_path_name() != expected_mesh_path:
            fail(
                f"Unexpected mesh for {name}: "
                f"{mesh.get_path_name() if mesh else 'None'}"
            )
        if not component.get_material(0):
            fail(f"Missing material on component: {name}")

    body = components["LootBagBody"]
    if body.get_collision_profile_name() != "OverlapAllDynamic":
        fail(
            "LootBagBody must use OverlapAllDynamic, got "
            f"{body.get_collision_profile_name()}"
        )

    for name in (
        "LootBagNeck",
        "LootBagKnot",
        "LootBagTieLeft",
        "LootBagTieRight",
    ):
        if components[name].get_collision_profile_name() != "NoCollision":
            fail(f"Decorative component must use NoCollision: {name}")

    item_name_text = cdo.get_editor_property("item_name_text")
    if item_name_text.get_editor_property("world_size") != 15.0:
        fail("ItemNameText world size is not configured")

    unreal.log(
        "[Rogue10mHeavyLootBagValidator] RESULT=PASSED "
        f"asset={ASSET_PATH} components={len(EXPECTED_COMPONENTS)}"
    )


if __name__ == "__main__":
    main()
