#include "World/OSWorldActors.h"

#include "Characters/OSEnemyCharacter.h"
#include "Characters/OSPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/OSGameMode.h"
#include "Core/OSProgress.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AOSCheckpoint::AOSCheckpoint()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Visual->SetStaticMesh(ConeMesh.Object);
	Visual->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.6f));
}

FTransform AOSCheckpoint::GetRespawnTransform() const
{
	const FVector Forward = GetActorForwardVector();
	const FVector Location = GetActorLocation() + Forward * RespawnDistance + FVector(0.f, 0.f, RespawnHeight);
	return FTransform(FRotator(0.f, (-Forward).Rotation().Yaw, 0.f), Location);
}

FString AOSCheckpoint::GetPrompt() const
{
	return UOSDataSubsystem::Texts().PromptShrine;
}

void AOSCheckpoint::Interact(AOSPlayerCharacter* Player)
{
	if (AOSGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSGameMode>())
	{
		GameMode->RestAtCheckpoint(this, Player);
	}
}

AOSMapExit::AOSMapExit()
{
	// A doorway slab: thin along the facing direction, 2 m wide, 3 m tall around the actor's location.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(CubeMesh.Object);
	Visual->SetRelativeScale3D(FVector(0.3f, 2.f, 3.f));
}

FTransform AOSMapExit::GetArrivalTransform() const
{
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Location = GetActorLocation() + Forward * ArrivalDistance + FVector(0.f, 0.f, ArrivalHeight);
	return FTransform(FRotator(0.f, Forward.Rotation().Yaw, 0.f), Location);
}

bool AOSMapExit::IsSealed(const FOSProgressState& State) const
{
	return !RequiresDefeated.IsNone() && !State.IsDefeated(RequiresDefeated);
}

FString AOSMapExit::GetPrompt() const
{
	const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	const bool bSealed = Progress && IsSealed(Progress->State);
	return bSealed ? UOSDataSubsystem::Texts().PromptExitSealed : UOSDataSubsystem::Texts().PromptExit;
}

void AOSMapExit::Interact(AOSPlayerCharacter* Player)
{
	if (AOSGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSGameMode>())
	{
		GameMode->TravelThrough(this, Player);
	}
}

AOSCurrencyPickup::AOSCurrencyPickup()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Visual->SetStaticMesh(SphereMesh.Object);
	Visual->SetRelativeScale3D(FVector(0.4f));
}

FString AOSCurrencyPickup::GetPrompt() const
{
	return UOSDataSubsystem::Texts().PromptPickup;
}

void AOSCurrencyPickup::Interact(AOSPlayerCharacter* Player)
{
	if (AOSGameMode* GameMode = GetWorld()->GetAuthGameMode<AOSGameMode>())
	{
		GameMode->RecoverDroppedCurrency();
	}
	Destroy();
}

AOSEnemySpawner::AOSEnemySpawner()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AOSEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	ResetSpawn();
}

void AOSEnemySpawner::ResetSpawn()
{
	if (AOSEnemyCharacter* Old = Spawned.Get())
	{
		Old->Destroy();
	}
	Spawned.Reset();

	const UOSProgressSubsystem* Progress = UOSProgressSubsystem::Get(this);
	if (Progress && Progress->State.IsDefeated(PersistentId))
	{
		return;
	}

	const FTransform SpawnTransform(GetActorRotation(), GetActorLocation());
	AOSEnemyCharacter* Enemy = GetWorld()->SpawnActorDeferred<AOSEnemyCharacter>(AOSEnemyCharacter::StaticClass(), SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Enemy)
	{
		Enemy->CharacterId = CharacterId;
		Enemy->PersistentId = PersistentId;
		Enemy->FinishSpawning(SpawnTransform);
		Spawned = Enemy;
	}
}
