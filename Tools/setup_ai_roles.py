"""通过 UE 原生编辑器 API 创建阶段 B 独立演示；已有资产只回读，保留作者修改。"""

import json
from pathlib import Path
import unreal

FOLDER = "/Game/Combat/Demo/AI/Roles"
MAP = FOLDER + "/L_CombatAI_Roles"


def require(path):
    """精确加载依赖，缺失时停止而非替换资产。"""
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing dependency: " + path)
    return asset


def save(asset):
    """蓝图先编译，检查状态后保存；其他 DataAsset 直接走原生保存。"""
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        if "ERROR" in str(asset.get_editor_property("status")).upper():
            raise RuntimeError("Blueprint compile failed: " + asset.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + asset.get_path_name())


def duplicate(name, source, configure):
    """只创建缺失资产，重复执行不会覆盖已保存的数值或蓝图。"""
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return require(path)
    asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, FOLDER, source)
    configure(asset)
    save(asset)
    return asset


def main():
    agent_source = require("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger")
    original_data = unreal.get_default_object(agent_source.generated_class()).get_editor_property("unit_data")
    units = []
    for suffix, identity, title in [("Guard", "ai_guard_unit", "野怪 AI"), ("Lane", "ai_lane_unit", "小兵 AI")]:
        def configure_data(data):
            data.set_editor_property("definition_name", identity)
            data.set_editor_property("display_name_text", title)
            data.set_editor_property("ai_profile", None)
            stats = data.get_editor_property("base_stats")
            stats.set_editor_property("attack_range", 200.0)
            stats.set_editor_property("attack_damage", 80.0)
            data.set_editor_property("base_stats", stats)
        data = duplicate("DA_AI_" + suffix + "Unit", original_data, configure_data)
        unit = duplicate("BP_AI_" + suffix + "Unit", agent_source,
                         lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property("unit_data", data))
        units.append(unit)

    arena_path = FOLDER + "/BP_AI_RoleArena"
    if unreal.EditorAssetLibrary.does_asset_exist(arena_path):
        arena_bp = require(arena_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.CombatAIRoleDemoArena)
        arena_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_AI_RoleArena", FOLDER, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(arena_bp.generated_class())
        defaults.set_editor_property("guard_profile", require(FOLDER + "/DA_AI_NeutralCamp"))
        defaults.set_editor_property("lane_profile", require(FOLDER + "/DA_AI_Lane"))
        defaults.set_editor_property("guard_class", units[0].generated_class())
        defaults.set_editor_property("lane_class", units[1].generated_class())
        defaults.set_editor_property("target_class", require("/Game/Combat/Demo/Heros/WoodenDummy/BP_WoodenDummy").generated_class())
        save(arena_bp)
    controller = duplicate("BP_AI_RolePlayerController", require("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController"),
                           lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property("command_pawn_class", unreal.CombatAIRoleObserverPawn.static_class()))
    original_mode = require("/Game/Combat/Demo/Framework/BP_CombatDemoGameMode")
    mode = duplicate("BP_AI_RoleGameMode", original_mode,
                     lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property("player_controller_class", controller.generated_class()))
    mode_defaults = unreal.get_default_object(mode.generated_class())
    # 修复本工具初次验证发现的错误字段：SAM 的 DefaultPawnClass 是战斗单位，观察 Pawn 属于 Controller。
    if mode_defaults.get_editor_property("default_pawn_class") == unreal.CombatAIRoleObserverPawn.static_class():
        mode_defaults.set_editor_property("default_pawn_class", unreal.get_default_object(original_mode.generated_class()).get_editor_property("default_pawn_class"))
        mode_defaults.set_editor_property("player_controller_class", controller.generated_class())
        save(mode)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exists = (Path(unreal.Paths.project_content_dir()) / "Combat/Demo/AI/Roles/L_CombatAI_Roles.umap").exists()
    if exists:
        if not level.load_level(MAP):
            raise RuntimeError("Role map readback failed")
    else:
        if not level.new_level_from_template(MAP, "/Game/Combat/Demo/Maps/L_CombatDemo"):
            raise RuntimeError("Role map template creation failed")
        for actor in actors.get_all_level_actors():
            # 删除操作仅作用于本次新建的模板副本；保留原关卡、地板、光照与导航体积。
            if (isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() != "Floor") or isinstance(actor, (unreal.CombatUnitCharacter, unreal.CombatWorldItem)):
                actors.destroy_actor(actor)
            elif isinstance(actor, unreal.PlayerStart):
                actor.set_actor_location(unreal.Vector(650, 0, 110), False, False)
                actor.set_actor_label("AI 角色演示观察起点")
        arena = actors.spawn_actor_from_class(arena_bp.generated_class(), unreal.Vector(0, 0, 110))
        arena.set_actor_label("阶段 B：野怪归位 / 小兵继续路线")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
        # 测试观察器仅在显式命令行开关下运行，不生成任何额外测试战斗单位。
        probe = actors.spawn_actor_from_class(unreal.CombatTestScenarioActor, unreal.Vector(0, 0, 0))
        probe.set_editor_property("auto_spawn_on_begin_play", False)
        probe.set_actor_label("仅显式开关启用的联机验证")
        if not level.save_current_level():
            raise RuntimeError("Role map save failed")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    arenas = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.CombatAIRoleDemoArena)]
    old_targets = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.CombatUnitCharacter)]
    if len(arenas) != 1 or old_targets:
        raise RuntimeError("Role map contains ambiguous preplaced targets or invalid arena count")
    report = {"map": MAP, "arena_count": len(arenas), "preplaced_target_count": len(old_targets),
              "game_mode": world.get_world_settings().get_editor_property("default_game_mode").get_path_name(),
              "units": [unreal.get_default_object(bp.generated_class()).get_editor_property("unit_data").get_path_name() for bp in units],
              "blueprints": [{"path": bp.get_path_name(), "status": str(bp.get_editor_property("status"))} for bp in units + [arena_bp, mode, controller]]}
    for bp in units + [arena_bp, mode, controller]:
        save(bp)
    output = Path(unreal.Paths.project_saved_dir()) / "AI-003"
    output.mkdir(parents=True, exist_ok=True)
    (output / "demo-readback.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("AIRolesDemoReadback " + json.dumps(report, ensure_ascii=False))


main()
