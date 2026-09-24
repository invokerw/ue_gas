"""通过 Editor API 配置 Shift 多选与 Demo 双英雄；-SelectionReadOnly 仅回读。"""

import json
from pathlib import Path

import unreal


INPUT_DIR = "/Game/Combat/Demo/Input"
ACTION_PATH = INPUT_DIR + "/IA_AddToSelection"
CONTROLLER_PATH = "/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController"
GAME_MODE_PATH = "/Game/Combat/Demo/Framework/BP_CombatDemoGameMode"


def require(path):
    """只操作明确列出的项目资产，缺失时失败。"""
    result = unreal.load_asset(path)
    if result is None:
        raise RuntimeError("Missing asset: " + path)
    return result


def mappings(context):
    """回读完整映射，证明旧按键没有被覆盖。"""
    return [
        {"action": item.action.get_path_name() if item.action else None,
         "key": str(item.key.get_editor_property("key_name"))}
        for item in context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    ]


def main():
    read_only = "-SelectionReadOnly" in unreal.SystemLibrary.get_command_line()
    context = require(INPUT_DIR + "/IMC_Default")
    controller, game_mode = require(CONTROLLER_PATH), require(GAME_MODE_PATH)
    defaults = unreal.get_default_object(controller.generated_class())
    mode_defaults = unreal.get_default_object(game_mode.generated_class())
    before = mappings(context)
    action = unreal.load_asset(ACTION_PATH) if unreal.EditorAssetLibrary.does_asset_exist(ACTION_PATH) else None
    previous = defaults.get_editor_property("add_to_selection_action")
    report = {"read_only": read_only, "before": before,
              "previous_extra_heroes": mode_defaults.get_editor_property("additional_controlled_unit_count")}
    if not read_only:
        conflicts = [entry for entry in before if entry["key"] in ("LeftShift", "RightShift")
                     and entry["action"] != ACTION_PATH + ".IA_AddToSelection"]
        if conflicts or (previous is not None and previous != action):
            raise RuntimeError("Selection input conflicts with existing configuration: " + str(conflicts))
        if action is None:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.InputAction)
            action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "IA_AddToSelection", INPUT_DIR, unreal.InputAction, factory)
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property("action_description", "按住追加框选；点击切换选中组成员")
        for key_name in ("LeftShift", "RightShift"):
            if not any(entry["action"] == action.get_path_name() and entry["key"] == key_name for entry in before):
                key = unreal.Key()
                key.set_editor_property("key_name", key_name)
                context.map_key(action, key)
        defaults.set_editor_property("add_to_selection_action", action)
        mode_defaults.set_editor_property("additional_controlled_unit_count", 1)
        for blueprint in (controller, game_mode):
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            if "ERROR" in str(blueprint.get_editor_property("status")).upper():
                raise RuntimeError("Blueprint compilation failed: " + blueprint.get_path_name())
        for asset in (action, context, controller, game_mode):
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("Save failed: " + asset.get_path_name())
    after = mappings(context)
    assigned = unreal.get_default_object(controller.generated_class()).get_editor_property("add_to_selection_action")
    extra = unreal.get_default_object(game_mode.generated_class()).get_editor_property("additional_controlled_unit_count")
    report.update({"after": after, "extra_heroes": extra,
                   "selection_action": assigned.get_path_name() if assigned else None,
                   "configured": bool(action and assigned == action and extra == 1 and all(
                       any(entry["action"] == action.get_path_name() and entry["key"] == key for entry in after)
                       for key in ("LeftShift", "RightShift")))})
    if not report["configured"] or not all(entry in after for entry in before):
        raise RuntimeError("Selection assets failed readback or changed an old mapping")
    output = Path(unreal.Paths.project_saved_dir()) / "MultiControl"
    output.mkdir(parents=True, exist_ok=True)
    (output / ("AssetReadback.json" if read_only else "AssetMigration.json")).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SelectionAssets " + json.dumps(report, ensure_ascii=False))


main()
