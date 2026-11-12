"""Rules that span definitions: unique ids, references, timelines that make sense.

The engine repeats the reference checks at startup (UOSDataSubsystem::Validate) as a safety net; this is the
place that reports them with a file and a line, before anything is built.
"""

from .source import Problem

# Section of a data file -> (C++ struct, what a human calls it)
SECTIONS = {
    "Actions": ("FOSActionDef", "action"),
    "Weapons": ("FOSWeaponDef", "weapon"),
    "Characters": ("FOSCharacterDef", "character"),
    "AIProfiles": ("FOSAIProfileDef", "AI profile"),
}

# Slots the engine itself triggers; anything else in a slot map is a typo.
SLOTS = ("Light", "Heavy", "Guard", "Dodge", "Stagger", "GuardBreak", "Parried", "Death")


class Entry:
    """One definition with where it came from."""

    def __init__(self, data, source, file):
        self.data = data        # converted, engine field names
        self.source = source    # LMap as written, YAML field names
        self.file = file

    @property
    def id(self):
        return self.data.get("Id")

    def line(self, key=None):
        return self.source.line_of(key) if key else self.source.line


class Registry:
    def __init__(self):
        self.sections = {name: {} for name in SECTIONS}

    def add(self, section, entry, problems):
        kind = SECTIONS[section][1]
        if not entry.id:
            return
        existing = self.sections[section].get(entry.id)
        if existing:
            problems.append(Problem(entry.file, entry.line("Id"), "{} '{}' is already defined at {}:{}".format(
                kind, entry.id, existing.file, existing.line("Id"))))
            return
        self.sections[section][entry.id] = entry


def check(registry, problems):
    actions = registry.sections["Actions"]
    weapons = registry.sections["Weapons"]
    characters = registry.sections["Characters"]
    profiles = registry.sections["AIProfiles"]

    def report(entry, line, message):
        problems.append(Problem(entry.file, line, message))

    def check_action(entry, line, action_id, owner):
        if action_id and action_id not in actions:
            report(entry, line, "{} uses action '{}', which is not defined anywhere".format(owner, action_id))

    def check_slots(entry, kind):
        slots = entry.source.get("Actions")
        if not isinstance(slots, dict):
            return
        for slot, action_id in slots.items():
            line = slots.line_of(slot)
            if slot not in SLOTS:
                report(entry, line, "{} '{}' has unknown slot '{}'; slots are {}".format(kind, entry.id, slot, ", ".join(SLOTS)))
            if isinstance(action_id, str):
                check_action(entry, line, action_id, "{} '{}'".format(kind, entry.id))

    for entry in actions.values():
        data = entry.data
        owner = "action '{}'".format(entry.id)
        duration = data.get("Duration", 0.5)
        held = duration <= 0

        check_action(entry, entry.line("ComboNext"), data.get("ComboNext"), owner)
        check_action(entry, entry.line("AutoNext"), data.get("AutoNext"), owner)
        if held and data.get("AutoNext"):
            report(entry, entry.line("AutoNext"), "{} is held (Duration 0), so it never reaches AutoNext".format(owner))

        windows = entry.source.get("Windows")
        for index, window in enumerate(data.get("Windows", [])):
            line = windows.line_of(index)
            start, end = window.get("Start", 0), window.get("End", 0)
            if start < 0 or (not held and start >= duration):
                report(entry, line, "{}: a {} window starts at {}, outside the action (0..{})".format(owner, window.get("Type", "Hit"), start, duration))
            if not held and end > duration:
                report(entry, line, "{}: a {} window ends at {}, after the action ends ({})".format(owner, window.get("Type", "Hit"), end, duration))

        if data.get("MoveDistance"):
            move_start, move_end = data.get("MoveStart", 0), data.get("MoveEnd", 0)
            if move_end <= move_start:
                report(entry, entry.line("MoveDistance"), "{} has MoveDistance but MoveEnd is not after MoveStart".format(owner))
            elif not held and move_end > duration:
                report(entry, entry.line("MoveEnd"), "{}: MoveEnd {} is after the action ends ({})".format(owner, move_end, duration))

    # An AutoNext chain that loops back would never return control.
    for entry in actions.values():
        seen, current = [entry.id], entry.data.get("AutoNext")
        while current in actions:
            if current in seen:
                report(entry, entry.line("AutoNext"), "action '{}' is part of an endless AutoNext loop: {}".format(
                    entry.id, " -> ".join(seen + [current])))
                break
            seen.append(current)
            current = actions[current].data.get("AutoNext")

    for entry in weapons.values():
        check_slots(entry, "weapon")

    for entry in characters.values():
        data = entry.data
        owner = "character '{}'".format(entry.id)
        if data.get("Weapon") and data["Weapon"] not in weapons:
            report(entry, entry.line("Weapon"), "{} uses weapon '{}', which is not defined anywhere".format(owner, data["Weapon"]))
        if data.get("AIProfile") and data["AIProfile"] not in profiles:
            report(entry, entry.line("AIProfile"), "{} uses AI profile '{}', which is not defined anywhere".format(owner, data["AIProfile"]))
        check_slots(entry, "character")

    for entry in profiles.values():
        data = entry.data
        owner = "AI profile '{}'".format(entry.id)
        moves = entry.source.get("Moves")
        if not data.get("Moves"):
            report(entry, entry.line(), "{} has no Moves, so the enemy would never attack".format(owner))
        for index, move in enumerate(data.get("Moves", [])):
            line = moves.line_of(index)
            if not move.get("Action"):
                report(entry, line, "{}: a move has no Action".format(owner))
            check_action(entry, line, move.get("Action"), owner)
            if move.get("MaxRange", 250) <= move.get("MinRange", 0):
                report(entry, line, "{}: move '{}' has MaxRange not above MinRange".format(owner, move.get("Action")))
            if move.get("MaxPhase", 99) < move.get("MinPhase", 0):
                report(entry, line, "{}: move '{}' has MaxPhase below MinPhase".format(owner, move.get("Action")))

        phases = entry.source.get("Phases")
        previous = 1.0
        for index, phase in enumerate(data.get("Phases", [])):
            line = phases.line_of(index)
            check_action(entry, line, phase.get("EnterAction"), owner)
            threshold = phase.get("HealthBelow", 0.5)
            if threshold >= previous:
                report(entry, line, "{}: phases must go from higher HealthBelow to lower, each below 1".format(owner))
            previous = threshold

        if data.get("AttackDelayMax", 1.5) < data.get("AttackDelayMin", 0.5):
            report(entry, entry.line("AttackDelayMax"), "{}: AttackDelayMax is below AttackDelayMin".format(owner))
