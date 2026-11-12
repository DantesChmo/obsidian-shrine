"""Tests of asset bindings: logical names in the data become object paths, and only known names of the right
type are accepted."""

import json
import unittest

from support import VALID_DATA, ProjectCase

ASSETS = """
Packs:
  - { Id: hero, Root: /Game/ThirdParty/Hero }
Assets:
  - { Id: hero_body, Type: SkeletalMesh, Pack: hero, Path: Meshes/Hero }
  - { Id: hero_swing, Type: Animation, Pack: hero, Path: Animations/Swing_A }
"""

WITH_MESH = VALID_DATA.replace("    Weapon: blade\n", "    Weapon: blade\n    Mesh: hero_body\n")


class Resolving(ProjectCase):
    def test_name_becomes_object_path(self):
        self.put("Game/assets.yaml", ASSETS)
        self.put("Game/data/a.yaml", WITH_MESH)
        build = self.build()
        self.assertEqual([str(problem) for problem in build.problems], [])
        character = json.loads(build.outputs["Content/Data/a.json"])["Characters"][0]
        self.assertEqual(character["Mesh"], "/Game/ThirdParty/Hero/Meshes/Hero.Hero")

    def test_cook_list_has_only_used_assets(self):
        self.put("Game/assets.yaml", ASSETS)
        self.put("Game/data/a.yaml", WITH_MESH)
        cook = json.loads(self.build().outputs["Content/AssetData/cook.json"])
        self.assertEqual(cook["Assets"], ["/Game/ThirdParty/Hero/Meshes/Hero.Hero"])

    def test_no_assets_file_means_no_assets(self):
        self.put("Game/data/a.yaml", VALID_DATA)
        self.assertClean()
        self.assertEqual(json.loads(self.build().outputs["Content/AssetData/cook.json"])["Assets"], [])


class Validation(ProjectCase):
    def test_unknown_name(self):
        self.put("Game/assets.yaml", ASSETS)
        self.put("Game/data/a.yaml", WITH_MESH.replace("Mesh: hero_body", "Mesh: hero_bdy"))
        self.assertProblem("Game/data/a.yaml:15", "asset 'hero_bdy'", "Game/assets.yaml")

    def test_a_path_is_not_a_name(self):
        self.put("Game/assets.yaml", ASSETS)
        self.put("Game/data/a.yaml", WITH_MESH.replace("Mesh: hero_body", "Mesh: /Game/ThirdParty/Hero/Meshes/Hero"))
        self.assertProblem("Game/data/a.yaml:15", "not in Game/assets.yaml")

    def test_wrong_type(self):
        self.put("Game/assets.yaml", ASSETS)
        self.put("Game/data/a.yaml", WITH_MESH.replace("Mesh: hero_body", "Mesh: hero_swing"))
        self.assertProblem("Game/data/a.yaml:15", "needs a SkeletalMesh", "'hero_swing' is a Animation")

    def test_unknown_pack(self):
        self.put("Game/assets.yaml", ASSETS.replace("Pack: hero, Path: Meshes", "Pack: villain, Path: Meshes"))
        self.assertProblem("Game/assets.yaml:4", "pack 'villain'")

    def test_bad_root(self):
        self.put("Game/assets.yaml", ASSETS.replace("/Game/ThirdParty/Hero", "Content/ThirdParty/Hero"))
        self.assertProblem("Game/assets.yaml:2", "content folder")

    def test_bad_path(self):
        self.put("Game/assets.yaml", ASSETS.replace("Path: Meshes/Hero", "Path: /Meshes/Hero.uasset"))
        self.assertProblem("Game/assets.yaml:4", "no leading slash")

    def test_unknown_type(self):
        self.put("Game/assets.yaml", ASSETS.replace("Type: SkeletalMesh", "Type: Mesh"))
        self.assertProblem("Game/assets.yaml:4", "must be one of")

    def test_duplicate_asset(self):
        self.put("Game/assets.yaml", ASSETS + "  - { Id: hero_body, Type: SkeletalMesh, Pack: hero, Path: Meshes/Other }\n")
        self.assertProblem("Game/assets.yaml:6", "asset 'hero_body' is defined twice")


if __name__ == "__main__":
    unittest.main()
