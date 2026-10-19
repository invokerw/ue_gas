"""DEF-012：原生创建神殿守卫英雄包，已有资产只回读，保留原游侠与波次。"""
import json
import re
from pathlib import Path
import unreal

ROOT='/Game/Combat/Demo/Defense'
FOLDER=ROOT+'/Guardian'
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    """必要引用缺失立即失败。"""
    obj=unreal.load_asset(path)
    if obj is None: raise RuntimeError('Missing asset: '+path)
    return obj


def save(obj):
    """原生编译保存；不以文本工具处理二进制。"""
    if isinstance(obj,unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        if 'ERROR' in str(obj.get_editor_property('status')): raise RuntimeError('Blueprint compile failed')
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj,False)


def create(name,cls,configure,parent=None,source=None):
    """仅创建缺失资产，不覆盖后续人工调优。"""
    if unreal.EditorAssetLibrary.does_asset_exist(FOLDER+'/'+name): return load(FOLDER+'/'+name)
    if source: obj=TOOLS.duplicate_asset(name,FOLDER,source)
    else:
        factory=unreal.BlueprintFactory() if parent else unreal.DataAssetFactory()
        factory.set_editor_property('parent_class' if parent else 'data_asset_class',parent or cls)
        obj=TOOLS.create_asset(name,FOLDER,unreal.Blueprint if parent else cls,factory)
    assert obj
    configure(obj); save(obj); return obj


def props(obj,values):
    """批量设置已声明的原生属性。"""
    for key,value in values.items(): obj.set_editor_property(key,value)


def tags(names):
    """Tag容器通过原生ImportText构造，标签均来自既有Native注册。"""
    result=unreal.GameplayTagContainer()
    assert result.import_text('(GameplayTags=('+','.join('(TagName="'+n+'")' for n in names)+'))')
    return result


def tag(name):
    """单个已注册GameplayTag。"""
    result=unreal.GameplayTag(); assert result.import_text('(TagName="'+name+'")'); return result


def definition(obj,name,title):
    props(obj,{'definition_name':name,'display_name_text':title})


def modifier(name,title,duration,runtime=None,params=None,stacks=1,stun=False,debuff=False):
    """所有时长、叠层和Runtime参数进入独立Modifier资产。"""
    def configure(obj):
        definition(obj,name,title)
        props(obj,{'runtime_class':runtime or unreal.CombatModifierRuntime.static_class(),'duration':duration,
                   'max_stacks':stacks,'runtime_parameters':params or {},'is_debuff':debuff,
                   'remove_on_death':True,'dispel_rule':unreal.CombatModifierDispelRule.NOT_DISPELLABLE})
        if stun: obj.set_editor_property('granted_tags',tags(['State.Stunned']))
    return create('DA_'+name,unreal.CombatModifierData,configure)


def material(name,color):
    """可辨认的灰盒材质，不下载外部美术。"""
    if unreal.EditorAssetLibrary.does_asset_exist(FOLDER+'/'+name):
        mat=load(FOLDER+'/'+name)
        if name=='M_GuardianArmor' and not mat.get_editor_property('used_with_skeletal_mesh'):
            mat.set_editor_property('used_with_skeletal_mesh',True); unreal.MaterialEditingLibrary.recompile_material(mat); save(mat)
        return mat
    mat=TOOLS.create_asset(name,FOLDER,unreal.Material,unreal.MaterialFactoryNew())
    if name=='M_GuardianArmor': mat.set_editor_property('used_with_skeletal_mesh',True)
    node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-200,0)
    node.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat); save(mat); return mat


def migrate():
    """依赖按Modifier→技能→技能组→英雄→入口顺序创建。"""
    shield=modifier('guardian_fortify_shield','固守 · 生命护盾',3,unreal.DefenseShieldRuntime.static_class(),{'shield_fraction':.2,'shield_cap_fraction':.4})
    passive_shield=modifier('guardian_resolve_shield','坚忍蓄盾',3,unreal.DefenseShieldRuntime.static_class(),{'shield_fraction':.05,'shield_cap_fraction':.4})
    stun=modifier('guardian_slam_stun','震地眩晕',1,stun=True,debuff=True)
    boss_stun=modifier('defense_boss_stagger_stun','Boss失衡 · 眩晕',2,stun=True,debuff=True)
    recovery=modifier('defense_boss_stagger_recovery','失衡保护 · 含眩晕后6秒免疫',8)
    stagger_bp=create('BP_BossStaggerRuntime',unreal.Blueprint,lambda bp:props(unreal.get_default_object(bp.generated_class()),{'stun_data':boss_stun,'recovery_data':recovery}),parent=unreal.DefenseBossStaggerRuntime.static_class())
    stagger=modifier('defense_boss_stagger','Boss失衡累计 · 8秒内4层',8,stagger_bp.generated_class(),{'stagger_threshold':4},stacks=4)
    passive_bp=create('BP_GuardianPassiveRuntime',unreal.Blueprint,lambda bp:props(unreal.get_default_object(bp.generated_class()),{'shield_data':passive_shield}),parent=unreal.DefenseGuardianPassiveRuntime.static_class())
    intrinsic=modifier('guardian_resolve_intrinsic','坚忍 · 三次受击后普攻蓄盾',0,passive_bp.generated_class(),{'hits_required':3,'internal_cooldown':4})
    if not intrinsic.get_editor_property('disabled_by_break'):
        intrinsic.set_editor_property('disabled_by_break',True); save(intrinsic)
    boss_id=unreal.PrimaryAssetId(unreal.PrimaryAssetType('CombatUnit'),load(ROOT+'/DA_DefenseBoss').get_editor_property('definition_name'))
    specs=[('guardian_shield_bash','盾击','PointTarget',3,.25,{'damage':[180,240,300],'radius':[400],'half_angle':[60],'mana_cost':[30],'cooldown':[8]}),
           ('guardian_fortify','固守','NoTarget',3,.1,{'mana_cost':[35],'cooldown':[12,11,10]}),
           ('guardian_ground_slam','震地','NoTarget',3,.35,{'damage':[80,120,160],'radius':[450],'half_angle':[180],'mana_cost':[45],'cooldown':[14]}),
           ('guardian_resolve','坚忍蓄盾','NoTarget',1,0,{'mana_cost':[0],'cooldown':[0]})]
    blueprints=[]
    for name,title,mode,levels,cast,specials in specs:
        def configure(data):
            definition(data,name,title)
            props(data,{'max_level':levels,'cast_point':cast,'behavior_tags':tags(['Ability.Behavior.'+mode]+(['Ability.Behavior.Passive'] if name=='guardian_resolve' else []))})
            rules=data.get_editor_property('targeting_rules')
            props(rules,{'target_team_tag':tag('TargetTeam.Enemy' if mode=='PointTarget' else 'TargetTeam.None'),
                         'cast_range':400 if mode=='PointTarget' else 0,'require_line_of_sight':True})
            data.set_editor_property('targeting_rules',rules)
            values={}
            for key,items in specials.items():
                entry=unreal.CombatSpecialValue(); assert entry.import_text('(Values=('+','.join(str(v) for v in items)+'))'); values[key]=entry
            data.set_editor_property('special_values',values)
            if name=='guardian_fortify':
                action=unreal.CombatAbilityAction()
                assert action.import_text('(Type=ApplyModifier,Target=Caster,ModifierData="'+shield.get_path_name()+'")')
                data.set_editor_property('actions',[action])
            if name=='guardian_resolve': data.set_editor_property('intrinsic_modifier',intrinsic)
        data=create('DA_'+name,unreal.CombatAbilityData,configure)
        area=name in ['guardian_shield_bash','guardian_ground_slam']
        def configure_bp(bp):
            cdo=unreal.get_default_object(bp.generated_class()); cdo.set_editor_property('ability_data',data)
            if area: props(cdo,{'boss_definition':boss_id,'control_modifier':stun if name=='guardian_ground_slam' else None,'boss_stagger_modifier':stagger if name=='guardian_ground_slam' else None})
        blueprints.append(create('GA_'+name,unreal.Blueprint,configure_bp,parent=(unreal.DefenseGuardianAreaAbility if area else unreal.CombatGameplayAbility).static_class()))
    def configure_set(obj):
        definition(obj,'defense_guardian_ability_set','神殿守卫技能组')
        entries=[]
        for bp in blueprints:
            entry=unreal.CombatAbilitySetEntry(); assert entry.import_text('(AbilityClass="'+bp.generated_class().get_path_name()+'",InitialLevel=1)'); entries.append(entry)
        obj.set_editor_property('abilities',entries)
    ability_set=create('DA_GuardianAbilitySet',unreal.CombatAbilitySet,configure_set)
    def configure_unit(obj):
        definition(obj,'defense_guardian','神殿守卫')
        props(obj,{'ability_sets':[ability_set],'attack_projectile_data':None,'base_attack_point':.3,'initial_items':[],
                   'ai_profile':None,'initial_level':1,'initial_experience':0,'gold_reward':0,'experience_reward':0})
        text=obj.get_editor_property('base_stats').export_text()
        for key,value in {'MaxHealth':1400,'MaxMana':240,'AttackDamage':85,'BaseAttackTime':1.8,'AttackRange':160,'Armor':8,'MoveSpeed':500,'HealthRegen':1,'ManaRegen':3}.items():
            text,count=re.subn(r'\b'+key+r'=[^,)]+',key+'='+str(value),text); assert count==1,key
        stats=unreal.CombatUnitBaseStats(); assert stats.import_text(text); obj.set_editor_property('base_stats',stats)
    data=create('DA_DefenseGuardian',unreal.CombatUnitData,configure_unit,source=load(ROOT+'/DA_DefenseHero'))
    armor=material('M_GuardianArmor',(.025,.24,.65)); weapon=material('M_GuardianWeapon',(.7,.55,.16))
    def configure_hero(bp):
        cdo=unreal.get_default_object(bp.generated_class()); cdo.set_editor_property('unit_data',data)
        original=unreal.get_default_object(load(ROOT+'/BP_DefenseHero').generated_class())
        source=original.get_editor_property('mesh'); mesh=cdo.get_editor_property('mesh')
        for field in ['skeletal_mesh_asset','anim_class','animation_mode','relative_location','relative_rotation','relative_scale3d']:
            mesh.set_editor_property(field,source.get_editor_property(field))
        for i in range(mesh.get_num_materials()): mesh.set_material(i,armor)
        cdo.get_editor_property('shield_mesh').set_material(0,armor)
        cdo.get_editor_property('weapon_mesh').set_material(0,weapon)
        cdo.get_editor_property('combat_overhead_widget_component').set_editor_property('widget_class',original.get_editor_property('combat_overhead_widget_component').get_editor_property('widget_class'))
    hero=create('BP_DefenseGuardian',unreal.Blueprint,configure_hero,parent=unreal.DefenseGuardianHero.static_class())
    mode=load(ROOT+'/BP_DefenseGameMode'); cdo=unreal.get_default_object(mode.generated_class())
    if cdo.get_editor_property('guardian_class')!=hero.generated_class(): cdo.set_editor_property('guardian_class',hero.generated_class()); save(mode)
    profile=load('/Game/Combat/Demo/AI/Defense/DA_DefenseAssassinAI')
    priorities=list(profile.get_editor_property('target_priorities'))
    hero_id=unreal.PrimaryAssetId(unreal.PrimaryAssetType('CombatUnit'),data.get_editor_property('definition_name'))
    if not any(p.get_editor_property('definition')==hero_id for p in priorities):
        priority=unreal.CombatAITargetPriority(); props(priority,{'definition':hero_id,'priority':10}); priorities.append(priority); profile.set_editor_property('target_priorities',priorities); save(profile)


def verify(readonly):
    """独立冷读身份、装配、字段和原生蓝图状态。"""
    data=load(FOLDER+'/DA_DefenseGuardian'); bp=load(FOLDER+'/BP_DefenseGuardian'); cdo=unreal.get_default_object(bp.generated_class())
    assert str(data.get_editor_property('definition_name'))=='defense_guardian'
    assert cdo.get_editor_property('unit_data')==data and cdo.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset')
    assert cdo.get_editor_property('combat_overhead_widget_component').get_editor_property('widget_class')
    assert data.get_editor_property('attack_projectile_data') is None and data.get_editor_property('ai_profile') is None
    assert unreal.get_default_object(load(ROOT+'/BP_DefenseGameMode').generated_class()).get_editor_property('guardian_class')==bp.generated_class()
    ability_set=load(FOLDER+'/DA_GuardianAbilitySet'); entries=list(ability_set.get_editor_property('abilities')); assert len(entries)==4
    skills=[]
    for entry in entries:
        ability=unreal.get_default_object(entry.get_editor_property('ability_class')).get_editor_property('ability_data')
        skills.append({'id':str(ability.get_editor_property('definition_name')),'levels':ability.get_editor_property('max_level'),'specials':{str(k):list(v.get_editor_property('values')) for k,v in ability.get_editor_property('special_values').items()}})
        assert entry.get_editor_property('initial_level')==1
    report={'hero':str(data.get_editor_property('definition_name')),'stats':data.get_editor_property('base_stats').export_text(),'skills':skills,'blueprints':{}}
    for path in unreal.EditorAssetLibrary.list_assets(FOLDER,recursive=True,include_folder=False):
        obj=load(path)
        if isinstance(obj,unreal.Blueprint):
            state=str(obj.get_editor_property('status')); assert 'BS_UP_TO_DATE' in state; report['blueprints'][path]=state
    out=Path(unreal.Paths.project_saved_dir())/'DEF-012'; out.mkdir(exist_ok=True,parents=True)
    (out/('cold-readback.json' if readonly else 'asset-migration.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('GuardianContentVerified '+json.dumps(report,ensure_ascii=False))


readonly='-DefenseGuardianReadOnly' in unreal.SystemLibrary.get_command_line()
if not readonly: migrate()
verify(readonly)
