#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/OSActionComponent.h"
#include "Combat/OSCombatComponent.h"
#include "Combat/OSStatsComponent.h"
#include "Data/OSDataSubsystem.h"
#include "Data/OSDataTypes.h"
#include "UObject/Package.h"

#define OS_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/** Stats + action + combat components wired together without an actor or a world. */
struct FOSTestRig
{
	UOSStatsComponent* Stats = nullptr;
	UOSActionComponent* Action = nullptr;
	UOSCombatComponent* Combat = nullptr;

	explicit FOSTestRig(float MaxHealth = 100.f, float MaxStamina = 100.f, float MaxPoise = 0.f)
	{
		FOSCharacterDef Def;
		Def.MaxHealth = MaxHealth;
		Def.MaxStamina = MaxStamina;
		Def.MaxPoise = MaxPoise;
		Def.StaminaRegen = 10.f;
		Def.StaminaRegenDelay = 1.f;
		Def.PoiseResetDelay = 2.f;

		Stats = NewObject<UOSStatsComponent>(GetTransientPackage());
		Stats->InitFromDef(Def);
		Action = NewObject<UOSActionComponent>(GetTransientPackage());
		Action->SetStats(Stats);
		Combat = NewObject<UOSCombatComponent>(GetTransientPackage());
		Combat->Setup(Stats, Action);
	}
};

namespace OSTest
{
	inline FOSActionWindow Window(EOSWindowType Type, float Start, float End = 0.f)
	{
		FOSActionWindow Result;
		Result.Type = Type;
		Result.Start = Start;
		Result.End = End;
		return Result;
	}

	inline FOSActionDef Action(FName Id, float Duration, float StaminaCost = 0.f)
	{
		FOSActionDef Def;
		Def.Id = Id;
		Def.Duration = Duration;
		Def.StaminaCost = StaminaCost;
		return Def;
	}

	/** Registers a definition in the live registry unless a previous test run already did. */
	inline void Register(const FOSActionDef& Def)
	{
		UOSDataSubsystem* Data = UOSDataSubsystem::Get();
		if (Data && !Data->FindAction(Def.Id)) { Data->AddAction(Def, TEXT("test")); }
	}

	inline void Register(const FOSWeaponDef& Def)
	{
		UOSDataSubsystem* Data = UOSDataSubsystem::Get();
		if (Data && !Data->FindWeapon(Def.Id)) { Data->AddWeapon(Def, TEXT("test")); }
	}

	inline void Register(const FOSCharacterDef& Def)
	{
		UOSDataSubsystem* Data = UOSDataSubsystem::Get();
		if (Data && !Data->FindCharacter(Def.Id)) { Data->AddCharacter(Def, TEXT("test")); }
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
