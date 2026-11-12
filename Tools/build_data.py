"""Builds the game's data from the YAML manifests in Game/.

    python Tools/build_data.py            validate, then write the generated JSON and Docs/REFERENCE.md
    python Tools/build_data.py --check    validate and fail if the generated files are out of date; writes nothing

Runs on any Python 3.11+, including the one bundled with Unreal Engine. Exit code 1 means problems were found.
"""

import argparse
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from osdata import project  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="do not write; fail if generated files are stale")
    arguments = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build = project.run(root)

    if not build.ok:
        for problem in build.problems:
            print(problem)
        print("\n{} problem(s). Nothing was written.".format(len(build.problems)))
        return 1

    if arguments.check:
        stale = project.stale_files(root, build)
        if stale:
            print("Generated files are out of date; run: python Tools/build_data.py")
            for path in stale:
                print("  " + path)
            return 1
        print("Data OK, generated files are up to date.")
        return 0

    changed = project.write(root, build)
    for path in changed:
        print("updated " + path)
    print("Data OK: {} file(s) generated, {} changed.".format(len(build.outputs), len(changed)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
