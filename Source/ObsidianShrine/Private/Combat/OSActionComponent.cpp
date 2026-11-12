#include "Combat/OSActionComponent.h"

#include "Combat/OSStatsComponent.h"
#include "Data/OSDataSubsystem.h"
#include "GameFramework/Actor.h"
#include "ObsidianShrine.h"

namespace
{
	float WindowEnd(const FOSActionWindow& Window)
	{
		return Window.End > Window.Start ? Window.End : TNumericLimits<float>::Max();
	}
}

UOSActionComponent::UOSActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSActionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Stats && GetOwner())
	{
		Stats = GetOwner()->FindComponentByClass<UOSStatsComponent>();
	}
}

FName UOSActionComponent::ResolveSlot(FName Slot) const
{
	const FName* Found = SlotMap.Find(Slot);
	return Found ? *Found : NAME_None;
}

bool UOSActionComponent::RequestSlot(FName Slot)
{
	if (!bActive || IsWindowActive(EOSWindowType::Cancel))
	{
		return StartSlot(Slot, false);
	}

	BufferedSlot = Slot;
	BufferAge = 0.f;
	return false;
}

void UOSActionComponent::StopSlot(FName Slot)
{
	if (BufferedSlot == Slot)
	{
		BufferedSlot = NAME_None;
	}
	if (bActive && CurrentSlot == Slot && Current.IsHeld())
	{
		EndCurrent(false);
	}
}

bool UOSActionComponent::StartSlot(FName Slot, bool bForce)
{
	FName ActionId = ResolveSlot(Slot);
	if (bActive && !bForce && Slot == CurrentSlot && !Current.ComboNext.IsNone())
	{
		ActionId = Current.ComboNext;
	}
	if (ActionId.IsNone())
	{
		return false;
	}
	return StartAction(ActionId, Slot, bForce);
}

bool UOSActionComponent::StartAction(FName ActionId, FName Slot, bool bForce)
{
	const UOSDataSubsystem* Data = UOSDataSubsystem::Get();
	const FOSActionDef* Def = Data ? Data->FindAction(ActionId) : nullptr;
	if (!Def)
	{
		UE_LOG(LogObsidian, Warning, TEXT("Unknown action '%s'"), *ActionId.ToString());
		return false;
	}
	return StartActionDef(*Def, Slot, bForce);
}

bool UOSActionComponent::StartActionDef(const FOSActionDef& Def, FName Slot, bool bForce)
{
	if (!bForce)
	{
		if (bActive && !IsWindowActive(EOSWindowType::Cancel))
		{
			return false;
		}
		if (Stats && (Stats->IsDead() || (Def.StaminaCost > 0.f && !Stats->HasStamina())))
		{
			return false;
		}
	}

	// Def may alias Current (restarting the same action), so copy before ending it.
	const FOSActionDef NewDef = Def;
	if (bActive)
	{
		EndCurrent(true);
	}

	Current = NewDef;
	CurrentSlot = Slot;
	WindowActive.Init(false, Current.Windows.Num());
	Time = 0.f;
	bActive = true;
	BufferedSlot = NAME_None;
	const int32 StartedSerial = ++Serial;

	if (Stats)
	{
		Stats->SpendStamina(Current.StaminaCost);
	}

	OnActionStarted.Broadcast(Current);
	if (Serial == StartedSerial)
	{
		UpdateWindows(0.f, 0.f);
	}
	return true;
}

void UOSActionComponent::Interrupt()
{
	EndCurrent(true);
}

void UOSActionComponent::Advance(float DeltaTime)
{
	if (!BufferedSlot.IsNone())
	{
		BufferAge += DeltaTime;
		if (BufferAge > BufferDuration)
		{
			BufferedSlot = NAME_None;
		}
	}

	if (bActive)
	{
		const int32 AdvancedSerial = Serial;
		const float PreviousTime = Time;
		Time += DeltaTime;

		UpdateWindows(PreviousTime, Time);
		if (Serial != AdvancedSerial)
		{
			return;
		}

		if (!Current.IsHeld() && Time >= Current.Duration)
		{
			const FName NextAction = Current.AutoNext;
			const FName Slot = CurrentSlot;
			EndCurrent(false);
			if (bActive)
			{
				return;
			}
			if (!NextAction.IsNone() && StartAction(NextAction, Slot, false))
			{
				return;
			}
		}
	}

	ConsumeBuffer();
}

void UOSActionComponent::ConsumeBuffer()
{
	if (BufferedSlot.IsNone() || (bActive && !IsWindowActive(EOSWindowType::Cancel)))
	{
		return;
	}

	const FName Slot = BufferedSlot;
	BufferedSlot = NAME_None;
	StartSlot(Slot, false);
}

void UOSActionComponent::UpdateWindows(float PreviousTime, float NewTime)
{
	const int32 UpdatedSerial = Serial;
	for (int32 Index = 0; Index < Current.Windows.Num(); ++Index)
	{
		const FOSActionWindow Window = Current.Windows[Index];
		const float End = WindowEnd(Window);
		const bool bWasActive = WindowActive[Index];
		const bool bNowActive = NewTime >= Window.Start && NewTime < End;

		// A long frame can step over a short window entirely; it still has to open and close.
		const bool bSkipped = !bWasActive && !bNowActive && PreviousTime < Window.Start && NewTime >= End;

		if ((bNowActive && !bWasActive) || bSkipped)
		{
			WindowActive[Index] = true;
			OnWindowChanged.Broadcast(Window.Type, true);
			if (Serial != UpdatedSerial) { return; }
		}
		if ((!bNowActive && bWasActive) || bSkipped)
		{
			WindowActive[Index] = false;
			OnWindowChanged.Broadcast(Window.Type, false);
			if (Serial != UpdatedSerial) { return; }
		}
	}
}

void UOSActionComponent::EndCurrent(bool bInterrupted)
{
	if (!bActive)
	{
		return;
	}

	const FOSActionDef Ended = Current;
	const TArray<bool> WasActive = MoveTemp(WindowActive);
	WindowActive.Reset();
	bActive = false;
	const int32 EndedSerial = ++Serial;

	for (int32 Index = 0; Index < WasActive.Num(); ++Index)
	{
		if (WasActive[Index])
		{
			OnWindowChanged.Broadcast(Ended.Windows[Index].Type, false);
			if (Serial != EndedSerial) { return; }
		}
	}
	OnActionEnded.Broadcast(Ended, bInterrupted);
}

bool UOSActionComponent::IsWindowActive(EOSWindowType Type) const
{
	if (!bActive)
	{
		return false;
	}
	for (int32 Index = 0; Index < WindowActive.Num(); ++Index)
	{
		if (WindowActive[Index] && Current.Windows[Index].Type == Type)
		{
			return true;
		}
	}
	return false;
}

float UOSActionComponent::GetMoveSpeedNow() const
{
	if (!bActive || Current.MoveDistance == 0.f || Current.MoveEnd <= Current.MoveStart)
	{
		return 0.f;
	}
	if (Time < Current.MoveStart || Time >= Current.MoveEnd)
	{
		return 0.f;
	}
	return Current.MoveDistance / (Current.MoveEnd - Current.MoveStart);
}

void UOSActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}
