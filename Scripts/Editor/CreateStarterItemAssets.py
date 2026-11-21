"""Import starter item icons and create data-driven inventory/equipment assets."""

from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "ItemIcons" / "Starter"
TEXTURE_ROOT = "/Game/Rogue10m/Items/Textures/Starter"
ITEM_ROOT = "/Game/DataAsset/Item/Starter"

ITEMS = (
    {
        "asset": "DA_Item_HealthPotion",
        "texture": "T_Item_HealthPotion",
        "source": "T_Item_HealthPotion.png",
        "id": "HealthPotion",
        "display_name": "회복 포션",
        "description": "붉은 약액이 담긴 기본 회복 포션입니다.",
        "category": unreal.Rogue10mItemCategory.CONSUMABLE,
        "rarity": unreal.Rogue10mItemRarity.COMMON,
        "slot": unreal.Rogue10mInventorySlotType.CONSUMABLE,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (1, 1),
        "stack": 20,
        "weight": 0.2,
        "scale": 1.0,
        "restore_health": 25.0,
    },
    {
        "asset": "DA_Item_IronLongsword",
        "texture": "T_Item_IronLongsword",
        "source": "T_Item_IronLongsword.png",
        "id": "IronLongsword",
        "display_name": "철제 장검",
        "description": "기본기에 충실한 초보자용 철제 장검입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.RARE,
        "slot": unreal.Rogue10mInventorySlotType.MAIN_WEAPON,
        "weapon": unreal.Rogue10mWeaponType.GREAT_SWORD,
        "size": (1, 3),
        "stack": 1,
        "weight": 3.5,
        "scale": 1.0,
        "stats": {
            "attack_power_bonus": 12.0,
            "critical_chance_bonus": 0.02,
        },
    },
    {
        "asset": "DA_Item_IronHelmet",
        "texture": "T_Item_IronHelmet",
        "source": "T_Item_IronHelmet.png",
        "id": "IronHelmet",
        "display_name": "철제 투구",
        "description": "머리를 안정적으로 보호하는 단순한 철제 투구입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.RARE,
        "slot": unreal.Rogue10mInventorySlotType.HEAD,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (2, 2),
        "stack": 1,
        "weight": 2.4,
        "scale": 1.0,
        "stats": {
            "defense_bonus": 5.0,
            "max_health_bonus": 10.0,
        },
    },
    {
        "asset": "DA_Item_GuardianHelmet",
        "texture": "T_Item_IronHelmet",
        "id": "GuardianHelmet",
        "display_name": "수호자의 투구",
        "description": "오랜 전투를 견딘 수호자의 문장이 새겨진 에픽 등급 철제 투구입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.EPIC,
        "slot": unreal.Rogue10mInventorySlotType.HEAD,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (2, 2),
        "stack": 1,
        "weight": 2.8,
        "scale": 1.0,
        "stats": {
            "defense_bonus": 10.0,
            "max_health_bonus": 20.0,
        },
    },
    {
        "asset": "DA_Item_LeatherArmor",
        "texture": "T_Item_LeatherArmor",
        "source": "T_Item_LeatherArmor.png",
        "id": "LeatherArmor",
        "display_name": "가죽 갑옷",
        "description": "움직임과 방어력의 균형을 맞춘 초보자용 가죽 갑옷입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.EPIC,
        "slot": unreal.Rogue10mInventorySlotType.ARMOR,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (2, 3),
        "stack": 1,
        "weight": 4.0,
        "scale": 1.0,
        "stats": {
            "defense_bonus": 12.0,
            "max_health_bonus": 25.0,
        },
    },
    {
        "asset": "DA_Item_LeatherBoots",
        "texture": "T_Item_LeatherBoots",
        "source": "T_Item_LeatherBoots.png",
        "id": "LeatherBoots",
        "display_name": "가죽 장화",
        "description": "발목을 보호하는 튼튼한 가죽 장화 한 쌍입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.COMMON,
        "slot": unreal.Rogue10mInventorySlotType.SHOES,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (2, 2),
        "stack": 1,
        "weight": 1.6,
        "scale": 1.0,
        "stats": {
            "defense_bonus": 3.0,
            "move_speed_bonus": 20.0,
        },
    },
    {
        "asset": "DA_Item_NoviceRing",
        "texture": "T_Item_NoviceRing",
        "source": "T_Item_NoviceRing.png",
        "id": "NoviceRing",
        "display_name": "초심자의 반지",
        "description": "작은 호박석이 박힌 소박한 은제 반지입니다.",
        "category": unreal.Rogue10mItemCategory.EQUIPMENT,
        "rarity": unreal.Rogue10mItemRarity.UNIQUE,
        "slot": unreal.Rogue10mInventorySlotType.RING,
        "weapon": unreal.Rogue10mWeaponType.UNARMED,
        "size": (1, 1),
        "stack": 1,
        "weight": 0.1,
        "scale": 1.0,
        "stats": {
            "attack_power_bonus": 3.0,
            "critical_chance_bonus": 0.03,
            "attack_speed_bonus": 0.04,
        },
    },
)


def import_texture(definition):
    source_name = definition.get("source")
    if not source_name:
        texture = unreal.EditorAssetLibrary.load_asset(
            f"{TEXTURE_ROOT}/{definition['texture']}"
        )
        if not texture:
            raise RuntimeError(f"Existing texture is unavailable: {definition['texture']}")
        return texture

    source = SOURCE_ROOT / source_name
    if not source.is_file():
        raise RuntimeError(f"Missing source icon: {source}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", TEXTURE_ROOT)
    task.set_editor_property("destination_name", definition["texture"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(
        f"{TEXTURE_ROOT}/{definition['texture']}"
    )
    if not texture:
        raise RuntimeError(f"Texture import failed: {definition['texture']}")
    # Match the project's known-good runtime UI texture path on UE 5.8.
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def create_or_load_item(definition, item_class):
    path = f"{ITEM_ROOT}/{definition['asset']}"
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", item_class)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            definition["asset"], ITEM_ROOT, item_class, factory
        )
    if not asset:
        raise RuntimeError(f"Item Data Asset creation failed: {path}")
    return asset


def main():
    item_class = unreal.load_class(None, "/Script/Rogue10m.Rogue10mItemDataAsset")
    if not item_class:
        raise RuntimeError("Rogue10mItemDataAsset is unavailable. Build the Editor target first.")
    for definition in ITEMS:
        texture = import_texture(definition)
        asset = create_or_load_item(definition, item_class)
        asset.set_editor_property("item_id", definition["id"])
        asset.set_editor_property("display_name", definition["display_name"])
        asset.set_editor_property("description", definition["description"])
        asset.set_editor_property("inventory_icon", texture)
        asset.set_editor_property("inventory_icon_scale", definition["scale"])
        asset.set_editor_property("category", definition["category"])
        asset.set_editor_property("rarity", definition["rarity"])
        asset.set_editor_property("equip_slot_type", definition["slot"])
        asset.set_editor_property("weapon_type", definition["weapon"])
        asset.set_editor_property("inventory_size", unreal.IntPoint(*definition["size"]))
        asset.set_editor_property("max_stack_size", definition["stack"])
        asset.set_editor_property("unit_weight", definition["weight"])
        asset.set_editor_property("restore_health", definition.get("restore_health", 0.0))
        equipment_stats = asset.get_editor_property("equipment_stats")
        stat_values = definition.get("stats", {})
        for property_name in (
            "attack_power_bonus",
            "defense_bonus",
            "max_health_bonus",
            "critical_chance_bonus",
            "attack_speed_bonus",
            "move_speed_bonus",
        ):
            equipment_stats.set_editor_property(property_name, stat_values.get(property_name, 0.0))
        asset.set_editor_property("equipment_stats", equipment_stats)
        asset.set_editor_property("can_drop", True)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

        expected_width = definition["size"][0] * 256
        expected_height = definition["size"][1] * 256
        actual_width = texture.blueprint_get_size_x()
        actual_height = texture.blueprint_get_size_y()
        if (actual_width, actual_height) != (expected_width, expected_height):
            raise RuntimeError(
                f"Texture size validation failed: {texture.get_path_name()} -> "
                f"{actual_width}x{actual_height}, expected {expected_width}x{expected_height}"
            )
        actual_size = asset.get_editor_property("inventory_size")
        if (actual_size.x, actual_size.y) != definition["size"]:
            raise RuntimeError(
                f"Inventory size validation failed: {asset.get_path_name()} -> {actual_size}"
            )
        if not asset.get_editor_property("inventory_icon"):
            raise RuntimeError(f"Inventory icon validation failed: {asset.get_path_name()}")
        unreal.log(
            f"[StarterItemAssets] Ready: {asset.get_path_name()} "
            f"({actual_size.x}x{actual_size.y}, {actual_width}x{actual_height}px)"
        )

    unreal.log("[StarterItemAssets] Seven starter inventory/equipment assets validated.")


if __name__ == "__main__":
    main()
