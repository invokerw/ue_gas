"""DEF-004：使用 UE 资产 API 创建原型强化和选择入口；重复执行保留卡片作者修改。"""
import json
import re
from pathlib import Path
import unreal

ROOT = '/Game/Combat/Demo/Defense'
FOLDER = ROOT + '/Augments'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """精确引用缺失即停止，不用同名资产猜测。"""
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError('Missing asset: ' + path)
    return asset


def save(asset):
    """蓝图编译并确认状态，再原生保存。"""
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        if 'ERROR' in str(asset.get_editor_property('status')):
            raise RuntimeError('Blueprint compile failed: ' + asset.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError('Save failed: ' + asset.get_path_name())


def new_data(name, cls, configure):
    """已存在的卡/效果只回读，新增才填写默认数值。"""
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    asset = TOOLS.create_asset(name, FOLDER, cls, factory)
    configure(asset)
    save(asset)
    return asset


def main():
    # 从已校验属性结构取得实际 FFieldPath 编码，再经 UE ImportText 解析新结构。
    source_change = load('/Game/Combat/Demo/Items/DA_item_armor_passive').get_editor_property('attribute_changes')[0]
    template = source_change.export_text()
    unreal.log('DefenseModifierTemplate ' + template)
    entries = [
        ('hunter', '猎手之力', '普攻基础攻击力 +15', 'AttackDamage', 15, 'FROST', False),
        ('tempo', '连射节奏', '攻击速度 +20', 'AttackSpeed', 20, 'FROST', False),
        ('stride', '追风步', '移动速度 +45', 'MoveSpeed', 45, 'FROST', False),
        ('reach', '远望', '普攻攻击距离 +100 厘米', 'AttackRange', 100, 'FROST', True),
        ('focus', '法术专注', '技能伤害增幅 +15%', 'SpellAmplifyPct', 0.15, 'SKILL', False),
        ('reserves', '法力储备', '最大法力 +150（不立即补满）', 'MaxMana', 150, 'SKILL', False),
        ('flow', '法力回流', '每秒法力恢复 +2', 'ManaRegen', 2, 'SKILL', False),
        ('cadence', '迅捷施法', '冷却缩减 +10%', 'CooldownReductionPct', 0.1, 'SKILL', True),
        ('armor', '坚守', '物理护甲 +5', 'Armor', 5, 'GUARDIAN', False),
        ('vitality', '生命堡垒', '最大生命 +250（不立即治疗）', 'MaxHealth', 250, 'GUARDIAN', False),
        ('recovery', '持续修复', '每秒生命恢复 +3', 'HealthRegen', 3, 'GUARDIAN', False),
        ('resistance', '魔法庇护', '魔法抗性 +12%', 'MagicResist', 0.12, 'GUARDIAN', True),
    ]
    cards = []
    for key, title, desc, attribute, amount, family, advanced in entries:
        def effect_config(effect, key=key, attribute=attribute, amount=amount):
            effect.set_editor_property('definition_name', 'defense_card_' + key + '_effect')
            effect.set_editor_property('remove_on_death', False)
            effect.set_editor_property('duration', 0.0)
            effect.set_editor_property('max_stacks', 1)
            effect.set_editor_property('is_debuff', False)
            change = unreal.CombatModifierAttributeChange()
            text = re.sub(r'Magnitude=[^,)]+', 'Magnitude=' + str(amount), template.replace('Armor', attribute))
            if not change.import_text(text):
                raise RuntimeError('Cannot import attribute: ' + text)
            effect.set_editor_property('attribute_changes', [change])
        effect = new_data('DA_Effect_' + key, unreal.CombatModifierData, effect_config)
        def card_config(card, key=key, title=title, desc=desc, effect=effect, family=family, advanced=advanced):
            card.set_editor_property('definition_name', 'defense_card_' + key)
            card.set_editor_property('display_name_text', title)
            card.set_editor_property('card_name', title)
            card.set_editor_property('short_description', desc)
            card.set_editor_property('family', getattr(unreal.DefenseAugmentFamily, family))
            card.set_editor_property('rarity', unreal.DefenseAugmentRarity.ADVANCED if advanced else unreal.DefenseAugmentRarity.BASIC)
            card.set_editor_property('earliest_wave', 1 if advanced else 0)
            card.set_editor_property('modifier_data', effect)
        cards.append(new_data('DA_Card_' + key, unreal.DefenseAugmentData, card_config))
    def pool_config(pool):
        pool.set_editor_property('definition_name', 'defense_prototype_pool')
        pool.set_editor_property('display_name_text', '个人构筑原型池')
        pool.set_editor_property('augments', cards)
    pool = new_data('DA_DefensePool', unreal.DefenseAugmentPoolData, pool_config)
    pc_path = ROOT + '/BP_DefensePlayerController'
    if unreal.EditorAssetLibrary.does_asset_exist(pc_path):
        pc = load(pc_path)
    else:
        pc = TOOLS.duplicate_asset('BP_DefensePlayerController', ROOT, load('/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController'))
        unreal.BlueprintEditorLibrary.reparent_blueprint(pc, unreal.DefensePlayerController.static_class())
        save(pc)
    run = load(ROOT + '/DA_DefenseRun')
    existing = run.get_editor_property('augment_pool')
    if existing is None:
        run.set_editor_property('augment_pool', pool)
        run.set_editor_property('choice_duration', 15.0)
        run.set_editor_property('intermission_duration', 25.0)
        save(run)
    elif existing != pool:
        raise RuntimeError('Unexpected authored pool; refusing to overwrite')
    mode = load(ROOT + '/BP_DefenseGameMode')
    cdo = unreal.get_default_object(mode.generated_class())
    cdo.set_editor_property('player_controller_class', pc.generated_class())
    cdo.set_editor_property('development_wait_for_start', True)
    save(mode)
    report = {
        'pool': pool.get_path_name(),
        'cards': [{'id': str(c.get_editor_property('definition_name')), 'name': str(c.get_editor_property('card_name')),
                   'modifier': c.get_editor_property('modifier_data').get_path_name(),
                   'changes': [a.export_text() for a in c.get_editor_property('modifier_data').get_editor_property('attribute_changes')]}
                  for c in pool.get_editor_property('augments')],
        'run': {'pool': run.get_editor_property('augment_pool').get_path_name(),
                'choice_seconds': run.get_editor_property('choice_duration'), 'intermission_seconds': run.get_editor_property('intermission_duration')},
        'controller': cdo.get_editor_property('player_controller_class').get_path_name(),
        'blueprints': {b.get_path_name(): str(b.get_editor_property('status')) for b in [pc, mode]},
    }
    output = Path(unreal.Paths.project_saved_dir()) / 'DEF-004'
    output.mkdir(parents=True, exist_ok=True)
    (output / 'asset-readback.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('DefenseAugmentsReadback ' + json.dumps(report, ensure_ascii=False))


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
