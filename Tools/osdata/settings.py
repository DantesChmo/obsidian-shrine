"""Game-wide manifests, one of each: settings, key bindings and texts.

    Game/game.yaml   -> Content/Data/game.json   {"Game": ...}    FOSGameDef
    Game/input.yaml  -> Content/Data/input.json  {"Input": ...}   FOSInputDef
    Game/texts.yaml  -> Content/Data/texts.json  {"Texts": ...}   FOSTextsDef

Each file is one struct written at the top level. A missing file is not an error here: the engine then uses
the defaults from OSDataTypes.h (and has no key bindings at all without input.yaml).

The start map is the one setting the engine reads from its own config before any game code runs, so it is
written into Config/DefaultEngine.ini, which makes that file partly generated.
"""

import difflib
import os
import re

from . import keys
from .source import LMap, LSeq, Problem, SourceError, load_file

# (source, section in the data file, C++ struct)
MANIFESTS = [
    ("Game/game.yaml", "Game", "FOSGameDef"),
    ("Game/input.yaml", "Input", "FOSInputDef"),
    ("Game/texts.yaml", "Texts", "FOSTextsDef"),
]
SECTIONS = tuple(section for _, section, _ in MANIFESTS)

ENGINE_CONFIG = "Config/DefaultEngine.ini"
_MAP_SETTINGS = ("GameDefaultMap", "EditorStartupMap")

# Bindings that take a whole stick or the mouse; every other binding takes buttons.
AXIS_BINDINGS = ("MoveStick", "LookMouse", "LookStick")

_PLACEHOLDER = re.compile(r"\{(\w+)\}")


class Manifest:
    def __init__(self, data, source, file):
        self.data = data        # converted, engine field names, only what was written
        self.source = source    # LMap as written
        self.file = file


def add_to_schema(schema):
    """Every input action must be listed, even if with no keys: a forgotten line would silently unbind it."""
    for field in schema.structs["FOSInputDef"].fields.values():
        field.required = True


def output_path(source):
    return "Content/Data/{}.json".format(os.path.splitext(os.path.basename(source))[0])


def build(root, build_state, checker, to_json):
    """Converts the manifests that exist. Fills build_state.settings and build_state.outputs."""
    for source, section, struct_name in MANIFESTS:
        if not os.path.exists(os.path.join(root, source)):
            continue
        try:
            document = load_file(os.path.join(root, source), source)
        except SourceError as error:
            build_state.problems.append(error.problem)
            continue

        converted = checker.convert_struct(document, struct_name, source, 1, source)
        build_state.settings[section] = Manifest(converted, document, source)

        output = output_path(source)
        if output in build_state.outputs:
            build_state.problems.append(Problem(source, 1, "{} is also produced by a file in Game/data; rename that file".format(output)))
            continue
        build_state.outputs[output] = to_json({section: converted}, source)


def _effective(schema, struct_name, data):
    """Written values over the C++ defaults, by engine field name."""
    values = {field.cpp_name: field.default for field in schema.structs[struct_name].fields.values()}
    values.update(data)
    return values


def _check_game(manifest, build_state, level_maps):
    schema, problems = build_state.schema, build_state.problems
    source = manifest.source
    game = _effective(schema, "FOSGameDef", manifest.data)

    def report(key, message, parent=None):
        holder = source.get(parent) if parent else source
        line = holder.line_of(key) if isinstance(holder, LMap) else source.line_of(parent)
        problems.append(Problem(manifest.file, line, message))

    characters = build_state.registry.sections["Characters"]
    player = characters.get(game["PlayerCharacter"])
    if player is None:
        report("PlayerCharacter", "PlayerCharacter '{}' is not a character defined in Game/data".format(game["PlayerCharacter"]))
    elif player.data.get("AIProfile"):
        report("PlayerCharacter", "PlayerCharacter '{}' has an AIProfile; the player is driven by input, pick a character without one".format(
            game["PlayerCharacter"]))

    if game["StartMap"] not in level_maps:
        known = ", ".join(sorted(level_maps)) or "none"
        report("StartMap", "StartMap '{}' is not the Map of any level in Game/levels (levels: {})".format(game["StartMap"], known))

    camera = _effective(schema, "FOSCameraDef", game.get("Camera") or {})
    if camera["MinPitch"] >= camera["MaxPitch"]:
        report("MinPitch", "Camera.MinPitch must be below Camera.MaxPitch", "Camera")

    lock_on = _effective(schema, "FOSLockOnDef", game.get("LockOn") or {})
    if lock_on["MinPitch"] >= lock_on["MaxPitch"]:
        report("MinPitch", "LockOn.MinPitch must be below LockOn.MaxPitch", "LockOn")
    if lock_on["BreakDistance"] < lock_on["AcquireDistance"]:
        report("BreakDistance", "LockOn.BreakDistance is below AcquireDistance: a target could be locked and lost in the same moment", "LockOn")


def _check_key(name, file, line, label, problems):
    if name in keys.ALL:
        return True
    close = difflib.get_close_matches(name, keys.ALL, n=1, cutoff=0.6)
    hint = " (did you mean '{}'?)".format(close[0]) if close else ""
    problems.append(Problem(file, line, "{}: '{}' is not a key name the engine knows{}".format(label, name, hint)))
    return False


def _check_input(manifest, build_state):
    problems = build_state.problems
    bound = {}  # key -> (action, line)
    for action, names in manifest.source.items():
        if not isinstance(names, LSeq) or action not in build_state.schema.structs["FOSInputDef"].fields:
            continue
        for index, name in enumerate(names):
            line = names.line_of(index)
            if not isinstance(name, str) or not _check_key(name, manifest.file, line, action, problems):
                continue
            is_axis = name in keys.AXES_2D
            if action in AXIS_BINDINGS and not is_axis:
                problems.append(Problem(manifest.file, line, "{} takes a stick or the mouse ({}), '{}' is a button".format(
                    action, ", ".join(keys.AXES_2D), name)))
            elif action not in AXIS_BINDINGS and is_axis:
                problems.append(Problem(manifest.file, line, "{} takes buttons, '{}' is a whole stick; sticks go to {}".format(
                    action, name, ", ".join(AXIS_BINDINGS))))
            if name in bound:
                problems.append(Problem(manifest.file, line, "'{}' is already bound to {} at line {}; a key can do one thing".format(
                    name, bound[name][0], bound[name][1])))
            else:
                bound[name] = (action, line)


def _check_texts(manifest, build_state):
    problems = build_state.problems
    fields = build_state.schema.structs["FOSTextsDef"].fields
    for key, text in manifest.source.items():
        field = fields.get(key)
        if field is None or field.type.kind != "string" or not isinstance(text, str):
            continue
        line = manifest.source.line_of(key)
        allowed = set(_PLACEHOLDER.findall(field.default or ""))
        for name in _PLACEHOLDER.findall(text):
            if name not in allowed:
                available = ", ".join("{{{}}}".format(n) for n in sorted(allowed)) or "none"
                problems.append(Problem(manifest.file, line, "{} uses {{{}}}, which the game does not fill in here (available: {})".format(
                    key, name, available)))
        if "{" in _PLACEHOLDER.sub("", text) or "}" in _PLACEHOLDER.sub("", text):
            problems.append(Problem(manifest.file, line, "{} has a stray '{{' or '}}'; braces are only for placeholders like {{Name}}".format(key)))

    labels = manifest.source.get("KeyLabels")
    if isinstance(labels, LMap):
        for name in labels:
            _check_key(name, manifest.file, labels.line_of(name), "KeyLabels", problems)


def check(build_state, level_maps):
    """Rules that need the rest of the game: characters, levels, key names."""
    if "Game" in build_state.settings:
        _check_game(build_state.settings["Game"], build_state, level_maps)
    if "Input" in build_state.settings:
        _check_input(build_state.settings["Input"], build_state)
    if "Texts" in build_state.settings:
        _check_texts(build_state.settings["Texts"], build_state)


def write_engine_config(root, build_state):
    """Points the engine's default maps at Game.StartMap. Adds the config to the generated outputs."""
    manifest = build_state.settings.get("Game")
    path = os.path.join(root, ENGINE_CONFIG)
    if manifest is None or not os.path.exists(path):
        return
    start_map = _effective(build_state.schema, "FOSGameDef", manifest.data)["StartMap"]
    if not isinstance(start_map, str) or "/" not in start_map:
        return
    value = "{}.{}".format(start_map, start_map.rsplit("/", 1)[1])

    with open(path, "r", encoding="utf-8") as handle:
        text = handle.read()
    for setting in _MAP_SETTINGS:
        text, count = re.subn(r"(?m)^{}=.*$".format(setting), "{}={}".format(setting, value), text)
        if count != 1:
            build_state.problems.append(Problem(ENGINE_CONFIG, 1, "expected exactly one '{}=' line to put StartMap into, found {}".format(
                setting, count)))
            return
    build_state.outputs[ENGINE_CONFIG] = text
