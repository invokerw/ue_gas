"""DEF-006：六件团队消耗品迁入原 Combat 商店，原生保存/回读后删除旧目录资产。"""
import json
from pathlib import Path
import unreal
ROOT='/Game/Combat/Demo/Defense'
FOLDER=ROOT+'/TeamShop'
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """只使用确定的资产路径，缺失立即失败。"""
    obj=unreal.load_asset(path)
    if obj is None: raise RuntimeError('Missing: '+path)
    return obj


def save(obj):
    """仅保存本次明确修改的资产，蓝图先编译。"""
    if isinstance(obj,unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')): raise RuntimeError('Blueprint failed: '+obj.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError('Save failed: '+obj.get_path_name())


def main():
    rows=[('team_guard','团队铁甲','全队护甲 +5，持续到下一波清场。',120,1,0,'甲'),
          ('team_repair','核心修复','立即修复核心 750 生命。',150,0,750,'修'),
          ('team_rally','集结号角','全队攻击力 +15，持续到下一波清场。',180,1,0,'攻'),
          ('team_supply','法力补给','全队每秒法力回复 +2，覆盖接下来两波。',200,2,0,'蓝'),
          ('team_fortify','魔法加固','全队魔法抗性 +12%，覆盖接下来两波。',240,2,0,'抗'),
          ('team_last_stand','决战鼓舞','全队攻击速度 +20，仅 Boss 整备可购。',300,1,0,'速')]
    items=[]
    for key,title,desc,price,waves,heal,glyph in rows:
        path=FOLDER+'/DA_Item_'+key
        if unreal.EditorAssetLibrary.does_asset_exist(path): item=load(path)
        else:
            factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.CombatItemData)
            item=TOOLS.create_asset('DA_Item_'+key,FOLDER,unreal.CombatItemData,factory)
            for prop,value in [('definition_name',key),('display_name_text',title),('description',desc),('purchase_price',price),('purchasable',True),('use_on_acquire',True),('sellable',False),('can_drop',False),('glyph',glyph)]:
                item.set_editor_property(prop,value)
            effect=unreal.DefenseTeamItemEffect(outer=item)
            effect.set_editor_property('duration_waves',waves)
            effect.set_editor_property('objective_heal',heal)
            effect.set_editor_property('allowed_phases',[unreal.DefensePhase.BOSS_PREPARATION] if key=='team_last_stand' else [unreal.DefensePhase.PREPARATION,unreal.DefensePhase.INTERMISSION,unreal.DefensePhase.BOSS_PREPARATION])
            if not heal: effect.set_editor_property('modifier_data',load(FOLDER+'/DA_'+key+'_effect'))
            item.set_editor_property('acquire_effect',effect)
            save(item)
        items.append(item)
    mode=load(ROOT+'/BP_DefenseGameMode'); cdo=unreal.get_default_object(mode.generated_class())
    path=ROOT+'/DA_DefenseUnifiedShop'
    if unreal.EditorAssetLibrary.does_asset_exist(path): shop=load(path)
    else:
        old=cdo.get_editor_property('shop_data')
        if old is None: raise RuntimeError('Defense mode has no existing Combat shop to reuse')
        shop=TOOLS.duplicate_asset('DA_DefenseUnifiedShop',ROOT,old)
        shop.set_editor_property('definition_name','defense_unified_shop')
        shop.set_editor_property('display_name_text','神殿商店')
        # 原生结构 ImportText 避免 EditDefaultsOnly 成员对临时结构实例写入的限制。
        category=unreal.CombatShopCategory()
        refs=','.join('"'+item.get_path_name()+'"' for item in items)
        if not category.import_text('(CategoryId="team_supplies",DisplayName="团队补给 · 获得即用",Page=Basic,SortOrder=50,Items=('+refs+'))'):
            raise RuntimeError('Cannot build team category')
        categories=list(shop.get_editor_property('categories')); categories.append(category); shop.set_editor_property('categories',categories); save(shop)
    cdo.set_editor_property('shop_data',shop); save(mode)
    hud=load(ROOT+'/BP_DefenseHUD'); unreal.get_default_object(hud.generated_class()).set_editor_property('shop_widget_class',unreal.CombatShopWidget.static_class()); save(hud)
    pc=load(ROOT+'/BP_DefensePlayerController'); save(pc)
    run=load(ROOT+'/DA_DefenseRun')
    # Run 已迁移并移除旧目录字段；保留冷回读和保存检查。
    save(run)
    old_path=ROOT+'/DA_DefenseTeamShop'
    if unreal.EditorAssetLibrary.does_asset_exist(old_path):
        refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(old_path,True)
        if refs: raise RuntimeError('Old directory still referenced: '+str(refs))
        if not unreal.EditorAssetLibrary.delete_asset(old_path): raise RuntimeError('Cannot delete retired directory')
    report={'shop':shop.get_path_name(),'shop_id':str(shop.get_editor_property('definition_name')),
            'categories':[c.export_text() for c in shop.get_editor_property('categories')],
            'items':[{'path':i.get_path_name(),'id':str(i.get_editor_property('definition_name')),'price':i.get_editor_property('purchase_price'),'use_on_acquire':i.get_editor_property('use_on_acquire'),'effect':i.get_editor_property('acquire_effect').get_class().get_path_name()} for i in items],
            'mode_shop':cdo.get_editor_property('shop_data').get_path_name(),'hud_shop':unreal.get_default_object(hud.generated_class()).get_editor_property('shop_widget_class').get_path_name(),
            'blueprints':{b.get_path_name():str(b.get_editor_property('status')) for b in [mode,hud,pc]},'retired_directory_exists':unreal.EditorAssetLibrary.does_asset_exist(old_path)}
    output=Path(unreal.Paths.project_saved_dir())/'DEF-006'; output.mkdir(parents=True,exist_ok=True)
    (output/'asset-migration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('DefenseUnifiedShopMigration '+json.dumps(report,ensure_ascii=False))

main()
