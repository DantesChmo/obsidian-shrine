# Obsidian Shrine

Технодемо souls-like RPG на Unreal Engine 5 (C++).

## Первый запуск на Windows

1. Установить через Epic Games Launcher **Unreal Engine 5.7** (или поправить `EngineAssociation` в
   `ObsidianShrine.uproject` под установленную версию).
2. Установить **Visual Studio 2022** с workload «Game development with C++» (MSVC, Windows SDK) — Unreal подскажет
   точные версии, если чего-то не хватит.
3. Установить **Git LFS**: `git lfs install`.
4. Клонировать репозиторий и в корне выполнить:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\Verify.ps1
```

Скрипт собирает проект, генерирует контент (уровень `L_Sandbox`) и прогоняет автотесты. Итог —
`Saved\Reports\summary.md`.

Проще всего попросить Claude Code на Windows: «спуллить, прогнать Verify.ps1 и починить, что сломалось».

## Что внутри

| Путь | Что |
|---|---|
| `Source/ObsidianShrine` | C++ код игры |
| `Source/ObsidianShrine/Private/Tests` | Автотесты |
| `Content/Python` | Пайплайн контента: уровни и ассеты создаются скриптами |
| `Scripts/*.ps1` | Сборка, контент, тесты для Windows |
| `Config` | Настройки движка и проекта |
