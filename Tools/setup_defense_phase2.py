"""DEF-009：创建Phase2剩余三件正式团队道具并替换三件原型目录项。"""
import json
from pathlib import Path
import unreal

ROOT='/Game/Combat/Demo/Defense'; FOLDER=ROOT+'/TeamShop'; TOOLS=unreal.AssetToolsHelpers.get_asset_tools()

def load(path):
    obj=unreal.load_asset(path)
    if obj is None: raise RuntimeError('Missing: '+path)
    return obj

def make(name, cls):
    path=FOLDER+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return load(path),False
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
    return TOOLS.create_asset(name,FOLDER,cls,factory),True

def save(obj):
    if isinstance(obj,unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')): raise RuntimeError('Blueprint compile failed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError('Save failed: '+obj.get_path_name())

def attr_modifier(name, display, attr, amount):
    source=load('/Game/Combat/Demo/Items/DA_item_armor_passive').get_editor_property('attribute_changes')[0].export_text()
    m,fresh=make(name,unreal.CombatModifierData)
    if fresh:
        m.set_editor_property('definition_name',name.lower())
        m.set_editor_property('display_name_text',display)
        m.set_editor_property('remove_on_death',False);m.set_editor_property('dispel_rule',unreal.CombatModifierDispelRule.NOT_DISPELLABLE)
        change=unreal.CombatModifierAttributeChange()
        if not change.import_text(source.replace('Armor',attr).replace('Magnitude=5','Magnitude='+str(amount))): raise RuntimeError('attr import failed')
        m.set_editor_property('attribute_changes',[change]); m.set_editor_property('remove_on_death',False)
        save(m)
    return m

def conditional_modifier(name, display, role, fraction):
    m,fresh=make(name,unreal.CombatModifierData)
    if fresh:
        m.set_editor_property('definition_name',name.lower());m.set_editor_property('display_name_text',display)
        m.set_editor_property('runtime_class',unreal.DefenseConditionalDamageRuntime.static_class())
        m.set_editor_property('runtime_parameters',{'target_role':float(role),'damage_fraction':float(fraction)})
        m.set_editor_property('remove_on_death',False);m.set_editor_property('dispel_rule',unreal.CombatModifierDispelRule.NOT_DISPELLABLE);save(m)
    return m

def main():
    hunt=conditional_modifier('DA_team_hunt_horn_effect','追猎号角效果',1,.15)
    agile=attr_modifier('DA_team_agile_circuit_effect','灵巧回路效果','CooldownReductionPct',.05)
    breach=conditional_modifier('DA_team_breach_inscription_effect','破城铭文效果',2,.12)
    rows=[('team_hunt_horn','追猎号角','下一波全队对攻城兵和刺客伤害 +15%。未知目标不会误加成。',260,'猎',hunt,unreal.DefenseTeamTargetRole.SIEGE_OR_ASSASSIN, .15),
          ('team_agile_circuit','灵巧回路','下一波全队主动技能冷却缩短5%，受40%总上限约束。',240,'敏',agile,unreal.DefenseTeamTargetRole.NONE,0),
          ('team_breach_inscription','破城铭文','下一场Boss战全队对Boss伤害 +12%；Boss目标按稳定DefinitionId识别。',280,'破',breach,unreal.DefenseTeamTargetRole.BOSS,.12)]
    items=[]
    for key,title,desc,price,glyph,mod,role,fraction in rows:
        item,fresh=make('DA_Item_'+key,unreal.CombatItemData)
        if fresh:
            for prop,val in [('definition_name',key),('display_name_text',title),('description',desc),('purchase_price',price),('purchasable',True),('use_on_acquire',True),('sellable',False),('can_drop',False),('glyph',glyph)]: item.set_editor_property(prop,val)
            e=unreal.DefenseTeamItemEffect(outer=item); e.set_editor_property('modifier_data',mod); e.set_editor_property('duration_waves',1); e.set_editor_property('conditional_target_role',role); e.set_editor_property('conditional_damage_fraction',fraction); e.set_editor_property('allowed_phases',[unreal.DefensePhase.PREPARATION,unreal.DefensePhase.INTERMISSION,unreal.DefensePhase.BOSS_PREPARATION]); item.set_editor_property('acquire_effect',e); save(item)
        items.append(item)
    shop=load(ROOT+'/DA_DefenseUnifiedShop'); cats=list(shop.get_editor_property('categories'))
    replacements={'team_rally':items[0],'team_supply':items[1],'team_last_stand':items[2]}
    for idx,c in enumerate(cats):
        if str(c.get_editor_property('category_id'))!='team_supplies': continue
        text=c.export_text()
        for old,new in replacements.items(): text=text.replace(FOLDER+'/DA_Item_'+old+'.DA_Item_'+old,new.get_path_name())
        text=text[:text.index(',DisplayName=')]+',DisplayName=INVTEXT("团队补给 · 获得即用")'+text[text.index(',Page='):]
        repl=unreal.CombatShopCategory();
        if not repl.import_text(text): raise RuntimeError('category import failed')
        cats[idx]=repl
    shop.set_editor_property('categories',cats);save(shop)
    for name in ['BP_DefenseGameMode','BP_DefenseHUD','BP_DefensePlayerController']: save(load(ROOT+'/'+name))
    team=[c for c in cats if str(c.get_editor_property('category_id'))=='team_supplies'][0]
    report={'items':[{'id':str(i.get_editor_property('definition_name')),'price':i.get_editor_property('purchase_price'),'effect':i.get_editor_property('acquire_effect').get_path_name()} for i in items], 'category':team.export_text(), 'blueprints':{n:str(load(ROOT+'/'+n).get_editor_property('status')) for n in ['BP_DefenseGameMode','BP_DefenseHUD','BP_DefensePlayerController']}}
    out=Path(unreal.Paths.project_saved_dir())/'DEF-009';out.mkdir(parents=True,exist_ok=True);(out/'asset-migration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('DefensePhase2Items '+json.dumps(report,ensure_ascii=False))
main()
