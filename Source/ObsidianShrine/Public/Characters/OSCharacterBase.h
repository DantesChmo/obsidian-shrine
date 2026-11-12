#pragma once

#include "CoreMinimal.h"
#include "Data/OSDataTypes.h"
#include "GameFramework/Character.h"
#include "Presentation/OSCosmeticsComponent.h"

#include "OSCharacterBase.generated.h"

class AOSCharacterBase;
class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class UOSActionComponent;
class UOSCombatComponent;
class UOSStatsComponent;
struct FOSHitInfo;
struct FOSHitResult;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOSOnCharacterDied, AOSCharacterBase*);

/**
 * Any combatant. Everything about it comes from the character definition named by CharacterId:
 * stats, weapon, action slots, visuals. Without imported meshes it shows a tinted placeholder body
 * whose weapon bar poses for wind-up, hit and guard, so combat is readable with zero assets.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AOSCharacterBase();

	/** Id of the character definition in Content/Data. */
	UPROPERTY(EditAnywhere, Category = "Obsidian")
	FName CharacterId;

	UPROPERTY(EditAnywhere, Category = "Obsidian")
	EOSFaction Faction = EOSFaction::Enemy;

	/**
	 * Rebuilds the character from its definition. bKeepState leaves health, stamina and the running action
	 * alone, which is what a data reload wants; otherwise the character starts fresh.
	 */
	bool ApplyDefinition(FName InCharacterId, bool bKeepState = false);
	const FOSCharacterDef& GetDefinition() const { return Definition; }

	UOSStatsComponent* GetStats() const { return Stats; }
	UOSActionComponent* GetAction() const { return Action; }
	UOSCombatComponent* GetCombat() const { return Combat; }
	UOSCosmeticsComponent* GetCosmetics() const { return Cosmetics; }

	/**
	 * Plays one of this character's cues: sound and effect here, camera shake for the player, hit-stop for
	 * this character and Other (the attacker, for hits).
	 */
	void PlayCue(EOSCue Cue, AOSCharacterBase* Other = nullptr);

	/** Freezes the character for Seconds of game time; overlapping hit-stops last until the longest ends. */
	void ApplyHitStop(float Seconds);
	bool IsInHitStop() const;

	/** Only the player has a camera to shake. */
	virtual void AddCameraShake(float Strength, float Duration) {}

	bool IsAlive() const;

	/** Who this character fights right now: lock-on target for the player, aggro target for AI. */
	virtual AActor* GetCombatTarget() const { return nullptr; }

	/** World-space direction the character wants to go (stick / towards target). Zero if none. */
	virtual FVector GetDesiredActionDirection() const { return FVector::ZeroVector; }

	FVector GetLastGroundedLocation() const { return LastGroundedLocation; }

	FOSOnCharacterDied OnDied;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSStatsComponent> Stats;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSActionComponent> Action;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSCosmeticsComponent> Cosmetics;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	FOSCharacterDef Definition;
	/** Copy of the definition's animation set; empty when it has none. */
	FOSAnimationSetDef AnimationSet;

	/** Set by subclasses each frame; the base turns it into speed and stamina drain. */
	bool bWantsSprint = false;
	bool bSprinting = false;

	/** Called once when health reaches zero, after the death action started. */
	virtual void OnDeath() {}

	/** The data was reloaded (os.ReloadData): take the new definition without resetting the character. */
	virtual void HandleDataReloaded();

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAnimSequenceBase>> AnimationCache;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> LoopingAnimation;

	FTimerHandle HitStopTimer;

	FVector ActionMoveDirection = FVector::ForwardVector;
	FVector LastGroundedLocation = FVector::ZeroVector;
	FLinearColor BaseColor = FLinearColor::Gray;
	int32 TintState = -1;
	bool bHasSkeletalMesh = false;
	/** The running action has its own animation, so locomotion must not replace it. */
	bool bActionAnimationPlaying = false;

	void HandleActionStarted(const FOSActionDef& Def);
	void HandleDeath();
	void HandleHitReceived(const FOSHitResult& Result, const FOSHitInfo& Hit);
	void EndHitStop();
	void UpdateFootsteps(float DeltaTime);

	void ApplyVisuals();
	void UpdateMovement(float DeltaTime);
	void UpdateFacing(float DeltaTime);
	void UpdatePlaceholder();
	void UpdateLocomotionAnimation();
	void PlayActionAnimation(const FOSActionDef& Def);
	UAnimSequenceBase* LoadAnimation(const FString& Path);
	void FaceDirection(const FVector& Direction);
};
