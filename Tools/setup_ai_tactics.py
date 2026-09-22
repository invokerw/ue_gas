"""通过 UE 原生编辑器 API 创建阶段 C 战术单位、演示蓝图与独立地图。"""

import json
from pathlib import Path

import unreal


FOLDER = "/Game/Combat/Demo/AI/Tactics"
MAP = FOLDER + "/L_CombatAI_Tactics"


def require(path):
    """精确加载依赖，缺失时停止而不是静默替换内容。"""
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing dependency: " + path)
    return asset


def save(asset):
    """蓝图先编译并检查状态，再用原生资产 API 保存。"""
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        if "ERROR" in str(asset.get_editor_property("status")).upper():
            raise RuntimeError("Blueprint compile failed: " + asset.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + asset.get_path_name())


def duplicate(name, source, configure):
    """只创建缺失资产；已有资产回读且不覆盖作者后续调整。"""
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return require(path)
    asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, FOLDER, source)
    if asset is None:
        raise RuntimeError("Duplicate failed: " + path)
    configure(asset)
    save(asset)
    return asset


def main():
    hero_profile = require(FOLDER + "/DA_AI_HeroTactics")
    ranged_profile = require(FOLDER + "/DA_AI_RangedGuard")
    if len(hero_profile.get_editor_property("ability_usage_rules")) != 1:
        raise RuntimeError("Hero profile must contain the commandlet-authored active heal rule")

    source_hero = require("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger")
    source_data = unreal.get_default_object(source_hero.generated_class()).get_editor_property("unit_data")
    source_set = require("/Game/Combat/Demo/Indicators/DA_IndicatorAbilitySet")

    def configure_set(asset):
        asset.set_editor_property("definition_name", "ai_tactics_hero_ability_set")
        asset.set_editor_property("display_name_text", "AI 战术 Hero 技能组")

    hero_set = duplicate("DA_AI_HeroAbilitySet", source_set, configure_set)

    def configure_hero_data(data):
        data.set_editor_property("definition_name", "ai_tactical_hero_unit")
        data.set_editor_property("display_name_text", "战术 Hero Bot")
        data.set_editor_property("ai_profile", hero_profile)
        data.set_editor_property("ability_sets", [hero_set])
        stats = data.get_editor_property("base_stats")
        stats.set_editor_property("attack_damage", 35.0)
        stats.set_editor_property("attack_range", 625.0)
        data.set_editor_property("base_stats", stats)

    def configure_ranged_data(data):
        data.set_editor_property("definition_name", "ai_tactical_ranged_unit")
        data.set_editor_property("display_name_text", "EQS 远程守卫")
        data.set_editor_property("ai_profile", ranged_profile)
        data.set_editor_property("ability_sets", [])
        stats = data.get_editor_property("base_stats")
        stats.set_editor_property("attack_damage", 35.0)
        stats.set_editor_property("attack_range", 700.0)
        data.set_editor_property("base_stats", stats)

    hero_data = duplicate("DA_AI_HeroUnit", source_data, configure_hero_data)
    ranged_data = duplicate("DA_AI_RangedUnit", source_data, configure_ranged_data)
    hero_bp = duplicate("BP_AI_HeroUnit", source_hero,
                        lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property("unit_data", hero_data))
    ranged_bp = duplicate("BP_AI_RangedUnit", source_hero,
                          lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property("unit_data", ranged_data))

    arena_path = FOLDER + "/BP_AI_TacticsArena"
    if unreal.EditorAssetLibrary.does_asset_exist(arena_path):
        arena_bp = require(arena_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.CombatAITacticalDemoArena)
        arena_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_AI_TacticsArena", FOLDER, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(arena_bp.generated_class())
        defaults.set_editor_property("hero_profile", hero_profile)
        defaults.set_editor_property("ranged_profile", ranged_profile)
        defaults.set_editor_property("hero_class", hero_bp.generated_class())
        defaults.set_editor_property("ranged_class", ranged_bp.generated_class())
        defaults.set_editor_property(
            "target_class", require("/Game/Combat/Demo/Heros/WoodenDummy/BP_WoodenDummy").generated_class())
        save(arena_bp)

    controller = duplicate(
        "BP_AI_TacticsPlayerController",
        require("/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController"),
        lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property(
            "command_pawn_class", unreal.CombatAIRoleObserverPawn.static_class()))
    source_mode = require("/Game/Combat/Demo/Framework/BP_CombatDemoGameMode")
    mode = duplicate(
        "BP_AI_TacticsGameMode", source_mode,
        lambda bp: unreal.get_default_object(bp.generated_class()).set_editor_property(
            "player_controller_class", controller.generated_class()))

    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_file = Path(unreal.Paths.project_content_dir()) / "Combat/Demo/AI/Tactics/L_CombatAI_Tactics.umap"
    if map_file.exists():
        if not level.load_level(MAP):
            raise RuntimeError("Tactics map readback failed")
    else:
        if not level.new_level_from_template(MAP, "/Game/Combat/Demo/Maps/L_CombatDemo"):
            raise RuntimeError("Tactics map template creation failed")
        for actor in actors.get_all_level_actors():
            remove = ((isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() != "Floor")
                      or isinstance(actor, (unreal.CombatUnitCharacter, unreal.CombatWorldItem,
                                            unreal.CombatAIDemoArena, unreal.CombatAIRoleDemoArena,
                                            unreal.CombatTestScenarioActor)))
            if remove:
                actors.destroy_actor(actor)
            elif isinstance(actor, unreal.PlayerStart):
                actor.set_actor_location(unreal.Vector(500, 0, 110), False, False)
                actor.set_actor_label("AI 战术演示观察起点")
        arena = actors.spawn_actor_from_class(arena_bp.generated_class(), unreal.Vector(0, 0, 110))
        arena.set_actor_label("阶段 C：Hero 边界施法 / 远程 EQS 站位")
        probe = actors.spawn_actor_from_class(unreal.CombatTestScenarioActor, unreal.Vector(0, 0, 0))
        probe.set_editor_property("auto_spawn_on_begin_play", False)
        probe.set_actor_label("仅显式开关启用的战术联机验证")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
        if not level.save_current_level():
            raise RuntimeError("Tactics map save failed")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    arenas = [actor for actor in actors.get_all_level_actors()
              if isinstance(actor, unreal.CombatAITacticalDemoArena)]
    preplaced_units = [actor for actor in actors.get_all_level_actors()
                       if isinstance(actor, unreal.CombatUnitCharacter)]
    if len(arenas) != 1 or preplaced_units:
        raise RuntimeError("Tactics map contains ambiguous units or arena count")

    assets = [hero_set, hero_data, ranged_data, hero_bp, ranged_bp, arena_bp, mode, controller]
    for asset in assets:
        save(asset)
    report = {
        "map": MAP,
        "arena_count": len(arenas),
        "preplaced_unit_count": len(preplaced_units),
        "game_mode": world.get_world_settings().get_editor_property("default_game_mode").get_path_name(),
        "hero_profile": hero_profile.get_path_name(),
        "ranged_profile": ranged_profile.get_path_name(),
        "hero_ability_set": hero_set.get_path_name(),
        "unit_data": [hero_data.get_path_name(), ranged_data.get_path_name()],
        "blueprints": [{"path": bp.get_path_name(), "status": str(bp.get_editor_property("status"))}
                       for bp in [hero_bp, ranged_bp, arena_bp, mode, controller]],
    }
    output = Path(unreal.Paths.project_saved_dir()) / "AI-004"
    output.mkdir(parents=True, exist_ok=True)
    (output / "tactics-demo-readback.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("AITacticsDemoReadback " + json.dumps(report, ensure_ascii=False))


main()
