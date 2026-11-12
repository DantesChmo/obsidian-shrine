#include "World/OSInteractable.h"

#include "Characters/OSPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AOSInteractable::AOSInteractable()
{
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Visual);
}

AOSInteractable* AOSInteractable::FindFor(const AOSPlayerCharacter* Player)
{
	if (!Player || !Player->GetWorld())
	{
		return nullptr;
	}

	AOSInteractable* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AOSInteractable> It(Player->GetWorld()); It; ++It)
	{
		AOSInteractable* Candidate = *It;
		const float Distance = FVector::Dist(Player->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= Candidate->InteractRadius && Distance < BestDistance && Candidate->CanInteract(Player))
		{
			Best = Candidate;
			BestDistance = Distance;
		}
	}
	return Best;
}
