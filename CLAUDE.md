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
  `Content/Python` (entry: `build_content.py`), so `Content/` can be rebuilt from text. Decided in stage 3: no
  Animation Blueprints either — meshes play one animation at a time in single-node mode from the character's
  animation set.
- Game content is authored as YAML manifests in `Game/` (definitions in `Game/data`, levels in `Game/levels`,
  settings / key bindings / texts in `Game/game.yaml`, `input.yaml`, `texts.yaml`) and
  compiled by `python3 Tools/build_data.py` into `Content/Data` and `Content/LevelData`. Never edit the generated
  JSON; commit it together with the YAML. Run the compiler and `python3 -B -m unittest discover -s Tools/tests`
  after every manifest or tool change: both run on the Mac.
- No gameplay numbers, key names or player-visible texts in C++: they are fields of `FOSGameDef`, `FOSInputDef`,
  `FOSTextsDef` or of a definition. What deliberately stays in code is listed in `Docs/ROADMAP.md`.
- A manifest field exists only together with the code that executes it. New enemies, weapons, attacks and levels
  must not need C++ or Python changes; a new mechanic is coded once and then exposed as a field.
- Field descriptions and ranges live in `Game/schema/fields.yaml` (Russian, user-facing); `Docs/REFERENCE.md` is
  generated from them. Field names, types and defaults come from `Public/Data/OSDataTypes.h`.
- Anything written but not yet run on Windows is listed in `Docs/UNVERIFIED.md`; keep it current.
- Every gameplay change ships with an automation test under `Source/ObsidianShrine/Private/Tests`, test names
  `ObsidianShrine.<Area>.<Name>`.
- Assets from Fab/Marketplace are imported on the Windows machine into `Content/ThirdParty/` and are not committed.
- Free assets only, unless the user explicitly approves a paid one.
- C++ naming: `OS` prefix for project types (`AOSGameMode`), `LogObsidian` log category, Public/Private layout by area.
- Scripts must run on Windows PowerShell 5.1.

## Architecture

- No GAS. `UOSDataSubsystem` (engine subsystem) loads the generated JSON; structs are in `Public/Data/OSDataTypes.h`.
  Read settings where they are used through the static `UOSDataSubsystem::Settings()` / `Bindings()` / `Texts()`
  (never copy them into members), so `os.ReloadData` takes effect at once. Whoever does keep a copy of a
  definition (characters, AI brain, key mappings) refreshes it on `OnReloaded`, keeping the running state.
- Data tools: `Tools/osdata` (`source` YAML with line numbers, `schema` from the C++ header, `shape` type checks,
  `rules` cross-references, `levels` level manifests, `settings` game/input/texts and the start map in
  `Config/DefaultEngine.ini`, `keys` valid key names, `reference` docs). Vendored PyYAML in `Tools/vendor`;
  keep the tools on Python 3.11 syntax (the engine's bundled Python runs them on Windows).
- An action is a timeline of windows (Hit, Invulnerable, Guard, PerfectGuard, HyperArmor, Cancel) run by
  `UOSActionComponent`; gameplay timing comes from data, never from animation notifies. Animations are optional,
  come from the character's animation set (`AnimationSets`) and are stretched to the action's duration.
- Data never holds asset paths: asset fields (marked `asset:` in `Game/schema/fields.yaml`) take logical names from
  `Game/assets.yaml`, which the compiler turns into object paths and lists in `Content/AssetData/cook.json`.
  Exact paths inside imported packs come from `Docs/ASSET_INVENTORY.md`, written by the content pipeline on Windows.
- Presentation (sound, effect, camera shake, hit-stop) is `Cosmetics` data played by `UOSCosmeticsComponent`.
- `UOSStatsComponent`, `UOSActionComponent`, `UOSCombatComponent` expose `Advance()`/pure functions and work without
  an actor or world; test them that way (`Private/Tests/OSTestHelpers.h`). Use a real world only for scene queries.
- `AOSCharacterBase` is fully defined by `CharacterId`; `AOSEnemyCharacter` + `UOSAIBrainComponent` cover regular
  enemies and bosses alike. Without meshes characters render as placeholders.
- Input actions are built in code in `AOSPlayerController`, their keys come from `Game/input.yaml`; the HUD is
  canvas-drawn (`AOSHUD`). No input or UI assets.
- Death loop and save: `AOSGameMode` + `UOSProgressSubsystem` (`FOSProgressState`).
- Levels are built by `os_content/level_builder.py` from `Content/LevelData/*.json`; it only places what the
  resolved manifest lists. Roadmap: `Docs/ROADMAP.md`.

## Git

- Commits are authored by the user only (DantesChmo <doni-rio@yandex.ru>). Never add Claude attribution: no
  `Co-Authored-By` trailers, no "Generated with" lines, in commits or PRs.
- Binary files go through Git LFS (see `.gitattributes`).

## Commands (Windows)

```powershell
.\Scripts\Verify.ps1          # data + build + content + tests, writes Saved\Reports\summary.md
.\Scripts\BuildData.ps1       # validate manifests, check generated JSON, run tool tests (-Write to regenerate)
.\Scripts\Build.ps1           # compile ObsidianShrineEditor Development
.\Scripts\BuildContent.ps1    # run the Python content pipeline
.\Scripts\RunTests.ps1        # automation tests, filter "ObsidianShrine"
```

Set `$env:UE_ROOT` if the engine is not found automatically.

## Milestones

M0 infrastructure → M1 player (camera, lock-on, dodge, light/heavy attacks, stamina, guard/perfect guard) →
M2 regular enemy → M3 two-phase boss → M4 checkpoint/death/currency/save loop → M5 visual pass.
