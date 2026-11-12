#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "OSPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Owns the input actions and key bindings. They are built in code (no input assets) and live on the
 * controller so they survive the pawn being replaced on respawn.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Creates the actions and registers the mapping context once. Safe to call repeatedly. */
	void EnsureInput();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookMouseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HeavyAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GuardAction;
	/** Keyboard: dodges on press. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeAction;
	/** Keyboard: sprint while held. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	/** Gamepad: tap to dodge, hold to sprint. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> DodgeSprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LockOnAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TargetLeftAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TargetRightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;

private:
	bool bContextRegistered = false;
};
