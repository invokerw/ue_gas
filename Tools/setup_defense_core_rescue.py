"""DEF-007：原生创建核心救援道具并替换原商店两项；旧资产保留用于回滚。"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Combat/Demo/Defense'
FOLDER = ROOT + '/TeamShop'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """必需资产缺失立即失败。"""
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError('Missing: ' + path)
    return obj


def create(name, cls):
    """保留已存在的正式内容；首次创建使用原生 DataAsset 工厂。"""
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return TOOLS.create_asset(name, FOLDER, cls, factory), True


def save(obj):
    """只保存本任务资产；蓝图编译失败时不宣称成功。"""
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')):
            raise RuntimeError('Blueprint failed: ' + obj.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError('Save failed: ' + obj.get_path_name())


def verify():
    """独立进程可只回读磁盘资产，不重新保存或修补结果。"""
    shop = load(ROOT + '/DA_DefenseUnifiedShop')
    categories = list(shop.get_editor_property('categories'))
    team = [c for c in categories if str(c.get_editor_property('category_id')) == 'team_supplies']
    if len(team) != 1 or len(team[0].get_editor_property('items')) != 6:
        raise RuntimeError('Expected exactly six team supplies')
    serialized = team[0].export_text()
    if 'DA_Item_team_guard.' in serialized or 'DA_Item_team_repair.' in serialized:
        raise RuntimeError('Prototype still offered')
    rows = []
    for key, price in [('team_shrine_barrier', 220), ('team_emergency_stone', 180)]:
        item = load(FOLDER + '/DA_Item_' + key)
        effect = item.get_editor_property('acquire_effect')
        if item.get_path_name() not in serialized or str(item.get_editor_property('definition_name')) != key:
            raise RuntimeError('Item identity/catalog mismatch: ' + key)
        if item.get_editor_property('purchase_price') != price or not item.get_editor_property('use_on_acquire'):
            raise RuntimeError('Item acquisition configuration mismatch: ' + key)
        shield = effect.get_editor_property('core_shield_data')
        rows.append({'id': key, 'price': price, 'shield_fraction': effect.get_editor_property('objective_shield_fraction'),
                     'heal_fraction': effect.get_editor_property('objective_heal_fraction'),
                     'shield_runtime': shield.get_editor_property('runtime_class').get_path_name() if shield else None})
    if abs(rows[0]['shield_fraction'] - .08) > .0001 or abs(rows[1]['heal_fraction'] - .15) > .0001:
        raise RuntimeError('Formal effect ratios mismatch')
    report = {'items': rows, 'categories': [c.export_text() for c in categories],
              'blueprints': {name: str(load(ROOT + '/' + name).get_editor_property('status'))
                             for name in ['BP_DefenseGameMode', 'BP_DefenseHUD', 'BP_DefensePlayerController']}}
    output = Path(unreal.Paths.project_saved_dir()) / 'DEF-007'
    output.mkdir(parents=True, exist_ok=True)
    readonly = '-DefenseCoreRescueReadOnly' in unreal.SystemLibrary.get_command_line()
    (output / ('cold-readback.json' if readonly else 'asset-migration.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('DefenseCoreRescueVerified ' + json.dumps(report, ensure_ascii=False))


def main():
    if '-DefenseCoreRescueReadOnly' in unreal.SystemLibrary.get_command_line():
        verify()
        return
    shield, fresh = create('DA_team_shrine_barrier_effect', unreal.CombatModifierData)
    if fresh:
        for prop, value in [('definition_name', 'team_shrine_barrier_effect'), ('display_name_text', '神殿屏障'),
                            ('runtime_class', unreal.DefenseCoreShieldRuntime.static_class()),
                            ('dispel_rule', unreal.CombatModifierDispelRule.NOT_DISPELLABLE), ('remove_on_death', True)]:
            shield.set_editor_property(prop, value)
        save(shield)
    rows = [('team_shrine_barrier', '神殿屏障', '立即获得核心最大生命8%的护盾，吸收物理、魔法和纯粹伤害；至下一波清场，不能叠加。生命流失绕过。', 220, '盾'),
            ('team_emergency_stone', '应急石', '立即修复核心最大生命15%，不超过生命上限；满血也会消费。', 180, '援')]
    items = []
    for key, title, desc, price, glyph in rows:
        item, fresh = create('DA_Item_' + key, unreal.CombatItemData)
        if fresh:
            for prop, value in [('definition_name', key), ('display_name_text', title), ('description', desc),
                                ('purchase_price', price), ('purchasable', True), ('use_on_acquire', True),
                                ('sellable', False), ('can_drop', False), ('glyph', glyph)]:
                item.set_editor_property(prop, value)
            effect = unreal.DefenseTeamItemEffect(outer=item)
            effect.set_editor_property('allowed_phases', [unreal.DefensePhase.PREPARATION, unreal.DefensePhase.INTERMISSION, unreal.DefensePhase.BOSS_PREPARATION])
            if key == 'team_shrine_barrier':
                effect.set_editor_property('core_shield_data', shield)
                effect.set_editor_property('objective_shield_fraction', .08)
                effect.set_editor_property('duration_waves', 1)
            else:
                effect.set_editor_property('objective_heal_fraction', .15)
            item.set_editor_property('acquire_effect', effect)
            save(item)
        items.append(item)
    shop = load(ROOT + '/DA_DefenseUnifiedShop')
    categories = list(shop.get_editor_property('categories'))
    for index, category in enumerate(categories):
        if str(category.get_editor_property('category_id')) != 'team_supplies':
            continue
        text = category.export_text()
        for old, new in zip(['team_guard', 'team_repair'], items):
            text = text.replace(FOLDER + '/DA_Item_' + old + '.DA_Item_' + old, new.get_path_name())
        replacement = unreal.CombatShopCategory()
        if not replacement.import_text(text):
            raise RuntimeError('Cannot update native category')
        categories[index] = replacement
    shop.set_editor_property('categories', categories)
    save(shop)
    for name in ['BP_DefenseGameMode', 'BP_DefenseHUD', 'BP_DefensePlayerController']:
        save(load(ROOT + '/' + name))
    verify()


main()
