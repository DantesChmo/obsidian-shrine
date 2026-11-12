"""Asset bindings: Game/assets.yaml gives every imported asset a logical name.

Definitions never contain asset paths. A field that takes an asset (marked `asset: <Type>` in
Game/schema/fields.yaml) holds a logical name, and the compiler writes the full object path into the
generated JSON. So moving or swapping a pack is a change to one line of assets.yaml, and a typo in a name is a
compile error instead of a missing animation in the game.

    Packs:   where an imported pack lives:       { Id: greystone, Root: /Game/ThirdParty/ParagonGreystone }
    Assets:  one asset of a pack, by its type:   { Id: greystone_body, Type: SkeletalMesh, Pack: greystone,
                                                   Path: Characters/Heroes/Greystone/Meshes/Greystone }

The assets actually referenced are listed in Content/AssetData/cook.json; the content pipeline turns that
list into a PrimaryAssetLabel, so a packaged game contains them although the data names them only as text.
"""

import os
import re

from .schema import Field, Struct, Type
from .source import LMap, LSeq, Problem, SourceError, load_file

ASSETS_PATH = "Game/assets.yaml"
COOK_LIST_PATH = "Content/AssetData/cook.json"

# Asset type -> what the engine loads it as (shown in the reference).
TYPES = {
    "SkeletalMesh": "USkeletalMesh",
    "StaticMesh": "UStaticMesh",
    "Animation": "UAnimSequenceBase",
    "Sound": "USoundBase",
    "Effect": "UNiagaraSystem or UParticleSystem",
}

STRUCTS = [
    Struct("AssetPack", [
        Field("Id", Type("name"), required=True),
        Field("Root", Type("string"), required=True),
    ]),
    Struct("AssetEntry", [
        Field("Id", Type("name"), required=True),
        Field("Type", Type("enum", ref="AssetType"), required=True),
        Field("Pack", Type("name"), required=True),
        Field("Path", Type("string"), required=True),
    ]),
]

# (heading, struct, where it is written, intro) - appended to the generated reference
REFERENCE_SECTIONS = [
    ("Набор ассетов", "AssetPack", "`Game/assets.yaml`, секция `Packs`",
     "Импортированный набор (например, с Fab) и папка, куда он положен в проекте."),
    ("Ассет", "AssetEntry", "`Game/assets.yaml`, секция `Assets`",
     "Логическое имя одного ассета. Поля данных, которые берут ассет (меш, анимация, звук, эффект), "
     "пишутся этим именем, а не путём."),
]

_ROOT_PATTERN = re.compile(r"/Game(/[A-Za-z0-9_]+)+")
_PATH_PATTERN = re.compile(r"[A-Za-z0-9_]+(/[A-Za-z0-9_]+)*")


def add_to_schema(schema):
    schema.enums["AssetType"] = list(TYPES)
    for struct in STRUCTS:
        schema.structs[struct.name] = struct


class AssetTable:
    """Logical names -> object paths, plus which of them the data actually uses."""

    def __init__(self, problems):
        self.problems = problems
        self.entries = {}   # id -> (type, object path)
        self.used = set()

    def resolve(self, value, asset_type, file, line, label, source=None):
        """value is a name, a list of names or a map of names; empty means "no asset". Returns the same shape
        with object paths, reporting unknown names and names of the wrong type. source: the value as written
        (LSeq / LMap), for the line of each item."""
        def line_of(key):
            return source.line_of(key) if hasattr(source, "line_of") else line

        if isinstance(value, list):
            return [self.resolve(item, asset_type, file, line_of(index), label) for index, item in enumerate(value)]
        if isinstance(value, dict):
            return {key: self.resolve(item, asset_type, file, line_of(key), "{}.{}".format(label, key)) for key, item in value.items()}
        if not value:
            return value

        entry = self.entries.get(value)
        if entry is None:
            self.problems.append(Problem(file, line, "{} names asset '{}', which is not in {}".format(label, value, ASSETS_PATH)))
            return ""
        if entry[0] != asset_type:
            self.problems.append(Problem(file, line, "{} needs a {} asset, but '{}' is a {}".format(label, asset_type, value, entry[0])))
            return ""
        self.used.add(value)
        return entry[1]

    def cook_list(self):
        return sorted(self.entries[name][1] for name in self.used)


def object_path(root, path):
    """/Game/Pack + Folder/Name -> /Game/Pack/Folder/Name.Name"""
    package = "{}/{}".format(root.rstrip("/"), path)
    return "{}.{}".format(package, path.rsplit("/", 1)[-1])


def load(root, checker, problems):
    """Reads Game/assets.yaml (optional) into an AssetTable."""
    table = AssetTable(problems)
    full = os.path.join(root, ASSETS_PATH)
    if not os.path.exists(full):
        return table
    try:
        document = load_file(full, ASSETS_PATH)
    except SourceError as error:
        problems.append(error.problem)
        return table
    if not isinstance(document, LMap):
        problems.append(Problem(ASSETS_PATH, 1, "{} must have the sections Packs and Assets".format(ASSETS_PATH)))
        return table

    def report(line, message):
        problems.append(Problem(ASSETS_PATH, line, message))

    for key in document:
        if key not in ("Packs", "Assets"):
            report(document.line_of(key), "unknown section '{}'; sections are Packs, Assets".format(key))

    def items(section):
        value = document.get(section)
        if value is None:
            return []
        if not isinstance(value, LSeq):
            report(document.line_of(section), "'{}' must be a list".format(section))
            return []
        return [(item, value.line_of(index)) for index, item in enumerate(value)]

    packs = {}
    for item, line in items("Packs"):
        pack = checker.convert_struct(item, "AssetPack", ASSETS_PATH, line, "pack")
        if "Id" not in pack or "Root" not in pack:
            continue
        if pack["Id"] in packs:
            report(line, "pack '{}' is defined twice".format(pack["Id"]))
            continue
        if not _ROOT_PATTERN.fullmatch(pack["Root"]):
            report(line, "pack '{}': Root must be a content folder like /Game/ThirdParty/Pack, got '{}'".format(pack["Id"], pack["Root"]))
            continue
        packs[pack["Id"]] = pack["Root"]

    for item, line in items("Assets"):
        asset = checker.convert_struct(item, "AssetEntry", ASSETS_PATH, line, "asset")
        if not all(key in asset for key in ("Id", "Type", "Pack", "Path")):
            continue
        if asset["Id"] in table.entries:
            report(line, "asset '{}' is defined twice".format(asset["Id"]))
            continue
        if asset["Pack"] not in packs:
            report(line, "asset '{}' is in pack '{}', which Packs does not define".format(asset["Id"], asset["Pack"]))
            continue
        if not _PATH_PATTERN.fullmatch(asset["Path"]):
            report(line, "asset '{}': Path is the folders and name inside the pack, like Animations/Attack_A "
                         "(no leading slash, no extension), got '{}'".format(asset["Id"], asset["Path"]))
            continue
        table.entries[asset["Id"]] = (asset["Type"], object_path(packs[asset["Pack"]], asset["Path"]))
    return table
