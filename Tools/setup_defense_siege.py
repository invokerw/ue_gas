"""DEF-010：UE原生攻城单位/外形/波次迁移，可独立只读冷验收。"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Combat/Demo/Defense'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """必需资产按精确路径读取。"""
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError('Missing asset: ' + path)
    return obj


def save(obj):
    """指定资产编译保存，失败立即中止，不跳过错误。"""
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')):
            raise RuntimeError('Compile failed: ' + obj.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError('Save failed: ' + obj.get_path_name())


def migrate():
    """仅首次建立定义和外形；波次重复执行必须保持编成和奖励。"""
    if unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/DA_DefenseSiege'):
        siege = load(ROOT + '/DA_DefenseSiege')
    else:
        siege = TOOLS.duplicate_asset('DA_DefenseSiege', ROOT, load(ROOT + '/DA_DefenseInvader'))
        siege.set_editor_property('definition_name', 'defense_siege')
        siege.set_editor_property('display_name_text', '攻城冲槌')
        stats = siege.get_editor_property('base_stats')
        # 原生结构ImportText保留未修改的字段，并拒绝找不到属性。
        import re
        text = stats.export_text()
        for field, value in {'MaxHealth':650, 'MoveSpeed':140, 'AttackDamage':80, 'BaseAttackTime':3,
                             'AttackRange':220, 'Armor':4, 'HealthRegen':0, 'AttackSpeed':100}.items():
            text, count = re.subn(r'\b' + field + r'=[^,)]+', field + '=' + str(value), text)
            if count != 1:
                raise RuntimeError('Missing base stat: ' + field)
        replacement = unreal.CombatUnitBaseStats()
        if not replacement.import_text(text):
            raise RuntimeError('Cannot import siege base stats')
        siege.set_editor_property('base_stats', replacement)
        siege.set_editor_property('base_attack_point', .65)
        siege.set_editor_property('gold_reward', 0)
        siege.set_editor_property('experience_reward', 0)
        siege.set_editor_property('attack_projectile_data', None)
        siege.set_editor_property('ai_profile', None)
        save(siege)
    if unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/M_DefenseSiege'):
        material = load(ROOT + '/M_DefenseSiege')
    else:
        material = TOOLS.create_asset('M_DefenseSiege', ROOT, unreal.Material, unreal.MaterialFactoryNew())
        color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -240, 0)
        color.set_editor_property('constant', unreal.LinearColor(.8, .22, .025, 1))
        unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(material)
        save(material)
    if unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/BP_DefenseSiege'):
        bp = load(ROOT + '/BP_DefenseSiege')
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.DefenseSiegeEnemy.static_class())
        bp = TOOLS.create_asset('BP_DefenseSiege', ROOT, unreal.Blueprint, factory)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('unit_data', siege)
        body = cdo.get_editor_property('body')
        body.set_material(0, material)
        save(bp)
    # 复用既有敌人的头顶表现；只补新蓝图尚未配置的Widget，保留手工替换。
    overhead = unreal.get_default_object(bp.generated_class()).get_editor_property('combat_overhead_widget_component')
    if overhead.get_editor_property('widget_class') is None:
        source = unreal.get_default_object(load(ROOT + '/BP_DefenseInvader').generated_class()).get_editor_property('combat_overhead_widget_component')
        widget = source.get_editor_property('widget_class')
        if widget is None:
            raise RuntimeError('Existing enemy has no overhead widget')
        overhead.set_editor_property('widget_class', widget)
        save(bp)
    for number in [3, 5]:
        wave = load(ROOT + '/DA_DefenseWave_' + str(number))
        groups = list(wave.get_editor_property('spawn_groups'))
        if any(g.get_editor_property('unit_data') == siege for g in groups):
            continue
        if len(groups) != 2 or {g.get_editor_property('route_index') for g in groups} != {0, 1}:
            raise RuntimeError('Unexpected authored composition: ' + str(number))
        updated = []
        for group in groups:
            count = group.get_editor_property('count')
            if count != (4 if number == 3 else 6):
                raise RuntimeError('Unexpected baseline count: ' + str(number))
            group.set_editor_property('count', count - 1)
            updated.append(group)
            extra = unreal.DefenseSpawnGroup()
            extra.set_editor_property('unit_class', bp.generated_class())
            extra.set_editor_property('unit_data', siege)
            extra.set_editor_property('count', 1)
            extra.set_editor_property('spawn_interval', group.get_editor_property('spawn_interval'))
            extra.set_editor_property('route_index', group.get_editor_property('route_index'))
            updated.append(extra)
        wave.set_editor_property('spawn_groups', updated)
        save(wave)


def verify(readonly):
    """冷读每个实际资产与数值，保存报告前执行断言。"""
    run = load(ROOT + '/DA_DefenseRun')
    siege = load(ROOT + '/DA_DefenseSiege')
    bp = load(ROOT + '/BP_DefenseSiege')
    cdo = unreal.get_default_object(bp.generated_class())
    stats = siege.get_editor_property('base_stats')
    if str(siege.get_editor_property('definition_name')) != 'defense_siege' or cdo.get_editor_property('unit_data') != siege:
        raise RuntimeError('Stable identity/CDO mismatch')
    values = {n: stats.get_editor_property(n) for n in ['max_health', 'move_speed', 'attack_damage', 'base_attack_time', 'attack_range', 'armor', 'health_regen']}
    if values != {'max_health':650, 'move_speed':140, 'attack_damage':80, 'base_attack_time':3, 'attack_range':220, 'armor':4, 'health_regen':0}:
        raise RuntimeError('Siege stats mismatch: ' + str(values))
    if siege.get_editor_property('gold_reward') or siege.get_editor_property('experience_reward') or siege.get_editor_property('ai_profile'):
        raise RuntimeError('Siege duplicates rewards or AI orders')
    waves = []
    for index, wave in enumerate(run.get_editor_property('waves')):
        groups = list(wave.get_editor_property('spawn_groups'))
        special = [g for g in groups if g.get_editor_property('unit_data') == siege]
        if sum(g.get_editor_property('count') for g in special) != (2 if index in [2,4] else 0):
            raise RuntimeError('Siege count mismatch: ' + str(index+1))
        if index in [2,4]:
            assert {g.get_editor_property('route_index') for g in special} == {0,1}
            assert sum(g.get_editor_property('count') for g in groups) == (8 if index == 2 else 12)
            assert wave.get_editor_property('gold_reward') == (160 if index == 2 else 240)
            assert wave.get_editor_property('experience_reward') == (400 if index == 2 else 600)
        waves.append({'wave':index+1, 'groups':[g.export_text() for g in groups], 'gold':wave.get_editor_property('gold_reward'), 'xp':wave.get_editor_property('experience_reward')})
    report = {'id':str(siege.get_editor_property('definition_name')), 'name':str(siege.get_editor_property('display_name_text')), 'stats':values,
              'blueprint':str(bp.get_editor_property('status')),
              'overhead_widget':cdo.get_editor_property('combat_overhead_widget_component').get_editor_property('widget_class').get_path_name(), 'body_material':cdo.get_editor_property('body').get_material(0).get_path_name(), 'waves':waves}
    assert 'BS_UP_TO_DATE' in report['blueprint']
    out = Path(unreal.Paths.project_saved_dir()) / 'DEF-010'
    out.mkdir(parents=True, exist_ok=True)
    (out / ('cold-readback.json' if readonly else 'asset-migration.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('DefenseSiegeContentVerified ' + json.dumps(report, ensure_ascii=False))


readonly = '-DefenseSiegeReadOnly' in unreal.SystemLibrary.get_command_line()
if not readonly:
    migrate()
verify(readonly)
