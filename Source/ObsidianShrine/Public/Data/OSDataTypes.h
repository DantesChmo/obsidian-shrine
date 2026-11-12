#pragma once

#include "CoreMinimal.h"

#include "OSDataTypes.generated.h"

/**
 * Plain data definitions loaded from Content/Data/*.json by UOSDataSubsystem.
 * Field names here are the JSON keys. Asset references are path strings so a definition
 * stays valid (and falls back to placeholder visuals) when the asset is not imported.
 */

/** Well-known action slots. A slot is resolved to an action id through the character and weapon definitions. */
namespace OSSlots
{
	inline const FName Light(TEXT("Light"));
	inline const FName Heavy(TEXT("Heavy"));
	inline const FName Guard(TEXT("Guard"));
	inline const FName Dodge(TEXT("Dodge"));
	inline const FName Stagger(TEXT("Stagger"));
	inline const FName GuardBreak(TEXT("GuardBreak"));
	inline const FName Parried(TEXT("Parried"));
	inline const FName Death(TEXT("Death"));
}

UENUM()
enum class EOSWindowType : uint8
{
	/** Weapon deals damage. */
	Hit,
	/** Incoming hits are ignored (dodge i-frames, boss transitions). */
	Invulnerable,
	/** Frontal hits are blocked for stamina. */
	Guard,
	/** Frontal hits are negated and the attacker is thrown into its Parried action. */
	PerfectGuard,
	/** Hits deal damage but cannot stagger. */
	HyperArmor,
	/** Another action may start (combo / dodge cancel). */
	Cancel
};

UENUM()
enum class EOSMoveDirection : uint8
{
	Forward,
	Backward,
	/** Desired direction of the character (stick for the player, towards target for AI); backward if none. */
	Input
};

UENUM()
enum class EOSFaction : uint8
{
	Player,
	Enemy
};

USTRUCT()
struct FOSActionWindow
{
	GENERATED_BODY()

	UPROPERTY()
	EOSWindowType Type = EOSWindowType::Hit;

	/** Seconds from action start. */
	UPROPERTY()
	float Start = 0.f;

	/** Seconds from action start; End <= Start means "until the action ends". */
	UPROPERTY()
	float End = 0.f;
};

/** One timed action: an attack, dodge, guard stance, hit reaction, death. */
USTRUCT()
struct FOSActionDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	/** Seconds. <= 0 means the action is held until its slot is released (guard). */
	UPROPERTY()
	float Duration = 0.5f;

	/** Spent on start. Starting only requires stamina above zero. */
	UPROPERTY()
	float StaminaCost = 0.f;

	/** Stamina regeneration multiplier while the action runs. */
	UPROPERTY()
	float StaminaRegenScale = 1.f;

	UPROPERTY()
	TArray<FOSActionWindow> Windows;

	/** Started instead of the slot's base action when the same slot is requested during this action. */
	UPROPERTY()
	FName ComboNext;

	/** Started automatically when this action finishes on its own (enemy strings). */
	UPROPERTY()
	FName AutoNext;

	UPROPERTY()
	float DamageScale = 1.f;

	UPROPERTY()
	float PoiseDamageScale = 1.f;

	/** Guards do not stop this attack. */
	UPROPERTY()
	bool bUnblockable = false;

	/** Displacement in cm applied between MoveStart and MoveEnd. */
	UPROPERTY()
	float MoveDistance = 0.f;

	UPROPERTY()
	float MoveStart = 0.f;

	UPROPERTY()
	float MoveEnd = 0.f;

	UPROPERTY()
	EOSMoveDirection MoveDirection = EOSMoveDirection::Forward;

	/** Turn to the target (or the desired direction) on the first frame. */
	UPROPERTY()
	bool bSnapFacing = false;

	/** The character keeps turning towards its target until this time. */
	UPROPERTY()
	float TrackingEnd = 0.f;

	/** Fraction of normal movement speed allowed while the action runs (0 = rooted). */
	UPROPERTY()
	float MoveSpeedScale = 0.f;

	/** UAnimMontage or UAnimSequence path. Optional. */
	UPROPERTY()
	FString Animation;

	/** Stretch the animation so its length matches Duration. */
	UPROPERTY()
	bool bFitAnimation = true;

	bool IsHeld() const { return Duration <= 0.f; }
};

USTRUCT()
struct FOSWeaponDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	float Damage = 10.f;

	UPROPERTY()
	float PoiseDamage = 10.f;

	/** Length of the hit volume in front of the wielder, cm. */
	UPROPERTY()
	float Reach = 160.f;

	UPROPERTY()
	float Radius = 60.f;

	/** Fraction of damage removed when guarding with this weapon. */
	UPROPERTY()
	float GuardDamageReduction = 0.8f;

	/** Stamina lost per point of guarded damage. */
	UPROPERTY()
	float GuardStaminaScale = 0.6f;

	/** Slot -> action id. */
	UPROPERTY()
	TMap<FName, FName> Actions;

	/** UStaticMesh path. Optional. */
	UPROPERTY()
	FString Mesh;

	UPROPERTY()
	FName AttachSocket = TEXT("hand_r");
};

USTRUCT()
struct FOSCharacterDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	float MaxHealth = 100.f;

	UPROPERTY()
	float MaxStamina = 100.f;

	UPROPERTY()
	float StaminaRegen = 35.f;

	UPROPERTY()
	float StaminaRegenDelay = 0.8f;

	/** 0 means every unarmoured hit staggers. */
	UPROPERTY()
	float MaxPoise = 0.f;

	UPROPERTY()
	float PoiseResetDelay = 4.f;

	UPROPERTY()
	float WalkSpeed = 400.f;

	UPROPERTY()
	float SprintSpeed = 620.f;

	UPROPERTY()
	float SprintStaminaPerSecond = 12.f;

	UPROPERTY()
	FName Weapon;

	/** Slot -> action id. Overrides the weapon's slots. */
	UPROPERTY()
	TMap<FName, FName> Actions;

	UPROPERTY()
	FName AIProfile;

	UPROPERTY()
	int32 CurrencyReward = 0;

	UPROPERTY()
	bool bBoss = false;

	UPROPERTY()
	float Scale = 1.f;

	/** "#RRGGBB" tint of the placeholder body. */
	UPROPERTY()
	FString PlaceholderColor;

	/** USkeletalMesh path. Optional: without it a placeholder body is shown. */
	UPROPERTY()
	FString Mesh;

	UPROPERTY()
	float MeshYaw = -90.f;

	/** UAnimInstance class path. Optional: without it animations play in single-node mode. */
	UPROPERTY()
	FString AnimClass;

	UPROPERTY()
	FString IdleAnimation;

	UPROPERTY()
	FString MoveAnimation;
};

USTRUCT()
struct FOSAIMoveDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Action;

	UPROPERTY()
	float MinRange = 0.f;

	UPROPERTY()
	float MaxRange = 250.f;

	UPROPERTY()
	float Weight = 1.f;

	UPROPERTY()
	float Cooldown = 0.f;

	UPROPERTY()
	int32 MinPhase = 0;

	UPROPERTY()
	int32 MaxPhase = 99;
};

USTRUCT()
struct FOSAIPhaseDef
{
	GENERATED_BODY()

	/** The phase starts when health fraction drops to this value or below. */
	UPROPERTY()
	float HealthBelow = 0.5f;

	/** Played once on entering the phase. Optional. */
	UPROPERTY()
	FName EnterAction;
};

USTRUCT()
struct FOSAIProfileDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	float AggroRadius = 1200.f;

	/** Distance from the spawn point at which the enemy gives up. */
	UPROPERTY()
	float LeashRadius = 3000.f;

	UPROPERTY()
	float AttackDelayMin = 0.5f;

	UPROPERTY()
	float AttackDelayMax = 1.5f;

	/** Degrees per second. */
	UPROPERTY()
	float TurnRate = 360.f;

	UPROPERTY()
	TArray<FOSAIMoveDef> Moves;

	/** Phases after the initial one, in order of decreasing HealthBelow. */
	UPROPERTY()
	TArray<FOSAIPhaseDef> Phases;
};

/** Shape of one data file. Any file may contain any subset of the arrays. */
USTRUCT()
struct FOSDataFile
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FOSActionDef> Actions;

	UPROPERTY()
	TArray<FOSWeaponDef> Weapons;

	UPROPERTY()
	TArray<FOSCharacterDef> Characters;

	UPROPERTY()
	TArray<FOSAIProfileDef> AIProfiles;
};
