"""DEF-014：用UE原生API创建首批四件防守装备，支持重复执行及只读冷验证。"""
import json
import re
from pathlib import Path
import unreal

ROOT = "/Game/Combat/Demo/Defense"
FOLDER = ROOT + "/Equipment"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROWS = [
    ("item_iron_bow", "铁制弓", 180, "弓", "直接普攻伤害 +12%；不放大技能或物品派生伤害。", "bow"),
    ("item_swift_string", "迅弦", 220, "弦", "普攻间隔缩短8%；不改变技能冷却。", "swift"),
    ("item_stone_guard", "石卫徽记", 160, "卫", "最大生命 +120；提高上限不补血，不生成护盾。", "stone"),
    ("item_hunter_scope", "猎手瞄准镜", 260, "猎", "对精英、攻城兵和Boss伤害 +12%；对普通兵和刺客不生效。", "scope"),
]


def load(path):
    """只读精确资产路径，缺失立即失败。"""
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError("Missing asset: " + path)
    return obj


def make(name, cls):
    """只创建缺失资产；已有内容不覆盖手工调整。"""
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        obj = load(path)
        if not isinstance(obj, cls):
            raise RuntimeError("Wrong existing class: " + path)
        return obj, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    obj = TOOLS.create_asset(name, FOLDER, cls, factory)
    if obj is None:
        raise RuntimeError("Create failed: " + path)
    return obj, True


def save(obj):
    """保存本步明确资产，拒绝保存失败。"""
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if "ERROR" in str(obj.get_editor_property("status")):
            raise RuntimeError("Blueprint compile failed: " + obj.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError("Save failed: " + obj.get_path_name())


def migrate():
    """独立Modifier与绑定装备；只追加原商店一个分类，保留团队与旧目录。"""
    items = []
    source = load("/Game/Combat/Demo/Items/DA_item_armor_passive").get_editor_property("attribute_changes")[0].export_text()
    for key, title, price, glyph, desc, kind in ROWS:
        mod, fresh = make("DA_equipment_" + kind, unreal.CombatModifierData)
        if fresh:
            mod.set_editor_property("definition_name", "equipment_" + kind)
            mod.set_editor_property("display_name_text", title + "效果")
            mod.set_editor_property("duration", 0)
            mod.set_editor_property("dispel_rule", unreal.CombatModifierDispelRule.NOT_DISPELLABLE)
            mod.set_editor_property("remove_on_death", True)
            if kind in ("bow", "scope"):
                mod.set_editor_property("runtime_class", unreal.DefenseEquipmentDamageRuntime.static_class())
                mod.set_editor_property("runtime_parameters", {
                    "damage_fraction": .12,
                    "direct_attack_only": 1.0 if kind == "bow" else 0.0,
                    "special_targets_only": 1.0 if kind == "scope" else 0.0,
                })
            else:
                text = source.replace("Armor", "BaseAttackTime" if kind == "swift" else "MaxHealth")
                text, count = re.subn(r"Magnitude=[^,)]+", "Magnitude=" + ("0.92" if kind == "swift" else "120"), text)
                if count != 1:
                    raise RuntimeError("Attribute magnitude template missing: " + source)
                if kind == "swift":
                    text, op_count = re.subn(r"ModifierOp=[^,)]+", "ModifierOp=MultiplyCompound", text)
                    if op_count != 1:
                        raise RuntimeError("Attribute operation template missing")
                change = unreal.CombatModifierAttributeChange()
                if not change.import_text(text):
                    raise RuntimeError("Cannot import attribute: " + text)
                mod.set_editor_property("attribute_changes", [change])
            save(mod)
        # 修复本任务首次生成时旧别名与UE5.8序列化名称不一致的AddBase；重复执行不漂移。
        if kind == "swift":
            changes = list(mod.get_editor_property("attribute_changes"))
            text = changes[0].export_text()
            if "ModifierOp=AddBase" in text and "Magnitude=0.920000" in text:
                replacement = unreal.CombatModifierAttributeChange()
                if not replacement.import_text(text.replace("ModifierOp=AddBase", "ModifierOp=MultiplyCompound")):
                    raise RuntimeError("Cannot migrate initial swift operation")
                mod.set_editor_property("attribute_changes", [replacement])
                save(mod)
        item, fresh = make("DA_" + key, unreal.CombatItemData)
        if fresh:
            for prop, value in [("definition_name", key), ("display_name_text", title), ("purchase_price", price),
                                ("glyph", glyph), ("description", desc + " 每种限持有1件；每次整备最多购买2件防守装备，出售不返还次数。"),
                                ("purchasable", True), ("sellable", True), ("max_owned_quantity", 1),
                                ("can_drop", False), ("sharing", unreal.CombatItemSharing.BOUND_UNIT)]:
                item.set_editor_property(prop, value)
            passive = unreal.CombatItemPassive()
            if not passive.import_text('(Modifier="' + mod.get_path_name() + '",UniqueGroup="' + key + '")'):
                raise RuntimeError("Cannot configure passive")
            item.set_editor_property("passives", [passive])
            item.set_editor_property("shop_policy", unreal.DefenseEquipmentShopPolicy(outer=item))
            item.set_editor_property("tint", unreal.LinearColor(.55, .72, .95, 1))
            save(item)
        items.append(item)
    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    categories = list(shop.get_editor_property("categories"))
    existing = [c for c in categories if str(c.get_editor_property("category_id")) == "defense_equipment"]
    if not existing:
        category = unreal.CombatShopCategory()
        refs = ",".join('"' + item.get_path_name() + '"' for item in items)
        if not category.import_text('(CategoryId="defense_equipment",DisplayName=INVTEXT("防守装备"),Page=Basic,SortOrder=40,Items=(' + refs + '))'):
            raise RuntimeError("Cannot configure equipment category")
        categories.append(category)
        shop.set_editor_property("categories", categories)
        save(shop)


def verify(readonly):
    """新进程按实际字段/引用回读；默认原型参数和四件分类必须精确成立。"""
    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    categories = list(shop.get_editor_property("categories"))
    gear = [c for c in categories if str(c.get_editor_property("category_id")) == "defense_equipment"]
    team = [c for c in categories if str(c.get_editor_property("category_id")) == "team_supplies"]
    assert len(gear) == 1 and len(gear[0].get_editor_property("items")) == 4
    assert len(team) == 1 and len(team[0].get_editor_property("items")) == 6
    report = []
    for key, title, price, glyph, desc, kind in ROWS:
        item = load(FOLDER + "/DA_" + key)
        assert str(item.get_editor_property("definition_name")) == key
        assert item.get_editor_property("purchase_price") == price
        assert item.get_editor_property("max_owned_quantity") == 1
        assert not item.get_editor_property("use_on_acquire") and not item.get_editor_property("can_drop")
        assert item.get_editor_property("shop_policy").get_class() == unreal.DefenseEquipmentShopPolicy.static_class()
        passives = list(item.get_editor_property("passives"))
        assert len(passives) == 1
        mod = passives[0].get_editor_property("modifier")
        assert mod == load(FOLDER + "/DA_equipment_" + kind)
        if kind == "swift":
            attributes = mod.get_editor_property("attribute_changes")
            assert len(attributes) == 1 and "ModifierOp=MultiplyCompound" in attributes[0].export_text()
        report.append({"id": key, "name": str(item.get_editor_property("display_name_text")), "price": price,
                       "policy": item.get_editor_property("shop_policy").get_class().get_name(),
                       "effect": mod.get_path_name(), "parameters": {str(k):v for k,v in mod.get_editor_property("runtime_parameters").items()},
                       "attributes": [c.export_text() for c in mod.get_editor_property("attribute_changes")]})
    mode = load(ROOT + "/BP_DefenseGameMode")
    assert unreal.get_default_object(mode.generated_class()).get_editor_property("shop_data") == shop
    out = Path(unreal.Paths.project_saved_dir()) / "DEF-014"
    out.mkdir(parents=True, exist_ok=True)
    (out / ("equipment-cold-read.json" if readonly else "equipment-assets.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DefenseEquipmentContentVerified " + json.dumps(report, ensure_ascii=False))


readonly = "-DefenseEquipmentReadOnly" in unreal.SystemLibrary.get_command_line()
if not readonly:
    migrate()
verify(readonly)
