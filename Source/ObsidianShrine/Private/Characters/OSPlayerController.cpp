#include "Characters/OSPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

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
		auto Map = [this](UInputAction* InputAction, FKey Key, bool bSwizzle = false, bool bNegate = false, bool bDeadZone = false)
		{
			FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(InputAction, Key);
			if (bDeadZone) { Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(MappingContext)); }
			if (bSwizzle) { Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext)); }
			if (bNegate) { Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext)); }
		};

		// Move: X = right, Y = forward.
		MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
		Map(MoveAction, EKeys::W, true, false);
		Map(MoveAction, EKeys::S, true, true);
		Map(MoveAction, EKeys::D, false, false);
		Map(MoveAction, EKeys::A, false, true);
		Map(MoveAction, EKeys::Gamepad_Left2D, false, false, true);

		LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
		Map(LookMouseAction, EKeys::Mouse2D);

		LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
		Map(LookStickAction, EKeys::Gamepad_Right2D, false, false, true);

		LightAction = MakeAction(TEXT("IA_Light"), EInputActionValueType::Boolean);
		Map(LightAction, EKeys::LeftMouseButton);
		Map(LightAction, EKeys::Gamepad_RightShoulder);

		HeavyAction = MakeAction(TEXT("IA_Heavy"), EInputActionValueType::Boolean);
		Map(HeavyAction, EKeys::F);
		Map(HeavyAction, EKeys::Gamepad_RightTrigger);

		GuardAction = MakeAction(TEXT("IA_Guard"), EInputActionValueType::Boolean);
		Map(GuardAction, EKeys::RightMouseButton);
		Map(GuardAction, EKeys::Gamepad_LeftShoulder);

		DodgeAction = MakeAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
		Map(DodgeAction, EKeys::SpaceBar);

		SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
		Map(SprintAction, EKeys::LeftShift);

		DodgeSprintAction = MakeAction(TEXT("IA_DodgeSprint"), EInputActionValueType::Boolean);
		Map(DodgeSprintAction, EKeys::Gamepad_FaceButton_Right);

		LockOnAction = MakeAction(TEXT("IA_LockOn"), EInputActionValueType::Boolean);
		Map(LockOnAction, EKeys::Q);
		Map(LockOnAction, EKeys::MiddleMouseButton);
		Map(LockOnAction, EKeys::Gamepad_RightThumbstick);

		TargetLeftAction = MakeAction(TEXT("IA_TargetLeft"), EInputActionValueType::Boolean);
		Map(TargetLeftAction, EKeys::MouseScrollDown);
		Map(TargetLeftAction, EKeys::Gamepad_DPad_Left);

		TargetRightAction = MakeAction(TEXT("IA_TargetRight"), EInputActionValueType::Boolean);
		Map(TargetRightAction, EKeys::MouseScrollUp);
		Map(TargetRightAction, EKeys::Gamepad_DPad_Right);

		InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
		Map(InteractAction, EKeys::E);
		Map(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
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
