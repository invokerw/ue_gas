"""DEF-017：UE原生保存基础回城、共享技能组和第八装备；幂等回读不覆盖人工调优。"""
import json
import re
from pathlib import Path
import unreal

ROOT = "/Game/Combat/Demo/Defense"
RECALL = ROOT + "/Recall"
GEAR = ROOT + "/Equipment"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError("Missing asset: " + path)
    return obj


def save(obj):
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        assert "ERROR" not in str(obj.get_editor_property("status"))
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, False)


def props(obj, values):
    for key, value in values.items():
        obj.set_editor_property(key, value)


def create(folder, name, cls, configure, parent=None):
    if unreal.EditorAssetLibrary.does_asset_exist(folder + "/" + name):
        return load(folder + "/" + name)
    unreal.EditorAssetLibrary.make_directory(folder)
    factory = unreal.BlueprintFactory() if parent else unreal.DataAssetFactory()
    factory.set_editor_property("parent_class" if parent else "data_asset_class", parent or cls)
    obj = TOOLS.create_asset(name, folder, unreal.Blueprint if parent else cls, factory)
    assert obj
    configure(obj)
    save(obj)
    return obj


def migrate():
    def configure_recall(obj):
        tags = unreal.GameplayTagContainer()
        assert tags.import_text('(GameplayTags=((TagName="Ability.Behavior.NoTarget"),(TagName="Ability.Behavior.Channelled")))')
        values = {}
        for key, value in {"mana_cost": 0, "cooldown": 30, "movement_tolerance": 1, "minimum_channel": 1}.items():
            entry = unreal.CombatSpecialValue()
            assert entry.import_text("(Values=(" + str(value) + "))")
            values[key] = entry
        props(obj, {"definition_name": "defense_recall", "display_name_text": "回城", "behavior_tags": tags,
                    "cast_point": 0.0, "channel_duration": 3.0, "channel_interval": .05,
                    "special_values": values, "channel_interrupt_order_policy": unreal.CombatChannelInterruptOrderPolicy.CLEAR_QUEUED_ORDERS})
        rules = obj.get_editor_property("targeting_rules")
        team = unreal.GameplayTag()
        assert team.import_text('(TagName="TargetTeam.None")')
        props(rules, {"target_team_tag": team, "cast_range": 0.0})
        obj.set_editor_property("targeting_rules", rules)
        policy = unreal.CombatCooldownPolicy(outer=obj)
        policy.set_editor_property("max_reduction", 0.0)
        obj.set_editor_property("cooldown_policy", policy)
    data = create(RECALL, "DA_defense_recall", unreal.CombatAbilityData, configure_recall)
    bp = create(RECALL, "GA_defense_recall", unreal.Blueprint,
                lambda obj: unreal.get_default_object(obj.generated_class()).set_editor_property("ability_data", data),
                parent=unreal.DefenseRecallAbility.static_class())
    def configure_set(obj):
        entry = unreal.CombatAbilitySetEntry()
        assert entry.import_text('(AbilityClass="' + bp.generated_class().get_path_name() + '",InitialLevel=1)')
        props(obj, {"definition_name": "defense_recall_ability_set", "display_name_text": "防守共享回城", "abilities": [entry]})
    ability_set = create(RECALL, "DA_DefenseRecallAbilitySet", unreal.CombatAbilitySet, configure_set)
    for path in (ROOT + "/DA_DefenseHero", ROOT + "/Guardian/DA_DefenseGuardian"):
        unit = load(path)
        sets = list(unit.get_editor_property("ability_sets"))
        if ability_set not in sets:
            unit.set_editor_property("ability_sets", sets + [ability_set])
            save(unit)
    mod = create(GEAR, "DA_equipment_return_beacon", unreal.CombatModifierData,
                 lambda obj: props(obj, {"definition_name": "equipment_return_beacon", "display_name_text": "回城信标 · 装备效果",
                    "duration": 0.0, "max_stacks": 1, "remove_on_death": True, "disabled_by_break": False,
                    "dispel_rule": unreal.CombatModifierDispelRule.NOT_DISPELLABLE,
                    "runtime_class": unreal.CombatModifierRuntime.static_class(),
                    "runtime_parameters": {"defense_recall_reduction_seconds": 1.0, "defense_respawn_reduction_seconds": 2.0}}))
    def configure_item(obj):
        passive = unreal.CombatItemPassive()
        assert passive.import_text('(Modifier="' + mod.get_path_name() + '",UniqueGroup="item_return_beacon")')
        props(obj, {"definition_name": "item_return_beacon", "display_name_text": "回城信标", "glyph": "归", "purchase_price": 150,
                    "description": "回城引导缩短1秒，死亡前装备有效时复活等待缩短2秒；与援军相加，仍须安全复活。每种限一件，每窗共限购两件。",
                    "purchasable": True, "sellable": True, "max_owned_quantity": 1, "can_drop": False,
                    "sharing": unreal.CombatItemSharing.BOUND_UNIT, "passives": [passive],
                    "shop_policy": unreal.DefenseEquipmentShopPolicy(outer=obj), "tint": unreal.LinearColor(.2, .8, .8, 1)})
    item = create(GEAR, "DA_item_return_beacon", unreal.CombatItemData, configure_item)
    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    cats = list(shop.get_editor_property("categories"))
    cat = next(c for c in cats if str(c.get_editor_property("category_id")) == "defense_equipment")
    refs = list(cat.get_editor_property("items"))
    if item not in refs:
        text, count = re.subn(r"Items=\([^)]*\)", "Items=(" + ",".join('"' + ref.get_path_name() + '"' for ref in refs + [item]) + ")", cat.export_text())
        assert count == 1
        updated = unreal.CombatShopCategory()
        assert updated.import_text(text)
        shop.set_editor_property("categories", [updated if str(c.get_editor_property("category_id")) == "defense_equipment" else c for c in cats])
        save(shop)


def verify():
    shop = load(ROOT + "/DA_DefenseUnifiedShop")
    cats = list(shop.get_editor_property("categories"))
    ids = {str(i.get_editor_property("definition_name")) for i in next(c for c in cats if str(c.get_editor_property("category_id")) == "defense_equipment").get_editor_property("items")}
    assert ids == {"item_iron_bow", "item_swift_string", "item_stone_guard", "item_hunter_scope", "item_mana_sigil", "item_repair_lens", "item_merchants_seal", "item_return_beacon"}
    assert len(next(c for c in cats if str(c.get_editor_property("category_id")) == "team_supplies").get_editor_property("items")) == 6
    item = load(GEAR + "/DA_item_return_beacon")
    mod = load(GEAR + "/DA_equipment_return_beacon")
    assert str(item.get_editor_property("definition_name")) == "item_return_beacon"
    assert item.get_editor_property("purchase_price") == 150 and item.get_editor_property("max_owned_quantity") == 1
    assert item.get_editor_property("purchasable") and item.get_editor_property("sellable")
    assert not item.get_editor_property("can_drop") and not item.get_editor_property("use_on_acquire") and not item.get_editor_property("recipe")
    assert item.get_editor_property("sharing") == unreal.CombatItemSharing.BOUND_UNIT
    assert item.get_editor_property("shop_policy").get_class() == unreal.DefenseEquipmentShopPolicy.static_class()
    assert len(item.get_editor_property("passives")) == 1 and item.get_editor_property("passives")[0].get_editor_property("modifier") == mod
    params = {str(k): v for k, v in mod.get_editor_property("runtime_parameters").items()}
    assert params == {"defense_recall_reduction_seconds": 1.0, "defense_respawn_reduction_seconds": 2.0}
    assert mod.get_editor_property("duration") == 0 and mod.get_editor_property("remove_on_death") and not mod.get_editor_property("disabled_by_break")
    assert mod.get_editor_property("runtime_class") == unreal.CombatModifierRuntime.static_class()
    data = load(RECALL + "/DA_defense_recall")
    bp = load(RECALL + "/GA_defense_recall")
    ability_set = load(RECALL + "/DA_DefenseRecallAbilitySet")
    assert unreal.get_default_object(bp.generated_class()).get_editor_property("ability_data") == data
    assert data.get_editor_property("channel_duration") == 3 and data.get_editor_property("channel_interval") > 0
    assert data.get_editor_property("cooldown_policy").get_editor_property("max_reduction") == 0
    for path in (ROOT + "/DA_DefenseHero", ROOT + "/Guardian/DA_DefenseGuardian"):
        assert list(load(path).get_editor_property("ability_sets")).count(ability_set) == 1
    out = Path(unreal.Paths.project_saved_dir()) / "DEF-017"
    out.mkdir(exist_ok=True)
    report = {"catalog_count": len(ids), "team_count": 6, "price": 150, "parameters": params, "channel": 3, "cooldown": 30, "hero_sets": 2}
    (out / "return-beacon-assets.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DefenseReturnBeaconVerified " + json.dumps(report))


if "-DefenseReturnBeaconReadOnly" not in unreal.SystemLibrary.get_command_line():
    migrate()
verify()
