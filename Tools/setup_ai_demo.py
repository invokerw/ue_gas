"""使用 UE 编辑器 API 创建独立 AI 演示蓝图/地图；重复执行只回读，不覆盖作者修改。"""

import json
from pathlib import Path

import unreal


FOLDER = "/Game/Combat/Demo/AI"
BLUEPRINT = FOLDER + "/BP_CombatAIDemoArena"
MAP = FOLDER + "/L_CombatAI"


def require(path):
    """加载精确依赖，缺失即失败，不自动替换其他资产。"""
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing dependency: " + path)
    return asset


def main():
    profile = require(FOLDER + "/DA_CombatAI_Basic")
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = unreal.EditorAssetLibrary.does_asset_exist(BLUEPRINT)
    if existing:
        blueprint = require(BLUEPRINT)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.CombatAIDemoArena)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_CombatAIDemoArena", FOLDER, unreal.Blueprint, factory)
        defaults = unreal.get_default_object(blueprint.generated_class())
        defaults.set_editor_property("profile", profile)
        defaults.set_editor_property("agent_class", require("/Game/Combat/Demo/Heros/DrowRanger/BP_DrowRanger").generated_class())
        defaults.set_editor_property("target_class", require("/Game/Combat/Demo/Heros/WoodenDummy/BP_WoodenDummy").generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        if "ERROR" in str(blueprint.get_editor_property("status")).upper():
            raise RuntimeError("AI arena Blueprint compile failed")
        if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError("AI arena Blueprint save failed")
    map_file = Path(unreal.Paths.project_content_dir()) / "Combat/Demo/AI/L_CombatAI.umap"
    if map_file.exists():
        if not level.load_level(MAP):
            raise RuntimeError("AI demo map readback failed")
    else:
        # 原生模板入口处理 World Partition 外部 Actor 重映射，旧地图不保存修改。
        if not level.new_level_from_template(MAP, "/Game/Combat/Demo/Maps/L_CombatDemo"):
            raise RuntimeError("AI demo map creation from template failed")
        arena = actors.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(-450, -450, 110))
        arena.set_actor_label("StateTree AI 演示：攻击 / 返回")
        if not level.save_current_level():
            raise RuntimeError("AI demo map save failed")
    defaults = unreal.get_default_object(blueprint.generated_class())
    arenas = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.CombatAIDemoArena)]
    report = {
        "blueprint": blueprint.get_path_name(),
        "blueprint_status": str(blueprint.get_editor_property("status")),
        "profile": defaults.get_editor_property("profile").get_path_name(),
        "map": MAP,
        "arena_count": len(arenas),
        "root_tree": profile.get_editor_property("root_tree").get_path_name(),
    }
    if len(arenas) != 1 or report["profile"] != profile.get_path_name():
        raise RuntimeError("AI demo readback mismatch: " + str(report))
    directory = Path(unreal.Paths.project_saved_dir()) / "AI-002"
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "demo-readback.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("AIDemoReadback " + json.dumps(report, ensure_ascii=False))


main()
