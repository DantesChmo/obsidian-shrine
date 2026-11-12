#include "Core/OSProgress.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "ObsidianShrine.h"

namespace
{
	const TCHAR* SaveSlot = TEXT("ObsidianShrine");
}

void FOSProgressState::AddCurrency(int32 Amount)
{
	Currency = FMath::Max(0, Currency + Amount);
}

void FOSProgressState::HandleDeath(const FVector& Location, const FString& Map, const FOSDeathDef& Rules)
{
	const float Fraction = FMath::Clamp(Rules.CurrencyLossFraction, 0.f, 1.f);
	const int32 Lost = FMath::Clamp(FMath::RoundToInt(Currency * Fraction), 0, Currency);
	const int32 Carried = Rules.bKeepPreviousDrop ? DroppedCurrency : 0;

	Currency -= Lost;
	DroppedCurrency = Lost + Carried;
	if (DroppedCurrency > 0)
	{
		DroppedLocation = Location;
		DroppedMap = Map;
	}
	else
	{
		DroppedMap.Reset();
	}
}

int32 FOSProgressState::RecoverDropped()
{
	const int32 Recovered = DroppedCurrency;
	Currency += Recovered;
	DroppedCurrency = 0;
	DroppedMap.Reset();
	return Recovered;
}

void FOSProgressState::MarkDefeated(FName PersistentId)
{
	if (!PersistentId.IsNone())
	{
		Defeated.AddUnique(PersistentId);
	}
}

bool FOSProgressState::IsDefeated(FName PersistentId) const
{
	return !PersistentId.IsNone() && Defeated.Contains(PersistentId);
}

UOSProgressSubsystem* UOSProgressSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOSProgressSubsystem>() : nullptr;
}

void UOSProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

void UOSProgressSubsystem::Save() const
{
	UOSSaveGame* SaveGame = NewObject<UOSSaveGame>();
	SaveGame->State = State;
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlot, 0))
	{
		UE_LOG(LogObsidian, Error, TEXT("Failed to write save slot '%s'"), SaveSlot);
	}
}

void UOSProgressSubsystem::Load()
{
	State = FOSProgressState();
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		return;
	}
	if (const UOSSaveGame* SaveGame = Cast<UOSSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
	{
		State = SaveGame->State;
		UE_LOG(LogObsidian, Log, TEXT("Loaded save: %d currency, checkpoint '%s'"), State.Currency, *State.LastCheckpoint.ToString());
	}
}

void UOSProgressSubsystem::ResetProgress()
{
	State = FOSProgressState();
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
}

static FAutoConsoleCommandWithWorld OSResetSaveCommand(
	TEXT("os.ResetSave"),
	TEXT("Wipes saved progress. Restart the level afterwards."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(World))
		{
			Progress->ResetProgress();
			UE_LOG(LogObsidian, Display, TEXT("Progress wiped"));
		}
	}));
