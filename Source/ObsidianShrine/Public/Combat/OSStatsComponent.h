#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OSDataTypes.h"

#include "OSStatsComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOSOnDeath);

/** Health, stamina and poise. All logic runs through Advance() so it can be driven without a world. */
UCLASS()
class OBSIDIANSHRINE_API UOSStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOSStatsComponent();

	/** Takes the limits and rates from Def and refills everything. */
	void InitFromDef(const FOSCharacterDef& Def);
	/** Takes the limits and rates from Def but keeps the current health, stamina and poise (clamped). */
	void UpdateFromDef(const FOSCharacterDef& Def);
	void RestoreAll();
	/** Sets the health of a living character, clamped to [1, MaxHealth]. Never kills or revives. */
	void SetHealth(float Value);

	void Advance(float DeltaTime);

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	float GetStaminaFraction() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }
	float GetPoise() const { return Poise; }

	bool IsDead() const { return bDead; }
	bool HasStamina() const { return Stamina > 0.f; }

	/** Clamps at zero and restarts the regeneration delay. */
	void SpendStamina(float Amount);

	/** Returns the damage actually applied. Fires OnDeath once when health reaches zero. */
	float ApplyDamage(float Amount);
	void Kill();

	/** Returns true when poise broke (it is then refilled). Always true for MaxPoise <= 0. */
	bool ApplyPoiseDamage(float Amount);

	/** Multiplier applied to stamina regeneration, set every frame by the owner. */
	float StaminaRegenScale = 1.f;

	FOSOnDeath OnDeath;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Health = 100.f;
	float MaxHealth = 100.f;
	float Stamina = 100.f;
	float MaxStamina = 100.f;
	float StaminaRegen = 35.f;
	float StaminaRegenDelay = 0.8f;
	float StaminaDelayLeft = 0.f;
	float Poise = 0.f;
	float MaxPoise = 0.f;
	float PoiseResetDelay = 4.f;
	float PoiseDelayLeft = 0.f;
	bool bDead = false;
};
