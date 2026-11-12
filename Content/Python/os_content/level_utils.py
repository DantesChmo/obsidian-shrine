import unreal

from os_content import GENERATED_TAG


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def open_or_create_level(asset_path):
    """Loads the level if it exists, otherwise creates it. Returns True on success."""
    directory = asset_path.rsplit("/", 1)[0]
    unreal.EditorAssetLibrary.make_directory(directory)

    level_subsystem = _level_subsystem()
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return level_subsystem.load_level(asset_path)
    return level_subsystem.new_level(asset_path)


def clear_generated_actors():
    actor_subsystem = _actor_subsystem()
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if actor.actor_has_tag(GENERATED_TAG):
            actor_subsystem.destroy_actor(actor)
            removed += 1
    unreal.log("os_content: removed {} generated actors".format(removed))


def spawn(actor_class, label, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), scale=None):
    """Spawns a tagged actor. rotation is (pitch, yaw, roll) in degrees."""
    actor = _actor_subsystem().spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]),
    )
    if actor is None:
        raise RuntimeError("Failed to spawn {} ({})".format(label, actor_class))

    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(GENERATED_TAG)])
    if scale is not None:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def spawn_static_mesh(mesh_path, label, location, scale=(1.0, 1.0, 1.0), rotation=(0.0, 0.0, 0.0)):
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        raise RuntimeError("Static mesh not found: {}".format(mesh_path))

    actor = spawn(unreal.StaticMeshActor, label, location, rotation, scale)
    actor.static_mesh_component.set_static_mesh(mesh)
    return actor


def spawn_game_actor(class_name, label, location, rotation=(0.0, 0.0, 0.0), properties=None):
    """Spawns a C++ gameplay actor (class name without the A prefix, e.g. "OSEnemySpawner").

    properties: {python_property_name: value}; strings are converted to unreal.Name.
    """
    actor_class = unreal.load_class(None, "/Script/ObsidianShrine.{}".format(class_name))
    if actor_class is None:
        raise RuntimeError("Gameplay class not found: {} (is the C++ module built?)".format(class_name))

    actor = spawn(actor_class, label, location, rotation)
    for name, value in (properties or {}).items():
        if isinstance(value, str):
            value = unreal.Name(value)
        actor.set_editor_property(name, value)
    return actor


def spawn_enemy(character_id, label, location, rotation=(0.0, 0.0, 0.0), persistent_id=None):
    """Places an enemy defined in Content/Data. persistent_id makes it stay dead once killed (bosses)."""
    properties = {"character_id": character_id}
    if persistent_id:
        properties["persistent_id"] = persistent_id
    return spawn_game_actor("OSEnemySpawner", label, location, rotation, properties)


def spawn_checkpoint(checkpoint_id, label, location, rotation=(0.0, 0.0, 0.0)):
    """Places a shrine. The player respawns 180 uu in front of it (its +X after rotation)."""
    return spawn_game_actor("OSCheckpoint", label, location, rotation, {"checkpoint_id": checkpoint_id})


def save_current_level():
    if not _level_subsystem().save_current_level():
        raise RuntimeError("Failed to save current level")
