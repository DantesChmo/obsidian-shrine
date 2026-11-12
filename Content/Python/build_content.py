"""Entry point for the content pipeline.

Run headless via Scripts/BuildContent.ps1, or inside the editor:
    py build_content.py

Writes Saved/Reports/content_status.json so the runner can tell success
from failure regardless of the editor's exit code. When launched with
-unattended the editor quits after the build.
"""

import json
import os
import traceback

import unreal

from os_content import asset_inventory, cook_label, level_builder

# A builder may return {"notes": [...]}: things to know that are not failures (shown in the summary).
BUILDERS = [
    ("levels", level_builder.build_all),
    ("cook_label", cook_label.build),
    ("asset_inventory", asset_inventory.build),
]


def _write_status(status):
    report_dir = os.path.join(unreal.Paths.project_saved_dir(), "Reports")
    os.makedirs(report_dir, exist_ok=True)
    with open(os.path.join(report_dir, "content_status.json"), "w") as handle:
        json.dump(status, handle, indent=2)


def main():
    status = {"ok": True, "builders": []}
    for name, builder in BUILDERS:
        try:
            result = builder()
            entry = {"name": name, "ok": True}
            if isinstance(result, dict):
                entry.update(result)
            status["builders"].append(entry)
        except Exception:
            error = traceback.format_exc()
            unreal.log_error("os_content: builder '{}' failed:\n{}".format(name, error))
            status["ok"] = False
            status["builders"].append({"name": name, "ok": False, "error": error})
            break
    _write_status(status)
    return status["ok"]


main()

if "-unattended" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
