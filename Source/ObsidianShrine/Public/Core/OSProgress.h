#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "OSProgress.generated.h"

/** Everything that persists between deaths and sessions. Plain data with the rules as methods, so it is testable. */
USTRUCT()
struct OBSIDIANSHRINE_API FOSProgressState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Currency = 0;

	/** Checkpoint the player respawns at. None: the level's PlayerStart. */
	UPROPERTY()
	FName LastCheckpoint;

	/** Currency lying where the player last died. Lost for good on the next death. */
	UPROPERTY()
	int32 DroppedCurrency = 0;

	UPROPERTY()
	FVector DroppedLocation = FVector::ZeroVector;

	UPROPERTY()
	FString DroppedMap;

	/** PersistentId of every enemy that must stay dead. */
	UPROPERTY()
	TArray<FName> Defeated;

	void AddCurrency(int32 Amount);
	/** Moves all held currency to the death spot, replacing whatever was dropped before. */
	void HandleDeath(const FVector& Location, const FString& Map);
	/** Returns the amount recovered. */
	int32 RecoverDropped();

	void MarkDefeated(FName PersistentId);
	bool IsDefeated(FName PersistentId) const;
};

UCLASS()
class OBSIDIANSHRINE_API UOSSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FOSProgressState State;
};

/** Holds the progress of the running game and mirrors it to the save slot. */
UCLASS()
class OBSIDIANSHRINE_API UOSProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UOSProgressSubsystem* Get(const UObject* WorldContextObject);

	FOSProgressState State;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void Save() const;
	void Load();
	/** Wipes progress in memory and on disk. Also available as the console command os.ResetSave. */
	void ResetProgress();
};
