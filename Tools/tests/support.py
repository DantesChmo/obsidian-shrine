"""Shared helpers for the tool tests: build a throwaway project and collect what the compiler says about it."""

import os
import shutil
import sys
import tempfile
import textwrap
import unittest

sys.dont_write_bytecode = True
TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)

from osdata import project  # noqa: E402

# A complete, valid data file that individual tests extend or break.
VALID_DATA = """
Actions:
  - Id: swing
    Duration: 1.0
    Windows:
      - { Type: Hit, Start: 0.3, End: 0.5 }
  - Id: die
    Duration: 2.0
Weapons:
  - Id: blade
    Actions:
      Light: swing
Characters:
  - Id: grunt
    Weapon: blade
    AIProfile: grunt_ai
    Actions:
      Death: die
AIProfiles:
  - Id: grunt_ai
    Moves:
      - { Action: swing, MinRange: 0, MaxRange: 200 }
"""


class ProjectCase(unittest.TestCase):
    """Each test gets an empty project that shares the real C++ header and field notes."""

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="os_tools_test_")
        self.addCleanup(shutil.rmtree, self.root, True)
        for shared in (project.HEADER_PATH, project.FIELDS_PATH):
            target = os.path.join(self.root, shared)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy(os.path.join(REPO, shared), target)

    def put(self, path, text):
        full = os.path.join(self.root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as handle:
            handle.write(textwrap.dedent(text).lstrip("\n"))

    def build(self):
        return project.run(self.root)

    def problems(self):
        return [str(problem) for problem in self.build().problems]

    def assertClean(self):
        self.assertEqual(self.problems(), [])

    def assertProblem(self, where, *fragments):
        """Exactly one problem is reported at `where` ("file:line") and it mentions every fragment."""
        found = self.problems()
        matching = [text for text in found if text.startswith(where + ": ")]
        self.assertEqual(len(matching), 1, "expected one problem at {}, got:\n{}".format(where, "\n".join(found) or "(none)"))
        for fragment in fragments:
            self.assertIn(fragment, matching[0])
        return matching[0]
