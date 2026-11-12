#include "AI/OSAIBrainComponent.h"

#include "Characters/OSCharacterBase.h"
#include "Combat/OSActionComponent.h"
#include "Combat/OSStatsComponent.h"
#include "Kismet/GameplayStatics.h"

UOSAIBrainComponent::UOSAIBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSAIBrainComponent::SetProfile(const FOSAIProfileDef& InProfile, bool bKeepState)
{
	Profile = InProfile;
	if (bKeepState && bHasProfile)
	{
		// The new profile may have fewer phases than the fight has already gone through.
		Phase = FMath::Min(Phase, Profile.Phases.Num());
		return;
	}
	bHasProfile = true;
	Phase = 0;
	Cooldowns.Reset();
	AttackDelayLeft = Profile.AttackDelayMin;
}

void UOSAIBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	Home = GetOwner()->GetActorLocation();
	Random.GenerateNewSeed();
}

int32 UOSAIBrainComponent::ComputePhase(const FOSAIProfileDef& Profile, float HealthFraction)
{
	int32 Result = 0;
	for (const FOSAIPhaseDef& PhaseDef : Profile.Phases)
	{
		if (HealthFraction <= PhaseDef.HealthBelow)
		{
			++Result;
		}
	}
	return Result;
}

int32 UOSAIBrainComponent::PickMove(const FOSAIProfileDef& Profile, float Distance, int32 Phase, const TMap<FName, float>& Cooldowns, FRandomStream& Random)
{
	TArray<int32, TInlineAllocator<8>> Usable;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Profile.Moves.Num(); ++Index)
	{
		const FOSAIMoveDef& Move = Profile.Moves[Index];
		const float* Cooldown = Cooldowns.Find(Move.Action);
		if (Move.Weight <= 0.f || Distance < Move.MinRange || Distance > Move.MaxRange
			|| Phase < Move.MinPhase || Phase > Move.MaxPhase || (Cooldown && *Cooldown > 0.f))
		{
			continue;
		}
		Usable.Add(Index);
		TotalWeight += Move.Weight;
	}
	if (Usable.Num() == 0)
	{
		return INDEX_NONE;
	}

	float Roll = Random.FRandRange(0.f, TotalWeight);
	for (const int32 Index : Usable)
	{
		Roll -= Profile.Moves[Index].Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Usable.Last();
}

AOSCharacterBase* UOSAIBrainComponent::FindPlayer() const
{
	AOSCharacterBase* Player = Cast<AOSCharacterBase>(UGameplayStatics::GetPlayerCharacter(this, 0));
	return (Player && Player->IsAlive()) ? Player : nullptr;
}

float UOSAIBrainComponent::GetApproachDistance() const
{
	float Closest = TNumericLimits<float>::Max();
	for (const FOSAIMoveDef& Move : Profile.Moves)
	{
		if (Phase >= Move.MinPhase && Phase <= Move.MaxPhase)
		{
			Closest = FMath::Min(Closest, Move.MaxRange);
		}
	}
	return Closest == TNumericLimits<float>::Max() ? 200.f : Closest * 0.8f;
}

void UOSAIBrainComponent::MoveTowards(AOSCharacterBase* Self, const FVector& Location, float StopDistance) const
{
	FVector Offset = Location - Self->GetActorLocation();
	Offset.Z = 0.f;
	if (Offset.SizeSquared() > FMath::Square(StopDistance))
	{
		Self->AddMovementInput(Offset.GetSafeNormal(), 1.f);
	}
}

void UOSAIBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AOSCharacterBase* Self = Cast<AOSCharacterBase>(GetOwner());
	if (!bHasProfile || !Self || !Self->IsAlive())
	{
		Target.Reset();
		return;
	}

	for (TPair<FName, float>& Cooldown : Cooldowns)
	{
		Cooldown.Value -= DeltaTime;
	}

	AOSCharacterBase* Player = FindPlayer();
	const float HomeDistance = FVector::Dist(Self->GetActorLocation(), Home);

	// Once leashed, the enemy ignores the player until it is back at its post.
	if (HomeDistance > Profile.LeashRadius)
	{
		bReturning = true;
	}
	else if (HomeDistance < 150.f)
	{
		bReturning = false;
	}

	if (!Player || bReturning)
	{
		Target.Reset();
	}
	else if (!Target.IsValid())
	{
		const bool bInRange = FVector::Dist(Self->GetActorLocation(), Player->GetActorLocation()) <= Profile.AggroRadius;
		const bool bWounded = Self->GetStats()->GetHealthFraction() < 1.f;
		if (bInRange || bWounded)
		{
			Target = Player;
			AttackDelayLeft = Profile.AttackDelayMin;
		}
	}

	UOSActionComponent* Action = Self->GetAction();

	// Phase transitions happen even mid-attack: the enter action interrupts whatever is running.
	const int32 NewPhase = ComputePhase(Profile, Self->GetStats()->GetHealthFraction());
	if (NewPhase > Phase)
	{
		Phase = NewPhase;
		const FName EnterAction = Profile.Phases[Phase - 1].EnterAction;
		if (!EnterAction.IsNone())
		{
			Action->StartAction(EnterAction, NAME_None, true);
		}
	}

	if (Action->IsBusy())
	{
		return;
	}

	AOSCharacterBase* Current = Target.Get();
	if (!Current)
	{
		// Walk back to the post.
		MoveTowards(Self, Home, 80.f);
		return;
	}

	AttackDelayLeft -= DeltaTime;
	const float Distance = FVector::Dist2D(Self->GetActorLocation(), Current->GetActorLocation());

	if (AttackDelayLeft <= 0.f)
	{
		const int32 MoveIndex = PickMove(Profile, Distance, Phase, Cooldowns, Random);
		if (MoveIndex != INDEX_NONE)
		{
			const FOSAIMoveDef& Move = Profile.Moves[MoveIndex];
			if (Action->StartAction(Move.Action))
			{
				Cooldowns.Add(Move.Action, Move.Cooldown);
				AttackDelayLeft = Random.FRandRange(Profile.AttackDelayMin, FMath::Max(Profile.AttackDelayMin, Profile.AttackDelayMax));
				return;
			}
		}
	}

	MoveTowards(Self, Current->GetActorLocation(), GetApproachDistance());
}
