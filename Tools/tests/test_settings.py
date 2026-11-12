"""Tests of the game-wide manifests: Game/game.yaml, Game/input.yaml, Game/texts.yaml."""

import json
import os
import textwrap
import unittest

from support import REPO, VALID_DATA, ProjectCase

from osdata import keys, project, settings

HERO = """
Characters:
  - Id: player
"""

LIGHTING = """
Presets:
  - Id: day
"""

LEVEL = """
Map: /Game/Maps/L_Test
Lighting: day
PlayerStart: { At: [0, 0, 100] }
"""

ENGINE_CONFIG = """
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Maps/L_Old.L_Old
EditorStartupMap=/Game/Maps/L_Old.L_Old
GlobalDefaultGameMode=/Script/ObsidianShrine.OSGameMode

[/Script/Engine.RendererSettings]
r.AllowStaticLighting=False
"""

GAME = """
PlayerCharacter: player
StartMap: /Game/Maps/L_Test
"""

INPUT = """
MoveForward: [W]
MoveBackward: [S]
MoveLeft: [A]
MoveRight: [D]
MoveStick: [Gamepad_Left2D]
LookMouse: [Mouse2D]
LookStick: [Gamepad_Right2D]
Light: [LeftMouseButton]
Heavy: [F]
Guard: [RightMouseButton]
Dodge: [SpaceBar]
Sprint: [LeftShift]
DodgeOrSprint: [Gamepad_FaceButton_Right]
LockOn: [Q]
TargetLeft: [MouseScrollDown]
TargetRight: [MouseScrollUp]
Interact: [E, Gamepad_FaceButton_Bottom]
"""


class SettingsCase(ProjectCase):
    """A project with a player, one level and an engine config, ready for a game.yaml."""

    def setUp(self):
        ProjectCase.setUp(self)
        self.put("Game/data/a.yaml", VALID_DATA)
        self.put("Game/data/hero.yaml", HERO)
        self.put("Game/lighting.yaml", LIGHTING)
        self.put("Game/levels/test.yaml", LEVEL)
        self.put(settings.ENGINE_CONFIG, ENGINE_CONFIG)

    def put_game(self, extra):
        """GAME (two lines) followed by more settings, which therefore start at line 3."""
        self.put("Game/game.yaml", GAME.lstrip("\n") + textwrap.dedent(extra).lstrip("\n"))

    def output(self, name):
        return json.loads(self.build().outputs["Content/Data/{}.json".format(name)])


class GameSettings(SettingsCase):
    def test_missing_manifests_are_not_an_error(self):
        self.assertClean()
        self.assertNotIn("Content/Data/game.json", self.build().outputs)
        self.assertNotIn(settings.ENGINE_CONFIG, self.build().outputs)

    def test_written_fields_reach_the_engine_under_the_game_section(self):
        self.put_game("""
            Camera:
              ArmLength: 500
            Death:
              KeepPreviousDrop: true
            """)
        self.assertClean()
        game = self.output("game")["Game"]
        self.assertEqual(game["Camera"], {"ArmLength": 500})
        self.assertEqual(game["Death"], {"bKeepPreviousDrop": True})
        self.assertEqual(game["PlayerCharacter"], "player")

    def test_unknown_setting_suggests_the_right_one(self):
        self.put_game("""
            Camera:
              ArmLenght: 500
            """)
        self.assertProblem("Game/game.yaml:4", "no field 'ArmLenght'", "ArmLength")

    def test_range_from_field_notes(self):
        self.put_game("""
            Death:
              CurrencyLossFraction: 1.5
            """)
        self.assertProblem("Game/game.yaml:4", "maximum is 1")

    def test_unknown_player_character(self):
        self.put("Game/game.yaml", GAME.replace("PlayerCharacter: player", "PlayerCharacter: hero"))
        self.assertProblem("Game/game.yaml:1", "'hero'", "not a character")

    def test_player_character_must_not_be_an_enemy(self):
        self.put("Game/game.yaml", GAME.replace("PlayerCharacter: player", "PlayerCharacter: grunt"))
        self.assertProblem("Game/game.yaml:1", "'grunt'", "AIProfile")

    def test_default_player_character_is_checked_too(self):
        self.put("Game/data/hero.yaml", HERO.replace("player", "pilgrim"))
        self.put("Game/game.yaml", "StartMap: /Game/Maps/L_Test\n")
        self.assertProblem("Game/game.yaml:1", "'player'", "not a character")

    def test_start_map_must_be_a_level(self):
        self.put("Game/game.yaml", GAME.replace("L_Test", "L_Tset"))
        self.assertProblem("Game/game.yaml:2", "'/Game/Maps/L_Tset'", "/Game/Maps/L_Test")

    def test_pitch_limits_must_be_ordered(self):
        self.put_game("""
            Camera:
              MinPitch: 60
            LockOn:
              MaxPitch: -60
            """)
        found = self.problems()
        self.assertTrue(any(":4:" in t and "Camera.MinPitch" in t for t in found), found)
        self.assertTrue(any(":6:" in t and "LockOn.MinPitch" in t for t in found), found)

    def test_lock_must_not_break_closer_than_it_is_acquired(self):
        self.put_game("""
            LockOn:
              BreakDistance: 1000
            """)
        self.assertProblem("Game/game.yaml:4", "BreakDistance", "AcquireDistance")

    def test_name_clash_with_a_definitions_file(self):
        self.put("Game/data/game.yaml", "Actions: []\n")
        self.put("Game/game.yaml", GAME)
        self.assertProblem("Game/game.yaml:1", "Content/Data/game.json", "rename")


class StartMap(SettingsCase):
    def test_start_map_is_written_into_the_engine_config(self):
        self.put("Game/game.yaml", GAME)
        build = self.build()
        self.assertEqual(build.problems, [])
        text = build.outputs[settings.ENGINE_CONFIG]
        self.assertIn("GameDefaultMap=/Game/Maps/L_Test.L_Test\n", text)
        self.assertIn("EditorStartupMap=/Game/Maps/L_Test.L_Test\n", text)
        self.assertIn("GlobalDefaultGameMode=/Script/ObsidianShrine.OSGameMode\n", text, "other lines are left alone")
        self.assertIn("r.AllowStaticLighting=False\n", text)
        self.assertNotIn("L_Old", text)

    def test_config_is_stale_until_written(self):
        self.put("Game/game.yaml", GAME)
        build = self.build()
        self.assertIn(settings.ENGINE_CONFIG, project.stale_files(self.root, build))
        project.write(self.root, build)
        self.assertEqual(project.stale_files(self.root, self.build()), [])

    def test_config_without_the_map_lines_is_reported(self):
        self.put(settings.ENGINE_CONFIG, "[/Script/Engine.RendererSettings]\nr.AllowStaticLighting=False\n")
        self.put("Game/game.yaml", GAME)
        self.assertProblem(settings.ENGINE_CONFIG + ":1", "GameDefaultMap")


class Input(SettingsCase):
    def test_valid_bindings(self):
        self.put("Game/input.yaml", INPUT)
        self.assertClean()
        bindings = self.output("input")["Input"]
        self.assertEqual(bindings["Interact"], ["E", "Gamepad_FaceButton_Bottom"])
        self.assertEqual(bindings["MoveStick"], ["Gamepad_Left2D"])

    def test_every_action_must_be_listed(self):
        self.put("Game/input.yaml", INPUT.replace("Heavy: [F]\n", ""))
        self.assertProblem("Game/input.yaml:1", "missing 'Heavy'")

    def test_an_action_may_be_unbound(self):
        self.put("Game/input.yaml", INPUT.replace("Heavy: [F]", "Heavy: []"))
        self.assertClean()
        self.assertEqual(self.output("input")["Input"]["Heavy"], [])

    def test_unknown_action(self):
        self.put("Game/input.yaml", INPUT + "Jump: [J]\n")
        self.assertProblem("Game/input.yaml:18", "no field 'Jump'")

    def test_unknown_key_suggests_the_right_one(self):
        self.put("Game/input.yaml", INPUT.replace("Dodge: [SpaceBar]", "Dodge: [Spacebar]"))
        self.assertProblem("Game/input.yaml:11", "'Spacebar'", "did you mean 'SpaceBar'")

    def test_key_bound_twice(self):
        self.put("Game/input.yaml", INPUT.replace("LockOn: [Q]", "LockOn: [F]"))
        self.assertProblem("Game/input.yaml:14", "'F'", "already bound to Heavy", "line 9")

    def test_button_where_a_stick_is_expected(self):
        self.put("Game/input.yaml", INPUT.replace("LookStick: [Gamepad_Right2D]", "LookStick: [Gamepad_RightThumbstick]"))
        self.assertProblem("Game/input.yaml:7", "LookStick takes a stick")

    def test_stick_where_a_button_is_expected(self):
        self.put("Game/input.yaml", INPUT.replace("Light: [LeftMouseButton]", "Light: [Mouse2D]").replace("LookMouse: [Mouse2D]", "LookMouse: []"))
        self.assertProblem("Game/input.yaml:8", "Light takes buttons")

    def test_one_key_instead_of_a_list(self):
        self.put("Game/input.yaml", INPUT.replace("Heavy: [F]", "Heavy: F"))
        self.assertProblem("Game/input.yaml:9", "must be a list")

    def test_key_lists_do_not_overlap(self):
        self.assertEqual(set(keys.AXES_2D) & set(keys.BUTTONS), set())
        self.assertEqual(len(keys.ALL), len(set(keys.ALL)))


class Texts(SettingsCase):
    def test_valid_texts(self):
        self.put("Game/texts.yaml", """
            InteractPrompt: "{Prompt} - press {Keys}"
            Currency: "{Amount} souls"
            KeyLabels:
              Gamepad_FaceButton_Bottom: A
            """)
        self.assertClean()
        texts = self.output("texts")["Texts"]
        self.assertEqual(texts["Currency"], "{Amount} souls")
        self.assertEqual(texts["KeyLabels"], {"Gamepad_FaceButton_Bottom": "A"})

    def test_placeholder_the_game_does_not_fill(self):
        self.put("Game/texts.yaml", """
            Currency: "Shards: {Count}"
            """)
        self.assertProblem("Game/texts.yaml:1", "{Count}", "{Amount}")

    def test_placeholder_in_a_text_that_has_none(self):
        self.put("Game/texts.yaml", """
            MessageDied: "YOU DIED, {Name}"
            """)
        self.assertProblem("Game/texts.yaml:1", "{Name}", "available: none")

    def test_stray_brace(self):
        self.put("Game/texts.yaml", """
            Currency: "Shards: {Amount"
            """)
        self.assertProblem("Game/texts.yaml:1", "stray")

    def test_unknown_text(self):
        self.put("Game/texts.yaml", """
            MessageDead: "YOU DIED"
            """)
        self.assertProblem("Game/texts.yaml:1", "no field 'MessageDead'", "MessageDied")

    def test_label_for_an_unknown_key(self):
        self.put("Game/texts.yaml", """
            KeyLabels:
              Gamepad_FaceButton_Botom: A
            """)
        self.assertProblem("Game/texts.yaml:2", "'Gamepad_FaceButton_Botom'", "did you mean")

    def test_text_that_yaml_reads_as_a_flag(self):
        self.put("Game/texts.yaml", """
            KeyLabels:
              Y: Yes
            """)
        self.assertProblem("Game/texts.yaml:2", "yes/no")


class Repository(unittest.TestCase):
    """The real project ships all three manifests and its engine config follows StartMap."""

    def setUp(self):
        self.build = project.run(REPO)

    def test_all_three_manifests_are_shipped(self):
        for source, section, _ in settings.MANIFESTS:
            self.assertTrue(os.path.exists(os.path.join(REPO, source)), source)
            self.assertIn(section, json.loads(self.build.outputs[settings.output_path(source)]))

    def test_engine_config_is_generated_from_start_map(self):
        start_map = json.loads(self.build.outputs["Content/Data/game.json"])["Game"]["StartMap"]
        self.assertIn("GameDefaultMap={0}.{1}\n".format(start_map, start_map.rsplit("/", 1)[1]), self.build.outputs[settings.ENGINE_CONFIG])

    def test_every_key_shown_to_the_player_has_a_readable_label(self):
        bindings = json.loads(self.build.outputs["Content/Data/input.json"])["Input"]
        labels = json.loads(self.build.outputs["Content/Data/texts.json"])["Texts"].get("KeyLabels", {})
        for key in bindings["Interact"]:
            self.assertTrue(len(key) == 1 or key in labels, "{} would be shown by its engine name; add it to KeyLabels".format(key))


if __name__ == "__main__":
    unittest.main()
