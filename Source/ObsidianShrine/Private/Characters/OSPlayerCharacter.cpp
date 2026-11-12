#include "Characters/OSPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/OSPlayerController.h"
#include "Combat/OSActionComponent.h"
#include "Combat/OSLockOnComponent.h"
#include "Core/OSGameMode.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "World/OSInteractable.h"

namespace
{
	const float MouseSensitivity = 0.12f;
	/** Degrees per second at full stick deflection. */
	const float StickLookSpeed = 160.f;
	const float MinPitch = -65.f;
	const float MaxPitch = 55.f;
	/** Holding the gamepad dodge button longer than this sprints instead of dodging. */
	const float DodgeTapTime = 0.25f;
}

AOSPlayerCharacter::AOSPlayerCharacter()
{
	CharacterId = TEXT("player");
	Faction = EOSFaction::Player;
	AutoPossessAI = EAutoPossessAI::Disabled;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 430.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	LockOn = CreateDefaultSubobject<UOSLockOnComponent>(TEXT("LockOn"));
}

AActor* AOSPlayerCharacter::GetCombatTarget() const
{
	return LockOn->GetTarget();
}

FVector AOSPlayerCharacter::GetDesiredActionDirection() const
{
	if (MoveInput.IsNearlyZero() || !GetController())
	{
		return FVector::ZeroVector;
	}
	const FRotationMatrix YawMatrix(FRotator(0.f, GetController()->GetControlRotation().Yaw, 0.f));
	return (YawMatrix.GetUnitAxis(EAxis::X) * MoveInput.Y + YawMatrix.GetUnitAxis(EAxis::Y) * MoveInput.X).GetSafeNormal();
}

void AOSPlayerCharacter::Tick(float DeltaTime)
{
	if (bDodgeSprintHeld)
	{
		DodgeSprintHeldTime += DeltaTime;
	}
	bWantsSprint = bSprintKeyHeld || (bDodgeSprintHeld && DodgeSprintHeldTime > DodgeTapTime);

	Super::Tick(DeltaTime);

	if (!IsAlive())
	{
		FocusedInteractable.Reset();
		return;
	}

	// Keep guarding through other actions while the button stays down.
	if (bGuardHeld && !GetAction()->IsBusy())
	{
		GetAction()->RequestSlot(OSSlots::Guard);
	}

	FocusedInteractable = AOSInteractable::FindFor(this);
}

void AOSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	AOSPlayerController* PlayerController = Cast<AOSPlayerController>(GetController());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PlayerController || !Input)
	{
		return;
	}
	PlayerController->EnsureInput();

	Input->BindAction(PlayerController->MoveAction, ETriggerEvent::Triggered, this, &AOSPlayerCharacter::OnMove);
	Input->BindAction(PlayerController->MoveAction, ETriggerEvent::Completed, this, &AOSPlayerCharacter::OnMoveEnd);
	Input->BindAction(PlayerController->LookMouseAction, ETriggerEvent::Triggered, this, &AOSPlayerCharacter::OnLookMouse);
	Input->BindAction(PlayerController->LookStickAction, ETriggerEvent::Triggered, this, &AOSPlayerCharacter::OnLookStick);
	Input->BindAction(PlayerController->LightAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnLight);
	Input->BindAction(PlayerController->HeavyAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnHeavy);
	Input->BindAction(PlayerController->GuardAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnGuardStart);
	Input->BindAction(PlayerController->GuardAction, ETriggerEvent::Completed, this, &AOSPlayerCharacter::OnGuardEnd);
	Input->BindAction(PlayerController->DodgeAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnDodge);
	Input->BindAction(PlayerController->SprintAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnSprintStart);
	Input->BindAction(PlayerController->SprintAction, ETriggerEvent::Completed, this, &AOSPlayerCharacter::OnSprintEnd);
	Input->BindAction(PlayerController->DodgeSprintAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnDodgeSprintStart);
	Input->BindAction(PlayerController->DodgeSprintAction, ETriggerEvent::Completed, this, &AOSPlayerCharacter::OnDodgeSprintEnd);
	Input->BindAction(PlayerController->LockOnAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnLockOn);
	Input->BindAction(PlayerController->TargetLeftAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnTargetLeft);
	Input->BindAction(PlayerController->TargetRightAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnTargetRight);
	Input->BindAction(PlayerController->InteractAction, ETriggerEvent::Started, this, &AOSPlayerCharacter::OnInteract);
}

void AOSPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
	if (!IsAlive())
	{
		return;
	}
	// Speed and whether movement is allowed at all are decided by AOSCharacterBase::UpdateMovement.
	AddMovementInput(GetDesiredActionDirection(), FMath::Min(1.f, MoveInput.Size()));
}

void AOSPlayerCharacter::OnMoveEnd(const FInputActionValue& Value)
{
	MoveInput = FVector2D::ZeroVector;
}

void AOSPlayerCharacter::Look(const FVector2D& Delta)
{
	// While locked on, the lock-on component owns the camera.
	if (!GetController() || LockOn->GetTarget())
	{
		return;
	}
	FRotator Rotation = GetController()->GetControlRotation();
	Rotation.Yaw += Delta.X;
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Delta.Y, MinPitch, MaxPitch);
	Rotation.Roll = 0.f;
	GetController()->SetControlRotation(Rotation);
}

void AOSPlayerCharacter::OnLookMouse(const FInputActionValue& Value)
{
	Look(Value.Get<FVector2D>() * MouseSensitivity);
}

void AOSPlayerCharacter::OnLookStick(const FInputActionValue& Value)
{
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	Look(Value.Get<FVector2D>() * StickLookSpeed * DeltaTime);
}

void AOSPlayerCharacter::OnLight(const FInputActionValue& Value)
{
	GetAction()->RequestSlot(OSSlots::Light);
}

void AOSPlayerCharacter::OnHeavy(const FInputActionValue& Value)
{
	GetAction()->RequestSlot(OSSlots::Heavy);
}

void AOSPlayerCharacter::OnGuardStart(const FInputActionValue& Value)
{
	bGuardHeld = true;
	GetAction()->RequestSlot(OSSlots::Guard);
}

void AOSPlayerCharacter::OnGuardEnd(const FInputActionValue& Value)
{
	bGuardHeld = false;
	GetAction()->StopSlot(OSSlots::Guard);
}

void AOSPlayerCharacter::OnDodge(const FInputActionValue& Value)
{
	GetAction()->RequestSlot(OSSlots::Dodge);
}

void AOSPlayerCharacter::OnSprintStart(const FInputActionValue& Value)
{
	bSprintKeyHeld = true;
}

void AOSPlayerCharacter::OnSprintEnd(const FInputActionValue& Value)
{
	bSprintKeyHeld = false;
}

void AOSPlayerCharacter::OnDodgeSprintStart(const FInputActionValue& Value)
{
	bDodgeSprintHeld = true;
	DodgeSprintHeldTime = 0.f;
}

void AOSPlayerCharacter::OnDodgeSprintEnd(const FInputActionValue& Value)
{
	if (bDodgeSprintHeld && DodgeSprintHeldTime <= DodgeTapTime)
	{
		GetAction()->RequestSlot(OSSlots::Dodge);
	}
	bDodgeSprintHeld = false;
}

void AOSPlayerCharacter::OnLockOn(const FInputActionValue& Value)
{
	LockOn->Toggle();
}

void AOSPlayerCharacter::OnTargetLeft(const FInputActionValue& Value)
{
	LockOn->Switch(-1.f);
}

void AOSPlayerCharacter::OnTargetRight(const FInputActionValue& Value)
{
	LockOn->Switch(1.f);
}

void AOSPlayerCharacter::OnInteract(const FInputActionValue& Value)
{
	if (!IsAlive() || GetAction()->IsBusy())
	{
		return;
	}
	if (AOSInteractable* Interactable = AOSInteractable::FindFor(this))
	{
		Interactable->Interact(this);
	}
}

void AOSPlayerCharacter::OnDeath()
{
	LockOn->Clear();
	if (AOSGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOSGameMode>() : nullptr)
	{
		GameMode->HandlePlayerDeath(this);
	}
}
