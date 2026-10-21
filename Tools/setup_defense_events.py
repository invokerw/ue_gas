"""DEF-013 正式事件资产：由 UE 原生工厂创建；新进程 -- 只读模式核对保存结果。"""
import json
from pathlib import Path
import unreal

ROOT='/Game/Combat/Demo/Defense'
FOLDER=ROOT+'/Events'
READ_ONLY='-DefenseEventsReadOnly' in unreal.SystemLibrary.get_command_line()
DEFINITIONS=[
 ('DA_EventVault', dict(definition_name='defense_event_vault', kind=unreal.DefenseEventKind.VAULT, trigger_after_wave=2,
 title='北侧宝库',safe_description='安全取出补给：全队获得240金币与60经验。',
 risk_description='冒险开启宝库：下一波增加一名敌人，成功后全队获得300金币，Boss奖励提高10%。',
 safe_gold=240,safe_experience=60,safe_core_heal_fraction=0.0,risk_budget_bonus=12,risk_gold=300,risk_experience=0,boss_reward_bonus_fraction=0.1)),
 ('DA_EventOath', dict(definition_name='defense_event_oath', kind=unreal.DefenseEventKind.OATH, trigger_after_wave=4,
 title='神殿誓约',safe_description='修复神殿核心：立即恢复核心最大生命的12%。',
 risk_description='接受悬赏：下一波增加一名敌人，成功后全队获得300金币，Boss奖励提高10%。',
 safe_gold=0,safe_experience=0,safe_core_heal_fraction=0.12,risk_budget_bonus=12,risk_gold=300,risk_experience=0,boss_reward_bonus_fraction=0.1))]

def load(path):
    """缺失资产立即失败，不能把空回读当成功。"""
    asset=unreal.load_asset(path)
    if asset is None: raise RuntimeError('Missing '+path)
    return asset

def migrate():
    """只创建缺失事件，不覆盖已有手工调优；对局显式引用正式资产。"""
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    events=[]
    for name,values in DEFINITIONS:
        path=FOLDER+'/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path): asset=load(path)
        else:
            factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.DefenseEventData)
            asset=tools.create_asset(name,FOLDER,unreal.DefenseEventData,factory)
            if asset is None: raise RuntimeError('Create failed '+path)
            for key,value in values.items(): asset.set_editor_property(key,value)
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False): raise RuntimeError('Save failed '+path)
        events.append(asset)
    run=load(ROOT+'/DA_DefenseRun')
    run.set_editor_property('events',events);run.set_editor_property('enable_prototype_events',True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(run,False): raise RuntimeError('Save run failed')
    for name in ['BP_DefenseGameMode','BP_DefensePlayerController','BP_DefenseHUD']:
        bp=load(ROOT+'/'+name);unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if 'BS_UP_TO_DATE' not in str(bp.get_editor_property('status')): raise RuntimeError('Blueprint failed '+name)
        if not unreal.EditorAssetLibrary.save_loaded_asset(bp,False): raise RuntimeError('Save blueprint failed '+name)

def verify():
    """冷读验证参数、对局接线和蓝图状态，不写入资产。"""
    run=load(ROOT+'/DA_DefenseRun'); events=list(run.get_editor_property('events'))
    if len(events)!=2: raise RuntimeError('Expected two authored events')
    rows=[]
    for event,(name,values) in zip(events,DEFINITIONS):
        if event.get_path_name()!=load(FOLDER+'/'+name).get_path_name(): raise RuntimeError('Run reference mismatch')
        for key,expected in values.items():
            actual=event.get_editor_property(key)
            if isinstance(expected,float): valid=abs(float(actual)-expected)<0.00001
            elif isinstance(expected,str): valid=str(actual)==expected
            else: valid=actual==expected
            if not valid: raise RuntimeError('Property mismatch '+name+'.'+key+': '+str(actual))
        rows.append({'path':event.get_path_name(),'id':'DefenseEvent:'+str(event.get_editor_property('definition_name')),
                     'trigger_after_wave':event.get_editor_property('trigger_after_wave')})
    blueprints={}
    for name in ['BP_DefenseGameMode','BP_DefensePlayerController','BP_DefenseHUD']:
        bp=load(ROOT+'/'+name);state=str(bp.get_editor_property('status'))
        if 'BS_UP_TO_DATE' not in state: raise RuntimeError('Cold blueprint not compiled '+name)
        blueprints[name]=state
    report={'read_only':READ_ONLY,'events':rows,'run':run.get_path_name(),'blueprints':blueprints}
    output=Path(unreal.Paths.project_saved_dir())/'DEF-013';output.mkdir(parents=True,exist_ok=True)
    (output/('event-cold-readback.json' if READ_ONLY else 'event-migration.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('DefenseEventsContentVerified '+json.dumps(report,ensure_ascii=False))

if not READ_ONLY: migrate()
verify()
