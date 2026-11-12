"""Reads YAML sources keeping line numbers, and refuses the YAML features that silently corrupt game data.

Plain PyYAML would happily turn `Color: #7A3FB8` into null (a comment), `Id: no` into False and a repeated key
into "last one wins". Here each of those is an error pointing at the line.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "vendor"))

import yaml  # noqa: E402  (vendored, see Tools/vendor/README.md)


class Problem:
    """One thing wrong with a source file."""

    def __init__(self, file, line, message):
        self.file = file
        self.line = line
        self.message = message

    def __str__(self):
        return "{}:{}: {}".format(self.file, self.line, self.message)

    def __repr__(self):
        return "Problem({!r})".format(str(self))


class SourceError(Exception):
    """A file that cannot be read at all (as opposed to data that fails validation)."""

    def __init__(self, problem):
        Exception.__init__(self, str(problem))
        self.problem = problem


class LMap(dict):
    """A mapping that remembers where it and each of its keys were written."""

    def __init__(self, file, line):
        dict.__init__(self)
        self.file = file
        self.line = line
        self.key_lines = {}

    def line_of(self, key):
        return self.key_lines.get(key, self.line)


class LSeq(list):
    """A sequence that remembers where it and each of its items were written."""

    def __init__(self, file, line):
        list.__init__(self)
        self.file = file
        self.line = line
        self.item_lines = []

    def line_of(self, index):
        return self.item_lines[index] if index < len(self.item_lines) else self.line


_TAG = "tag:yaml.org,2002:"


def _fail(file, node, message):
    raise SourceError(Problem(file, node.start_mark.line + 1, message))


def _construct(loader, file, node):
    line = node.start_mark.line + 1

    if isinstance(node, yaml.MappingNode):
        result = LMap(file, line)
        for key_node, value_node in node.value:
            if not isinstance(key_node, yaml.ScalarNode) or key_node.tag != _TAG + "str":
                _fail(file, key_node, "keys must be plain text")
            key = key_node.value
            if key == "<<":
                _fail(file, key_node, "merge keys (<<) are not supported; write the fields out")
            if key in result:
                _fail(file, key_node, "key '{}' is repeated (first at line {})".format(key, result.key_lines[key]))
            result[key] = _construct(loader, file, value_node)
            result.key_lines[key] = key_node.start_mark.line + 1
        return result

    if isinstance(node, yaml.SequenceNode):
        result = LSeq(file, line)
        for item in node.value:
            result.append(_construct(loader, file, item))
            result.item_lines.append(item.start_mark.line + 1)
        return result

    tag = node.tag
    if tag == _TAG + "str":
        return node.value
    if tag == _TAG + "null":
        # Kept as None: the validator reports it with a hint, because it knows which field it is.
        return None
    if tag == _TAG + "bool":
        if node.value.lower() not in ("true", "false"):
            _fail(file, node, "'{0}' reads as a yes/no flag in YAML; write true/false, or \"{0}\" in quotes if it is text".format(node.value))
        return node.value.lower() == "true"
    if tag in (_TAG + "int", _TAG + "float"):
        return loader.construct_object(node)
    _fail(file, node, "'{0}' is read by YAML as {1}; put it in quotes: \"{0}\"".format(node.value, tag.replace(_TAG, "a ")))


def load_text(text, file):
    """Parses one YAML document. Returns an LMap; an empty file gives an empty LMap."""
    loader = yaml.SafeLoader(text)
    try:
        node = loader.get_single_node()
        if node is None:
            return LMap(file, 1)
        if not isinstance(node, yaml.MappingNode):
            _fail(file, node, "the file must be a mapping of sections (e.g. 'Actions:'), not a list or a single value")
        return _construct(loader, file, node)
    except yaml.MarkedYAMLError as error:
        mark = error.problem_mark or error.context_mark
        line = mark.line + 1 if mark else 1
        raise SourceError(Problem(file, line, "not valid YAML: {}".format(error.problem or error.context)))
    finally:
        loader.dispose()


def load_file(path, display_name=None):
    with open(path, "r", encoding="utf-8") as handle:
        return load_text(handle.read(), display_name or path)
