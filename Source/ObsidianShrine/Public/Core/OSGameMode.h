#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "OSGameMode.generated.h"

class AOSCheckpoint;
class AOSEnemyCharacter;
class AOSPlayerCharacter;

/**
 * Rules of the death loop: rest at a shrine, die and drop currency, respawn at the last shrine with the
 * enemies reset, recover the drop or lose it on the next death. Progress lives in UOSProgressSubsystem;
 * the numbers (respawn delay, what a death costs) are the Death section of the game settings.
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

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	FTimerHandle RespawnTimer;

	void RespawnPlayer();
	void ResetEnemies();
	/** Makes the world match the save: one pickup if currency was dropped on this map, none otherwise. */
	void RefreshDroppedPickup();
	AOSCheckpoint* FindCheckpoint(FName CheckpointId) const;
	void ShowMessage(const FString& Message, float Duration) const;
};
