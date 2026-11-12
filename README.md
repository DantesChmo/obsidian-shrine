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
| `Content/Data/*.json` | Все игровые данные: действия, оружие, персонажи, ИИ |
| `Content/Python` | Пайплайн контента: уровни и ассеты создаются скриптами |
| `Scripts/*.ps1` | Сборка, контент, тесты для Windows |
| `Config` | Настройки движка и проекта |

## Управление

| Действие | Клавиатура / мышь | Геймпад |
|---|---|---|
| Движение / камера | WASD / мышь | левый / правый стик |
| Лёгкая атака (комбо из трёх) | ЛКМ | RB |
| Тяжёлая атака | F | RT |
| Блок (нажать прямо перед ударом — идеальный блок) | ПКМ (держать) | LB (держать) |
| Уклонение | Пробел | B (коротко) |
| Бег | Shift (держать) | B (держать) |
| Захват цели / сменить цель | Q или СКМ / колесо мыши | R3 / крестовина влево-вправо |
| Взаимодействие (святилище, потерянные осколки) | E | A |

Консоль: `os.DebugCombat 1` — показать зоны поражения, `os.ResetSave` — стереть прогресс.

Пока ассеты не импортированы, персонажи — цветные цилиндры: синий игрок, красные враги, фиолетовый босс.
«Нос» показывает, куда смотрит персонаж; планка-оружие поднимается на замахе, ложится вперёд на длину реальной
зоны удара в момент попадания и держится поперёк груди в блоке. Белеет персонаж в кадрах неуязвимости.

## Как добавлять контент

Код менять не нужно — только JSON в `Content/Data` (любой файл может содержать любые из массивов `Actions`,
`Weapons`, `Characters`, `AIProfiles`; удобно держать один файл на врага) и скрипт уровня. Все поля с комментариями —
в `Source/ObsidianShrine/Public/Data/OSDataTypes.h`.

- **Действие** (`Actions`) — таймлайн в секундах: `Duration`, `StaminaCost`, окна `Windows`
  (`Hit`, `Invulnerable`, `Guard`, `PerfectGuard`, `HyperArmor`, `Cancel`), рывок `MoveDistance/MoveStart/MoveEnd`,
  продолжение комбо `ComboNext`, автоцепочка `AutoNext`, анимация `Animation` (растягивается под `Duration`).
- **Оружие** (`Weapons`) — урон, длина и радиус зоны удара, параметры блока, слоты `Light/Heavy/Guard` → действия.
- **Персонаж** (`Characters`) — статы, оружие, слоты реакций (`Dodge`, `Stagger`, `GuardBreak`, `Parried`, `Death`),
  `AIProfile`, награда, `bBoss`, меш и анимации (`Mesh`, `AnimClass`, `IdleAnimation`, `MoveAnimation`).
- **ИИ** (`AIProfiles`) — радиусы, набор атак `Moves` с дистанцией, весом, перезарядкой и фазой; фазы босса `Phases`
  по порогу здоровья с действием на входе.
- **Расстановка на уровне** — в скрипте уровня (`Content/Python/os_content/*_level.py`):
  `level_utils.spawn_enemy("husk", ...)`, `spawn_enemy(..., persistent_id="...")` для босса,
  `level_utils.spawn_checkpoint("id", ...)`.

Ошибки в данных (опечатка в id, окно за пределами действия и т. п.) ловит тест
`ObsidianShrine.Data.ShippedDataIsValid` и лог при старте.
