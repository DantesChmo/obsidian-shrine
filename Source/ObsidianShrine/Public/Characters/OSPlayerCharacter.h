#pragma once

#include "CoreMinimal.h"
#include "Characters/OSCharacterBase.h"

#include "OSPlayerCharacter.generated.h"

class AOSInteractable;
class UCameraComponent;
class UOSLockOnComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * The player: third-person camera, input, lock-on and interaction on top of the shared combat core.
 * Which character it is and how the camera behaves come from the game settings.
 */
UCLASS()
class OBSIDIANSHRINE_API AOSPlayerCharacter : public AOSCharacterBase
{
	GENERATED_BODY()

public:
	AOSPlayerCharacter();

	UOSLockOnComponent* GetLockOn() const { return LockOn; }
	AOSInteractable* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	virtual AActor* GetCombatTarget() const override;
	virtual FVector GetDesiredActionDirection() const override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void OnDeath() override;
	virtual void HandleDataReloaded() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Obsidian")
	TObjectPtr<UOSLockOnComponent> LockOn;

	TWeakObjectPtr<AOSInteractable> FocusedInteractable;

	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bGuardHeld = false;
	bool bSprintKeyHeld = false;
	bool bDodgeSprintHeld = false;
	float DodgeSprintHeldTime = 0.f;

	void ApplyCameraSettings();
	void Look(const FVector2D& Delta);

	void OnMove(const FInputActionValue& Value);
	void OnMoveEnd(const FInputActionValue& Value);
	void OnLookMouse(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnLight(const FInputActionValue& Value);
	void OnHeavy(const FInputActionValue& Value);
	void OnGuardStart(const FInputActionValue& Value);
	void OnGuardEnd(const FInputActionValue& Value);
	void OnDodge(const FInputActionValue& Value);
	void OnSprintStart(const FInputActionValue& Value);
	void OnSprintEnd(const FInputActionValue& Value);
	void OnDodgeSprintStart(const FInputActionValue& Value);
	void OnDodgeSprintEnd(const FInputActionValue& Value);
	void OnLockOn(const FInputActionValue& Value);
	void OnTargetLeft(const FInputActionValue& Value);
	void OnTargetRight(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
};
