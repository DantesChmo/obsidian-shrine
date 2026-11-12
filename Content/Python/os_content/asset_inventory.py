"""Writes Docs/ASSET_INVENTORY.md: every usable asset of the imported packs in Content/ThirdParty.

The packs are imported on the build machine only, so this listing is how the data side (Game/assets.yaml,
written elsewhere) learns the exact paths, skeletons and animation lengths. Paths are written relative to the
pack folder, the way Game/assets.yaml takes them.
"""

import os

import unreal

THIRD_PARTY = "/Game/ThirdParty"
OUTPUT = "Docs/ASSET_INVENTORY.md"

# Engine class -> asset type of Game/assets.yaml. Other classes (textures, materials, ...) are not listed.
TYPES = {
    "SkeletalMesh": "SkeletalMesh",
    "StaticMesh": "StaticMesh",
    "AnimSequence": "Animation",
    "AnimMontage": "Animation",
    "AnimComposite": "Animation",
    "SoundWave": "Sound",
    "SoundCue": "Sound",
    "MetaSoundSource": "Sound",
    "NiagaraSystem": "Effect",
    "ParticleSystem": "Effect",
}


def _class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except AttributeError:
        return str(asset_data.asset_class)


def _skeleton_name(asset):
    try:
        skeleton = asset.get_editor_property("skeleton")
    except Exception:
        return ""
    return skeleton.get_name() if skeleton else ""


def _details(asset, asset_type):
    if asset_type == "Animation":
        try:
            length = asset.get_play_length()
        except Exception:
            length = None
        return _skeleton_name(asset), "{:.2f} s".format(length) if length is not None else ""
    if asset_type == "SkeletalMesh":
        return _skeleton_name(asset), ""
    return "", ""


def build():
    """Returns notes for the content status."""
    if not unreal.EditorAssetLibrary.does_directory_exist(THIRD_PARTY):
        packs = {}
    else:
        packs = {}
        for path in unreal.EditorAssetLibrary.list_assets(THIRD_PARTY, recursive=True, include_folder=False):
            asset_data = unreal.EditorAssetLibrary.find_asset_data(path)
            asset_type = TYPES.get(_class_name(asset_data))
            if asset_type is None:
                continue
            package = str(asset_data.package_name)
            relative = package[len(THIRD_PARTY) + 1:]
            pack, _, inner = relative.partition("/")
            skeleton, extra = _details(unreal.load_asset(package), asset_type)
            packs.setdefault(pack, []).append((asset_type, inner, _class_name(asset_data), skeleton, extra))

    lines = [
        "# Опись импортированных ассетов",
        "",
        "Файл пишет конвейер контента (`Content/Python/os_content/asset_inventory.py`) на машине сборки — руками не правится.",
        "Пути даны относительно папки набора: так их берёт поле `Path` в `Game/assets.yaml`.",
        "",
    ]
    if not packs:
        lines.append("В `Content/ThirdParty` пока нет наборов.")
    for pack in sorted(packs):
        lines += ["## {}".format(pack), "", "`Root: {}/{}`".format(THIRD_PARTY, pack), "",
                  "| Тип | Path | Класс | Скелет | Длина |", "|---|---|---|---|---|"]
        for row in sorted(packs[pack]):
            lines.append("| {} | `{}` | {} | {} | {} |".format(*row))
        lines.append("")

    target = os.path.join(unreal.Paths.project_dir(), OUTPUT)
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    count = sum(len(rows) for rows in packs.values())
    unreal.log("os_content: listed {} assets of {} packs in {}".format(count, len(packs), OUTPUT))
    return {"notes": ["{} assets of {} packs listed in {}".format(count, len(packs), OUTPUT)]}
