#include "Characters/OSEnemyCharacter.h"

#include "AI/OSAIBrainComponent.h"
#include "Core/OSGameMode.h"
#include "Data/OSDataSubsystem.h"
#include "Engine/World.h"
#include "ObsidianShrine.h"

AOSEnemyCharacter::AOSEnemyCharacter()
{
	Faction = EOSFaction::Enemy;
	Brain = CreateDefaultSubobject<UOSAIBrainComponent>(TEXT("Brain"));
}

void AOSEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyProfile(false);
}

void AOSEnemyCharacter::HandleDataReloaded()
{
	Super::HandleDataReloaded();
	ApplyProfile(true);
}

void AOSEnemyCharacter::ApplyProfile(bool bKeepState)
{
	const UOSDataSubsystem* Data = UOSDataSubsystem::Get();
	const FOSAIProfileDef* Profile = Data ? Data->FindAIProfile(GetDefinition().AIProfile) : nullptr;
	if (Profile)
	{
		Brain->SetProfile(*Profile, bKeepState);
	}
	else
	{
		UE_LOG(LogObsidian, Warning, TEXT("Enemy '%s' has no AI profile and will stand still"), *CharacterId.ToString());
	}
}

bool AOSEnemyCharacter::IsAggressive() const
{
	return Brain->GetTarget() != nullptr;
}

AActor* AOSEnemyCharacter::GetCombatTarget() const
{
	return Brain->GetTarget();
}

FVector AOSEnemyCharacter::GetDesiredActionDirection() const
{
	const AActor* Target = GetCombatTarget();
	return Target ? (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
}

void AOSEnemyCharacter::OnDeath()
{
	if (AOSGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOSGameMode>() : nullptr)
	{
		GameMode->HandleEnemyDeath(this);
	}
	SetLifeSpan(UOSDataSubsystem::Settings().Death.CorpseLifetime);
}
