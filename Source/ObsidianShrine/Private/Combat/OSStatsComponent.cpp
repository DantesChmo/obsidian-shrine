#include "Combat/OSStatsComponent.h"

UOSStatsComponent::UOSStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOSStatsComponent::InitFromDef(const FOSCharacterDef& Def)
{
	UpdateFromDef(Def);
	RestoreAll();
}

void UOSStatsComponent::UpdateFromDef(const FOSCharacterDef& Def)
{
	MaxHealth = FMath::Max(1.f, Def.MaxHealth);
	MaxStamina = FMath::Max(0.f, Def.MaxStamina);
	StaminaRegen = Def.StaminaRegen;
	StaminaRegenDelay = Def.StaminaRegenDelay;
	MaxPoise = FMath::Max(0.f, Def.MaxPoise);
	PoiseResetDelay = Def.PoiseResetDelay;

	// A living character is never killed by a smaller limit; a dead one stays at zero.
	Health = bDead ? 0.f : FMath::Clamp(Health, 1.f, MaxHealth);
	Stamina = FMath::Min(Stamina, MaxStamina);
	Poise = FMath::Min(Poise, MaxPoise);
}

void UOSStatsComponent::RestoreAll()
{
	Health = MaxHealth;
	Stamina = MaxStamina;
	Poise = MaxPoise;
	StaminaDelayLeft = 0.f;
	PoiseDelayLeft = 0.f;
	bDead = false;
}

void UOSStatsComponent::SetHealth(float Value)
{
	if (!bDead)
	{
		Health = FMath::Clamp(Value, FMath::Min(1.f, MaxHealth), MaxHealth);
	}
}

void UOSStatsComponent::Advance(float DeltaTime)
{
	if (bDead)
	{
		return;
	}

	if (StaminaDelayLeft > 0.f)
	{
		StaminaDelayLeft -= DeltaTime;
	}
	else if (Stamina < MaxStamina)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegen * StaminaRegenScale * DeltaTime);
	}

	if (Poise < MaxPoise)
	{
		PoiseDelayLeft -= DeltaTime;
		if (PoiseDelayLeft <= 0.f)
		{
			Poise = MaxPoise;
		}
	}
}

void UOSStatsComponent::SpendStamina(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	Stamina = FMath::Max(0.f, Stamina - Amount);
	StaminaDelayLeft = StaminaRegenDelay;
}

float UOSStatsComponent::ApplyDamage(float Amount)
{
	if (bDead || Amount <= 0.f)
	{
		return 0.f;
	}

	const float Applied = FMath::Min(Health, Amount);
	Health -= Applied;
	if (Health <= 0.f)
	{
		Health = 0.f;
		bDead = true;
		OnDeath.Broadcast();
	}
	return Applied;
}

void UOSStatsComponent::Kill()
{
	ApplyDamage(Health);
}

bool UOSStatsComponent::ApplyPoiseDamage(float Amount)
{
	if (MaxPoise <= 0.f)
	{
		return true;
	}
	if (Amount <= 0.f)
	{
		return false;
	}

	Poise -= Amount;
	PoiseDelayLeft = PoiseResetDelay;
	if (Poise <= 0.f)
	{
		Poise = MaxPoise;
		return true;
	}
	return false;
}

void UOSStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}
