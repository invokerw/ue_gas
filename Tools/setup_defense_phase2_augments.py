"""DEF-009：把防守强化池从12张属性原型扩展为24张稳定卡。"""
import json
from pathlib import Path
import unreal
ROOT='/Game/Combat/Demo/Defense'; FOLDER=ROOT+'/Augments'; TOOLS=unreal.AssetToolsHelpers.get_asset_tools()

def load(path):
 o=unreal.load_asset(path)
 if o is None: raise RuntimeError('Missing: '+path)
 return o

def save(o):
 if not unreal.EditorAssetLibrary.save_loaded_asset(o,False): raise RuntimeError('Save failed '+o.get_path_name())

def make(name,cls):
 p=FOLDER+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(p): return load(p),False
 f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',cls);return TOOLS.create_asset(name,FOLDER,cls,f),True

def main():
 pool=load(ROOT+'/Augments/DA_DefensePool'); cards=list(pool.get_editor_property('augments'))
 source=load('/Game/Combat/Demo/Items/DA_item_armor_passive').get_editor_property('attribute_changes')[0].export_text()
 rows=[
 ('frost_bloom','霜冻·冰花','家族计数：霜冻弹幕。普攻攻击力 +12。','FROST','AttackDamage',12,False,0,[]),
 ('frost_split','霜冻·分流','家族计数：霜冻弹幕。攻击距离 +80。','FROST','AttackRange',80,False,0,[]),
 ('frost_cold','霜冻·冷锋','霜冻终局卡；需两张霜冻卡且第3波后出现。攻速 +15。','FROST','AttackSpeed',15,True,2,['defense_card_hunter','defense_card_tempo']),
 ('skill_echo','技能·回响','家族计数：技能爆发。技能增幅 +10%。','SKILL','SpellAmplifyPct',.10,False,0,[]),
 ('skill_reserve','技能·留力','家族计数：技能爆发。最大法力 +120。','SKILL','MaxMana',120,False,0,[]),
 ('skill_ember','技能·余烬','技能终局卡；需两张技能卡且第3波后出现。冷却缩减 +5%。','SKILL','CooldownReductionPct',.05,True,2,['defense_card_focus','defense_card_reserves']),
 ('guardian_shell','守护·甲壳','家族计数：守护反击。护甲 +4。','GUARDIAN','Armor',4,False,0,[]),
 ('guardian_pulse','守护·脉冲','家族计数：守护反击。最大生命 +180。','GUARDIAN','MaxHealth',180,False,0,[]),
 ('guardian_wall','守护·城墙','守护终局卡；需两张守护卡且第3波后出现。魔抗 +8%。','GUARDIAN','MagicResist',.08,True,2,['defense_card_armor','defense_card_vitality']),
 ('economy_cache','风险·宝匣','家族计数：风险富集。金币型构筑的基础生存：最大生命 +100。','ECONOMY','MaxHealth',100,False,0,[]),
 ('economy_hunt','风险·悬赏','家族计数：风险富集。攻击力 +10。','ECONOMY','AttackDamage',10,False,0,[]),
 ('economy_contract','风险·破釜','风险终局卡；需两张风险卡且第3波后出现。生命恢复 +2。','ECONOMY','HealthRegen',2,True,2,['defense_card_hunter','defense_card_reserves'])]
 for key,title,desc,fam,attr,amount,final,earliest,prereq in rows:
  effect,fresh=make('DA_Effect_'+key,unreal.CombatModifierData)
  if fresh:
   effect.set_editor_property('definition_name','defense_card_'+key+'_effect');effect.set_editor_property('display_name_text',title);effect.set_editor_property('remove_on_death',False);effect.set_editor_property('dispel_rule',unreal.CombatModifierDispelRule.NOT_DISPELLABLE)
   c=unreal.CombatModifierAttributeChange();text=source.replace('Armor',attr).replace('Magnitude=5','Magnitude='+str(amount));
   if not c.import_text(text): raise RuntimeError('Import failed '+key)
   effect.set_editor_property('attribute_changes',[c]);save(effect)
  card,fresh=make('DA_Card_'+key,unreal.DefenseAugmentData)
  if fresh:
   card.set_editor_property('definition_name','defense_card_'+key);card.set_editor_property('display_name_text',title);card.set_editor_property('card_name',title);card.set_editor_property('short_description',desc);card.set_editor_property('family',getattr(unreal.DefenseAugmentFamily,fam));card.set_editor_property('rarity',unreal.DefenseAugmentRarity.FINAL if final else unreal.DefenseAugmentRarity.BASIC);card.set_editor_property('earliest_wave',earliest);card.set_editor_property('modifier_data',effect);card.set_editor_property('prerequisites',[unreal.PrimaryAssetId(unreal.PrimaryAssetType('DefenseAugment'),x) for x in prereq]);save(card)
  if not any(str(c.get_editor_property('definition_name'))=='defense_card_'+key for c in cards): cards.append(card)
 pool.set_editor_property('augments',cards);save(pool)
 report={'pool':pool.get_path_name(),'count':len(cards),'ids':[str(c.get_editor_property('definition_name')) for c in cards],'final':[str(c.get_editor_property('definition_name')) for c in cards if c.get_editor_property('rarity')==unreal.DefenseAugmentRarity.FINAL]}
 out=Path(unreal.Paths.project_saved_dir())/'DEF-009';out.mkdir(parents=True,exist_ok=True);(out/'augment-migration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('DefensePhase2Augments '+json.dumps(report,ensure_ascii=False))
main()
