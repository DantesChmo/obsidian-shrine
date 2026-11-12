#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSDataTypes.h"

#include "OSAIBrainComponent.generated.h"

class AOSCharacterBase;

/**
 * Data-driven enemy behaviour: wait for the player, close in, pick moves from the AI profile by range,
 * weight, cooldown and phase. Steers directly towards the target (no navmesh), which is enough for arenas.
 */
UCLASS()
class OBSIDIANSHRINE_API UOSAIBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSAIBrainComponent();

	void SetProfile(const FOSAIProfileDef& InProfile);
	const FOSAIProfileDef& GetProfile() const { return Profile; }

	/** Non-null while the enemy is aggressive. */
	AOSCharacterBase* GetTarget() const { return Target.Get(); }
	int32 GetPhase() const { return Phase; }

	/** Index into Profile.Moves of a usable move picked by weight, or INDEX_NONE. */
	static int32 PickMove(const FOSAIProfileDef& Profile, float Distance, int32 Phase, const TMap<FName, float>& Cooldowns, FRandomStream& Random);

	/** Number of phase thresholds the health fraction has dropped through. */
	static int32 ComputePhase(const FOSAIProfileDef& Profile, float HealthFraction);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FOSAIProfileDef Profile;
	bool bHasProfile = false;

	TWeakObjectPtr<AOSCharacterBase> Target;
	FVector Home = FVector::ZeroVector;
	int32 Phase = 0;
	bool bReturning = false;
	float AttackDelayLeft = 0.f;
	TMap<FName, float> Cooldowns;
	FRandomStream Random;

	AOSCharacterBase* FindPlayer() const;
	void MoveTowards(AOSCharacterBase* Self, const FVector& Location, float StopDistance) const;
	/** Distance at which the enemy stops approaching: inside the reach of its closest-range move. */
	float GetApproachDistance() const;
};
