#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSDataTypes.h"

#include "OSCombatComponent.generated.h"

class UOSActionComponent;
class UOSStatsComponent;

struct FOSHitInfo
{
	float Damage = 0.f;
	float PoiseDamage = 0.f;
	bool bUnblockable = false;
	/** Where the attack comes from; decides whether a guard faces it. */
	FVector Origin = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Instigator;
};

/** What the defender is doing at the moment of impact. */
struct FOSDefenseState
{
	bool bInvulnerable = false;
	bool bGuarding = false;
	bool bPerfectGuard = false;
	bool bHyperArmor = false;
	bool bFacingAttacker = true;
	float GuardDamageReduction = 0.f;
	float GuardStaminaScale = 1.f;
	float Stamina = 0.f;
};

enum class EOSHitOutcome : uint8
{
	/** Invulnerable or already dead. */
	Ignored,
	PerfectGuarded,
	Guarded,
	GuardBroken,
	Hit,
	Staggered,
	Killed
};

struct FOSHitResult
{
	EOSHitOutcome Outcome = EOSHitOutcome::Ignored;
	float HealthDamage = 0.f;
	float StaminaDamage = 0.f;
	float PoiseDamage = 0.f;
};

namespace OSCombat
{
	/**
	 * Pure damage rules. Returns Ignored, PerfectGuarded, Guarded, GuardBroken or Hit;
	 * Staggered and Killed are decided by UOSCombatComponent once the numbers are applied to stats.
	 */
	OBSIDIANSHRINE_API FOSHitResult ResolveHit(const FOSHitInfo& Hit, const FOSDefenseState& Defense);
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FOSOnHitReceived, const FOSHitResult&, const FOSHitInfo&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOSOnHitDealt, const FOSHitResult&, AActor* /*Victim*/);

/**
 * Turns Hit windows of the action component into damage on other factions, and resolves incoming hits
 * against the owner's stats and current action (i-frames, guard, perfect guard, poise).
 */
UCLASS()
class OBSIDIANSHRINE_API UOSCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSCombatComponent();

	/** Called from BeginPlay with the owner's components; call directly when there is no owner. */
	void Setup(UOSStatsComponent* InStats, UOSActionComponent* InAction);

	void SetWeapon(const FOSWeaponDef& InWeapon) { Weapon = InWeapon; }
	const FOSWeaponDef& GetWeapon() const { return Weapon; }

	EOSFaction Faction = EOSFaction::Enemy;

	FOSHitResult ReceiveHit(const FOSHitInfo& Hit);

	FOSOnHitReceived OnHitReceived;
	FOSOnHitDealt OnHitDealt;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOSStatsComponent> Stats;

	UPROPERTY(Transient)
	TObjectPtr<UOSActionComponent> Action;

	FOSWeaponDef Weapon;

	/** Actors already hit by the current Hit window. */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	void HandleWindowChanged(EOSWindowType Type, bool bBegin);
	void SweepHits();
	FOSDefenseState BuildDefenseState(const FOSHitInfo& Hit) const;
};
