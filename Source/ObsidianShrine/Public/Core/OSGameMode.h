#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "OSGameMode.generated.h"

/** Default game mode for the tech demo. Gameplay rules (checkpoints, death loop) land here in M4. */
UCLASS()
class OBSIDIANSHRINE_API AOSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOSGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};
