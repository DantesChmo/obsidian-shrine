"""Checks a parsed YAML value against a schema type and converts it to what the engine reads.

Shape only: keys, types, enum members, ranges. Cross-references between definitions are in rules.py.
"""

import difflib

from .source import LMap, LSeq, Problem

_NULL_HINT = "is empty; if the value starts with '#' put it in quotes, otherwise remove the line"


def _suggest(word, candidates):
    close = difflib.get_close_matches(word, list(candidates), n=1, cutoff=0.6)
    return " (did you mean '{}'?)".format(close[0]) if close else ""


class ShapeChecker:
    def __init__(self, schema, problems):
        self.schema = schema
        self.problems = problems

    def _report(self, file, line, message):
        self.problems.append(Problem(file, line, message))

    def convert_struct(self, value, struct_name, file, line, label):
        """Returns a plain dict keyed by engine field names, in the order written. Reports problems as it goes."""
        struct = self.schema.structs[struct_name]
        if not isinstance(value, LMap):
            self._report(file, line, "{} must be a set of fields (Key: value)".format(label))
            return {}

        result = {}
        for key, item in value.items():
            key_line = value.line_of(key)
            field = struct.fields.get(key)
            if field is None:
                hint = _suggest(key, struct.fields)
                self._report(file, key_line, "{} has no field '{}'{}".format(label, key, hint))
                continue
            converted = self.convert(item, field.type, file, key_line, "{}.{}".format(label, key))
            if converted is None:
                continue
            if field.minimum is not None and isinstance(converted, (int, float)) and converted < field.minimum:
                self._report(file, key_line, "{}.{} is {}, the minimum is {}".format(label, key, converted, field.minimum))
            if field.maximum is not None and isinstance(converted, (int, float)) and converted > field.maximum:
                self._report(file, key_line, "{}.{} is {}, the maximum is {}".format(label, key, converted, field.maximum))
            result[field.cpp_name] = converted

        for field in struct.fields.values():
            if field.required and field.cpp_name not in result and field.name not in value:
                self._report(file, value.line, "{} is missing '{}'".format(label, field.name))
        return result

    def convert(self, value, type_, file, line, label):
        """Returns the converted value, or None after reporting a problem."""
        kind = type_.kind

        if value is None:
            self._report(file, line, "{} {}".format(label, _NULL_HINT))
            return None

        if kind in ("name", "string"):
            if not isinstance(value, str):
                self._report(file, line, "{} must be text, got {}; put it in quotes".format(label, _describe(value)))
                return None
            if kind == "name" and (not value or value != value.strip()):
                self._report(file, line, "{} must be a non-empty id without surrounding spaces".format(label))
                return None
            return value

        if kind == "float":
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                self._report(file, line, "{} must be a number, got {}".format(label, _describe(value)))
                return None
            return value

        if kind == "int":
            if isinstance(value, bool) or not isinstance(value, int):
                self._report(file, line, "{} must be a whole number, got {}".format(label, _describe(value)))
                return None
            return value

        if kind == "bool":
            if not isinstance(value, bool):
                self._report(file, line, "{} must be true or false, got {}".format(label, _describe(value)))
                return None
            return value

        if kind == "enum":
            members = self.schema.enums[type_.ref]
            if value not in members:
                self._report(file, line, "{} must be one of {}, got {}{}".format(
                    label, ", ".join(members), _describe(value), _suggest(str(value), members)))
                return None
            return value

        if kind == "vector":
            if (not isinstance(value, LSeq) or len(value) != 3
                    or any(isinstance(v, bool) or not isinstance(v, (int, float)) for v in value)):
                self._report(file, line, "{} must be three numbers: [x, y, z]".format(label))
                return None
            return list(value)

        if kind == "array":
            if not isinstance(value, LSeq):
                self._report(file, line, "{} must be a list".format(label))
                return None
            items = []
            for index, item in enumerate(value):
                converted = self.convert(item, type_.item, file, value.line_of(index), "{}[{}]".format(label, index + 1))
                if converted is not None:
                    items.append(converted)
            return items

        if kind == "map":
            if not isinstance(value, LMap):
                self._report(file, line, "{} must be a set of Key: value pairs".format(label))
                return None
            result = {}
            for key, item in value.items():
                converted = self.convert(item, type_.item, file, value.line_of(key), "{}.{}".format(label, key))
                if converted is not None:
                    result[key] = converted
            return result

        if kind == "struct":
            return self.convert_struct(value, type_.ref, file, line, label)

        raise ValueError("unknown kind {}".format(kind))


def _describe(value):
    if isinstance(value, bool):
        return "'{}'".format(str(value).lower())
    if isinstance(value, (LMap, dict)):
        return "a set of fields"
    if isinstance(value, (LSeq, list)):
        return "a list"
    if isinstance(value, str):
        return "'{}'".format(value)
    return "the number {}".format(value)
