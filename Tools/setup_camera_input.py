"""通过 Unreal Editor API 配置精确的相机输入资产；-CameraReadOnly 只回读。

使用 -run=pythonscript -script=... 运行。仅新增 Follow Action、Space 映射及
Demo Controller 引用；已占用的 Space 拒绝覆盖，所有其他映射保持原样。
"""

import json
from pathlib import Path

import unreal


INPUT_DIR = "/Game/Combat/Demo/Input"
ACTION = INPUT_DIR + "/IA_CameraFollow"
CONTEXT = INPUT_DIR + "/IMC_Default"
CONTROLLER = "/Game/Combat/Demo/Framework/BP_CombatDemoPlayerController"


def require(path):
    """精确读取目标，缺失时停止迁移。"""
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: " + path)
    return asset


def mappings(context):
    """保存全部按键/Action 对，作为不覆盖既有输入的证据。"""
    return [
        {"action": m.action.get_path_name() if m.action else None,
         "key": str(m.key.get_editor_property("key_name"))}
        for m in context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    ]


def main():
    read_only = "-CameraReadOnly" in unreal.SystemLibrary.get_command_line()
    context, blueprint = require(CONTEXT), require(CONTROLLER)
    before = mappings(context)
    defaults = unreal.get_default_object(blueprint.generated_class())
    previous_action = defaults.get_editor_property("camera_follow_action")
    action = unreal.load_asset(ACTION) if unreal.EditorAssetLibrary.does_asset_exist(ACTION) else None
    report = {"read_only": read_only, "before": before,
              "previous_follow_action": previous_action.get_path_name() if previous_action else None}
    if not read_only:
        conflicts = [m for m in before if m["key"] == "SpaceBar" and m["action"] != ACTION + ".IA_CameraFollow"]
        if conflicts:
            raise RuntimeError("Space already mapped: " + str(conflicts))
        if previous_action and previous_action != action:
            raise RuntimeError("Controller already uses another follow action")
        if action is None:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.InputAction)
            action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "IA_CameraFollow", INPUT_DIR, unreal.InputAction, factory)
        if not isinstance(action, unreal.InputAction):
            raise RuntimeError("Unexpected action class")
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property("action_description", "按住跟随当前主控单位，松开停在当前位置")
        if not any(m["action"] == action.get_path_name() and m["key"] == "SpaceBar" for m in before):
            space = unreal.Key()
            space.set_editor_property("key_name", "SpaceBar")
            context.map_key(action, space)
        defaults.set_editor_property("camera_follow_action", action)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        if "ERROR" in str(blueprint.get_editor_property("status")).upper():
            raise RuntimeError("Controller blueprint compile failed")
        for asset in (action, context, blueprint):
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("Save failed: " + asset.get_path_name())
    defaults = unreal.get_default_object(blueprint.generated_class())
    assigned = defaults.get_editor_property("camera_follow_action")
    report.update({"after": mappings(context), "follow_action": assigned.get_path_name() if assigned else None,
                   "blueprint_status": str(blueprint.get_editor_property("status")),
                   "pawn_class": str(defaults.get_editor_property("command_pawn_class")),
                   "action_type": str(action.get_editor_property("value_type")) if action else None})
    if not read_only and not (assigned == action and all(m in report["after"] for m in before)):
        raise RuntimeError("Readback or preserved mappings check failed")
    report["configured"] = bool(action and assigned == action and any(
        m["action"] == action.get_path_name() and m["key"] == "SpaceBar" for m in report["after"]))
    output = Path(unreal.Paths.project_saved_dir()) / "CameraValidation"
    output.mkdir(parents=True, exist_ok=True)
    (output / ("InputReadback.json" if read_only else "InputMigration.json")).write_text(
        json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("CameraInput " + json.dumps(report, ensure_ascii=False))


main()
