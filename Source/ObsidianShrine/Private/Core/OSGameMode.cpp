#include "Core/OSGameMode.h"

#include "ObsidianShrine.h"

AOSGameMode::AOSGameMode()
{
	// Until the player character exists (M1) the engine's ADefaultPawn fly-camera is used.
}

void AOSGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogObsidian, Log, TEXT("AOSGameMode::InitGame map=%s"), *MapName);
}
