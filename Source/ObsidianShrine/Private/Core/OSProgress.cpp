#include "Core/OSProgress.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ObsidianShrine.h"
#include "UObject/Package.h"

namespace
{
	const TCHAR* SaveSlot = TEXT("ObsidianShrine");
}

void FOSProgressState::AddCurrency(int32 Amount)
{
	Currency = FMath::Max(0, Currency + Amount);
}

void FOSProgressState::RestAt(FName Checkpoint, const FString& Map)
{
	LastCheckpoint = Checkpoint;
	LastCheckpointMap = Map;
}

FString FOSProgressState::RespawnMapIfElsewhere(const FString& CurrentMap) const
{
	if (LastCheckpoint.IsNone() || LastCheckpointMap.IsEmpty() || IsSameMap(LastCheckpointMap, CurrentMap))
	{
		return FString();
	}
	return LastCheckpointMap;
}

bool FOSProgressState::IsSameMap(const FString& A, const FString& B)
{
	return !A.IsEmpty() && FPackageName::GetShortName(A).Equals(FPackageName::GetShortName(B), ESearchCase::IgnoreCase);
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

void FOSTravelState::Begin(FName InArrivalExit, float InHealth)
{
	ArrivalExit = InArrivalExit;
	Health = InHealth;
	bHasHealth = InHealth > 0.f;
}

void FOSTravelState::Clear()
{
	*this = FOSTravelState();
}

FName FOSTravelState::TakeArrival()
{
	const FName Result = ArrivalExit;
	ArrivalExit = NAME_None;
	return Result;
}

bool FOSTravelState::TakeHealth(float& OutHealth)
{
	if (!bHasHealth)
	{
		return false;
	}
	OutHealth = Health;
	bHasHealth = false;
	Health = 0.f;
	return true;
}

UOSProgressSubsystem* UOSProgressSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UOSProgressSubsystem>() : nullptr;
}

FString UOSProgressSubsystem::CurrentMapPath(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) : FString();
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
		UE_LOG(LogObsidian, Log, TEXT("Loaded save: %d currency, checkpoint '%s' on '%s'"), State.Currency,
			*State.LastCheckpoint.ToString(), *State.LastCheckpointMap);
	}
}

void UOSProgressSubsystem::ResetProgress()
{
	State = FOSProgressState();
	Travel.Clear();
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
