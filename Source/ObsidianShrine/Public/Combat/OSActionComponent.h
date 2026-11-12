#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSDataTypes.h"

#include "OSActionComponent.generated.h"

class UOSStatsComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOSOnActionStarted, const FOSActionDef&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOSOnActionEnded, const FOSActionDef&, bool /*bInterrupted*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOSOnWindowChanged, EOSWindowType, bool /*bBegin*/);

/**
 * Runs one data-defined action at a time: timeline windows, stamina gate, input buffer, combos.
 * Knows nothing about movement, animation or damage; other systems react to its delegates and
 * poll its windows. All logic runs through Advance() so it can be driven without a world.
 */
UCLASS()
class OBSIDIANSHRINE_API UOSActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSActionComponent();

	/** Optional. Without stats, actions are free and never blocked by death. */
	void SetStats(UOSStatsComponent* InStats) { Stats = InStats; }
	void SetSlotMap(const TMap<FName, FName>& InSlotMap) { SlotMap = InSlotMap; }
	FName ResolveSlot(FName Slot) const;

	/**
	 * Player-style input. Starts the slot's action if idle or inside a Cancel window,
	 * otherwise remembers it for Controls.InputBuffer seconds. Returns true if an action started now.
	 */
	bool RequestSlot(FName Slot);

	/** Ends a held action (Duration <= 0) started from this slot and drops the slot from the buffer. */
	void StopSlot(FName Slot);

	/** Starts the slot's action. bForce interrupts anything and ignores stamina and death. */
	bool StartSlot(FName Slot, bool bForce = false);
	bool StartAction(FName ActionId, FName Slot = NAME_None, bool bForce = false);
	bool StartActionDef(const FOSActionDef& Def, FName Slot = NAME_None, bool bForce = false);

	void Interrupt();

	void Advance(float DeltaTime);

	bool IsBusy() const { return bActive; }
	const FOSActionDef& GetCurrent() const { return Current; }
	FName GetCurrentSlot() const { return CurrentSlot; }
	float GetTime() const { return Time; }
	bool IsWindowActive(EOSWindowType Type) const;

	/** cm/s of the action's own displacement right now, 0 outside its move range. */
	float GetMoveSpeedNow() const;

	FOSOnActionStarted OnActionStarted;
	FOSOnActionEnded OnActionEnded;
	FOSOnWindowChanged OnWindowChanged;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOSStatsComponent> Stats;

	TMap<FName, FName> SlotMap;

	FOSActionDef Current;
	FName CurrentSlot;
	TArray<bool> WindowActive;
	bool bActive = false;
	float Time = 0.f;

	/** Bumped whenever the running action changes; lets loops notice a delegate handler replaced it. */
	int32 Serial = 0;

	FName BufferedSlot;
	float BufferAge = 0.f;

	void UpdateWindows(float PreviousTime, float NewTime);
	void EndCurrent(bool bInterrupted);
	void ConsumeBuffer();
};
