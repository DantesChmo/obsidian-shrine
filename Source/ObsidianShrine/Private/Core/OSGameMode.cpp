#include "Core/OSGameMode.h"

#include "Characters/OSEnemyCharacter.h"
#include "Characters/OSPlayerCharacter.h"
#include "Characters/OSPlayerController.h"
#include "Combat/OSStatsComponent.h"
#include "Core/OSProgress.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "ObsidianShrine.h"
#include "TimerManager.h"
#include "UI/OSHUD.h"
#include "World/OSWorldActors.h"

AOSGameMode::AOSGameMode()
{
	DefaultPawnClass = AOSPlayerCharacter::StaticClass();
	PlayerControllerClass = AOSPlayerController::StaticClass();
	HUDClass = AOSHUD::StaticClass();
}

void AOSGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogObsidian, Log, TEXT("AOSGameMode::InitGame map=%s"), *MapName);
}

void AOSGameMode::StartPlay()
{
	Super::StartPlay();
	if (!ResumeFromSave())
	{
		RefreshDroppedPickup();
	}
}

bool AOSGameMode::ResumeFromSave()
{
	UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (!Progress || Progress->bResumeChecked)
	{
		return false;
	}
	Progress->bResumeChecked = true;

	// Only the start map continues elsewhere: opening another map directly (in the editor, or with the
	// console command open) plays that map.
	const FString CurrentMap = UOSProgressSubsystem::CurrentMapPath(this);
	if (!FOSProgressState::IsSameMap(CurrentMap, UOSDataSubsystem::Settings().StartMap))
	{
		return false;
	}
	const FString SavedMap = Progress->State.RespawnMapIfElsewhere(CurrentMap);
	if (SavedMap.IsEmpty())
	{
		return false;
	}
	UE_LOG(LogObsidian, Log, TEXT("Continuing the save on %s"), *SavedMap);
	LeaveForMap(SavedMap);
	return true;
}

void AOSGameMode::RestartPlayer(AController* NewPlayer)
{
	UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (NewPlayer && Progress)
	{
		// Entered through an exit: come out at the linked exit.
		const FName Arrival = Progress->Travel.TakeArrival();
		if (const AOSMapExit* Exit = FindExit(Arrival))
		{
			RestartPlayerAtTransform(NewPlayer, Exit->GetArrivalTransform());
			return;
		}
		if (!Arrival.IsNone())
		{
			UE_LOG(LogObsidian, Error, TEXT("Arrival exit '%s' is not on this map; using the usual start"), *Arrival.ToString());
		}

		if (const AOSCheckpoint* Checkpoint = FindCheckpoint(Progress->State.LastCheckpoint))
		{
			RestartPlayerAtTransform(NewPlayer, Checkpoint->GetRespawnTransform());
			return;
		}
	}
	Super::RestartPlayer(NewPlayer);
}

AOSMapExit* AOSGameMode::FindExit(FName ExitId) const
{
	if (ExitId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AOSMapExit> It(GetWorld()); It; ++It)
	{
		if (It->ExitId == ExitId)
		{
			return *It;
		}
	}
	return nullptr;
}

void AOSGameMode::TravelThrough(AOSMapExit* Exit, AOSPlayerCharacter* Player)
{
	UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (!Exit || !Player || !Progress || bLeavingMap || Player->GetStats()->IsDead() || Exit->IsSealed(Progress->State))
	{
		return;
	}
	if (Exit->TargetMap.IsNone())
	{
		UE_LOG(LogObsidian, Error, TEXT("Exit '%s' leads nowhere (no TargetMap)"), *Exit->ExitId.ToString());
		return;
	}

	Progress->Travel.Begin(Exit->TargetExit, Player->GetStats()->GetHealth());
	LeaveForMap(Exit->TargetMap.ToString());
}

void AOSGameMode::LeaveForMap(const FString& MapPath)
{
	bLeavingMap = true;
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}

AOSCheckpoint* AOSGameMode::FindCheckpoint(FName CheckpointId) const
{
	if (CheckpointId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AOSCheckpoint> It(GetWorld()); It; ++It)
	{
		if (It->CheckpointId == CheckpointId)
		{
			return *It;
		}
	}
	return nullptr;
}

void AOSGameMode::HandlePlayerDeath(AOSPlayerCharacter* Player)
{
	const FOSDeathDef& Rules = UOSDataSubsystem::Settings().Death;
	if (UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this))
	{
		Progress->Travel.Clear();
		Progress->State.HandleDeath(Player->GetLastGroundedLocation(), UGameplayStatics::GetCurrentLevelName(this, true), Rules);
		Progress->Save();
	}

	// A zero delay still goes through the timer, so the respawn never runs inside the death callback.
	const float Delay = FMath::Max(Rules.RespawnDelay, KINDA_SMALL_NUMBER);
	ShowMessage(UOSDataSubsystem::Texts().MessageDied, Delay);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AOSGameMode::RespawnPlayer, Delay, false);
}

void AOSGameMode::RespawnPlayer()
{
	// The last shrine is on another map: loading it respawns the player there with every enemy fresh.
	const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	const FString RespawnMap = Progress ? Progress->State.RespawnMapIfElsewhere(UOSProgressSubsystem::CurrentMapPath(this)) : FString();
	if (!RespawnMap.IsEmpty())
	{
		LeaveForMap(RespawnMap);
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (APawn* OldPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		OldPawn->Destroy();
	}

	ResetEnemies();
	RefreshDroppedPickup();
	RestartPlayer(PlayerController);
}

void AOSGameMode::HandleEnemyDeath(AOSEnemyCharacter* Enemy)
{
	UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (!Progress)
	{
		return;
	}

	Progress->State.AddCurrency(Enemy->GetDefinition().CurrencyReward);
	if (!Enemy->PersistentId.IsNone())
	{
		Progress->State.MarkDefeated(Enemy->PersistentId);
	}
	if (Enemy->IsBoss())
	{
		ShowMessage(UOSDataSubsystem::Texts().MessageBossDefeated, UOSDataSubsystem::Settings().Messages.BossDefeatedDuration);
	}
	Progress->Save();
}

void AOSGameMode::RestAtCheckpoint(AOSCheckpoint* Checkpoint, AOSPlayerCharacter* Player)
{
	if (UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this))
	{
		Progress->State.RestAt(Checkpoint->CheckpointId, UOSProgressSubsystem::CurrentMapPath(this));
		Progress->Save();
	}

	Player->GetStats()->RestoreAll();
	ResetEnemies();
	ShowMessage(UOSDataSubsystem::Texts().MessageRested, UOSDataSubsystem::Settings().Messages.RestedDuration);
}

void AOSGameMode::RecoverDroppedCurrency()
{
	if (UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this))
	{
		Progress->State.RecoverDropped();
		Progress->Save();
	}
}

void AOSGameMode::ResetEnemies()
{
	for (TActorIterator<AOSEnemySpawner> It(GetWorld()); It; ++It)
	{
		It->ResetSpawn();
	}
}

void AOSGameMode::RefreshDroppedPickup()
{
	for (TActorIterator<AOSCurrencyPickup> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}

	const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (Progress && Progress->State.DroppedCurrency > 0 && Progress->State.DroppedMap == UGameplayStatics::GetCurrentLevelName(this, true))
	{
		GetWorld()->SpawnActor<AOSCurrencyPickup>(AOSCurrencyPickup::StaticClass(), Progress->State.DroppedLocation, FRotator::ZeroRotator);
	}
}

void AOSGameMode::ShowMessage(const FString& Message, float Duration) const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (AOSHUD* HUD = PlayerController ? Cast<AOSHUD>(PlayerController->GetHUD()) : nullptr)
	{
		HUD->ShowMessage(Message, Duration);
	}
}
