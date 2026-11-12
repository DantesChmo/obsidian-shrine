"""Tests of level manifests: validation, and that the manifest-driven builder places exactly what the old
hand-written sandbox script placed."""

import importlib
import json
import os
import sys
import types
import unittest
from unittest import mock

from support import REPO, VALID_DATA, ProjectCase

from osdata import project

LIGHTING = """
Presets:
  - Id: day
"""

LEVEL = """
Map: /Game/Maps/L_Test
Lighting: day
PlayerStart: { At: [0, 0, 100], Yaw: 90 }
Blocks:
  - { Name: Floor, At: [0, 0, -50], Size: [4000, 2000, 100] }
Shrines:
  - { Id: test_shrine, At: [0, -300, 80] }
Enemies:
  - { Character: grunt, At: [500, 0, 100], Yaw: 180 }
"""


class LevelCase(ProjectCase):
    def setUp(self):
        ProjectCase.setUp(self)
        self.put("Game/data/a.yaml", VALID_DATA)
        self.put("Game/lighting.yaml", LIGHTING)

    def level(self, name="test"):
        return json.loads(self.build().outputs["Content/LevelData/{}.json".format(name)])


class Resolving(LevelCase):
    def test_valid_level_resolves_to_a_flat_list(self):
        self.put("Game/levels/test.yaml", LEVEL)
        self.assertClean()
        level = self.level()
        self.assertEqual(level["Map"], "/Game/Maps/L_Test")
        self.assertEqual(level["Lighting"], {"SunPitch": -40.0, "SunYaw": 30.0, "Fog": True})
        self.assertEqual([spawn["Kind"] for spawn in level["Spawns"]], ["Block", "PlayerStart", "Shrine", "Enemy"])

    def test_block_size_in_centimetres_becomes_cube_scale(self):
        self.put("Game/levels/test.yaml", LEVEL)
        self.assertEqual(self.level()["Spawns"][0]["Scale"], [40.0, 20.0, 1.0])

    def test_yaw_becomes_pitch_yaw_roll(self):
        self.put("Game/levels/test.yaml", LEVEL)
        spawns = self.level()["Spawns"]
        self.assertEqual(spawns[1]["Rotation"], [0, 90, 0])
        self.assertEqual(spawns[3]["Rotation"], [0, 180, 0])

    def test_default_labels(self):
        self.put("Game/levels/test.yaml", LEVEL)
        self.assertEqual([spawn["Label"] for spawn in self.level()["Spawns"]],
                         ["Floor", "PlayerStart", "Shrine_test_shrine", "Enemy_grunt_1"])


class Validation(LevelCase):
    def test_unknown_character(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("Character: grunt", "Character: gruntt"))
        self.assertProblem("Game/levels/test.yaml:9", "character 'gruntt'", "not defined")

    def test_unknown_lighting(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("Lighting: day", "Lighting: night"))
        self.assertProblem("Game/levels/test.yaml:2", "lighting preset 'night'")

    def test_bad_map_path(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("/Game/Maps/L_Test", "Maps/Test"))
        self.assertProblem("Game/levels/test.yaml:1", "/Game/Maps/L_Name")

    def test_missing_player_start(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("PlayerStart: { At: [0, 0, 100], Yaw: 90 }\n", ""))
        self.assertProblem("Game/levels/test.yaml:1", "missing 'PlayerStart'")

    def test_unknown_field_and_bad_vector(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("Size: [4000, 2000, 100]", "Size: [4000, 2000], Colour: red"))
        found = self.problems()
        self.assertTrue(any("three numbers" in text for text in found), found)
        self.assertTrue(any("no field 'Colour'" in text for text in found), found)

    def test_zero_sized_block(self):
        self.put("Game/levels/test.yaml", LEVEL.replace("[4000, 2000, 100]", "[4000, 0, 100]"))
        self.assertProblem("Game/levels/test.yaml:5", "above 0")

    def test_duplicate_names_within_a_level(self):
        self.put("Game/levels/test.yaml", LEVEL + "  - { Character: grunt, Name: Floor, At: [0, 0, 100] }\n")
        self.assertProblem("Game/levels/test.yaml:10", "named 'Floor'")

    def test_shrine_id_must_be_unique_across_levels(self):
        self.put("Game/levels/a.yaml", LEVEL)
        self.put("Game/levels/b.yaml", LEVEL.replace("L_Test", "L_Other"))
        self.assertProblem("Game/levels/b.yaml:7", "shrine 'test_shrine'", "Game/levels/a.yaml:7")

    def test_map_must_be_unique_across_levels(self):
        self.put("Game/levels/a.yaml", LEVEL)
        self.put("Game/levels/b.yaml", LEVEL.replace("test_shrine", "other_shrine"))
        self.assertProblem("Game/levels/b.yaml:1", "map '/Game/Maps/L_Test'")

    def test_persistent_id_must_be_unique_across_levels(self):
        enemy = "  - { Character: grunt, At: [0, 900, 100], PersistentId: the_one }\n"
        self.put("Game/levels/a.yaml", LEVEL + enemy)
        self.put("Game/levels/b.yaml", LEVEL.replace("L_Test", "L_Other").replace("test_shrine", "other_shrine") + enemy)
        self.assertProblem("Game/levels/b.yaml:10", "PersistentId 'the_one'", "unique in the whole game")

    def test_boss_needs_a_persistent_id(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("    Weapon: blade", "    Weapon: blade\n    Boss: true"))
        self.put("Game/levels/test.yaml", LEVEL)
        self.assertProblem("Game/levels/test.yaml:9", "boss 'grunt' needs a PersistentId")

    def test_enemy_without_ai(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("    AIProfile: grunt_ai\n", ""))
        self.put("Game/levels/test.yaml", LEVEL)
        self.assertProblem("Game/levels/test.yaml:9", "no AIProfile")


class Recorder:
    """Stands in for os_content.level_utils and records every placement call in a comparable form."""

    def __init__(self):
        self.calls = []
        self.actors = []

    def _record(self, *call):
        self.calls.append(call)
        actor = mock.MagicMock()
        self.actors.append(actor)
        return actor

    def open_or_create_level(self, asset_path):
        self.calls.append(("open", asset_path))
        return True

    def clear_generated_actors(self):
        self.calls.append(("clear",))

    def save_current_level(self):
        self.calls.append(("save",))

    def spawn(self, actor_class, label, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), scale=None):
        return self._record("spawn", actor_class, label, tuple(location), tuple(rotation), scale)

    def spawn_static_mesh(self, mesh_path, label, location, scale=(1.0, 1.0, 1.0), rotation=(0.0, 0.0, 0.0)):
        return self._record("mesh", mesh_path, label, tuple(location), tuple(scale), tuple(rotation))

    def spawn_enemy(self, character_id, label, location, rotation=(0.0, 0.0, 0.0), persistent_id=None):
        return self._record("enemy", character_id, label, tuple(location), tuple(rotation), persistent_id)

    def spawn_checkpoint(self, checkpoint_id, label, location, rotation=(0.0, 0.0, 0.0)):
        return self._record("shrine", checkpoint_id, label, tuple(location), tuple(rotation))

    def follow_ups(self):
        """What the builder did to each spawned actor afterwards (mobility, light settings)."""
        return [actor.mock_calls for actor in self.actors]


class BuilderMatchesHandWrittenScript(unittest.TestCase):
    """Runs both level scripts against a fake `unreal` and compares what they would place."""

    def setUp(self):
        self.fake_unreal = mock.MagicMock(name="unreal")
        content_python = os.path.join(REPO, "Content", "Python")
        patcher = mock.patch.dict(sys.modules, {"unreal": self.fake_unreal})
        patcher.start()
        self.addCleanup(patcher.stop)
        sys.path.insert(0, content_python)
        self.addCleanup(sys.path.remove, content_python)
        for name in [name for name in sys.modules if name == "os_content" or name.startswith("os_content.")]:
            del sys.modules[name]
        self.addCleanup(lambda: [sys.modules.pop(n) for n in list(sys.modules) if n == "os_content" or n.startswith("os_content.")])

    def _run(self, module_name, call):
        recorder = Recorder()
        package = importlib.import_module("os_content")
        stub = types.ModuleType("os_content.level_utils")
        for name in dir(recorder):
            if not name.startswith("_"):
                setattr(stub, name, getattr(recorder, name))
        with mock.patch.dict(sys.modules, {"os_content.level_utils": stub}), mock.patch.object(package, "level_utils", stub, create=True):
            sys.modules.pop(module_name, None)
            module = importlib.import_module(module_name)
            call(module)
        return recorder

    def test_sandbox_manifest_places_the_same_things_as_the_script(self):
        with open(os.path.join(REPO, "Content", "LevelData", "sandbox.json"), encoding="utf-8") as handle:
            level = json.load(handle)

        scripted = self._run("os_content.sandbox_level", lambda module: module.build())
        from_manifest = self._run("os_content.level_builder", lambda module: module.build_level(level))

        self.assertEqual(len(scripted.calls), 19, "the reference script itself changed")
        self.assertEqual(from_manifest.calls, scripted.calls)
        self.assertEqual(from_manifest.follow_ups(), scripted.follow_ups())

    def test_unknown_spawn_kind_is_an_error(self):
        level = {"Map": "/Game/Maps/L_X", "Lighting": {}, "Spawns": [{"Kind": "Door", "Label": "D"}]}
        with self.assertRaises(RuntimeError):
            self._run("os_content.level_builder", lambda module: module.build_level(level))


if __name__ == "__main__":
    unittest.main()
