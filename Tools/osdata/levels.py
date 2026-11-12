"""Level manifests: Game/levels/*.yaml -> a flat list of things to place, applied in the editor by
Content/Python/os_content/level_builder.py.

A manifest deliberately knows only the handful of things a level is made of here: boxes, a player start,
shrines, enemies and a lighting preset. No arbitrary actor classes, no per-light settings, no loops or
inheritance; a mechanic gets a manifest field when the game gets the code for it.
"""

import os
import re

from .schema import Field, Struct, Type
from .shape import ShapeChecker
from .source import LMap, LSeq, Problem, SourceError, load_file

LIGHTING_PATH = "Game/lighting.yaml"

_VECTOR = Type("vector")
_FLOAT = Type("float")
_NAME = Type("name")


def _struct(name, *fields):
    return Struct(name, list(fields))


STRUCTS = [
    _struct("LightingPreset",
            Field("Id", _NAME, required=True),
            Field("SunPitch", _FLOAT, default=-40.0),
            Field("SunYaw", _FLOAT, default=30.0),
            Field("Fog", Type("bool"), default=True)),
    _struct("LevelStart",
            Field("At", _VECTOR, required=True),
            Field("Yaw", _FLOAT, default=0.0)),
    _struct("LevelBlock",
            Field("Name", _NAME, required=True),
            Field("At", _VECTOR, required=True),
            Field("Size", _VECTOR, required=True),
            Field("Rotation", _VECTOR, default=[0, 0, 0])),
    _struct("LevelShrine",
            Field("Id", _NAME, required=True),
            Field("At", _VECTOR, required=True),
            Field("Yaw", _FLOAT, default=0.0),
            Field("InteractRadius", _FLOAT, default=0.0),
            Field("RespawnDistance", _FLOAT, default=180.0),
            Field("RespawnHeight", _FLOAT, default=100.0),
            Field("Name", _NAME)),
    _struct("LevelEnemy",
            Field("Character", _NAME, required=True),
            Field("At", _VECTOR, required=True),
            Field("Yaw", _FLOAT, default=0.0),
            Field("PersistentId", _NAME),
            Field("Name", _NAME)),
    _struct("Level",
            Field("Map", Type("string"), required=True),
            Field("Lighting", _NAME, required=True),
            Field("PlayerStart", Type("struct", ref="LevelStart"), required=True),
            Field("Blocks", Type("array", item=Type("struct", ref="LevelBlock"))),
            Field("Shrines", Type("array", item=Type("struct", ref="LevelShrine"))),
            Field("Enemies", Type("array", item=Type("struct", ref="LevelEnemy")))),
]

STRUCTS_BY_NAME = {struct.name: struct for struct in STRUCTS}

# (heading, struct, where it is written, intro) - appended to the generated reference
REFERENCE_SECTIONS = [
    ("Уровень", "Level", "`Game/levels/<имя>.yaml`, один файл — один уровень",
     "Из чего состоит уровень. Координаты — `[x, y, z]` в сантиметрах, `z` — вверх. "
     "`Yaw` — куда объект смотрит: 0 — вдоль `+x`, 90 — вдоль `+y`."),
    ("Старт игрока", "LevelStart", "поле `PlayerStart` уровня",
     "Где игрок появляется, пока не отдохнул ни у одного святилища."),
    ("Блок", "LevelBlock", "внутри `Blocks` уровня",
     "Прямоугольная коробка: пол, стена, колонна, пандус. Из них складывается серая геометрия уровня."),
    ("Святилище", "LevelShrine", "внутри `Shrines` уровня",
     "Точка отдыха и возрождения."),
    ("Враг на уровне", "LevelEnemy", "внутри `Enemies` уровня",
     "Ставит на уровень персонажа, описанного в `Game/data`."),
    ("Освещение", "LightingPreset", "`Game/lighting.yaml`, секция `Presets`",
     "Готовые наборы освещения; уровень выбирает один по имени."),
]

_MAP_PATTERN = re.compile(r"/Game/Maps/[A-Za-z][A-Za-z0-9_]*")
_BLOCK_UNIT = 100.0  # the engine's basic cube is 100 cm


def add_to_schema(schema):
    for struct in STRUCTS:
        schema.structs[struct.name] = struct


def _load_presets(root, checker, problems):
    presets = {}
    path = os.path.join(root, LIGHTING_PATH)
    if not os.path.exists(path):
        return presets
    try:
        document = load_file(path, LIGHTING_PATH)
    except SourceError as error:
        problems.append(error.problem)
        return presets

    for key in document:
        if key != "Presets":
            problems.append(Problem(LIGHTING_PATH, document.line_of(key), "unknown section '{}'; the only section is Presets".format(key)))
    items = document.get("Presets")
    if not isinstance(items, LSeq):
        problems.append(Problem(LIGHTING_PATH, document.line, "'Presets' must be a list"))
        return presets

    for index, item in enumerate(items):
        line = items.line_of(index)
        converted = checker.convert_struct(item, "LightingPreset", LIGHTING_PATH, line, "lighting preset #{}".format(index + 1))
        preset_id = converted.get("Id")
        if not preset_id:
            continue
        if preset_id in presets:
            problems.append(Problem(LIGHTING_PATH, line, "lighting preset '{}' is defined twice".format(preset_id)))
            continue
        defaults = {field.cpp_name: field.default for field in checker.schema.structs["LightingPreset"].fields.values()}
        defaults.update(converted)
        defaults.pop("Id")
        presets[preset_id] = defaults
    return presets


def _rotation(yaw):
    """(pitch, yaw, roll) as the level builder expects."""
    return [0, yaw, 0]


def _resolve(level, source, path, presets, characters, seen, problems):
    """Checks one converted level against everything else and flattens it to a spawn list."""

    def report(line, message):
        problems.append(Problem(path, line, message))

    def claim(kind, value, line):
        """Ids that must be unique in the whole game, because they are stored in the save."""
        if not value:
            return
        key = (kind, value)
        if key in seen:
            report(line, "{} '{}' is already used at {}:{}; it must be unique in the whole game".format(kind, value, *seen[key]))
        else:
            seen[key] = (path, line)

    map_path = level.get("Map", "")
    if map_path and not _MAP_PATTERN.fullmatch(map_path):
        report(source.line_of("Map"), "Map must look like /Game/Maps/L_Name, got '{}'".format(map_path))
    claim("map", map_path, source.line_of("Map"))

    lighting = level.get("Lighting")
    if lighting and lighting not in presets:
        report(source.line_of("Lighting"), "lighting preset '{}' is not defined in {}".format(lighting, LIGHTING_PATH))

    spawns = []
    labels = {}

    def add(spawn, line):
        label = spawn["Label"]
        if label in labels:
            report(line, "two objects in this level are named '{}' (first at line {}); give one a different Name".format(label, labels[label]))
        labels[label] = line
        spawns.append(spawn)

    blocks = source.get("Blocks") if isinstance(source.get("Blocks"), LSeq) else None
    for index, block in enumerate(level.get("Blocks", [])):
        line = blocks.line_of(index)
        if not all(key in block for key in ("Name", "At", "Size")):
            continue
        if any(side <= 0 for side in block["Size"]):
            report(line, "block '{}': every side of Size must be above 0".format(block["Name"]))
        add({"Kind": "Block", "Label": block["Name"], "Location": block["At"],
             "Rotation": block.get("Rotation", [0, 0, 0]),
             "Scale": [side / _BLOCK_UNIT for side in block["Size"]]}, line)

    start = level.get("PlayerStart")
    if start and "At" in start:
        add({"Kind": "PlayerStart", "Label": "PlayerStart", "Location": start["At"],
             "Rotation": _rotation(start.get("Yaw", 0))}, source.line_of("PlayerStart"))

    shrine_defaults = {field.name: field.default for field in STRUCTS_BY_NAME["LevelShrine"].fields.values()}
    shrines = source.get("Shrines") if isinstance(source.get("Shrines"), LSeq) else None
    for index, shrine in enumerate(level.get("Shrines", [])):
        line = shrines.line_of(index)
        if not all(key in shrine for key in ("Id", "At")):
            continue
        claim("shrine", shrine["Id"], line)
        add({"Kind": "Shrine", "Label": shrine.get("Name", "Shrine_{}".format(shrine["Id"])), "Id": shrine["Id"],
             "Location": shrine["At"], "Rotation": _rotation(shrine.get("Yaw", 0)),
             "InteractRadius": shrine.get("InteractRadius", shrine_defaults["InteractRadius"]),
             "RespawnDistance": shrine.get("RespawnDistance", shrine_defaults["RespawnDistance"]),
             "RespawnHeight": shrine.get("RespawnHeight", shrine_defaults["RespawnHeight"])}, line)

    enemies = source.get("Enemies") if isinstance(source.get("Enemies"), LSeq) else None
    for index, enemy in enumerate(level.get("Enemies", [])):
        line = enemies.line_of(index)
        if not all(key in enemy for key in ("Character", "At")):
            continue
        character = characters.get(enemy["Character"])
        persistent_id = enemy.get("PersistentId", "")
        if character is None:
            report(line, "enemy uses character '{}', which is not defined in Game/data".format(enemy["Character"]))
        else:
            if not character.data.get("AIProfile"):
                report(line, "character '{}' has no AIProfile, so as an enemy it would just stand there".format(enemy["Character"]))
            if character.data.get("bBoss") and not persistent_id:
                report(line, "boss '{}' needs a PersistentId, otherwise it comes back after every rest".format(enemy["Character"]))
        claim("PersistentId", persistent_id, line)
        add({"Kind": "Enemy", "Label": enemy.get("Name", "Enemy_{}_{}".format(enemy["Character"], index + 1)),
             "Character": enemy["Character"], "PersistentId": persistent_id,
             "Location": enemy["At"], "Rotation": _rotation(enemy.get("Yaw", 0))}, line)

    return {"Map": map_path, "Lighting": presets.get(lighting, {}), "Spawns": spawns}


def build(root, level_files, build_state):
    """Returns {source path: resolved level}. Problems go to build_state.problems."""
    problems = build_state.problems
    checker = ShapeChecker(build_state.schema, problems)
    presets = _load_presets(root, checker, problems)
    characters = build_state.registry.sections["Characters"]

    resolved = {}
    seen = {}
    for path in level_files:
        try:
            document = load_file(os.path.join(root, path), path)
        except SourceError as error:
            problems.append(error.problem)
            continue
        level = checker.convert_struct(document, "Level", path, 1, "level")
        if isinstance(document, LMap):
            resolved[path] = _resolve(level, document, path, presets, characters, seen, problems)
    return resolved
