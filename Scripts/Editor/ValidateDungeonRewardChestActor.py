"""Validate the dungeon reward chest Blueprint in a fresh Unreal process."""

import unreal


ASSET_PATH = "/Game/World/Rewards/BP_DungeonRewardChest"
EXPECTED_MESH_COMPONENTS = {
    "ChestBase": "/Engine/BasicShapes/Cube.Cube",
    "ChestBaseFoot": "/Engine/BasicShapes/Cube.Cube",
    "ChestBaseRim": "/Engine/BasicShapes/Cube.Cube",
    "ChestBaseBandLeft": "/Engine/BasicShapes/Cube.Cube",
    "ChestBaseBandRight": "/Engine/BasicShapes/Cube.Cube",
    "ChestLidCrown": "/Engine/BasicShapes/Cylinder.Cylinder",
    "ChestLidFront": "/Engine/BasicShapes/Cube.Cube",
    "ChestLidBandLeft": "/Engine/BasicShapes/Cube.Cube",
    "ChestLidBandRight": "/Engine/BasicShapes/Cube.Cube",
    "ChestLatch": "/Engine/BasicShapes/Cube.Cube",
    "ChestLockCore": "/Engine/BasicShapes/Sphere.Sphere",
}


def fail(message):
    unreal.log_error(f"[Rogue10mDungeonRewardChestValidator] {message}")
    raise RuntimeError(message)


def normalized_name(obj):
    return obj.get_name().removesuffix("_GEN_VARIABLE")


def main():
    blueprint = unreal.load_asset(ASSET_PATH)
    if not blueprint:
        fail(f"Blueprint could not be loaded: {ASSET_PATH}")

    expected_parent = unreal.Actor.static_class()
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

    actor_tags = {str(tag) for tag in cdo.get_editor_property("tags")}
    for required_tag in ("DungeonRewardChest", "RewardContainer"):
        if required_tag not in actor_tags:
            fail(f"Missing Actor tag: {required_tag}")

    if cdo.is_actor_tick_enabled():
        fail("Actor Tick must be disabled")

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
        if component:
            components[normalized_name(component)] = component

    lid_pivot = components.get("LidPivot")
    if not isinstance(lid_pivot, unreal.SceneComponent):
        fail("LidPivot SceneComponent is missing")

    for name, expected_mesh_path in EXPECTED_MESH_COMPONENTS.items():
        component = components.get(name)
        if not isinstance(component, unreal.StaticMeshComponent):
            fail(f"Missing StaticMeshComponent: {name}")
        mesh = component.get_editor_property("static_mesh")
        if not mesh or mesh.get_path_name() != expected_mesh_path:
            fail(
                f"Unexpected mesh for {name}: "
                f"{mesh.get_path_name() if mesh else 'None'}"
            )
        if not component.get_material(0):
            fail(f"Missing material on component: {name}")

    for blocking_name in ("ChestBase", "ChestLidCrown"):
        if (
            components[blocking_name].get_collision_profile_name()
            != "BlockAllDynamic"
        ):
            fail(f"Blocking collision is missing: {blocking_name}")

    for decorative_name in (
        set(EXPECTED_MESH_COMPONENTS)
        - {"ChestBase", "ChestLidCrown"}
    ):
        if (
            components[decorative_name].get_collision_profile_name()
            != "NoCollision"
        ):
            fail(f"Decorative component must use NoCollision: {decorative_name}")

    interaction_zone = components.get("RewardInteractionZone")
    if not isinstance(interaction_zone, unreal.BoxComponent):
        fail("RewardInteractionZone BoxComponent is missing")
    if interaction_zone.get_collision_profile_name() != "OverlapAllDynamic":
        fail("RewardInteractionZone must use OverlapAllDynamic")
    if not interaction_zone.get_editor_property("generate_overlap_events"):
        fail("RewardInteractionZone overlap events must be enabled")

    reward_glow = components.get("RewardGlow")
    if not isinstance(reward_glow, unreal.PointLightComponent):
        fail("RewardGlow PointLightComponent is missing")
    if reward_glow.get_editor_property("cast_shadows"):
        fail("RewardGlow must not cast shadows")
    if reward_glow.get_editor_property("attenuation_radius") > 220.0:
        fail("RewardGlow attenuation radius is too large")

    unreal.log(
        "[Rogue10mDungeonRewardChestValidator] RESULT=PASSED "
        f"asset={ASSET_PATH} meshes={len(EXPECTED_MESH_COMPONENTS)} "
        "interaction=1 glow=1"
    )


if __name__ == "__main__":
    main()
