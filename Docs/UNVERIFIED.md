# Не проверено запуском

Всё, что написано на Mac и ещё ни разу не собиралось и не запускалось в Unreal. Это рабочий список для первой
сессии на Windows: идти сверху вниз, вычёркивать проверенное.

Проверено на Mac и в этот список не входит: компилятор манифестов и его тесты (`Tools/`), корректность всех
манифестов в `Game/`, совпадение расстановки песочницы из манифеста со старым скриптом.

## Сборка C++

Весь модуль `Source/ObsidianShrine` (около 3600 строк) ни разу не компилировался.
Наиболее вероятные места ошибок:

- `Private/Characters/OSPlayerController.cpp` — создание действий ввода и модификаторов в коде;
- `Private/Characters/OSCharacterBase.cpp` — анимации без Animation Blueprint, динамический материал заглушки;
- `Private/Data/OSDataSubsystem.cpp` — разбор JSON в структуры;
- `Private/UI/OSHUD.cpp` — рисование на холсте.

## Автотесты

- Все тесты `ObsidianShrine.*` написаны вслепую.
- `ObsidianShrine.World.AttackHitsOnlyEnemiesInFront` создаёт игровой мир вручную — самый хрупкий тест.

## Сборка уровней в редакторе

- `Content/Python/os_content/level_builder.py` и `level_utils.py` обращаются к Python-API редактора, которое
  на Mac подменялось заглушкой.
- Если `level_builder` не отработает: в `Content/Python/build_content.py` временно вернуть
  `sandbox_level.build` — это прежний скрипт песочницы, он оставлен как запасной вариант.
- После первого успешного прогона удалить `os_content/sandbox_level.py` и тест
  `BuilderMatchesHandWrittenScript` в `Tools/tests/test_levels.py`.

## Скрипты Windows

- `Scripts/BuildData.ps1` — путь к Python из поставки движка
  (`Engine\Binaries\ThirdParty\Python3\Win64\python.exe`) взят по памяти.
- Остальные скрипты `Scripts/*.ps1` тоже ни разу не запускались.

## Игра

- Баланс всех врагов и боссов выставлен без единого запуска: урон, тайминги, дистанции атак.
- Второй уровень `L_AshenCrypt` пока открывается только командой консоли `open L_AshenCrypt`:
  переходов между картами нет (этап 2 роадмапа).
- Упаковка игры не пробовалась. Пути к ассетам в данных — строки; сборщик релиза может их не увидеть.
