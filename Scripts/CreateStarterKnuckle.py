"""Author the starter item through Unreal APIs; never rewrites an existing asset.

Run in UnrealEditor-Cmd with PythonScriptPlugin enabled:
  -run=pythonscript -script=<absolute path to this file>
The native project module must already be built and loadable.
"""
import json
import unreal

ASSET_PATH = "/Game/DataAsset/Item/Starter/DA_Item_StarterKnuckle"


def validate(asset):
    checks = {
        "class": asset.get_class().get_path_name() == "/Script/Rogue10m.Rogue10mItemDataAsset",
        "item_id": str(asset.get_editor_property("item_id")) == "Item.Starter.Knuckle",
        "weapon": asset.get_editor_property("weapon_type") == unreal.Rogue10mWeaponType.KNUCKLE,
        "slot": asset.get_editor_property("equip_slot_type") == unreal.Rogue10mInventorySlotType.MAIN_WEAPON,
        "category": asset.get_editor_property("category") == unreal.Rogue10mItemCategory.EQUIPMENT,
        "stack": asset.get_editor_property("max_stack_size") == 1,
    }
    if not all(checks.values()):
        raise RuntimeError("Existing or created starter asset failed validation: " + json.dumps(checks))
    return checks


if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    checks = validate(asset)
    unreal.log("STARTER_KNUCKLE=" + json.dumps({"path": ASSET_PATH, "created": False, "checks": checks}))
else:
    asset_class = unreal.load_class(None, "/Script/Rogue10m.Rogue10mItemDataAsset")
    if asset_class is None:
        raise RuntimeError("Rogue10m item class is not available")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_Item_StarterKnuckle", "/Game/DataAsset/Item/Starter", asset_class, factory)
    if asset is None:
        raise RuntimeError("Starter asset creation failed")
    asset.set_editor_property("item_id", "Item.Starter.Knuckle")
    asset.set_editor_property("display_name", "수련용 너클")
    asset.set_editor_property("description", "매 판 시작할 때 지급되는 기본 너클입니다. 다른 무기는 필드에서 획득합니다.")
    asset.set_editor_property("weapon_type", unreal.Rogue10mWeaponType.KNUCKLE)
    asset.set_editor_property("equip_slot_type", unreal.Rogue10mInventorySlotType.MAIN_WEAPON)
    asset.set_editor_property("category", unreal.Rogue10mItemCategory.EQUIPMENT)
    asset.set_editor_property("rarity", unreal.Rogue10mItemRarity.COMMON)
    asset.set_editor_property("inventory_size", unreal.IntPoint(2, 1))
    asset.set_editor_property("max_stack_size", 1)
    asset.set_editor_property("unit_weight", 0.5)
    asset.set_editor_property("inventory_tint", unreal.LinearColor(0.95, 0.46, 0.24, 1.0))
    checks = validate(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Starter asset was created but save failed; inspect before retry")
    unreal.log("STARTER_KNUCKLE=" + json.dumps({"path": ASSET_PATH, "created": True, "checks": checks}))
