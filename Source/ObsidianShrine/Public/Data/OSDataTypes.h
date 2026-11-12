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

	/** Full width in degrees of the frontal sector a guard covers; 360 guards all around. */
	UPROPERTY()
	float GuardAngle = 180.f;

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

	/** Degrees per second, both when turning to the movement direction and to the target. */
	UPROPERTY()
	float TurnRate = 720.f;

	/** cm/s^2 when starting to move. */
	UPROPERTY()
	float Acceleration = 4096.f;

	/** cm/s^2 when input stops. */
	UPROPERTY()
	float BrakingDeceleration = 4096.f;

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

	UPROPERTY()
	TArray<FOSAIMoveDef> Moves;

	/** Phases after the initial one, in order of decreasing HealthBelow. */
	UPROPERTY()
	TArray<FOSAIPhaseDef> Phases;
};

/** Third-person camera of the player. */
USTRUCT()
struct FOSCameraDef
{
	GENERATED_BODY()

	/** Distance from the player, cm. */
	UPROPERTY()
	float ArmLength = 430.f;

	/** How far above the player's centre the camera sits, cm. */
	UPROPERTY()
	float HeightOffset = 70.f;

	/** How quickly the camera catches up with the player; 0 disables the lag. */
	UPROPERTY()
	float LagSpeed = 12.f;

	/** Degrees per mouse count. */
	UPROPERTY()
	float MouseSensitivity = 0.12f;

	/** Degrees per second at full stick deflection. */
	UPROPERTY()
	float StickSpeed = 160.f;

	UPROPERTY()
	float MinPitch = -65.f;

	UPROPERTY()
	float MaxPitch = 55.f;
};

USTRUCT()
struct FOSLockOnDef
{
	GENERATED_BODY()

	UPROPERTY()
	float AcquireDistance = 2200.f;

	UPROPERTY()
	float BreakDistance = 2800.f;

	/** Half-angle of the cone in front of the camera in which targets can be acquired, degrees. */
	UPROPERTY()
	float AcquireAngle = 55.f;

	UPROPERTY()
	float CameraSpeed = 9.f;

	/** The camera looks this many degrees below the line to the target. */
	UPROPERTY()
	float CameraPitchOffset = -12.f;

	/** The camera aims from this height above the player's centre, so close targets do not pitch it wildly. */
	UPROPERTY()
	float PivotHeight = 80.f;

	UPROPERTY()
	float MinPitch = -50.f;

	UPROPERTY()
	float MaxPitch = 30.f;
};

USTRUCT()
struct FOSControlsDef
{
	GENERATED_BODY()

	/** Seconds a button press is remembered while another action is running. */
	UPROPERTY()
	float InputBuffer = 0.4f;

	/** Holding the dodge-or-sprint button longer than this sprints instead of dodging. */
	UPROPERTY()
	float DodgeTapTime = 0.25f;
};

USTRUCT()
struct FOSDeathDef
{
	GENERATED_BODY()

	/** Seconds between the player's death and the respawn. */
	UPROPERTY()
	float RespawnDelay = 3.5f;

	/** Anything below this height is treated as a fall to death. */
	UPROPERTY()
	float KillHeight = -3000.f;

	/** Seconds an enemy's corpse stays before it is removed. */
	UPROPERTY()
	float CorpseLifetime = 6.f;

	/** Fraction of held currency left at the death spot; the rest stays with the player. */
	UPROPERTY()
	float CurrencyLossFraction = 1.f;

	/** Currency still lying from an earlier death is added to the new drop instead of being lost. */
	UPROPERTY()
	bool bKeepPreviousDrop = false;
};

/** How long the centre-screen messages stay, seconds. The death message lasts until the respawn. */
USTRUCT()
struct FOSMessagesDef
{
	GENERATED_BODY()

	UPROPERTY()
	float BossDefeatedDuration = 4.f;

	UPROPERTY()
	float RestedDuration = 2.5f;
};

/** Game-wide settings: Content/Data/game.json, section Game. */
USTRUCT()
struct FOSGameDef
{
	GENERATED_BODY()

	/** Character definition the player is built from. */
	UPROPERTY()
	FName PlayerCharacter = TEXT("player");

	/** Map the game opens with. The data compiler writes it to Config/DefaultEngine.ini. */
	UPROPERTY()
	FString StartMap = TEXT("/Game/Maps/L_Sandbox");

	/** Distance at which the player can use a world object, unless the object sets its own. */
	UPROPERTY()
	float InteractRadius = 260.f;

	UPROPERTY()
	FOSCameraDef Camera;

	UPROPERTY()
	FOSLockOnDef LockOn;

	UPROPERTY()
	FOSControlsDef Controls;

	UPROPERTY()
	FOSDeathDef Death;

	UPROPERTY()
	FOSMessagesDef Messages;
};

/**
 * Key bindings: Content/Data/input.json, section Input. Every field is one input action of the game and
 * lists the engine key names (EKeys) bound to it.
 */
USTRUCT()
struct FOSInputDef
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> MoveForward;

	UPROPERTY()
	TArray<FName> MoveBackward;

	UPROPERTY()
	TArray<FName> MoveLeft;

	UPROPERTY()
	TArray<FName> MoveRight;

	/** 2D axes. */
	UPROPERTY()
	TArray<FName> MoveStick;

	/** 2D axes scaled by Camera.MouseSensitivity. */
	UPROPERTY()
	TArray<FName> LookMouse;

	/** 2D axes scaled by Camera.StickSpeed. */
	UPROPERTY()
	TArray<FName> LookStick;

	UPROPERTY()
	TArray<FName> Light;

	UPROPERTY()
	TArray<FName> Heavy;

	UPROPERTY()
	TArray<FName> Guard;

	/** Dodges on press. */
	UPROPERTY()
	TArray<FName> Dodge;

	/** Sprints while held. */
	UPROPERTY()
	TArray<FName> Sprint;

	/** Tap to dodge, hold to sprint. */
	UPROPERTY()
	TArray<FName> DodgeOrSprint;

	UPROPERTY()
	TArray<FName> LockOn;

	UPROPERTY()
	TArray<FName> TargetLeft;

	UPROPERTY()
	TArray<FName> TargetRight;

	UPROPERTY()
	TArray<FName> Interact;
};

/**
 * Every text the game shows: Content/Data/texts.json, section Texts.
 * {Name} placeholders are filled in by the game; a text may only use the placeholders its default uses.
 */
USTRUCT()
struct FOSTextsDef
{
	GENERATED_BODY()

	/** {Keys}: the keys bound to Interact; {Prompt}: what the object offers. */
	UPROPERTY()
	FString InteractPrompt = TEXT("[{Keys}]  {Prompt}");

	/** Put between key names when an action has several keys. */
	UPROPERTY()
	FString KeySeparator = TEXT(" / ");

	UPROPERTY()
	FString PromptInteract = TEXT("Interact");

	UPROPERTY()
	FString PromptShrine = TEXT("Rest at shrine");

	UPROPERTY()
	FString PromptPickup = TEXT("Recover lost shards");

	UPROPERTY()
	FString Currency = TEXT("Shards: {Amount}");

	UPROPERTY()
	FString MessageDied = TEXT("YOU DIED");

	UPROPERTY()
	FString MessageBossDefeated = TEXT("ENEMY FELLED");

	UPROPERTY()
	FString MessageRested = TEXT("SHRINE KINDLED");

	/** Engine key name -> how it is shown in prompts. Keys not listed are shown by their engine name. */
	UPROPERTY()
	TMap<FName, FString> KeyLabels;
};

/** Shape of one data file. Any file may contain any subset of the sections. */
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

	/** At most one file may contain each of the three sections below. */
	UPROPERTY()
	FOSGameDef Game;

	UPROPERTY()
	FOSInputDef Input;

	UPROPERTY()
	FOSTextsDef Texts;
};
