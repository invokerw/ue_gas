"""DEF-011：原生创建刺客与独立角色配置，保留攻城和每波奖励。"""
import json
import re
from pathlib import Path
import unreal

ROOT = '/Game/Combat/Demo/Defense'
AIROOT = '/Game/Combat/Demo/AI/Defense'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """缺失必需引用立即失败。"""
    obj = unreal.load_asset(path)
    if obj is None:
        raise RuntimeError('Missing asset: ' + path)
    return obj


def save(obj):
    """原生编译和保存，错误不吞掉。"""
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')):
            raise RuntimeError('Blueprint compile failed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError('Save failed: ' + obj.get_path_name())


def migrate():
    """只创建缺失资产；二次执行不覆盖人工调优，不重复插入编成。"""
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/DA_DefenseAssassin'):
        data = TOOLS.duplicate_asset('DA_DefenseAssassin', ROOT, load(ROOT + '/DA_DefenseInvader'))
        data.set_editor_property('definition_name', 'defense_assassin')
        data.set_editor_property('display_name_text', '游猎刺客')
        text = data.get_editor_property('base_stats').export_text()
        for field, value in {'MaxHealth':320, 'MoveSpeed':360, 'AttackDamage':28, 'BaseAttackTime':1.2,
                             'AttackRange':120, 'Armor':1, 'HealthRegen':0, 'AttackSpeed':100}.items():
            text, count = re.subn(r'\b' + field + r'=[^,)]+', field + '=' + str(value), text)
            if count != 1:
                raise RuntimeError('Missing field: ' + field)
        stats = unreal.CombatUnitBaseStats()
        assert stats.import_text(text)
        data.set_editor_property('base_stats', stats)
        data.set_editor_property('base_attack_point', .2)
        data.set_editor_property('gold_reward', 0)
        data.set_editor_property('experience_reward', 0)
        data.set_editor_property('attack_projectile_data', None)
        data.set_editor_property('ai_profile', None)
        save(data)
    data = load(ROOT + '/DA_DefenseAssassin')
    if not unreal.EditorAssetLibrary.does_asset_exist(AIROOT + '/DA_DefenseAssassinAI'):
        profile = TOOLS.duplicate_asset('DA_DefenseAssassinAI', AIROOT, load('/Game/Combat/Demo/AI/Roles/DA_AI_Lane'))
        for key, value in {'definition_name':'defense_assassin_ai', 'display_name_text':'游猎刺客路线角色',
                           'b_enable_perception':True, 'distance_weight':0, 'threat_weight':0,
                           'route_engagement_min_priority':10, 'missing_health_weight':100, 'leash_distance':1600, 'max_engagement_seconds':8,
                           'b_return_after_combat':False, 'b_clear_memory_on_return':True}.items():
            # UE Python通常省略bool字段的b前缀。
            profile.set_editor_property(key[2:] if key.startswith('b_') else key, value)
        perception = profile.get_editor_property('perception')
        for key, value in {'radius':1200, 'active_interval':.2, 'idle_interval':.25, 'memory_seconds':.5,
                           'require_line_of_sight':True, 'observe_damage_threat':False}.items():
            perception.set_editor_property(key, value)
        profile.set_editor_property('perception', perception)
        priority = unreal.CombatAITargetPriority()
        priority.set_editor_property('definition', unreal.PrimaryAssetId(unreal.PrimaryAssetType('CombatUnit'), load(ROOT + '/DA_DefenseHero').get_editor_property('definition_name')))
        priority.set_editor_property('priority', 10)
        profile.set_editor_property('target_priorities', [priority])
        save(profile)
    profile = load(AIROOT + '/DA_DefenseAssassinAI')
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/M_DefenseAssassin'):
        mat = TOOLS.create_asset('M_DefenseAssassin', ROOT, unreal.Material, unreal.MaterialFactoryNew())
        color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -240, 0)
        color.set_editor_property('constant', unreal.LinearColor(.5, .015, .8, 1))
        unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        save(mat)
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT + '/BP_DefenseAssassin'):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.DefenseAssassinEnemy.static_class())
        bp = TOOLS.create_asset('BP_DefenseAssassin', ROOT, unreal.Blueprint, factory)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('unit_data', data)
        cdo.get_editor_property('body').set_material(0, load(ROOT + '/M_DefenseAssassin'))
        normal = unreal.get_default_object(load(ROOT + '/BP_DefenseInvader').generated_class())
        widget = normal.get_editor_property('combat_overhead_widget_component').get_editor_property('widget_class')
        assert widget
        cdo.get_editor_property('combat_overhead_widget_component').set_editor_property('widget_class', widget)
        save(bp)
    bp = load(ROOT + '/BP_DefenseAssassin')
    for number in [4,5]:
        wave = load(ROOT + '/DA_DefenseWave_' + str(number))
        groups = list(wave.get_editor_property('spawn_groups'))
        if any(g.get_editor_property('unit_data') == data for g in groups):
            continue
        updated = []
        for group in groups:
            if str(group.get_editor_property('unit_data').get_editor_property('definition_name')) == 'defense_invader':
                assert group.get_editor_property('count') == 5
                group.set_editor_property('count', 4)
                updated.append(group)
                extra = unreal.DefenseSpawnGroup()
                for key, value in {'unit_class':bp.generated_class(), 'unit_data':data, 'role_profile':profile,
                                   'count':1, 'route_index':group.get_editor_property('route_index'),
                                   'spawn_interval':group.get_editor_property('spawn_interval')}.items():
                    extra.set_editor_property(key, value)
                updated.append(extra)
            else:
                updated.append(group)
        wave.set_editor_property('spawn_groups', updated)
        save(wave)


def verify(readonly):
    """回读真实保存内容，不修改对象。"""
    run = load(ROOT + '/DA_DefenseRun')
    data = load(ROOT + '/DA_DefenseAssassin')
    bp = load(ROOT + '/BP_DefenseAssassin')
    cdo = unreal.get_default_object(bp.generated_class())
    profile = load(AIROOT + '/DA_DefenseAssassinAI')
    assert cdo.get_editor_property('unit_data') == data
    assert cdo.get_editor_property('combat_overhead_widget_component').get_editor_property('widget_class')
    assert 'BS_UP_TO_DATE' in str(bp.get_editor_property('status'))
    assert data.get_editor_property('ai_profile') is None
    assert data.get_editor_property('gold_reward') == data.get_editor_property('experience_reward') == 0
    stats = {n:data.get_editor_property('base_stats').get_editor_property(n) for n in ['max_health','move_speed','attack_damage','base_attack_time','armor','attack_range']}
    assert stats['max_health']==320 and stats['move_speed']==360 and stats['attack_damage']==28
    assert profile.get_editor_property('missing_health_weight')==100
    assert str(profile.get_editor_property('target_priorities')[0].get_editor_property('definition').primary_asset_name)==str(load(ROOT + '/DA_DefenseHero').get_editor_property('definition_name'))
    waves = []
    for i,wave in enumerate(run.get_editor_property('waves')):
        groups = list(wave.get_editor_property('spawn_groups'))
        assassins = [g for g in groups if g.get_editor_property('unit_data')==data]
        assert sum(g.get_editor_property('count') for g in assassins)==(2 if i in [3,4] else 0)
        if i in [3,4]:
            assert {g.get_editor_property('route_index') for g in assassins}=={0,1}
            assert all(g.get_editor_property('role_profile')==profile for g in assassins)
            assert sum(g.get_editor_property('count') for g in groups)==(10 if i==3 else 12)
            assert wave.get_editor_property('gold_reward')==(190 if i==3 else 240)
            assert wave.get_editor_property('experience_reward')==(500 if i==3 else 600)
        waves.append({'wave':i+1,'groups':[g.export_text() for g in groups]})
    report={'stats':stats,'profile':profile.get_path_name(),'perception':profile.get_editor_property('perception').export_text(),
            'root':profile.get_editor_property('root_tree').get_path_name(),'blueprint':str(bp.get_editor_property('status')),'waves':waves}
    out=Path(unreal.Paths.project_saved_dir())/'DEF-011'
    out.mkdir(exist_ok=True,parents=True)
    (out/('cold-readback.json' if readonly else 'asset-migration.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('DefenseAssassinContentVerified '+json.dumps(report,ensure_ascii=False))


readonly = '-DefenseAssassinReadOnly' in unreal.SystemLibrary.get_command_line()
if not readonly:
    migrate()
verify(readonly)
