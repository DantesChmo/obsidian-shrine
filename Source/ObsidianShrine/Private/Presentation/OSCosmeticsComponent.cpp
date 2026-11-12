#include "Presentation/OSCosmeticsComponent.h"

#include "Combat/OSCombatComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ObsidianShrine.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

UOSCosmeticsComponent::UOSCosmeticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSCosmeticsComponent::SetDefinition(const FOSCosmeticsDef& InDefinition)
{
	Definition = InDefinition;
}

const FOSCueDef& UOSCosmeticsComponent::GetCue(const FOSCosmeticsDef& Def, EOSCue Cue)
{
	switch (Cue)
	{
	case EOSCue::Hit: return Def.Hit;
	case EOSCue::Guard: return Def.Guard;
	case EOSCue::PerfectGuard: return Def.PerfectGuard;
	case EOSCue::Dodge: return Def.Dodge;
	case EOSCue::Footstep: return Def.Footstep;
	case EOSCue::Death: return Def.Death;
	default: return Def.PhaseChange;
	}
}

bool UOSCosmeticsComponent::CueForHit(EOSHitOutcome Outcome, EOSCue& OutCue)
{
	switch (Outcome)
	{
	case EOSHitOutcome::PerfectGuarded:
		OutCue = EOSCue::PerfectGuard;
		return true;
	case EOSHitOutcome::Guarded:
	case EOSHitOutcome::GuardBroken:
		OutCue = EOSCue::Guard;
		return true;
	case EOSHitOutcome::Hit:
	case EOSHitOutcome::Staggered:
	case EOSHitOutcome::Killed:
		OutCue = EOSCue::Hit;
		return true;
	default:
		return false;
	}
}

const FOSCueDef& UOSCosmeticsComponent::Play(EOSCue Cue, const FVector& Location)
{
	const FOSCueDef& Def = GetCue(Definition, Cue);
	UWorld* World = GetWorld();
	if (!World)
	{
		return Def;
	}

	if (USoundBase* Sound = Cast<USoundBase>(LoadAsset(Def.Sound)))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Def.Volume);
	}

	UObject* Effect = LoadAsset(Def.Effect);
	const FRotator Rotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Effect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Niagara, Location, Rotation, FVector(Def.EffectScale));
	}
	else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Effect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Location, Rotation, FVector(Def.EffectScale));
	}
	else if (Effect)
	{
		UE_LOG(LogObsidian, Warning, TEXT("%s: '%s' is not a Niagara system or a particle system"), *GetNameSafe(GetOwner()), *Def.Effect);
	}
	return Def;
}

int32 UOSCosmeticsComponent::AdvanceFootsteps(float Distance)
{
	const float Stride = FMath::Max(10.f, Definition.FootstepDistance);
	FootstepTravel += FMath::Max(0.f, Distance);
	const int32 Steps = FMath::FloorToInt(FootstepTravel / Stride);
	FootstepTravel -= Steps * Stride;
	return Steps;
}

UObject* UOSCosmeticsComponent::LoadAsset(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	if (const TObjectPtr<UObject>* Cached = AssetCache.Find(Path))
	{
		return *Cached;
	}
	UObject* Asset = FSoftObjectPath(Path).TryLoad();
	if (!Asset)
	{
		UE_LOG(LogObsidian, Warning, TEXT("%s: asset '%s' not found"), *GetNameSafe(GetOwner()), *Path);
	}
	AssetCache.Add(Path, Asset);
	return Asset;
}
