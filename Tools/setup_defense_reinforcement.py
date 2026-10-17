"""DEF-008：使用UE原生API创建援军印记并替换原商店的一项；支持独立只读验收。"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Combat/Demo/Defense'
FOLDER = ROOT + '/TeamShop'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """必需资产缺失时中止。"""
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError('Missing: ' + path)
    return obj


def create(name, cls):
    """已有资产保持手工配置，新资产使用原生工厂。"""
    if unreal.EditorAssetLibrary.does_asset_exist(FOLDER + '/' + name):
        return load(FOLDER + '/' + name), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return TOOLS.create_asset(name, FOLDER, cls, factory), True


def save(obj):
    """编译蓝图并保存指定资产。"""
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')):
            raise RuntimeError('Compile failed: ' + obj.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError('Save failed: ' + obj.get_path_name())


def main():
    readonly = '-DefenseReinforcementReadOnly' in unreal.SystemLibrary.get_command_line()
    shop = load(ROOT + '/DA_DefenseUnifiedShop')
    if not readonly:
        shield, fresh = create('DA_team_reinforcement_shield', unreal.CombatModifierData)
        if fresh:
            for name, value in [('definition_name', 'team_reinforcement_shield'), ('display_name_text', '援军护盾'),
                                ('runtime_class', unreal.DefenseShieldRuntime.static_class()), ('duration', 4.0),
                                ('dispel_rule', unreal.CombatModifierDispelRule.NOT_DISPELLABLE), ('remove_on_death', True)]:
                shield.set_editor_property(name, value)
            save(shield)
        item, fresh = create('DA_Item_team_reinforcement_mark', unreal.CombatItemData)
        if fresh:
            for name, value in [('definition_name', 'team_reinforcement_mark'), ('display_name_text', '援军印记'),
                                ('description', '接下来两次英雄死亡各缩短3秒复活等待，复活后获得最大生命20%的护盾，持续4秒。购买前已阵亡者不追溯。'),
                                ('purchase_price', 200), ('purchasable', True), ('use_on_acquire', True), ('sellable', False), ('can_drop', False), ('glyph', '复')]:
                item.set_editor_property(name, value)
            effect = unreal.DefenseTeamItemEffect(outer=item)
            for name, value in [('reinforcement_shield_data', shield), ('reinforcement_deaths', 2), ('respawn_reduction', 3.0), ('respawn_shield_fraction', .2),
                                ('allowed_phases', [unreal.DefensePhase.PREPARATION, unreal.DefensePhase.INTERMISSION, unreal.DefensePhase.BOSS_PREPARATION])]:
                effect.set_editor_property(name, value)
            item.set_editor_property('acquire_effect', effect)
            save(item)
        categories = list(shop.get_editor_property('categories'))
        for index, category in enumerate(categories):
            if str(category.get_editor_property('category_id')) != 'team_supplies':
                continue
            text = category.export_text().replace(FOLDER + '/DA_Item_team_fortify.DA_Item_team_fortify', item.get_path_name())
            replacement = unreal.CombatShopCategory()
            if not replacement.import_text(text):
                raise RuntimeError('Native category update failed')
            categories[index] = replacement
        shop.set_editor_property('categories', categories)
        save(shop)
        for name in ['BP_DefenseGameMode', 'BP_DefenseHUD', 'BP_DefensePlayerController']:
            save(load(ROOT + '/' + name))
    item = load(FOLDER + '/DA_Item_team_reinforcement_mark')
    effect = item.get_editor_property('acquire_effect')
    shield = effect.get_editor_property('reinforcement_shield_data')
    team = [c for c in shop.get_editor_property('categories') if str(c.get_editor_property('category_id')) == 'team_supplies']
    if len(team) != 1 or len(team[0].get_editor_property('items')) != 6 or item.get_path_name() not in team[0].export_text() or 'DA_Item_team_fortify.' in team[0].export_text():
        raise RuntimeError('Catalog mismatch')
    report = {'title': str(item.get_editor_property('display_name_text')), 'category_name': str(team[0].get_editor_property('display_name')), 'definition': str(item.get_editor_property('definition_name')), 'price': item.get_editor_property('purchase_price'),
              'use_on_acquire': item.get_editor_property('use_on_acquire'), 'charges': effect.get_editor_property('reinforcement_deaths'),
              'reduction': effect.get_editor_property('respawn_reduction'), 'shield_fraction': effect.get_editor_property('respawn_shield_fraction'),
              'duration': shield.get_editor_property('duration'), 'runtime': shield.get_editor_property('runtime_class').get_path_name(),
              'core_runtime': load(FOLDER + '/DA_team_shrine_barrier_effect').get_editor_property('runtime_class').get_path_name(),
              'category': team[0].export_text(), 'blueprints': {n: str(load(ROOT + '/' + n).get_editor_property('status')) for n in ['BP_DefenseGameMode', 'BP_DefenseHUD', 'BP_DefensePlayerController']}}
    if report['title'] != '援军印记' or report['category_name'] != '团队补给 · 获得即用':
        raise RuntimeError('Chinese display text mismatch')
    if report['definition'] != 'team_reinforcement_mark' or report['price'] != 200 or report['charges'] != 2 or report['reduction'] != 3 or report['duration'] != 4 or abs(report['shield_fraction'] - .2) > .0001:
        raise RuntimeError('Formal reinforcement values mismatch')
    output = Path(unreal.Paths.project_saved_dir()) / 'DEF-008'
    output.mkdir(parents=True, exist_ok=True)
    (output / ('cold-readback.json' if readonly else 'asset-migration.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('DefenseReinforcementVerified ' + json.dumps(report, ensure_ascii=False))


main()
