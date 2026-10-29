# Obsidian Shrine

Souls-like action RPG tech demo. Unreal Engine 5 (C++), target platform Win64 only.

## Roles and machines

- The user directs and does manual QA only. Claude writes everything: code, configs, content scripts, docs, test plans.
- **Mac (development):** code is written here. Unreal is NOT installed there and nothing is built there.
- **Windows (build/test):** has Unreal Engine and its own Claude Code session. That session pulls, runs `Scripts\Verify.ps1`,
  fixes build/test breakages it can fix, and commits. The user plays the build there.
- Sync is via the GitHub repo. After a Windows run, read `Saved\Reports\summary.md` first.

## Rules

- No Blueprints and no hand-edited binary assets. Gameplay is C++; every asset and level is produced by Python scripts in
  `Content/Python` (entry: `build_content.py`), so `Content/` can be rebuilt from text. The only accepted exception is
  Animation Blueprints if a C++/script-only route proves impossible — decide explicitly, do not drift into it.
- Data (weapons, stats, enemy movesets) lives in data tables/JSON, not hard-coded.
- Every gameplay change ships with an automation test under `Source/ObsidianShrine/Private/Tests`, test names
  `ObsidianShrine.<Area>.<Name>`.
- Assets from Fab/Marketplace are imported on the Windows machine into `Content/ThirdParty/` and are not committed.
- Free assets only, unless the user explicitly approves a paid one.
- C++ naming: `OS` prefix for project types (`AOSGameMode`), `LogObsidian` log category, Public/Private layout by area.
- Scripts must run on Windows PowerShell 5.1.

## Git

- Commits are authored by the user only (DantesChmo <doni-rio@yandex.ru>). Never add Claude attribution: no
  `Co-Authored-By` trailers, no "Generated with" lines, in commits or PRs.
- Binary files go through Git LFS (see `.gitattributes`).

## Commands (Windows)

```powershell
.\Scripts\Verify.ps1          # build + content + tests, writes Saved\Reports\summary.md
.\Scripts\Build.ps1           # compile ObsidianShrineEditor Development
.\Scripts\BuildContent.ps1    # run the Python content pipeline
.\Scripts\RunTests.ps1        # automation tests, filter "ObsidianShrine"
```

Set `$env:UE_ROOT` if the engine is not found automatically.

## Milestones

M0 infrastructure → M1 player (camera, lock-on, dodge, light/heavy attacks, stamina, guard/perfect guard) →
M2 regular enemy → M3 two-phase boss → M4 checkpoint/death/currency/save loop → M5 visual pass.
