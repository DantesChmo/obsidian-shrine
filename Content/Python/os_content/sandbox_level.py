"""L_Sandbox: greybox test arena with one of everything: shrine, regular enemies, boss."""

import unreal

from os_content import level_utils

LEVEL_PATH = "/Game/Maps/L_Sandbox"

CUBE = "/Engine/BasicShapes/Cube.Cube"


def _build_lighting():
    sun = level_utils.spawn(unreal.DirectionalLight, "Sun", location=(0, 0, 1000), rotation=(-40, 30, 0))
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("atmosphere_sun_light", True)

    level_utils.spawn(unreal.SkyAtmosphere, "SkyAtmosphere")

    sky_light = level_utils.spawn(unreal.SkyLight, "SkyLight", location=(0, 0, 500))
    sky_light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_light.light_component.set_editor_property("real_time_capture", True)

    level_utils.spawn(unreal.ExponentialHeightFog, "HeightFog", location=(0, 0, -100))


def _build_geometry():
    # Floor: 80m x 80m. BasicShapes cube is 100uu, so scale 80 => 8000uu.
    level_utils.spawn_static_mesh(CUBE, "Floor", location=(0, 0, -50), scale=(80, 80, 1))

    # Pillars to test camera collision and lock-on line of sight.
    for index, (x, y) in enumerate([(1200, 800), (-1200, 800), (1200, -800), (-1200, -800)]):
        level_utils.spawn_static_mesh(CUBE, "Pillar_{}".format(index), location=(x, y, 300), scale=(2, 2, 6))

    # Ramp and ledge to test slopes and falling.
    level_utils.spawn_static_mesh(CUBE, "Ramp", location=(0, 2000, 150), scale=(4, 8, 0.5), rotation=(0, 0, 20))
    level_utils.spawn_static_mesh(CUBE, "Ledge", location=(0, 2800, 250), scale=(6, 4, 5))


def _build_gameplay():
    # Shrine right behind the player start, facing the arena (+Y).
    level_utils.spawn_checkpoint("sandbox_start", "Shrine_Start", location=(0, -1900, 80), rotation=(0, 90, 0))

    level_utils.spawn_enemy("husk", "Enemy_Husk_0", location=(-500, 300, 100), rotation=(0, -90, 0))
    level_utils.spawn_enemy("husk", "Enemy_Husk_1", location=(600, 900, 100), rotation=(0, -90, 0))

    # Boss in the far corner, well outside the husks' fight.
    level_utils.spawn_enemy(
        "shrine_warden", "Boss_ShrineWarden", location=(-2600, -2600, 200), rotation=(0, 45, 0),
        persistent_id="sandbox_shrine_warden")


def build():
    if not level_utils.open_or_create_level(LEVEL_PATH):
        raise RuntimeError("Could not open or create {}".format(LEVEL_PATH))

    level_utils.clear_generated_actors()
    _build_lighting()
    _build_geometry()
    level_utils.spawn(unreal.PlayerStart, "PlayerStart", location=(0, -1500, 100), rotation=(0, 90, 0))
    _build_gameplay()

    level_utils.save_current_level()
    unreal.log("os_content: built {}".format(LEVEL_PATH))
