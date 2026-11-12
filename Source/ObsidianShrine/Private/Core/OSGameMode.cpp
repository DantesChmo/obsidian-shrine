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
	RefreshDroppedPickup();
}

void AOSGameMode::RestartPlayer(AController* NewPlayer)
{
	const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	const AOSCheckpoint* Checkpoint = Progress ? FindCheckpoint(Progress->State.LastCheckpoint) : nullptr;
	if (NewPlayer && Checkpoint)
	{
		RestartPlayerAtTransform(NewPlayer, Checkpoint->GetRespawnTransform());
		return;
	}
	Super::RestartPlayer(NewPlayer);
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
		Progress->State.LastCheckpoint = Checkpoint->CheckpointId;
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
