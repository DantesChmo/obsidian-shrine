#pragma once

#include "CoreMinimal.h"
#include "Data/OSDataTypes.h"
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

	/** Map of LastCheckpoint, as a package path (/Game/Maps/L_Name). Empty in saves older than map travel. */
	UPROPERTY()
	FString LastCheckpointMap;

	/** Currency lying where the player last died. What the next death does to it is up to the death rules. */
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
	void RestAt(FName Checkpoint, const FString& Map);
	/**
	 * The map the player has to be taken to in order to respawn at LastCheckpoint, or empty when that is
	 * CurrentMap or no shrine on a known map has been rested at.
	 */
	FString RespawnMapIfElsewhere(const FString& CurrentMap) const;
	/**
	 * Leaves Rules.CurrencyLossFraction of the held currency at the death spot. Whatever was still lying
	 * from an earlier death is lost, or moves to the new spot if Rules.bKeepPreviousDrop.
	 */
	void HandleDeath(const FVector& Location, const FString& Map, const FOSDeathDef& Rules);
	/** Returns the amount recovered. */
	int32 RecoverDropped();

	void MarkDefeated(FName PersistentId);
	bool IsDefeated(FName PersistentId) const;

	/** Compares maps by their short name, so /Game/Maps/L_A and L_A are the same map. */
	static bool IsSameMap(const FString& A, const FString& B);
};

/**
 * What carries over a map change inside one session: where the player comes out and how much health they
 * had. Never saved: quitting between two maps continues from the last shrine.
 */
struct OBSIDIANSHRINE_API FOSTravelState
{
	void Begin(FName InArrivalExit, float InHealth);
	void Clear();
	/** The exit the player arrives at on this map, once; None when the map was not entered through an exit. */
	FName TakeArrival();
	/** The health carried from the previous map, once. False when nothing is carried. */
	bool TakeHealth(float& OutHealth);

private:
	FName ArrivalExit;
	float Health = 0.f;
	bool bHasHealth = false;
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

	/** Session only, see FOSTravelState. */
	FOSTravelState Travel;

	/** Set once the first map of the session has decided whether to continue on the saved map. */
	bool bResumeChecked = false;

	/** Package path of the world's map (/Game/Maps/L_Name), without the editor's play-in-editor prefix. */
	static FString CurrentMapPath(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void Save() const;
	void Load();
	/** Wipes progress in memory and on disk, and forgets any travel in progress. Also available as the console command os.ResetSave. */
	void ResetProgress();
};
