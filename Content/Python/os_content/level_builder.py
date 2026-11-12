"""Builds every level from its resolved manifest in Content/LevelData/*.json.

The manifests are written as YAML in Game/levels and compiled by Tools/build_data.py, which also validates
them. This module only places what the resolved file lists; it makes no decisions of its own.
"""

import json
import os

import unreal

from os_content import level_utils

CUBE = "/Engine/BasicShapes/Cube.Cube"


def level_data_dir():
    return os.path.join(unreal.Paths.project_content_dir(), "LevelData")


def _build_lighting(lighting):
    sun = level_utils.spawn(
        unreal.DirectionalLight, "Sun", location=(0, 0, 1000),
        rotation=(lighting.get("SunPitch", -40), lighting.get("SunYaw", 30), 0))
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)

    level_utils.spawn(unreal.SkyAtmosphere, "SkyAtmosphere")

    sky_light = level_utils.spawn(unreal.SkyLight, "SkyLight", location=(0, 0, 500))
    sky_light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_light.light_component.set_editor_property("real_time_capture", True)

    if lighting.get("Fog", True):
        level_utils.spawn(unreal.ExponentialHeightFog, "HeightFog", location=(0, 0, -100))


def _spawn_block(spawn):
    level_utils.spawn_static_mesh(
        CUBE, spawn["Label"], location=tuple(spawn["Location"]), scale=tuple(spawn["Scale"]),
        rotation=tuple(spawn["Rotation"]))


def _spawn_player_start(spawn):
    level_utils.spawn(
        unreal.PlayerStart, spawn["Label"], location=tuple(spawn["Location"]), rotation=tuple(spawn["Rotation"]))


def _spawn_shrine(spawn):
    level_utils.spawn_checkpoint(
        spawn["Id"], spawn["Label"], location=tuple(spawn["Location"]), rotation=tuple(spawn["Rotation"]),
        interact_radius=spawn["InteractRadius"], respawn_distance=spawn["RespawnDistance"],
        respawn_height=spawn["RespawnHeight"])


def _spawn_enemy(spawn):
    level_utils.spawn_enemy(
        spawn["Character"], spawn["Label"], location=tuple(spawn["Location"]), rotation=tuple(spawn["Rotation"]),
        persistent_id=spawn.get("PersistentId") or None)


_SPAWNERS = {
    "Block": _spawn_block,
    "PlayerStart": _spawn_player_start,
    "Shrine": _spawn_shrine,
    "Enemy": _spawn_enemy,
}


def build_level(level):
    """level: the parsed content of one Content/LevelData/*.json file."""
    map_path = level["Map"]
    if not level_utils.open_or_create_level(map_path):
        raise RuntimeError("Could not open or create {}".format(map_path))

    level_utils.clear_generated_actors()
    _build_lighting(level.get("Lighting", {}))
    for spawn in level["Spawns"]:
        spawner = _SPAWNERS.get(spawn["Kind"])
        if spawner is None:
            raise RuntimeError("{}: unknown spawn kind '{}' (is this script older than the data?)".format(map_path, spawn["Kind"]))
        spawner(spawn)

    level_utils.save_current_level()
    unreal.log("os_content: built {} ({} objects)".format(map_path, len(level["Spawns"])))


def build_all():
    directory = level_data_dir()
    names = sorted(name for name in os.listdir(directory) if name.endswith(".json"))
    if not names:
        raise RuntimeError("No level data in {}; run Tools/build_data.py".format(directory))

    for name in names:
        with open(os.path.join(directory, name), "r", encoding="utf-8") as handle:
            build_level(json.load(handle))
