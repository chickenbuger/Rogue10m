"""Author identity elixirs and their Crimson Slime loot via Unreal Editor APIs."""

import unreal


ITEM_ROOT = "/Game/DataAsset/Item/Starter"
LOOT_ROOT = "/Game/Rogue10m/World/OpenWorld/Loot"
LOOT_NAME = "DA_Loot_OW_CrimsonSlime_IdentityElixirs"
CRIMSON_PROFILE = "/Game/Rogue10m/World/OpenWorld/Monsters/DA_OW_CrimsonSlime"
CRIMSON_BLUEPRINT = "/Game/Rogue10m/World/OpenWorld/Encounters/BP_OW_CrimsonSlime"
DROP_CHANCE = 0.25
ITEMS = (
    ("InternalEnergyElixir", "내공 영약", "내공", "INTERNAL_ENERGY", (0.30, 0.76, 0.35)),
    ("FightingSpiritElixir", "투지 영약", "투지", "FIGHTING_SPIRIT", (0.90, 0.36, 0.24)),
    ("ManaElixir", "마나 영약", "마나", "MANA", (0.32, 0.59, 0.93)),
)


def get_or_create(root, name, asset_class):
    package = f"{root}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        asset = unreal.load_asset(package)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_class)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, asset_class, factory)
    if not isinstance(asset, asset_class):
        raise RuntimeError(f"Unexpected asset at {package}")
    return asset


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save {asset.get_path_name()}")


def configure_item(item, definition, bottle_icon, bottle_mesh):
    item_id, name, identity_label, enum_value, color = definition
    item.set_editor_properties({
        "item_id": item_id,
        "display_name": name,
        "description": f"이번 탐험의 {identity_label} 성장 단계가 1 증가합니다.",
        "category": unreal.Rogue10mItemCategory.CONSUMABLE,
        "equip_slot_type": unreal.Rogue10mInventorySlotType.CONSUMABLE,
        "rarity": unreal.Rogue10mItemRarity.RARE,
        "inventory_size": unreal.IntPoint(1, 1),
        "max_stack_size": 5,
        "unit_weight": 0.2,
        "inventory_icon": bottle_icon,
        "inventory_tint": unreal.LinearColor(*color, 1.0),
        "dropped_world_mesh": bottle_mesh,
        "restore_health": 0.0,
        "identity_growth_type": getattr(unreal.Rogue10mSkillTreeIdentity, enum_value),
        "identity_growth_amount": 1,
    })


def main():
    monster = unreal.load_asset(CRIMSON_PROFILE)
    monster_class = unreal.EditorAssetLibrary.load_blueprint_class(CRIMSON_BLUEPRINT)
    if not monster or not monster_class or unreal.get_default_object(monster_class).get_editor_property("monster_data") != monster:
        raise RuntimeError("Crimson Slime Blueprint no longer uses the expected OpenRunMap profile")

    table_path = f"{LOOT_ROOT}/{LOOT_NAME}.{LOOT_NAME}"
    current_table = monster.get_editor_property("loot_table")
    if current_table and current_table.get_path_name() != table_path:
        raise RuntimeError(f"Crimson Slime already uses {current_table.get_path_name()}")

    potion = unreal.load_asset(f"{ITEM_ROOT}/DA_Item_HealthPotion")
    bottle_icon = potion.get_editor_property("inventory_icon") if potion else None
    if not bottle_icon:
        raise RuntimeError("The existing Health Potion icon is unavailable")
    bottle_mesh = potion.get_editor_property("dropped_world_mesh")

    items = []
    for definition in ITEMS:
        item = get_or_create(ITEM_ROOT, f"DA_Item_{definition[0]}", unreal.Rogue10mItemDataAsset)
        configure_item(item, definition, bottle_icon, bottle_mesh)
        items.append(item)

    table = get_or_create(LOOT_ROOT, LOOT_NAME, unreal.Rogue10mLootTableDataAsset)
    entries = []
    for item in items:
        entry = unreal.Rogue10mLootEntry()
        entry.set_editor_properties({"item_data": item, "drop_chance": DROP_CHANCE, "min_quantity": 1, "max_quantity": 1})
        entries.append(entry)
    table.set_editor_properties({
        "loot_table_id": "CrimsonSlimeIdentityElixirs",
        "entries": entries,
        "balance_notes": "붉은 점액체 처치 시 영약마다 25% 확률로 1개 드롭합니다.",
    })
    if table.validate_loot_table() is None:
        raise RuntimeError("Invalid Crimson Slime identity elixir loot table")

    for item in items:
        save(item)
    save(table)
    if current_table != table:
        monster.set_editor_property("loot_table", table)
        save(monster)
    unreal.log("RESULT=IDENTITY_GROWTH_ITEMS_CREATED")


if __name__ == "__main__":
    main()
