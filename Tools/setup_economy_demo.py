"""Read, create/update, and verify the exact Demo economy assets in Unreal Editor.

Run with ``-EconomyDryRun`` for a read-only inventory. Without that switch the
script migrates Combat definition assets to schema v2, configures six existing
Demo items, creates the economy/shop DataAssets, and assigns them to the Demo
GameMode Blueprint defaults.
"""

import unreal


ROOT = "/Game/Combat/Demo"
ECONOMY_DIR = ROOT + "/Economy"
ECONOMY_PATH = ECONOMY_DIR + "/DA_DemoEconomy"
SHOP_PATH = ECONOMY_DIR + "/DA_DemoShop"
GAME_MODE_PATH = ROOT + "/Framework/BP_CombatDemoGameMode"
ITEM_PATHS = {
    "potion": ROOT + "/Items/DA_healing_potion",
    "ring": ROOT + "/Items/DA_armor_ring",
    "boots": ROOT + "/Items/DA_travel_boots",
    "idol": ROOT + "/Items/DA_guardian_idol",
    "wand": ROOT + "/Items/DA_lightning_wand",
    "blade": ROOT + "/Items/DA_thunder_blade",
}
UNIT_REWARD_PATH = ROOT + "/Heros/WoodenDummy/DA_WoodenDummyUnit"


def load_required(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Required asset is missing: " + path)
    return asset


def describe_item(label, item):
    unreal.log(
        "ECON_READ item={} class={} definition={} schema={}".format(
            label,
            item.get_class().get_name(),
            item.get_editor_property("definition_name"),
            item.get_editor_property("schema_version"),
        )
    )


def create_data_asset(asset_name, package_path, asset_class):
    existing = unreal.EditorAssetLibrary.load_asset(package_path + "/" + asset_name)
    if existing is not None:
        if not isinstance(existing, asset_class):
            raise RuntimeError("Asset exists with unexpected class: " + existing.get_path_name())
        return existing
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, asset_class, factory
    )
    if created is None:
        raise RuntimeError("Failed to create " + package_path + "/" + asset_name)
    return created


def ingredient(item, quantity):
    return unreal.CombatItemRecipeIngredient(item=item, quantity=quantity)


def category(category_id, display_name, page, sort_order, items):
    return unreal.CombatShopCategory(
        category_id=category_id,
        display_name=display_name,
        page=page,
        sort_order=sort_order,
        items=items,
    )


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Failed to save " + asset.get_path_name())


def migrate_schema():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    migrated = []
    for data in registry.get_assets_by_path("/Game/Combat", recursive=True):
        asset = data.get_asset()
        if isinstance(asset, unreal.CombatDefinitionData):
            if asset.get_editor_property("schema_version") != 2:
                if not asset.upgrade_schema_to_current():
                    raise RuntimeError("Schema migration rejected: " + asset.get_path_name())
                save(asset)
                migrated.append(asset.get_path_name())
    unreal.log("ECON_WRITE migrated_schema_count={}".format(len(migrated)))


def configure_demo(items):
    leaf_settings = {
        "potion": (100, ["恢复", "药水", "消耗品"]),
        "ring": (150, ["护甲", "属性", "圆环"]),
        "boots": (500, ["移动", "鞋", "装备"]),
        "idol": (350, ["光环", "辅助", "雕像"]),
    }
    for key, (price, keywords) in leaf_settings.items():
        item = items[key]
        item.set_editor_property("purchase_price", price)
        item.set_editor_property("purchasable", True)
        item.set_editor_property("sellable", True)
        item.set_editor_property("recipe_scroll", False)
        item.set_editor_property("recipe", [])
        item.set_editor_property("search_keywords", keywords)

    wand = items["wand"]
    wand.set_editor_property("purchase_price", 0)
    wand.set_editor_property("purchasable", False)
    wand.set_editor_property("sellable", True)
    wand.set_editor_property("recipe", [ingredient(items["ring"], 2)])
    wand.set_editor_property("craft_priority", 10)
    wand.set_editor_property("search_keywords", ["法器", "闪电", "升级"])

    blade = items["blade"]
    blade.set_editor_property("purchase_price", 0)
    blade.set_editor_property("purchasable", False)
    blade.set_editor_property("sellable", True)
    blade.set_editor_property(
        "recipe", [ingredient(wand, 1), ingredient(items["boots"], 1)]
    )
    blade.set_editor_property("craft_priority", 20)
    blade.set_editor_property("search_keywords", ["兵刃", "闪电", "升级"])
    for item in items.values():
        save(item)

    economy = create_data_asset("DA_DemoEconomy", ECONOMY_DIR, unreal.CombatEconomyData)
    economy.set_editor_property("definition_name", "demo_economy")
    economy.set_editor_property("display_name_text", "Demo 经济规则")
    economy.set_editor_property("gold_cap", 99999)
    economy.set_editor_property("starting_gold", 600)
    economy.set_editor_property("passive_gold_per_minute", 100)
    economy.set_editor_property("full_refund_seconds", 10.0)
    economy.set_editor_property("sell_value_basis_points", 5000)
    save(economy)

    shop = create_data_asset("DA_DemoShop", ECONOMY_DIR, unreal.CombatShopData)
    shop.set_editor_property("definition_name", "demo_shop")
    shop.set_editor_property("display_name_text", "Demo 商店")
    shop.set_editor_property(
        "categories",
        [
            category("consumables", "消耗品", unreal.CombatShopPage.BASIC, 0, [items["potion"]]),
            category("attributes", "属性", unreal.CombatShopPage.BASIC, 10, [items["ring"], items["idol"]]),
            category("equipment", "装备", unreal.CombatShopPage.BASIC, 20, [items["boots"]]),
            category("artifacts", "法器", unreal.CombatShopPage.UPGRADE, 0, [wand]),
            category("weapons", "兵刃", unreal.CombatShopPage.UPGRADE, 10, [blade]),
        ],
    )
    save(shop)

    dummy = load_required(UNIT_REWARD_PATH)
    dummy.set_editor_property("gold_reward", 150)
    save(dummy)

    game_mode = load_required(GAME_MODE_PATH)
    generated_class = game_mode.generated_class()
    defaults = unreal.get_default_object(generated_class)
    defaults.set_editor_property("economy_data", economy)
    defaults.set_editor_property("shop_data", shop)
    defaults.set_editor_property("enable_economy_debug_commands", True)
    save(game_mode)
    migrate_schema()
    return economy, shop


def verify(items, economy=None, shop=None):
    if economy is None:
        economy = unreal.EditorAssetLibrary.load_asset(ECONOMY_PATH)
    if shop is None:
        shop = unreal.EditorAssetLibrary.load_asset(SHOP_PATH)
    if economy is None or shop is None:
        unreal.log("ECON_VERIFY economy_or_shop_missing=true")
        return
    categories = shop.get_editor_property("categories")
    game_mode = load_required(GAME_MODE_PATH)
    defaults = unreal.get_default_object(game_mode.generated_class())
    unreal.log(
        "ECON_VERIFY economy={} schema={} cap={} start={} passive={} shop={} shop_schema={} categories={} game_mode_economy={} game_mode_shop={} debug={}".format(
            economy.get_editor_property("definition_name"),
            economy.get_editor_property("schema_version"),
            economy.get_editor_property("gold_cap"),
            economy.get_editor_property("starting_gold"),
            economy.get_editor_property("passive_gold_per_minute"),
            shop.get_editor_property("definition_name"),
            shop.get_editor_property("schema_version"),
            len(categories),
            defaults.get_editor_property("economy_data").get_path_name(),
            defaults.get_editor_property("shop_data").get_path_name(),
            defaults.get_editor_property("enable_economy_debug_commands"),
        )
    )
    for label, item in items.items():
        unreal.log(
            "ECON_VERIFY item={} definition={} price={} purchasable={} sellable={} recipe_count={} priority={} schema={}".format(
                label,
                item.get_editor_property("definition_name"),
                item.get_editor_property("purchase_price"),
                item.get_editor_property("purchasable"),
                item.get_editor_property("sellable"),
                len(item.get_editor_property("recipe")),
                item.get_editor_property("craft_priority"),
                item.get_editor_property("schema_version"),
            )
        )


def main():
    dry_run = "-EconomyDryRun" in unreal.SystemLibrary.get_command_line()
    items = {label: load_required(path) for label, path in ITEM_PATHS.items()}
    for label, item in items.items():
        if not isinstance(item, unreal.CombatItemData):
            raise RuntimeError("Expected CombatItemData: " + item.get_path_name())
        describe_item(label, item)
    if dry_run:
        verify(items)
        unreal.log("ECON_READ complete=true")
        return
    unreal.EditorAssetLibrary.make_directory(ECONOMY_DIR)
    economy, shop = configure_demo(items)
    verify(items, economy, shop)
    unreal.log("ECON_WRITE complete=true")


main()
