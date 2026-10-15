"""通过 UE Editor API 创建防守灰盒；只创建缺失目标，重复执行只回读作者资产。"""
import json
from pathlib import Path
import unreal

FOLDER = "/Game/Combat/Demo/Defense"
MAP = FOLDER + "/L_Defense"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def require(path):
    """精确加载依赖，缺失即停止。"""
    value = unreal.load_asset(path)
    if value is None:
        raise RuntimeError("Missing dependency: " + path)
    return value


def save(asset):
    """蓝图先编译再保存，失败不写成功记录。"""
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        if "ERROR" in str(asset.get_editor_property("status")).upper():
            raise RuntimeError("Compile failed: " + asset.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + asset.get_path_name())


def create(name, cls, configure, source=None, parent=None):
    """限定新目录，新建后配置保存；已有资产不覆盖。"""
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return require(path)
    if source:
        obj = TOOLS.duplicate_asset(name, FOLDER, source)
    else:
        factory = unreal.BlueprintFactory() if parent else unreal.DataAssetFactory()
        factory.set_editor_property("parent_class" if parent else "data_asset_class", parent or cls)
        obj = TOOLS.create_asset(name, FOLDER, unreal.Blueprint if parent else cls, factory)
    if obj is None:
        raise RuntimeError("Creation failed: " + path)
    configure(obj)
    save(obj)
    return obj


def defaults(bp):
    return unreal.get_default_object(bp.generated_class())


def identity(obj, name, title):
    obj.set_editor_property("definition_name", name)
    obj.set_editor_property("display_name_text", title)


def main():
    source_hero = require("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger")
    source_mode = require("/Game/Combat/Demo/Framework/BP_CombatDemoGameMode")
    source_controller = require("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController")
    original_hero_data = defaults(source_hero).get_editor_property("unit_data")

    def hero_data(data):
        identity(data, "defense_ranger", "神殿游侠")
        data.set_editor_property("experience_reward", 0)
        data.set_editor_property("gold_reward", 0)
        data.set_editor_property("initial_items", [])
        data.set_editor_property("ai_profile", None)
        stats = data.get_editor_property("base_stats")
        for field, value in {"max_health": 900.0, "attack_damage": 100.0, "move_speed": 600.0,
                             "attack_range": 650.0}.items():
            stats.set_editor_property(field, value)
        data.set_editor_property("base_stats", stats)
    hero_def = create("DA_DefenseHero", unreal.CombatUnitData, hero_data, source=original_hero_data)
    hero = create("BP_DefenseHero", unreal.Blueprint,
                  lambda bp: defaults(bp).set_editor_property("unit_data", hero_def), source=source_hero)

    def unit_data(data, name, title, hp, damage, speed, team):
        identity(data, name, title)
        data.set_editor_property("experience_reward", 0)
        data.set_editor_property("gold_reward", 0)
        data.set_editor_property("initial_team_id", unreal.CombatTeamId(value=team))
        stats = data.get_editor_property("base_stats")
        for field, value in {"max_health": hp, "attack_damage": damage, "move_speed": speed,
                             "attack_range": 150.0, "base_attack_time": 1.7}.items():
            stats.set_editor_property(field, value)
        data.set_editor_property("base_stats", stats)
    core = create("DA_DefenseCore", unreal.CombatUnitData,
                  lambda d: unit_data(d,"defense_core","守护神殿",5000.0,0.0,0.0,1))
    melee = create("DA_DefenseInvader", unreal.CombatUnitData,
                   lambda d: unit_data(d,"defense_invader","侵袭者",160.0,12.0,260.0,2))
    boss = create("DA_DefenseBoss", unreal.CombatUnitData,
                  lambda d: unit_data(d,"defense_boss","裂隙统领",2400.0,40.0,220.0,2))

    def enemy_blueprint(bp, definition, scale):
        defaults(bp).set_editor_property("unit_data",definition)
        defaults(bp).get_editor_property("body").set_relative_scale3d(unreal.Vector(scale,scale,scale*1.8))
        defaults(bp).get_editor_property("combat_overhead_widget_component").set_editor_property("widget_class",
            require("/Game/Combat/Demo/UI/WBP_CombatOverhead").generated_class())
    invader_bp=create("BP_DefenseInvader",unreal.Blueprint,
                      lambda b: enemy_blueprint(b,melee,0.65),parent=unreal.DefenseEnemy)
    boss_bp=create("BP_DefenseBoss",unreal.Blueprint,
                   lambda b: enemy_blueprint(b,boss,1.1),parent=unreal.DefenseEnemy)

    buffs=[]
    for name, title, source in [("armor","坚守", "/Game/Combat/Demo/Items/DA_item_armor_passive"),
                                ("speed","疾行", "/Game/Combat/Demo/Items/DA_item_boots_passive")]:
        def configure(d, name=name, title=title):
            identity(d,"defense_augment_"+name,title)
            d.set_editor_property("remove_on_death",False)
            d.set_editor_property("duration",0.0)
            d.set_editor_property("max_stacks",1)
        buffs.append(create("DA_DefenseAugment_"+name,unreal.CombatModifierData,configure,source=require(source)))

    waves=[]
    for index in range(6):
        def configure(w, index=index):
            identity(w,"defense_wave_"+str(index+1),"第 "+str(index+1)+" 波")
            w.set_editor_property("boss_wave",index==5)
            w.set_editor_property("max_duration",180.0 if index==5 else 90.0)
            w.set_editor_property("gold_reward",[100,130,160,190,240,300][index])
            w.set_editor_property("experience_reward",[200,300,400,500,600,0][index])
            if index in (0,2): w.set_editor_property("fixed_augment",buffs[0 if index==0 else 1])
            groups=[]
            for route in range(1 if index==5 else 2):
                g=unreal.DefenseSpawnGroup()
                g.set_editor_property("unit_class",(boss_bp if index==5 else invader_bp).generated_class())
                g.set_editor_property("unit_data",boss if index==5 else melee)
                g.set_editor_property("count",1 if index==5 else index+2)
                g.set_editor_property("route_index",route)
                g.set_editor_property("spawn_interval",1.2)
                g.set_editor_property("boss",index==5)
                groups.append(g)
            w.set_editor_property("spawn_groups",groups)
        waves.append(create("DA_DefenseWave_"+str(index+1),unreal.DefenseWaveData,configure))

    def run_data(r):
        identity(r,"defense_phase1","守护神殿 · 防守原型")
        r.set_editor_property("waves",waves)
        r.set_editor_property("objective_data",core)
        r.set_editor_property("objective_location",unreal.Vector(700,0,110))
        r.set_editor_property("respawn_location",unreal.Vector(1200,0,110))
        r.set_editor_property("preparation_duration",40.0)
        r.set_editor_property("intermission_duration",8.0)
        routes=[]
        for label,y in [("west",-800),("east",800)]:
            route=unreal.DefenseRoute()
            route.set_editor_property("route_name",label)
            route.set_editor_property("spawn_location",unreal.Vector(-1700,y,110))
            route.set_editor_property("waypoints",[unreal.Vector(-500,y,0),unreal.Vector(200,y*0.5,0)])
            routes.append(route)
        r.set_editor_property("routes",routes)
    run=create("DA_DefenseRun",unreal.DefenseRunData,run_data)

    def economy_data(e):
        identity(e,"defense_economy","防守金币")
        e.set_editor_property("starting_gold",250)
        e.set_editor_property("passive_gold_per_minute",0)
    economy=create("DA_DefenseEconomy",unreal.CombatEconomyData,economy_data,
                   source=defaults(source_mode).get_editor_property("economy_data"))
    def hud_config(bp):
        original=unreal.get_default_object(defaults(source_mode).get_editor_property("hud_class"))
        for name in ["widget_class","log_widget_class"]:
            defaults(bp).set_editor_property(name,original.get_editor_property(name))
        defaults(bp).set_editor_property("shop_widget_class",None)
    hud=create("BP_DefenseHUD",unreal.Blueprint,hud_config,parent=unreal.DefenseHUD)
    def mode_config(bp):
        cdo=defaults(bp)
        for name,value in {"run_data":run,"default_pawn_class":hero.generated_class(),
                           "player_controller_class":source_controller.generated_class(),
                           "hud_class":hud.generated_class(),"additional_controlled_unit_count":0,
                           "economy_data":economy,"shop_data":defaults(source_mode).get_editor_property("shop_data")}.items():
            cdo.set_editor_property(name,value)
    mode=create("BP_DefenseGameMode",unreal.Blueprint,mode_config,parent=unreal.DefenseGameMode)
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_file=Path(unreal.Paths.project_content_dir())/"Combat/Demo/Defense/L_Defense.umap"
    if map_file.exists():
        if not level.load_level(MAP): raise RuntimeError("Defense map load failed")
    else:
        if not level.new_level_from_template(MAP,"/Game/Combat/Demo/Maps/L_CombatDemo"):
            raise RuntimeError("Defense map creation failed")
        for actor in actors.get_all_level_actors():
            if isinstance(actor,(unreal.CombatUnitCharacter,unreal.CombatWorldItem,unreal.CombatTestScenarioActor)):
                actors.destroy_actor(actor)
            elif isinstance(actor,unreal.StaticMeshActor) and actor.get_actor_label()!="Floor":
                actors.destroy_actor(actor)
            elif isinstance(actor,unreal.PlayerStart):
                actor.set_actor_location(unreal.Vector(1200,-200,110),False,False)
        # 原型路面是无碰撞标识，不改变可行走地板与导航。
        cube=require("/Engine/BasicShapes/Cube")
        for label,y in [("西路",-800),("东路",800)]:
            marker=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-700,y,4))
            marker.set_actor_label("防守路线_"+label)
            mesh=marker.static_mesh_component
            mesh.set_static_mesh(cube)
            mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            marker.set_actor_scale3d(unreal.Vector(22,1.8,0.04))
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode",mode.generated_class())
        probe=actors.spawn_actor_from_class(unreal.CombatDefenseNetworkScenario,unreal.Vector(0,0,0))
        probe.set_actor_label("仅 DefenseSmoke 开关启用的验证")
        if not level.save_current_level(): raise RuntimeError("Save map failed")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    report={"map":MAP,"mode":world.get_world_settings().get_editor_property("default_game_mode").get_path_name(),
            "waves":[w.get_path_name() for w in run.get_editor_property("waves")],
            "routes":len(run.get_editor_property("routes")),
            "blueprints":[{"path":b.get_path_name(),"status":str(b.get_editor_property("status"))}
                          for b in [hero,invader_bp,boss_bp,hud,mode]],
            "spawn_groups":[[{"class":g.get_editor_property("unit_class").get_path_name(),
                               "definition":"CombatUnit:"+str(g.get_editor_property("unit_data").get_editor_property("definition_name")),
                               "count":g.get_editor_property("count"),"route":g.get_editor_property("route_index")}
                              for g in w.get_editor_property("spawn_groups")] for w in waves],
            "actors":[{"label":a.get_actor_label(),"class":a.get_class().get_name(),
                       "location":str(a.get_actor_location()),"bounds":str(a.get_actor_bounds(False))} for a in actors.get_all_level_actors()],
            "buffs":[{"path":b.get_path_name(),"changes":str(b.get_editor_property("attribute_changes"))} for b in buffs]}
    out=Path(unreal.Paths.project_saved_dir())/"DEF-002"
    out.mkdir(parents=True,exist_ok=True)
    (out/"assets-readback.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    unreal.log("DefenseAssetsReadback "+json.dumps(report,ensure_ascii=False))


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
