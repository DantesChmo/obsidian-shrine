# Vendored Python libraries

Used only by the data tools in `Tools/`, never shipped with the game.

| Library | Version | Licence | Source |
|---|---|---|---|
| PyYAML (`yaml/`) | 6.0.2 | MIT (`yaml/LICENSE`) | https://pypi.org/project/PyYAML/6.0.2/ — the pure-Python `lib/yaml` from the source distribution, unmodified |

Vendored rather than installed so the tools run with any stock Python 3.11+, including the one bundled with
Unreal Engine, without `pip`.
