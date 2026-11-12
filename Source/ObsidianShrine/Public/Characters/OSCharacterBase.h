#pragma once

#include "CoreMinimal.h"
#include "Data/OSDataTypes.h"
#include "GameFramework/Character.h"

#include "OSCharacterBase.generated.h"

class AOSCharacterBase;
class UAnimSequenceBase;
class UMaterialInstanceDynamic;
class UOSActionComponent;
class UOSCombatComponent;
class UOSStatsComponent;
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

	bool ApplyDefinition(FName InCharacterId);
	const FOSCharacterDef& GetDefinition() const { return Definition; }

	UOSStatsComponent* GetStats() const { return Stats; }
	UOSActionComponent* GetAction() const { return Action; }
	UOSCombatComponent* GetCombat() const { return Combat; }

	bool IsAlive() const;

	/** Who this character fights right now: lock-on target for the player, aggro target for AI. */
	virtual AActor* GetCombatTarget() const { return nullptr; }

	/** World-space direction the character wants to go (stick / towards target). Zero if none. */
	virtual FVector GetDesiredActionDirection() const { return FVector::ZeroVector; }

	FVector GetLastGroundedLocation() const { return LastGroundedLocation; }

	FOSOnCharacterDied OnDied;

	virtual void BeginPlay() override;
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
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	FOSCharacterDef Definition;

	/** Set by subclasses each frame; the base turns it into speed and stamina drain. */
	bool bWantsSprint = false;
	bool bSprinting = false;

	/** Degrees per second when turning towards the combat target. */
	float TurnRate = 720.f;

	/** Called once when health reaches zero, after the death action started. */
	virtual void OnDeath() {}

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAnimSequenceBase>> AnimationCache;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> LoopingAnimation;

	FVector ActionMoveDirection = FVector::ForwardVector;
	FVector LastGroundedLocation = FVector::ZeroVector;
	FLinearColor BaseColor = FLinearColor::Gray;
	int32 TintState = -1;
	bool bHasSkeletalMesh = false;
	bool bUsesAnimInstance = false;

	void HandleActionStarted(const FOSActionDef& Def);
	void HandleDeath();

	void ApplyVisuals();
	void UpdateMovement(float DeltaTime);
	void UpdateFacing(float DeltaTime);
	void UpdatePlaceholder();
	void UpdateLocomotionAnimation();
	void PlayActionAnimation(const FOSActionDef& Def);
	UAnimSequenceBase* LoadAnimation(const FString& Path);
	void FaceDirection(const FVector& Direction);
};
