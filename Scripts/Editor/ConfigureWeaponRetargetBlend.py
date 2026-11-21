"""Enable continuity across the two existing Human Male retarget graphs."""

import unreal


ASSETS = (
    '/Game/Character/Customization/Retargeting/ABP_Retarget_Hu_M',
    '/Game/Rogue10m/Animation/Combat/GreatSword20261110/ABP_Human_GreatSword',
)


def main():
    native = unreal.Rogue10mWeaponRetargetAnimInstance
    blueprints = [unreal.load_asset(path) for path in ASSETS]
    if not all(isinstance(blueprint, unreal.AnimBlueprint) for blueprint in blueprints):
        raise RuntimeError('The existing Human Male retarget graphs are required')
    for blueprint in blueprints:
        skeleton = blueprint.get_editor_property('target_skeleton')
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, native.static_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        generated_class = blueprint.generated_class()
        if not generated_class or not isinstance(unreal.get_default_object(generated_class), native):
            raise RuntimeError(f'Could not compile the retarget parent in {blueprint.get_path_name()}')
        if not skeleton or blueprint.get_editor_property('target_skeleton') != skeleton:
            raise RuntimeError(f'Retarget skeleton changed in {blueprint.get_path_name()}')
        print(f'WEAPON_RETARGET_BLEND graph={blueprint.get_path_name()} skeleton={skeleton.get_path_name()}')
    for blueprint in blueprints:
        if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
            raise RuntimeError(f'Could not save {blueprint.get_path_name()}')
    print('RESULT=WEAPON_RETARGET_BLEND_CONFIGURED')


if __name__ == '__main__':
    main()
