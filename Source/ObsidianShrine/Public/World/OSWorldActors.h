#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OSInteractable.h"

#include "OSWorldActors.generated.h"

class AOSEnemyCharacter;

/** Shrine: resting here refills the player, respawns enemies and becomes the respawn point. */
UCLASS()
class OBSIDIANSHRINE_API AOSCheckpoint : public AOSInteractable
{
	GENERATED_BODY()

public:
	AOSCheckpoint();

	/** Unique within the game; stored in the save. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName CheckpointId;

	/** Where the player stands after respawning: in front of the shrine, facing it. */
	FTransform GetRespawnTransform() const;

	virtual FString GetPrompt() const override { return TEXT("Rest at shrine"); }
	virtual void Interact(AOSPlayerCharacter* Player) override;
};

/** Currency left where the player died. */
UCLASS()
class OBSIDIANSHRINE_API AOSCurrencyPickup : public AOSInteractable
{
	GENERATED_BODY()

public:
	AOSCurrencyPickup();

	virtual FString GetPrompt() const override { return TEXT("Recover lost shards"); }
	virtual void Interact(AOSPlayerCharacter* Player) override;
};

/** Places one enemy in a level. The enemy itself is defined by CharacterId in Content/Data. */
UCLASS()
class OBSIDIANSHRINE_API AOSEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AOSEnemySpawner();

	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName CharacterId;

	/** Non-empty: once killed, this enemy never comes back (bosses). Must be unique within the game. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName PersistentId;

	/** Removes the current enemy and spawns a fresh one, unless it is persistently defeated. */
	void ResetSpawn();

	AOSEnemyCharacter* GetSpawned() const { return Spawned.Get(); }

	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<AOSEnemyCharacter> Spawned;
};
