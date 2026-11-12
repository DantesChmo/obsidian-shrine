"""Tests of the data compiler. Run from the repository root: python -m unittest discover -s Tools/tests"""

import json
import os
import unittest

from support import REPO, VALID_DATA, ProjectCase

from osdata import project, schema


class SchemaFromHeader(unittest.TestCase):
    def setUp(self):
        with open(os.path.join(REPO, project.HEADER_PATH), encoding="utf-8") as handle:
            self.schema = schema.parse_cpp_header(handle.read())

    def test_reads_enums(self):
        self.assertEqual(self.schema.enums["EOSMoveDirection"], ["Forward", "Backward", "Input"])
        self.assertIn("PerfectGuard", self.schema.enums["EOSWindowType"])

    def test_reads_types_and_defaults(self):
        action = self.schema.structs["FOSActionDef"].fields
        self.assertEqual(action["Duration"].default, 0.5)
        self.assertEqual(action["Windows"].type.kind, "array")
        self.assertEqual(action["Windows"].type.item.ref, "FOSActionWindow")
        self.assertEqual(action["MoveDirection"].default, "Forward")
        self.assertEqual(self.schema.structs["FOSWeaponDef"].fields["AttachSocket"].default, "hand_r")
        self.assertEqual(self.schema.structs["FOSWeaponDef"].fields["Actions"].type.kind, "map")
        self.assertEqual(self.schema.structs["FOSCharacterDef"].fields["CurrencyReward"].type.kind, "int")

    def test_flags_lose_the_b_prefix_in_yaml_only(self):
        field = self.schema.structs["FOSActionDef"].fields["SnapFacing"]
        self.assertEqual(field.cpp_name, "bSnapFacing")
        self.assertNotIn("bSnapFacing", self.schema.structs["FOSActionDef"].fields)

    def test_every_section_of_a_data_file_is_known(self):
        from osdata import rules
        self.assertEqual(set(self.schema.structs["FOSDataFile"].fields), set(rules.SECTIONS))


class YamlPitfalls(ProjectCase):
    def test_valid_file_is_clean(self):
        self.put("Game/data/a.yaml", VALID_DATA)
        self.assertClean()

    def test_unquoted_colour_is_caught(self):
        self.put("Game/data/a.yaml", """
            Characters:
              - Id: grunt
                PlaceholderColor: #B0503A
            """)
        self.assertProblem("Game/data/a.yaml:3", "PlaceholderColor", "quotes")

    def test_yes_no_is_caught(self):
        self.put("Game/data/a.yaml", """
            Characters:
              - Id: no
            """)
        self.assertProblem("Game/data/a.yaml:2", "yes/no")

    def test_repeated_key_is_caught(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
                Duration: 2
            """)
        self.assertProblem("Game/data/a.yaml:4", "repeated", "line 3")

    def test_broken_yaml_reports_the_line(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Windows: [ { Type: Hit
            """)
        self.assertTrue(any("not valid YAML" in text for text in self.problems()))

    def test_number_where_text_is_expected(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: 42
                Duration: 1
            """)
        self.assertProblem("Game/data/a.yaml:2", "must be text")


class Shape(ProjectCase):
    def test_unknown_field_suggests_the_right_one(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
                StaminaCots: 5
            """)
        self.assertProblem("Game/data/a.yaml:4", "no field 'StaminaCots'", "StaminaCost")

    def test_cpp_flag_name_points_to_the_yaml_name(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
                bSnapFacing: true
            """)
        self.assertProblem("Game/data/a.yaml:4", "no field 'bSnapFacing'", "did you mean 'SnapFacing'")

    def test_unknown_enum_value(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
                Windows:
                  - { Type: Hti, Start: 0.1 }
            """)
        self.assertProblem("Game/data/a.yaml:5", "must be one of", "did you mean 'Hit'")

    def test_wrong_types(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: long
                SnapFacing: 1
            Characters:
              - Id: grunt
                CurrencyReward: 1.5
            """)
        found = self.problems()
        self.assertTrue(any(":3:" in t and "must be a number" in t for t in found), found)
        self.assertTrue(any(":4:" in t and "true or false" in t for t in found), found)
        self.assertTrue(any(":7:" in t and "whole number" in t for t in found), found)

    def test_range_from_field_notes(self):
        self.put("Game/data/a.yaml", """
            Weapons:
              - Id: blade
                GuardDamageReduction: 1.5
            """)
        self.assertProblem("Game/data/a.yaml:3", "maximum is 1")

    def test_missing_required_fields(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - StaminaCost: 5
            """)
        found = self.problems()
        self.assertTrue(any("missing 'Id'" in t for t in found), found)
        self.assertTrue(any("missing 'Duration'" in t for t in found), found)

    def test_unknown_section(self):
        self.put("Game/data/a.yaml", """
            Monsters:
              - Id: grunt
            """)
        self.assertProblem("Game/data/a.yaml:1", "unknown section 'Monsters'")


class References(ProjectCase):
    def test_unknown_action_points_at_the_line_that_names_it(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("Light: swing", "Light: swnig"))
        self.assertProblem("Game/data/a.yaml:11", "weapon 'blade'", "'swnig'")

    def test_unknown_weapon_and_profile(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("Weapon: blade", "Weapon: axe").replace("AIProfile: grunt_ai", "AIProfile: boss_ai"))
        found = self.problems()
        self.assertTrue(any("weapon 'axe'" in t for t in found), found)
        self.assertTrue(any("AI profile 'boss_ai'" in t for t in found), found)

    def test_duplicate_id_across_files(self):
        self.put("Game/data/a.yaml", VALID_DATA)
        self.put("Game/data/b.yaml", """
            Actions:
              - Id: swing
                Duration: 1
            """)
        self.assertProblem("Game/data/b.yaml:2", "already defined", "Game/data/a.yaml:2")

    def test_definitions_may_live_in_different_files(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
            """)
        self.put("Game/data/sub/b.yaml", """
            Weapons:
              - Id: blade
                Actions: { Light: swing }
            """)
        self.assertClean()

    def test_unknown_slot(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("Death: die", "Deth: die"))
        self.assertProblem("Game/data/a.yaml:17", "unknown slot 'Deth'")

    def test_window_outside_the_action(self):
        self.put("Game/data/a.yaml", VALID_DATA.replace("Start: 0.3, End: 0.5", "Start: 1.3, End: 1.5"))
        self.assertTrue(any(":5:" in t and "outside the action" in t for t in self.problems()))

    def test_held_action_may_have_open_ended_windows(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: guard
                Duration: 0
                Windows:
                  - { Type: PerfectGuard, Start: 0, End: 0.2 }
                  - { Type: Guard, Start: 0 }
            """)
        self.assertClean()

    def test_auto_next_loop(self):
        self.put("Game/data/a.yaml", """
            Actions:
              - Id: one
                Duration: 1
                AutoNext: two
              - Id: two
                Duration: 1
                AutoNext: one
            """)
        self.assertTrue(any("endless AutoNext loop" in t for t in self.problems()))

    def test_phases_must_descend(self):
        self.put("Game/data/a.yaml", VALID_DATA + """
    Phases:
      - { HealthBelow: 0.3 }
      - { HealthBelow: 0.6 }
""")
        self.assertProblem("Game/data/a.yaml:25", "higher HealthBelow to lower")

    def test_profile_without_moves(self):
        self.put("Game/data/a.yaml", """
            AIProfiles:
              - Id: lazy
            """)
        self.assertProblem("Game/data/a.yaml:2", "no Moves")


class Output(ProjectCase):
    def test_json_uses_engine_names_and_keeps_only_what_was_written(self):
        self.put("Game/data/pack/a.yaml", """
            Actions:
              - Id: swing
                Duration: 1
                SnapFacing: true
            """)
        build = self.build()
        self.assertEqual(build.problems, [])
        document = json.loads(build.outputs["Content/Data/pack/a.json"])
        self.assertIn("Game/data/pack/a.yaml", document["_generated"])
        self.assertEqual(document["Actions"], [{"Id": "swing", "Duration": 1, "bSnapFacing": True}])

    def test_write_then_check_then_orphan_removal(self):
        self.put("Game/data/a.yaml", VALID_DATA)
        build = self.build()
        self.assertIn("Content/Data/a.json", project.stale_files(self.root, build))
        project.write(self.root, build)
        self.assertEqual(project.stale_files(self.root, self.build()), [])

        os.remove(os.path.join(self.root, "Game/data/a.yaml"))
        self.put("Content/Data/handmade.json", '{"Actions": []}')
        build = self.build()
        self.assertEqual(project.stale_files(self.root, build), ["Content/Data/a.json"])
        project.write(self.root, build)
        self.assertFalse(os.path.exists(os.path.join(self.root, "Content/Data/a.json")))
        self.assertTrue(os.path.exists(os.path.join(self.root, "Content/Data/handmade.json")), "hand-written files are never deleted")

    def test_nothing_is_written_by_run(self):
        self.put("Game/data/a.yaml", VALID_DATA)
        self.build()
        self.assertFalse(os.path.exists(os.path.join(self.root, "Content")))


class Repository(unittest.TestCase):
    """The real project: its manifests are valid and the committed generated files match them."""

    def test_shipped_manifests_are_valid(self):
        build = project.run(REPO)
        self.assertEqual([str(problem) for problem in build.problems], [])

    def test_generated_files_are_up_to_date(self):
        build = project.run(REPO)
        self.assertEqual(project.stale_files(REPO, build), [], "run: python Tools/build_data.py")

    def test_every_field_is_described(self):
        build = project.run(REPO)
        from osdata import reference
        for struct_name in reference.documented_structs():
            for field in build.schema.structs[struct_name].fields.values():
                self.assertTrue(field.doc, "{}.{} has no description".format(struct_name, field.name))


if __name__ == "__main__":
    unittest.main()
