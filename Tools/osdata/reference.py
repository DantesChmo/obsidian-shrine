"""Generates Docs/REFERENCE.md: every field a manifest may contain, with type, default, range and meaning."""

# (heading, struct, where it is written, intro)
SECTIONS = [
    ("Действие", "FOSActionDef", "`Game/data/*.yaml`, секция `Actions`",
     "Одно действие персонажа: атака, уклонение, блок, реакция на удар, смерть. Это таймлайн в секундах: "
     "что и когда происходит, задают окна `Windows`."),
    ("Окно действия", "FOSActionWindow", "внутри `Windows` у действия",
     "Отрезок времени внутри действия, в течение которого что-то включено."),
    ("Оружие", "FOSWeaponDef", "`Game/data/*.yaml`, секция `Weapons`",
     "Урон, размер зоны удара, свойства блока и то, какие действия вызывают кнопки."),
    ("Персонаж", "FOSCharacterDef", "`Game/data/*.yaml`, секция `Characters`",
     "Игрок, враг или босс: характеристики, оружие, реакции, внешний вид."),
    ("Профиль ИИ", "FOSAIProfileDef", "`Game/data/*.yaml`, секция `AIProfiles`",
     "Поведение врага: когда замечает игрока, чем и с какой дистанции атакует, какие у него фазы."),
    ("Атака в профиле ИИ", "FOSAIMoveDef", "внутри `Moves` у профиля ИИ",
     "Одна атака из набора врага и условия, при которых он может её выбрать."),
    ("Фаза босса", "FOSAIPhaseDef", "внутри `Phases` у профиля ИИ",
     "Фаза начинается, когда здоровье падает до порога. Фазы идут по убыванию порога."),
]


SETTINGS_SECTIONS = [
    ("Настройки игры", "FOSGameDef", "`Game/game.yaml`",
     "Общие настройки: кем играет игрок, с какой карты начинается игра, камера, захват цели, правила смерти."),
    ("Камера", "FOSCameraDef", "раздел `Camera` в `Game/game.yaml`",
     "Камера от третьего лица, пока цель не захвачена."),
    ("Захват цели", "FOSLockOnDef", "раздел `LockOn` в `Game/game.yaml`",
     "Когда цель можно захватить, когда захват срывается и как ведёт себя камера, пока он держится."),
    ("Отклик управления", "FOSControlsDef", "раздел `Controls` в `Game/game.yaml`",
     "Тайминги ввода."),
    ("Смерть и возрождение", "FOSDeathDef", "раздел `Death` в `Game/game.yaml`",
     "Что происходит, когда игрок или враг погибает."),
    ("Сообщения", "FOSMessagesDef", "раздел `Messages` в `Game/game.yaml`",
     "Сколько секунд держатся крупные надписи по центру экрана. Надпись о смерти висит до возрождения."),
    ("Управление", "FOSInputDef", "`Game/input.yaml`",
     "Какие клавиши и кнопки геймпада что делают. У каждого действия — список; пустой список `[]` отключает действие. "
     "Одна клавиша — одно действие. Имена клавиш — как в движке: буквы `A`…`Z`, цифры `Zero`…`Nine`, `SpaceBar`, "
     "`LeftShift`, `LeftMouseButton`, `MouseScrollUp`; геймпад — `Gamepad_FaceButton_Bottom` (A), `_Right` (B), "
     "`_Left` (X), `_Top` (Y), `Gamepad_LeftShoulder` (LB), `Gamepad_RightTrigger` (RT), `Gamepad_RightThumbstick` "
     "(R3), `Gamepad_DPad_Left`. Полный список — `Tools/osdata/keys.py`; опечатку компилятор покажет и подскажет."),
    ("Тексты", "FOSTextsDef", "`Game/texts.yaml`",
     "Все надписи, которые видит игрок. В фигурных скобках — места, куда игра подставляет значение; "
     "использовать можно только те, что есть в тексте по умолчанию."),
]


def all_sections():
    from . import levels
    return SECTIONS + levels.REFERENCE_SECTIONS + SETTINGS_SECTIONS


SLOT_NOTES = [
    ("Light", "лёгкая атака", "оружие"),
    ("Heavy", "тяжёлая атака", "оружие"),
    ("Guard", "блок (удерживаемое действие)", "оружие"),
    ("Dodge", "уклонение", "персонаж"),
    ("Stagger", "реакция на пробитый баланс", "персонаж"),
    ("GuardBreak", "реакция на пробитый блок", "персонаж"),
    ("Parried", "реакция атакующего на идеальный блок противника", "персонаж"),
    ("Death", "смерть", "персонаж"),
]


def documented_structs():
    return [section[1] for section in all_sections()]


def _format_default(field):
    kind = field.type.kind
    if field.required:
        return "обязательно"
    if kind in ("array", "map", "struct") or field.default in ("", None):
        return "—"
    if kind == "bool":
        return "true" if field.default else "false"
    if kind == "float":
        return "{:g}".format(field.default)
    if kind == "string":
        return "`{}`".format(field.default.replace(" ", "\u00a0"))
    return str(field.default)


def _format_type(field, schema):
    type_ = field.type
    names = {"name": "id", "string": "текст", "float": "число", "int": "целое", "bool": "true / false", "vector": "[x, y, z]"}

    def one(t):
        if t.kind == "enum":
            return " / ".join("`{}`".format(value) for value in schema.enums[t.ref])
        if t.kind == "struct":
            return "см. «{}»".format(next((s[0] for s in all_sections() if s[1] == t.ref), t.ref))
        if t.kind == "array":
            return "список: {}".format(one(t.item))
        if t.kind == "map":
            return "{} → {}".format("слот" if field.name == "Actions" else "имя", one(t.item))
        return names[t.kind]

    return one(type_)


def _format_range(field):
    if field.minimum is not None and field.maximum is not None:
        return "{:g} … {:g}".format(field.minimum, field.maximum)
    if field.minimum is not None:
        return "≥ {:g}".format(field.minimum)
    if field.maximum is not None:
        return "≤ {:g}".format(field.maximum)
    return ""


def render(schema, sections=None):
    lines = [
        "# Справочник полей",
        "",
        "Этот файл создаётся скриптом `Tools/build_data.py` из схемы данных — руками его не правят.",
        "Описания и допустимые диапазоны лежат в `Game/schema/fields.yaml`.",
        "",
        "Общие правила записи:",
        "",
        "- время — в секундах, расстояния — в сантиметрах, углы — в градусах;",
        "- `id` — короткое имя латиницей без пробелов (`husk_slash`); по нему сущности ссылаются друг на друга;",
        "- текст, который начинается с `#` или похож на число либо на `yes` / `no`, берётся в кавычки: `\"#B0503A\"`;",
        "- поле можно не писать — тогда действует значение по умолчанию.",
        "",
    ]

    for heading, struct_name, where, intro in (sections or all_sections()):
        struct = schema.structs[struct_name]
        lines += ["## {}".format(heading), "", "Где пишется: {}.".format(where), "", intro, "",
                  "| Поле | Тип | По умолчанию | Диапазон | Что значит |", "|---|---|---|---|---|"]
        for field in struct.fields.values():
            lines.append("| `{}` | {} | {} | {} | {} |".format(
                field.name, _format_type(field, schema), _format_default(field), _format_range(field),
                field.doc.replace("|", "\\|").replace("\n", " ")))
        lines.append("")

        if struct_name == "FOSCharacterDef":
            lines += ["### Слоты действий", "",
                      "Поле `Actions` у оружия и персонажа связывает слот с действием. У персонажа слот перекрывает "
                      "такой же слот оружия.", "",
                      "| Слот | Когда срабатывает | Где обычно задаётся |", "|---|---|---|"]
            lines += ["| `{}` | {} | {} |".format(*note) for note in SLOT_NOTES]
            lines.append("")

    return "\n".join(lines)
