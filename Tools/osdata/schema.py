"""The shape of game data: structs, fields and enums.

For gameplay definitions the shape is read straight from the C++ header that the engine deserialises into
(OSDataTypes.h), so there is no second schema to keep in sync. Manifests that have no C++ struct (levels)
declare theirs in Python with the same classes.
"""

import re


class Type:
    """kind: name | string | float | int | bool | enum | struct | array | map | vector."""

    def __init__(self, kind, ref=None, item=None):
        self.kind = kind
        self.ref = ref      # enum or struct name
        self.item = item    # element Type of an array, value Type of a map

    def describe(self):
        if self.kind == "array":
            return "list of {}".format(self.item.describe())
        if self.kind == "map":
            return "map of name -> {}".format(self.item.describe())
        if self.kind in ("enum", "struct"):
            return self.ref
        return {"name": "id", "string": "text", "float": "number", "int": "whole number", "bool": "true/false",
                "vector": "[x, y, z]"}[self.kind]


class Field:
    def __init__(self, name, type_, default=None, doc="", cpp_name=None, required=False):
        self.name = name                    # as written in YAML
        self.cpp_name = cpp_name or name    # as the engine reads it from JSON
        self.type = type_
        self.default = default
        self.doc = doc
        self.required = required
        self.minimum = None
        self.maximum = None


class Struct:
    def __init__(self, name, fields):
        self.name = name
        self.fields = {field.name: field for field in fields}


class Schema:
    def __init__(self):
        self.enums = {}     # name -> [values]
        self.structs = {}   # name -> Struct


_SCALARS = {"FName": "name", "FString": "string", "float": "float", "double": "float", "int32": "int", "bool": "bool"}


def _parse_type(text, schema_enums):
    text = text.strip()
    if text in _SCALARS:
        return Type(_SCALARS[text])
    match = re.fullmatch(r"TArray<\s*(.+?)\s*>", text)
    if match:
        return Type("array", item=_parse_type(match.group(1), schema_enums))
    match = re.fullmatch(r"TMap<\s*FName\s*,\s*(.+?)\s*>", text)
    if match:
        return Type("map", item=_parse_type(match.group(1), schema_enums))
    if text in schema_enums:
        return Type("enum", ref=text)
    if re.fullmatch(r"FOS\w+", text):
        return Type("struct", ref=text)
    raise ValueError("unsupported C++ type '{}'".format(text))


def _parse_default(text, type_):
    if text is None:
        return {"name": "", "string": "", "float": 0.0, "int": 0, "bool": False, "array": [], "map": {}}.get(type_.kind)
    text = text.strip()
    if type_.kind == "bool":
        return text == "true"
    if type_.kind == "float":
        return float(text.rstrip("f"))
    if type_.kind == "int":
        return int(text)
    if type_.kind == "enum":
        return text.split("::")[-1]
    match = re.fullmatch(r'TEXT\("(.*)"\)', text)
    if match:
        return match.group(1)
    raise ValueError("unsupported C++ default '{}'".format(text))


def yaml_name(cpp_name, type_):
    """bSnapFacing is written SnapFacing in YAML: the b prefix is a C++ convention, not something to author."""
    if type_.kind == "bool" and len(cpp_name) > 1 and cpp_name[0] == "b" and cpp_name[1].isupper():
        return cpp_name[1:]
    return cpp_name


def _clean_comment(text):
    lines = [line.strip().lstrip("*").strip() for line in text.strip().strip("/").strip("*").splitlines()]
    return " ".join(line for line in lines if line)


def parse_cpp_header(text):
    """Extracts UENUMs and USTRUCTs (their UPROPERTY fields) from a header written in the project's regular style."""
    schema = Schema()

    for match in re.finditer(r"enum class (\w+)\s*:\s*\w+\s*\{(.*?)\};", text, re.S):
        body = re.sub(r"/\*.*?\*/", "", match.group(2), flags=re.S)
        body = re.sub(r"//[^\n]*", "", body)
        schema.enums[match.group(1)] = [value.strip() for value in body.split(",") if value.strip()]

    field_pattern = re.compile(
        r"(?:/\*\*(?P<doc>(?:(?!\*/).)*?)\*/\s*)?UPROPERTY\(\)\s*(?P<type>[\w:<>, ]+?)\s+(?P<name>\w+)(?:\s*=\s*(?P<default>[^;]+))?;",
        re.S)
    for match in re.finditer(r"struct (\w+)\s*\{(.*?)\n\};", text, re.S):
        fields = []
        for field_match in field_pattern.finditer(match.group(2)):
            type_ = _parse_type(field_match.group("type"), schema.enums)
            cpp_name = field_match.group("name")
            fields.append(Field(
                yaml_name(cpp_name, type_), type_,
                default=_parse_default(field_match.group("default"), type_),
                doc=_clean_comment(field_match.group("doc") or ""),
                cpp_name=cpp_name))
        if fields:
            schema.structs[match.group(1)] = Struct(match.group(1), fields)

    return schema
