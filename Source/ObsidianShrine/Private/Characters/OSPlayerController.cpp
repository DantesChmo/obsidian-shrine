#include "Characters/OSPlayerController.h"

#include "Data/OSDataSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "ObsidianShrine.h"

void AOSPlayerController::EnsureInput()
{
	if (!MappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Player"));

		auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
		{
			UInputAction* NewAction = NewObject<UInputAction>(this, Name);
			NewAction->ValueType = Type;
			return NewAction;
		};

		// Move: X = right, Y = forward.
		MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
		LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
		LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
		LightAction = MakeAction(TEXT("IA_Light"), EInputActionValueType::Boolean);
		HeavyAction = MakeAction(TEXT("IA_Heavy"), EInputActionValueType::Boolean);
		GuardAction = MakeAction(TEXT("IA_Guard"), EInputActionValueType::Boolean);
		DodgeAction = MakeAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
		SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
		DodgeSprintAction = MakeAction(TEXT("IA_DodgeSprint"), EInputActionValueType::Boolean);
		LockOnAction = MakeAction(TEXT("IA_LockOn"), EInputActionValueType::Boolean);
		TargetLeftAction = MakeAction(TEXT("IA_TargetLeft"), EInputActionValueType::Boolean);
		TargetRightAction = MakeAction(TEXT("IA_TargetRight"), EInputActionValueType::Boolean);
		InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);

		ApplyBindings();

		if (UOSDataSubsystem* Data = UOSDataSubsystem::Get())
		{
			Data->OnReloaded.AddUObject(this, &AOSPlayerController::ApplyBindings);
		}
	}

	if (!bContextRegistered)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
			bContextRegistered = true;
		}
	}
}

void AOSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOSDataSubsystem* Data = UOSDataSubsystem::Get())
	{
		Data->OnReloaded.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AOSPlayerController::ApplyBindings()
{
	if (!MappingContext)
	{
		return;
	}
	MappingContext->UnmapAll();

	enum EBindingModifiers { Plain = 0, Swizzle = 1, Negate = 2, DeadZone = 4 };
	auto Map = [this](UInputAction* InputAction, const TArray<FName>& Keys, int32 Modifiers = Plain)
	{
		for (const FName KeyName : Keys)
		{
			const FKey Key(KeyName);
			if (!Key.IsValid())
			{
				UE_LOG(LogObsidian, Error, TEXT("Input: unknown key '%s' bound to %s"), *KeyName.ToString(), *InputAction->GetName());
				continue;
			}
			FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(InputAction, Key);
			if (Modifiers & DeadZone) { Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(MappingContext)); }
			if (Modifiers & Swizzle) { Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext)); }
			if (Modifiers & Negate) { Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext)); }
		}
	};

	const FOSInputDef& Input = UOSDataSubsystem::Bindings();

	// A key gives X; forward and backward are swizzled onto Y.
	Map(MoveAction, Input.MoveForward, Swizzle);
	Map(MoveAction, Input.MoveBackward, Swizzle | Negate);
	Map(MoveAction, Input.MoveRight);
	Map(MoveAction, Input.MoveLeft, Negate);
	Map(MoveAction, Input.MoveStick, DeadZone);

	Map(LookMouseAction, Input.LookMouse);
	Map(LookStickAction, Input.LookStick, DeadZone);

	Map(LightAction, Input.Light);
	Map(HeavyAction, Input.Heavy);
	Map(GuardAction, Input.Guard);
	Map(DodgeAction, Input.Dodge);
	Map(SprintAction, Input.Sprint);
	Map(DodgeSprintAction, Input.DodgeOrSprint);
	Map(LockOnAction, Input.LockOn);
	Map(TargetLeftAction, Input.TargetLeft);
	Map(TargetRightAction, Input.TargetRight);
	Map(InteractAction, Input.Interact);

	if (bContextRegistered)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->RequestRebuildControlMappings();
		}
	}
}
