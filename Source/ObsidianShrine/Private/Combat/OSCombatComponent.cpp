#include "Combat/OSCombatComponent.h"

#include "Combat/OSActionComponent.h"
#include "Combat/OSStatsComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarOSDebugCombat(
	TEXT("os.DebugCombat"), 0, TEXT("1: draw weapon hit volumes while Hit windows are open."));

FOSHitResult OSCombat::ResolveHit(const FOSHitInfo& Hit, const FOSDefenseState& Defense)
{
	FOSHitResult Result;

	if (Defense.bInvulnerable)
	{
		Result.Outcome = EOSHitOutcome::Ignored;
		return Result;
	}

	const bool bCanBlock = Defense.bFacingAttacker && !Hit.bUnblockable;

	if (bCanBlock && Defense.bPerfectGuard)
	{
		Result.Outcome = EOSHitOutcome::PerfectGuarded;
		return Result;
	}

	if (bCanBlock && Defense.bGuarding)
	{
		Result.StaminaDamage = Hit.Damage * Defense.GuardStaminaScale;
		Result.HealthDamage = Hit.Damage * (1.f - FMath::Clamp(Defense.GuardDamageReduction, 0.f, 1.f));
		Result.Outcome = Result.StaminaDamage >= Defense.Stamina ? EOSHitOutcome::GuardBroken : EOSHitOutcome::Guarded;
		return Result;
	}

	Result.Outcome = EOSHitOutcome::Hit;
	Result.HealthDamage = Hit.Damage;
	Result.PoiseDamage = Defense.bHyperArmor ? 0.f : Hit.PoiseDamage;
	return Result;
}

bool OSCombat::IsInGuardArc(const FVector& Forward, const FVector& ToAttacker, float GuardAngle)
{
	const FVector Direction = ToAttacker.GetSafeNormal2D();
	if (Direction.IsNearlyZero() || GuardAngle >= 360.f)
	{
		return true;
	}
	if (GuardAngle <= 0.f)
	{
		return false;
	}
	const float Cosine = FVector::DotProduct(Forward.GetSafeNormal2D(), Direction);
	return Cosine >= FMath::Cos(FMath::DegreesToRadians(GuardAngle * 0.5f)) - KINDA_SMALL_NUMBER;
}

UOSCombatComponent::UOSCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Action && GetOwner())
	{
		Setup(GetOwner()->FindComponentByClass<UOSStatsComponent>(), GetOwner()->FindComponentByClass<UOSActionComponent>());
	}
}

void UOSCombatComponent::Setup(UOSStatsComponent* InStats, UOSActionComponent* InAction)
{
	if (Action)
	{
		Action->OnWindowChanged.RemoveAll(this);
	}
	Stats = InStats;
	Action = InAction;
	if (Action)
	{
		Action->OnWindowChanged.AddUObject(this, &UOSCombatComponent::HandleWindowChanged);
	}
}

void UOSCombatComponent::HandleWindowChanged(EOSWindowType Type, bool bBegin)
{
	if (Type == EOSWindowType::Hit && bBegin)
	{
		HitActors.Reset();
		SweepHits();
	}
}

void UOSCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Action && Action->IsWindowActive(EOSWindowType::Hit))
	{
		SweepHits();
	}
}

void UOSCombatComponent::SweepHits()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Action)
	{
		return;
	}

	// A capsule lying along the owner's forward axis, from the owner's centre out to Reach.
	const float Scale = Owner->GetActorScale3D().X;
	const float Radius = FMath::Max(1.f, Weapon.Radius * Scale);
	const float HalfLength = FMath::Max(Radius, Weapon.Reach * Scale * 0.5f);
	const FVector Forward = Owner->GetActorForwardVector();
	const FVector Center = Owner->GetActorLocation() + Forward * HalfLength;
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Forward).ToQuat();

	if (CVarOSDebugCombat.GetValueOnGameThread() != 0)
	{
		DrawDebugCapsule(World, Center, HalfLength, Radius, Rotation, FColor::Red, false, 0.1f);
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OSWeaponSweep), false, Owner);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, Rotation, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeCapsule(Radius, HalfLength), Params);

	FOSHitInfo Hit;
	Hit.Damage = Weapon.Damage * Action->GetCurrent().DamageScale;
	Hit.PoiseDamage = Weapon.PoiseDamage * Action->GetCurrent().PoiseDamageScale;
	Hit.bUnblockable = Action->GetCurrent().bUnblockable;
	Hit.Origin = Owner->GetActorLocation();
	Hit.Instigator = Owner;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!Victim || Victim == Owner || HitActors.Contains(Victim))
		{
			continue;
		}
		UOSCombatComponent* VictimCombat = Victim->FindComponentByClass<UOSCombatComponent>();
		if (!VictimCombat || VictimCombat->Faction == Faction)
		{
			continue;
		}

		HitActors.Add(Victim);
		const FOSHitResult Result = VictimCombat->ReceiveHit(Hit);
		OnHitDealt.Broadcast(Result, Victim);

		if (Result.Outcome == EOSHitOutcome::PerfectGuarded)
		{
			// Our action is replaced, so this Hit window is over.
			Action->StartSlot(OSSlots::Parried, true);
			return;
		}
	}
}

FOSDefenseState UOSCombatComponent::BuildDefenseState(const FOSHitInfo& Hit) const
{
	FOSDefenseState Defense;
	if (Action)
	{
		Defense.bInvulnerable = Action->IsWindowActive(EOSWindowType::Invulnerable);
		Defense.bGuarding = Action->IsWindowActive(EOSWindowType::Guard);
		Defense.bPerfectGuard = Action->IsWindowActive(EOSWindowType::PerfectGuard);
		Defense.bHyperArmor = Action->IsWindowActive(EOSWindowType::HyperArmor);
	}
	if (const AActor* Owner = GetOwner())
	{
		Defense.bFacingAttacker = OSCombat::IsInGuardArc(Owner->GetActorForwardVector(), Hit.Origin - Owner->GetActorLocation(), Weapon.GuardAngle);
	}
	Defense.GuardDamageReduction = Weapon.GuardDamageReduction;
	Defense.GuardStaminaScale = Weapon.GuardStaminaScale;
	Defense.Stamina = Stats ? Stats->GetStamina() : 0.f;
	return Defense;
}

FOSHitResult UOSCombatComponent::ReceiveHit(const FOSHitInfo& Hit)
{
	FOSHitResult Result;
	if (!Stats || Stats->IsDead())
	{
		return Result;
	}

	Result = OSCombat::ResolveHit(Hit, BuildDefenseState(Hit));
	if (Result.Outcome == EOSHitOutcome::Ignored)
	{
		return Result;
	}

	Stats->SpendStamina(Result.StaminaDamage);
	Result.HealthDamage = Stats->ApplyDamage(Result.HealthDamage);

	if (Stats->IsDead())
	{
		// The owner reacts to UOSStatsComponent::OnDeath.
		Result.Outcome = EOSHitOutcome::Killed;
	}
	else if (Result.Outcome == EOSHitOutcome::GuardBroken)
	{
		if (Action) { Action->StartSlot(OSSlots::GuardBreak, true); }
	}
	else if (Result.Outcome == EOSHitOutcome::Hit && Result.PoiseDamage > 0.f && Stats->ApplyPoiseDamage(Result.PoiseDamage))
	{
		Result.Outcome = EOSHitOutcome::Staggered;
		if (Action) { Action->StartSlot(OSSlots::Stagger, true); }
	}

	OnHitReceived.Broadcast(Result, Hit);
	return Result;
}
