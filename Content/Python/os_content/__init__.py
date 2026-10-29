"""Obsidian Shrine content pipeline.

Every asset and level in the project is produced by these scripts, so the
whole Content folder can be rebuilt from text. Actors spawned by a builder
carry GENERATED_TAG; rebuilding a level removes only those actors.
"""

GENERATED_TAG = "os.generated"
