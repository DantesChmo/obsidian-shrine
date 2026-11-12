#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "OSLockOnComponent.generated.h"

class AOSCharacterBase;

/**
 * Target lock for the player: picks a target, keeps the camera on it, drops it when it dies or gets too far.
 * Distances, angles and camera behaviour come from the LockOn section of the game settings.
 */
UCLASS()
class OBSIDIANSHRINE_API UOSLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSLockOnComponent();

	AOSCharacterBase* GetTarget() const { return Target.Get(); }

	void Toggle();
	void Clear();
	/** Direction < 0 switches to the nearest target on the left, > 0 on the right. */
	void Switch(float Direction);

	/**
	 * Lower is better; negative means the candidate cannot be acquired.
	 * Mostly angular, so the target nearest to the centre of the screen wins.
	 */
	static float ScoreCandidate(const FVector& ViewLocation, const FVector& ViewDirection, const FVector& CandidateLocation,
		float MaxDistance, float MaxAngleDegrees);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<AOSCharacterBase> Target;

	bool IsCandidate(const AOSCharacterBase* Candidate) const;
	bool HasLineOfSight(const FVector& ViewLocation, const AOSCharacterBase* Candidate) const;
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
};
