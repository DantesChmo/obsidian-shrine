#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/OSInteractable.h"

#include "OSWorldActors.generated.h"

class AOSEnemyCharacter;
struct FOSProgressState;

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

	/** How far in front of the shrine the player respawns, cm. Set by the level manifest. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	float RespawnDistance = 0.f;

	/** How far above the shrine the player respawns, cm. Set by the level manifest. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	float RespawnHeight = 0.f;

	/** Where the player stands after respawning: in front of the shrine, facing it. */
	FTransform GetRespawnTransform() const;

	virtual FString GetPrompt() const override;
	virtual void Interact(AOSPlayerCharacter* Player) override;
};

/**
 * Passage to another map. Using it loads TargetMap and puts the player at the exit TargetExit there; the
 * level manifests link the two (field To of an exit). Health carries over; enemies of the new map are fresh.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSMapExit : public AOSInteractable
{
	GENERATED_BODY()

public:
	AOSMapExit();

	/** Unique within the game; other exits lead here by it. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName ExitId;

	/** Package path of the map this exit leads to (/Game/Maps/L_Name). Set by the level manifest. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName TargetMap;

	/** ExitId on TargetMap where the player comes out. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName TargetExit;

	/** Non-empty: the exit stays sealed until the enemy with this PersistentId is defeated. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName RequiresDefeated;

	/** How far in front of the exit a player arriving through it stands, cm. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	float ArrivalDistance = 0.f;

	/** How far above the exit's centre a player arriving through it stands, cm. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	float ArrivalHeight = 0.f;

	/** In front of the exit, facing away from it, into the level. */
	FTransform GetArrivalTransform() const;
	bool IsSealed(const FOSProgressState& State) const;

	virtual FString GetPrompt() const override;
	virtual void Interact(AOSPlayerCharacter* Player) override;
};

/** Currency left where the player died. */
UCLASS()
class OBSIDIANSHRINE_API AOSCurrencyPickup : public AOSInteractable
{
	GENERATED_BODY()

public:
	AOSCurrencyPickup();

	virtual FString GetPrompt() const override;
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
