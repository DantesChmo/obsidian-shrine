#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSDataTypes.h"

#include "OSCosmeticsComponent.generated.h"

enum class EOSHitOutcome : uint8;

/** Presentation events a character has cues for (FOSCosmeticsDef). */
enum class EOSCue : uint8
{
	Hit,
	Guard,
	PerfectGuard,
	Dodge,
	Footstep,
	Death,
	PhaseChange
};

/**
 * Plays the sounds and effects of a character's cosmetics definition. Hit-stop and camera shake are applied
 * by the character, which knows who else is involved. Assets load on first use and are cached.
 */
UCLASS()
class OBSIDIANSHRINE_API UOSCosmeticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSCosmeticsComponent();

	/** Keeps the footstep distance already walked, so a data reload does not skip or double a step. */
	void SetDefinition(const FOSCosmeticsDef& InDefinition);
	const FOSCosmeticsDef& GetDefinition() const { return Definition; }

	static const FOSCueDef& GetCue(const FOSCosmeticsDef& Def, EOSCue Cue);
	/** The cue the defender plays for a hit outcome; false for outcomes that show nothing (Ignored). */
	static bool CueForHit(EOSHitOutcome Outcome, EOSCue& OutCue);

	/** Plays the cue's sound and effect at Location and returns the cue for the rest (shake, hit-stop). */
	const FOSCueDef& Play(EOSCue Cue, const FVector& Location);

	/** Adds walked distance; returns how many footsteps fell into it. */
	int32 AdvanceFootsteps(float Distance);

private:
	FOSCosmeticsDef Definition;
	float FootstepTravel = 0.f;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UObject>> AssetCache;

	UObject* LoadAsset(const FString& Path);
};
