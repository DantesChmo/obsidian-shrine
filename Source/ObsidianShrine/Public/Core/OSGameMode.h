#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "OSGameMode.generated.h"

class AOSCheckpoint;
class AOSEnemyCharacter;
class AOSMapExit;
class AOSPlayerCharacter;

/**
 * Rules of the death loop: rest at a shrine, die and drop currency, respawn at the last shrine with the
 * enemies reset, recover the drop or lose it on the next death. Progress lives in UOSProgressSubsystem;
 * the numbers (respawn delay, what a death costs) are the Death section of the game settings.
 *
 * Several maps: exits load another map; the last shrine may be on any map, and both a respawn and the
 * start of a session (on the start map) load its map first.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOSGameMode();

	void HandlePlayerDeath(AOSPlayerCharacter* Player);
	void HandleEnemyDeath(AOSEnemyCharacter* Enemy);
	void RestAtCheckpoint(AOSCheckpoint* Checkpoint, AOSPlayerCharacter* Player);
	void RecoverDroppedCurrency();
	/** Loads the exit's target map unless the exit is sealed or a travel is already under way. */
	void TravelThrough(AOSMapExit* Exit, AOSPlayerCharacter* Player);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	FTimerHandle RespawnTimer;
	bool bLeavingMap = false;

	void RespawnPlayer();
	/** At the first map of a session: when it is the start map and the save is elsewhere, goes there. */
	bool ResumeFromSave();
	void LeaveForMap(const FString& MapPath);
	void ResetEnemies();
	/** Makes the world match the save: one pickup if currency was dropped on this map, none otherwise. */
	void RefreshDroppedPickup();
	AOSCheckpoint* FindCheckpoint(FName CheckpointId) const;
	AOSMapExit* FindExit(FName ExitId) const;
	void ShowMessage(const FString& Message, float Duration) const;
};
