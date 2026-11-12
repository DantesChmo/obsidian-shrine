"""Makes sure a packaged game contains every asset the data names.

The data refers to assets by path strings, which the cooker does not follow. Tools/build_data.py lists the
assets in use in Content/AssetData/cook.json; this builder puts them into a PrimaryAssetLabel that is always
cooked. Assets that are not imported yet are skipped and reported: the game falls back to placeholders.
"""

import json
import os

import unreal

LABEL_DIR = "/Game/Generated"
LABEL_NAME = "PAL_GameAssets"


def _cook_list():
    path = os.path.join(unreal.Paths.project_content_dir(), "AssetData", "cook.json")
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle).get("Assets", [])


def _label():
    path = "{}/{}".format(LABEL_DIR, LABEL_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    unreal.EditorAssetLibrary.make_directory(LABEL_DIR)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.PrimaryAssetLabel)
    label = unreal.AssetToolsHelpers.get_asset_tools().create_asset(LABEL_NAME, LABEL_DIR, unreal.PrimaryAssetLabel, factory)
    if label is None:
        raise RuntimeError("Could not create {}".format(path))
    return label


def build():
    """Returns notes for the content status."""
    found, missing = [], []
    for object_path in _cook_list():
        package = object_path.split(".", 1)[0]
        asset = unreal.load_asset(package) if unreal.EditorAssetLibrary.does_asset_exist(package) else None
        (found if asset else missing).append(asset or object_path)

    label = _label()
    rules = unreal.PrimaryAssetRules()
    rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
    label.set_editor_property("rules", rules)
    label.set_editor_property("label_assets_in_my_directory", False)
    label.set_editor_property("explicit_assets", found)
    unreal.EditorAssetLibrary.save_loaded_asset(label, False)

    unreal.log("os_content: {} assets in {}, {} not imported".format(len(found), LABEL_NAME, len(missing)))
    notes = ["{} assets marked for packaging".format(len(found))]
    notes += ["not imported (placeholder in game): {}".format(path) for path in missing]
    return {"notes": notes}
