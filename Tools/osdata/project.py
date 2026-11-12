"""Turns the YAML sources under Game/ into the JSON files the engine and the level builder read.

    Game/data/**/*.yaml      -> Content/Data/**/*.json        gameplay definitions (engine, at runtime)
    Game/levels/*.yaml       -> Content/LevelData/*.json      resolved spawn lists (level builder, in the editor)
    Game/game.yaml, input.yaml, texts.yaml
                             -> Content/Data/<name>.json      settings, key bindings, texts (engine, at runtime)
                             -> Config/DefaultEngine.ini      only the two default-map lines, from Game.StartMap
    Game/schema/fields.yaml                                   descriptions and ranges of every field
                             -> Docs/REFERENCE.md             generated field reference

Generated files are committed, so the game runs from a fresh checkout. `check` fails when they are stale.
"""

import json
import os

from . import levels, reference, rules, schema as schema_module, settings
from .shape import ShapeChecker
from .source import LMap, LSeq, Problem, SourceError, load_file

GENERATED_KEY = "_generated"

HEADER_PATH = "Source/ObsidianShrine/Public/Data/OSDataTypes.h"
DATA_SOURCES = "Game/data"
FIELDS_PATH = "Game/schema/fields.yaml"
LEVEL_SOURCES = "Game/levels"
DATA_OUTPUT = "Content/Data"
LEVEL_OUTPUT = "Content/LevelData"
REFERENCE_PATH = "Docs/REFERENCE.md"


class Build:
    """Everything one run produces: problems found and the files that should exist."""

    def __init__(self):
        self.problems = []
        self.outputs = {}       # project-relative path -> text
        self.registry = rules.Registry()
        self.settings = {}      # section -> settings.Manifest, for the game-wide manifests that exist
        self.schema = None

    @property
    def ok(self):
        return not self.problems


def _yaml_files(root, directory):
    found = []
    base = os.path.join(root, directory)
    for folder, _, names in os.walk(base):
        for name in names:
            if name.endswith((".yaml", ".yml")):
                found.append(os.path.relpath(os.path.join(folder, name), root).replace(os.sep, "/"))
    return sorted(found)


def _to_json(payload, source):
    document = {GENERATED_KEY: "from {} by Tools/build_data.py - edit the YAML, not this file".format(source)}
    document.update(payload)
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def load_schema(root, problems):
    """C++ structs plus the level manifest shape, with descriptions and ranges applied from fields.yaml."""
    with open(os.path.join(root, HEADER_PATH), "r", encoding="utf-8") as handle:
        schema = schema_module.parse_cpp_header(handle.read())
    levels.add_to_schema(schema)
    settings.add_to_schema(schema)

    for field in schema.structs["FOSActionDef"].fields.values():
        field.required = field.name in ("Id", "Duration")
    for struct_name in ("FOSWeaponDef", "FOSCharacterDef", "FOSAIProfileDef"):
        schema.structs[struct_name].fields["Id"].required = True

    try:
        notes = load_file(os.path.join(root, FIELDS_PATH), FIELDS_PATH)
    except SourceError as error:
        problems.append(error.problem)
        return schema

    documented = reference.documented_structs()
    for struct_name, fields in notes.items():
        struct = schema.structs.get(struct_name)
        if struct is None or not isinstance(fields, LMap):
            problems.append(Problem(FIELDS_PATH, notes.line_of(struct_name), "'{}' is not a known struct".format(struct_name)))
            continue
        for field_name, note in fields.items():
            line = fields.line_of(field_name)
            field = struct.fields.get(field_name)
            if field is None:
                problems.append(Problem(FIELDS_PATH, line, "{} has no field '{}'".format(struct_name, field_name)))
                continue
            if not isinstance(note, LMap) or set(note) - {"doc", "min", "max"}:
                problems.append(Problem(FIELDS_PATH, line, "{}.{} must have only doc, min, max".format(struct_name, field_name)))
                continue
            if isinstance(note.get("doc"), str):
                field.doc = note["doc"].strip()
            field.minimum = note.get("min")
            field.maximum = note.get("max")
            field.described = True

    for struct_name in documented:
        for field in schema.structs[struct_name].fields.values():
            if not getattr(field, "described", False):
                problems.append(Problem(FIELDS_PATH, 1, "{}.{} has no description; add it so the reference stays complete".format(
                    struct_name, field.name)))
    return schema


def _build_data(root, build):
    checker = ShapeChecker(build.schema, build.problems)
    for path in _yaml_files(root, DATA_SOURCES):
        try:
            document = load_file(os.path.join(root, path), path)
        except SourceError as error:
            build.problems.append(error.problem)
            continue

        payload = {}
        for section, items in document.items():
            line = document.line_of(section)
            if section not in rules.SECTIONS:
                build.problems.append(Problem(path, line, "unknown section '{}'; sections are {}".format(
                    section, ", ".join(rules.SECTIONS))))
                continue
            if not isinstance(items, LSeq):
                build.problems.append(Problem(path, line, "'{}' must be a list (each item starts with '- ')".format(section)))
                continue

            struct_name, kind = rules.SECTIONS[section]
            converted_items = []
            for index, item in enumerate(items):
                item_line = items.line_of(index)
                item_id = item.get("Id") if isinstance(item, LMap) else None
                label = "{} '{}'".format(kind, item_id) if isinstance(item_id, str) else "{} #{}".format(kind, index + 1)
                converted = checker.convert_struct(item, struct_name, path, item_line, label)
                converted_items.append(converted)
                if isinstance(item, LMap):
                    build.registry.add(section, rules.Entry(converted, item, path), build.problems)
            payload[section] = converted_items

        relative = os.path.relpath(path, DATA_SOURCES).replace(os.sep, "/")
        output = "{}/{}.json".format(DATA_OUTPUT, os.path.splitext(relative)[0])
        build.outputs[output] = _to_json(payload, path)

    rules.check(build.registry, build.problems)


def run(root):
    """Validates everything and computes the generated files. Writes nothing."""
    build = Build()
    build.schema = load_schema(root, build.problems)
    _build_data(root, build)
    settings.build(root, build, ShapeChecker(build.schema, build.problems), _to_json)

    resolved_levels = levels.build(root, _yaml_files(root, LEVEL_SOURCES), build)
    for path, payload in resolved_levels.items():
        name = os.path.splitext(os.path.basename(path))[0]
        build.outputs["{}/{}.json".format(LEVEL_OUTPUT, name)] = _to_json(payload, path)

    settings.check(build, {payload["Map"] for payload in resolved_levels.values()})
    settings.write_engine_config(root, build)

    build.outputs[REFERENCE_PATH] = reference.render(build.schema)
    build.problems.sort(key=lambda problem: (problem.file, problem.line))
    return build


def _is_generated(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return GENERATED_KEY in json.load(handle)
    except (OSError, ValueError):
        return False


def _orphans(root, build):
    """Generated JSON on disk whose YAML source is gone."""
    found = []
    for directory in (DATA_OUTPUT, LEVEL_OUTPUT):
        for folder, _, names in os.walk(os.path.join(root, directory)):
            for name in names:
                relative = os.path.relpath(os.path.join(folder, name), root).replace(os.sep, "/")
                if name.endswith(".json") and relative not in build.outputs and _is_generated(os.path.join(folder, name)):
                    found.append(relative)
    return sorted(found)


def stale_files(root, build):
    """Generated files that differ from what is on disk, are missing, or should be deleted."""
    stale = []
    for path, text in sorted(build.outputs.items()):
        full = os.path.join(root, path)
        try:
            with open(full, "r", encoding="utf-8") as handle:
                if handle.read() != text:
                    stale.append(path)
        except OSError:
            stale.append(path)
    return stale + _orphans(root, build)


def write(root, build):
    """Writes the generated files and removes orphans. Returns the paths that changed."""
    changed = stale_files(root, build)
    for path in _orphans(root, build):
        os.remove(os.path.join(root, path))
    for path, text in build.outputs.items():
        if path in changed:
            full = os.path.join(root, path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
    return changed
