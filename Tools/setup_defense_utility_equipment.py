"""DEF-015：原生保存两件条件装备与守卫冷却策略，重复执行/只读冷读均校验正式值。"""
import json
import re
from pathlib import Path
import unreal
ROOT="/Game/Combat/Demo/Defense"
FOLDER=ROOT+"/Equipment"
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
ROWS=[("item_mana_sigil","回能符",240,"能","Q/E冷却缩减增加8个百分点，与全局缩减相加后最多40%；W/R、物品和已开始冷却不变。","mana",.08),
      ("item_repair_lens","修复透镜",200,"修","本人发起的核心修复量+20%；队友、取消和超时自动修复不受影响。","repair",.20)]
GUARD=[("guardian_shield_bash",True),("guardian_fortify",False),("guardian_ground_slam",True)]

def load(path):
    """精确路径冷加载，缺失立即失败。"""
    obj=unreal.load_asset(path)
    if obj is None: raise RuntimeError("Missing asset: "+path)
    return obj

def save(obj):
    """只保存明确的本任务资产，不无差别保存蓝图或关卡。"""
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError("Save failed: "+obj.get_path_name())

def create(name,cls):
    """已有资产不覆盖；错误类型立即失败。"""
    if unreal.EditorAssetLibrary.does_asset_exist(FOLDER+"/"+name):
        obj=load(FOLDER+"/"+name)
        if not isinstance(obj,cls):raise RuntimeError("Wrong class: "+obj.get_path_name())
        return obj,False
    factory=unreal.DataAssetFactory();factory.set_editor_property("data_asset_class",cls)
    obj=TOOLS.create_asset(name,FOLDER,cls,factory)
    if obj is None:raise RuntimeError("Cannot create "+name)
    return obj,True

def migrate():
    """新资产配置真实参数；原目录追加两项，已有守卫策略只在空值时迁移。"""
    items=[]
    for key,title,price,glyph,desc,kind,amount in ROWS:
        mod,fresh=create("DA_equipment_"+kind,unreal.CombatModifierData)
        if fresh:
            for prop,value in {"definition_name":"equipment_"+kind,"display_name_text":title+" · 装备效果","duration":0.,"max_stacks":1,
                "remove_on_death":True,"disabled_by_break":False,"dispel_rule":unreal.CombatModifierDispelRule.NOT_DISPELLABLE,
                "runtime_class":unreal.CombatModifierRuntime.static_class() if kind=="mana" else unreal.DefenseCoreRepairRuntime.static_class(),
                "runtime_parameters":{"defense_qe_cooldown_reduction" if kind=="mana" else "core_repair_fraction":amount}}.items():mod.set_editor_property(prop,value)
            save(mod)
        item,fresh=create("DA_"+key,unreal.CombatItemData)
        if fresh:
            for prop,value in {"definition_name":key,"display_name_text":title,"purchase_price":price,"glyph":glyph,"description":desc+" 每种限一件，每窗共限购两件防守装备。",
                "purchasable":True,"sellable":True,"max_owned_quantity":1,"can_drop":False,"sharing":unreal.CombatItemSharing.BOUND_UNIT}.items():item.set_editor_property(prop,value)
            passive=unreal.CombatItemPassive();assert passive.import_text('(Modifier="'+mod.get_path_name()+'",UniqueGroup="'+key+'")')
            item.set_editor_property("passives",[passive]);item.set_editor_property("shop_policy",unreal.DefenseEquipmentShopPolicy(outer=item))
            item.set_editor_property("tint",unreal.LinearColor(.35,.85,.7,1));save(item)
        items.append(item)
    shop=load(ROOT+"/DA_DefenseUnifiedShop");categories=list(shop.get_editor_property("categories"))
    gear=next(c for c in categories if str(c.get_editor_property("category_id"))=="defense_equipment")
    refs=list(gear.get_editor_property("items"));changed=False
    for item in items:
        if item not in refs:refs.append(item);changed=True
    if changed:
        # EditDefaultsOnly结构体字段用原生ImportText重建，避免实例编辑保护；保留其余原始字段。
        text,count=re.subn(r"Items=\([^)]*\)","Items=("+",".join('"'+ref.get_path_name()+'"' for ref in refs)+")",gear.export_text())
        if count!=1:raise RuntimeError("Category item template missing")
        updated=unreal.CombatShopCategory()
        if not updated.import_text(text):raise RuntimeError("Cannot rebuild category")
        categories=[updated if str(c.get_editor_property("category_id"))=="defense_equipment" else c for c in categories]
        shop.set_editor_property("categories",categories);save(shop)
    for key,qe in GUARD:
        data=load(ROOT+"/Guardian/DA_"+key)
        if data.get_editor_property("cooldown_policy") is None:
            policy=unreal.CombatCooldownPolicy(outer=data);policy.set_editor_property("max_reduction",.4)
            if qe:policy.set_editor_property("bonus_parameter_key","defense_qe_cooldown_reduction")
            data.set_editor_property("cooldown_policy",policy);save(data)

def verify():
    """回读正式六件目录、原团队及条件策略，拒绝数值或类型漂移。"""
    shop=load(ROOT+"/DA_DefenseUnifiedShop");cats=list(shop.get_editor_property("categories"))
    gear=next(c for c in cats if str(c.get_editor_property("category_id"))=="defense_equipment")
    ids={str(i.get_editor_property("definition_name")) for i in gear.get_editor_property("items")}
    original={"item_iron_bow","item_swift_string","item_stone_guard","item_hunter_scope","item_mana_sigil","item_repair_lens"}
    assert ids in (original,original|{"item_merchants_seal"})
    assert len(next(c for c in cats if str(c.get_editor_property("category_id"))=="team_supplies").get_editor_property("items"))==6
    report={"items":[],"policies":[]}
    for key,title,price,glyph,desc,kind,amount in ROWS:
        item=load(FOLDER+"/DA_"+key);mod=load(FOLDER+"/DA_equipment_"+kind)
        assert str(item.get_editor_property("definition_name"))==key and item.get_editor_property("purchase_price")==price
        assert item.get_editor_property("max_owned_quantity")==1 and not item.get_editor_property("use_on_acquire") and not item.get_editor_property("can_drop")
        assert item.get_editor_property("shop_policy").get_class()==unreal.DefenseEquipmentShopPolicy.static_class()
        assert item.get_editor_property("passives")[0].get_editor_property("modifier")==mod
        assert mod.get_editor_property("runtime_class")== (unreal.CombatModifierRuntime.static_class() if kind=="mana" else unreal.DefenseCoreRepairRuntime.static_class())
        params={str(k):v for k,v in mod.get_editor_property("runtime_parameters").items()}
        assert abs(params["defense_qe_cooldown_reduction" if kind=="mana" else "core_repair_fraction"]-amount)<.00001
        report["items"].append({"id":key,"price":price,"parameters":params})
    for key,qe in GUARD:
        policy=load(ROOT+"/Guardian/DA_"+key).get_editor_property("cooldown_policy")
        assert policy and abs(policy.get_editor_property("max_reduction")-.4)<.00001
        param=str(policy.get_editor_property("bonus_parameter_key"));assert param==("defense_qe_cooldown_reduction" if qe else "None")
        report["policies"].append({"ability":key,"max_reduction":.4,"parameter":param})
    out=Path(unreal.Paths.project_saved_dir())/"DEF-015";out.mkdir(parents=True,exist_ok=True)
    (out/"utility-assets.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    unreal.log("DefenseUtilityEquipmentVerified "+json.dumps(report,ensure_ascii=False))

if "-DefenseUtilityReadOnly" not in unreal.SystemLibrary.get_command_line():migrate()
verify()
