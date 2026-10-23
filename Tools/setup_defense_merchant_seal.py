"""DEF-016：原生保存商贾印与统一商店折扣参数，重复执行/只读冷读校验正式值。"""
import json
import re
from pathlib import Path
import unreal

ROOT = "/Game/Combat/Demo/Defense"
FOLDER = ROOT + "/Equipment"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ITEM_ID = "item_merchants_seal"
PRICE = 240
DISCOUNT = 0.05


def load(path):
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError("Missing asset: " + path)
    return obj


def save(obj):
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError("Save failed " + obj.get_path_name())


def create(name, cls):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        obj = load(path)
        if not isinstance(obj, cls):
            raise RuntimeError("Wrong class: " + obj.get_path_name())
        return obj, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    obj = TOOLS.create_asset(name, FOLDER, cls, factory)
    if obj is None:
        raise RuntimeError("Cannot create " + name)
    return obj, True


def migrate():
    mod, fresh = create("DA_equipment_merchants_seal", unreal.CombatModifierData)
    if fresh:
        for prop, value in {
            "definition_name": "equipment_merchants_seal",
            "display_name_text": "商贾印 · 装备效果",
            "duration": 0.0,
            "max_stacks": 1,
            "remove_on_death": True,
            "disabled_by_break": False,
            "dispel_rule": unreal.CombatModifierDispelRule.NOT_DISPELLABLE,
            "runtime_class": unreal.CombatModifierRuntime.static_class(),
            "runtime_parameters": {"shop_discount_fraction": DISCOUNT},
        }.items():
            mod.set_editor_property(prop, value)
        save(mod)

    item, fresh = create("DA_" + ITEM_ID, unreal.CombatItemData)
    if fresh:
        for prop, value in {
            "definition_name": ITEM_ID,
            "display_name_text": "商贾印",
            "purchase_price": PRICE,
            "glyph": "商",
            "description": "商店与重抽价格降低5%，与其他经济折扣相加但总折扣最多20%。每种限一件，每窗共限购两件防守装备。",
            "purchasable": True,
            "sellable": True,
            "max_owned_quantity": 1,
            "can_drop": False,
            "sharing": unreal.CombatItemSharing.BOUND_UNIT,
        }.items():
            item.set_editor_property(prop, value)
        passive = unreal.CombatItemPassive()
        if not passive.import_text('(Modifier="' + mod.get_path_name() + '",UniqueGroup="' + ITEM_ID + '")'):
            raise RuntimeError("Passive import failed")
        item.set_editor_property("passives", [passive])
        item.set_editor_property("shop_policy", unreal.DefenseEquipmentShopPolicy(outer=item))
        item.set_editor_property("tint", unreal.LinearColor(.86, .64, .28, 1))
        save(item)

    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    categories = list(shop.get_editor_property("categories"))
    gear = next(c for c in categories if str(c.get_editor_property("category_id")) == "defense_equipment")
    refs = list(gear.get_editor_property("items"))
    if item not in refs:
        refs.append(item)
        text, count = re.subn(r"Items=\([^)]*\)", "Items=(" + ",".join('"' + ref.get_path_name() + '"' for ref in refs) + ")", gear.export_text())
        if count != 1:
            raise RuntimeError("Category item template missing")
        updated = unreal.CombatShopCategory()
        if not updated.import_text(text):
            raise RuntimeError("Cannot rebuild category")
        categories = [updated if str(c.get_editor_property("category_id")) == "defense_equipment" else c for c in categories]
        shop.set_editor_property("categories", categories)
        save(shop)


def verify():
    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    categories = list(shop.get_editor_property("categories"))
    gear = next(c for c in categories if str(c.get_editor_property("category_id")) == "defense_equipment")
    ids = {str(i.get_editor_property("definition_name")) for i in gear.get_editor_property("items")}
    expected = {"item_iron_bow", "item_swift_string", "item_stone_guard", "item_hunter_scope", "item_mana_sigil", "item_repair_lens", ITEM_ID}
    assert ids == expected, (ids, expected)
    assert len(next(c for c in categories if str(c.get_editor_property("category_id")) == "team_supplies").get_editor_property("items")) == 6
    item = load(FOLDER + "/DA_" + ITEM_ID)
    mod = load(FOLDER + "/DA_equipment_merchants_seal")
    assert str(item.get_editor_property("definition_name")) == ITEM_ID
    assert item.get_editor_property("purchase_price") == PRICE
    assert item.get_editor_property("max_owned_quantity") == 1
    assert not item.get_editor_property("use_on_acquire") and not item.get_editor_property("can_drop")
    assert item.get_editor_property("purchasable") and item.get_editor_property("sellable")
    assert item.get_editor_property("sharing") == unreal.CombatItemSharing.BOUND_UNIT
    assert not item.get_editor_property("recipe")
    assert len(item.get_editor_property("passives")) == 1
    assert item.get_editor_property("shop_policy").get_class() == unreal.DefenseEquipmentShopPolicy.static_class()
    assert item.get_editor_property("passives")[0].get_editor_property("modifier") == mod
    assert mod.get_editor_property("runtime_class") == unreal.CombatModifierRuntime.static_class()
    assert str(mod.get_editor_property("definition_name")) == "equipment_merchants_seal"
    assert mod.get_editor_property("duration") == 0.0 and mod.get_editor_property("max_stacks") == 1
    assert mod.get_editor_property("remove_on_death") and not mod.get_editor_property("disabled_by_break")
    assert mod.get_editor_property("dispel_rule") == unreal.CombatModifierDispelRule.NOT_DISPELLABLE
    params = {str(k): v for k, v in mod.get_editor_property("runtime_parameters").items()}
    assert abs(params["shop_discount_fraction"] - DISCOUNT) < 0.00001
    report = {"item": ITEM_ID, "price": PRICE, "discount": DISCOUNT, "catalog_count": len(ids), "team_supply_count": 6}
    out = Path(unreal.Paths.project_saved_dir()) / "DEF-016"
    out.mkdir(parents=True, exist_ok=True)
    (out / "merchant-assets.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DefenseMerchantSealVerified " + json.dumps(report, ensure_ascii=False))


if "-DefenseMerchantSealReadOnly" not in unreal.SystemLibrary.get_command_line():
    migrate()
verify()
