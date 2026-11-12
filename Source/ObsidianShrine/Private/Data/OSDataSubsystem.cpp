#include "Data/OSDataSubsystem.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObsidianShrine.h"

UOSDataSubsystem* UOSDataSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UOSDataSubsystem>() : nullptr;
}

FString UOSDataSubsystem::GetDataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
}

void UOSDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Reload();
}

void UOSDataSubsystem::Reload()
{
	Reset();
	LoadFromDirectory(GetDataDir());

	const TArray<FString> Errors = Validate();
	for (const FString& Error : Errors)
	{
		UE_LOG(LogObsidian, Error, TEXT("Data: %s"), *Error);
	}
	UE_LOG(LogObsidian, Log, TEXT("Data: %d actions, %d weapons, %d characters, %d AI profiles, %d problems"),
		Actions.Num(), Weapons.Num(), Characters.Num(), AIProfiles.Num(), Errors.Num());
}

void UOSDataSubsystem::Reset()
{
	Actions.Reset();
	Weapons.Reset();
	Characters.Reset();
	AIProfiles.Reset();
	LoadErrors.Reset();
}

int32 UOSDataSubsystem::LoadFromDirectory(const FString& Directory)
{
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.json"), true, false);
	Files.Sort();

	if (Files.Num() == 0)
	{
		ReportLoadError(FString::Printf(TEXT("no data files found in %s"), *Directory));
		return 1;
	}

	int32 Failed = 0;
	for (const FString& File : Files)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *File))
		{
			ReportLoadError(FString::Printf(TEXT("%s: cannot read file"), *File));
			++Failed;
			continue;
		}
		if (!LoadFromJsonString(Json, FPaths::GetCleanFilename(File)))
		{
			++Failed;
		}
	}
	return Failed;
}

bool UOSDataSubsystem::LoadFromJsonString(const FString& Json, const FString& SourceName)
{
	FOSDataFile File;
	FText Reason;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File, 0, 0, false, &Reason))
	{
		ReportLoadError(FString::Printf(TEXT("%s: invalid JSON: %s"), *SourceName, *Reason.ToString()));
		return false;
	}

	for (const FOSActionDef& Def : File.Actions) { AddAction(Def, SourceName); }
	for (const FOSWeaponDef& Def : File.Weapons) { AddWeapon(Def, SourceName); }
	for (const FOSCharacterDef& Def : File.Characters) { AddCharacter(Def, SourceName); }
	for (const FOSAIProfileDef& Def : File.AIProfiles) { AddAIProfile(Def, SourceName); }
	return true;
}

namespace
{
	template <typename DefType>
	bool AddDefinition(TMap<FName, DefType>& Map, const DefType& Def, const TCHAR* Kind, const FString& SourceName, FString& OutError)
	{
		if (Def.Id.IsNone())
		{
			OutError = FString::Printf(TEXT("%s: %s without Id"), *SourceName, Kind);
			return false;
		}
		if (Map.Contains(Def.Id))
		{
			OutError = FString::Printf(TEXT("%s: duplicate %s '%s'"), *SourceName, Kind, *Def.Id.ToString());
			return false;
		}
		Map.Add(Def.Id, Def);
		return true;
	}
}

void UOSDataSubsystem::AddAction(const FOSActionDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(Actions, Def, TEXT("action"), SourceName, Error)) { ReportLoadError(Error); }
}

void UOSDataSubsystem::AddWeapon(const FOSWeaponDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(Weapons, Def, TEXT("weapon"), SourceName, Error)) { ReportLoadError(Error); }
}

void UOSDataSubsystem::AddCharacter(const FOSCharacterDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(Characters, Def, TEXT("character"), SourceName, Error)) { ReportLoadError(Error); }
}

void UOSDataSubsystem::AddAIProfile(const FOSAIProfileDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(AIProfiles, Def, TEXT("AI profile"), SourceName, Error)) { ReportLoadError(Error); }
}

void UOSDataSubsystem::ReportLoadError(const FString& Message)
{
	LoadErrors.Add(Message);
}

TArray<FString> UOSDataSubsystem::Validate() const
{
	TArray<FString> Errors = LoadErrors;

	auto CheckAction = [this, &Errors](FName ActionId, const FString& Owner)
	{
		if (!ActionId.IsNone() && !Actions.Contains(ActionId))
		{
			Errors.Add(FString::Printf(TEXT("%s references unknown action '%s'"), *Owner, *ActionId.ToString()));
		}
	};

	for (const TPair<FName, FOSActionDef>& Pair : Actions)
	{
		const FOSActionDef& Def = Pair.Value;
		const FString Owner = FString::Printf(TEXT("action '%s'"), *Def.Id.ToString());

		CheckAction(Def.ComboNext, Owner);
		CheckAction(Def.AutoNext, Owner);

		if (Def.IsHeld() && !Def.AutoNext.IsNone())
		{
			Errors.Add(FString::Printf(TEXT("%s is held (Duration <= 0) and can never reach AutoNext"), *Owner));
		}
		for (const FOSActionWindow& Window : Def.Windows)
		{
			if (Window.Start < 0.f || (!Def.IsHeld() && Window.Start >= Def.Duration))
			{
				Errors.Add(FString::Printf(TEXT("%s has a window starting at %.2f outside its duration %.2f"), *Owner, Window.Start, Def.Duration));
			}
		}
		if (Def.MoveDistance != 0.f && Def.MoveEnd <= Def.MoveStart)
		{
			Errors.Add(FString::Printf(TEXT("%s has MoveDistance but MoveEnd <= MoveStart"), *Owner));
		}
	}

	for (const TPair<FName, FOSWeaponDef>& Pair : Weapons)
	{
		const FString Owner = FString::Printf(TEXT("weapon '%s'"), *Pair.Key.ToString());
		for (const TPair<FName, FName>& Slot : Pair.Value.Actions)
		{
			CheckAction(Slot.Value, Owner);
		}
	}

	for (const TPair<FName, FOSCharacterDef>& Pair : Characters)
	{
		const FOSCharacterDef& Def = Pair.Value;
		const FString Owner = FString::Printf(TEXT("character '%s'"), *Def.Id.ToString());

		if (!Def.Weapon.IsNone() && !Weapons.Contains(Def.Weapon))
		{
			Errors.Add(FString::Printf(TEXT("%s references unknown weapon '%s'"), *Owner, *Def.Weapon.ToString()));
		}
		if (!Def.AIProfile.IsNone() && !AIProfiles.Contains(Def.AIProfile))
		{
			Errors.Add(FString::Printf(TEXT("%s references unknown AI profile '%s'"), *Owner, *Def.AIProfile.ToString()));
		}
		for (const TPair<FName, FName>& Slot : Def.Actions)
		{
			CheckAction(Slot.Value, Owner);
		}
		if (Def.MaxHealth <= 0.f)
		{
			Errors.Add(FString::Printf(TEXT("%s has MaxHealth <= 0"), *Owner));
		}
	}

	for (const TPair<FName, FOSAIProfileDef>& Pair : AIProfiles)
	{
		const FOSAIProfileDef& Def = Pair.Value;
		const FString Owner = FString::Printf(TEXT("AI profile '%s'"), *Def.Id.ToString());

		if (Def.Moves.Num() == 0)
		{
			Errors.Add(FString::Printf(TEXT("%s has no moves"), *Owner));
		}
		for (const FOSAIMoveDef& Move : Def.Moves)
		{
			if (Move.Action.IsNone())
			{
				Errors.Add(FString::Printf(TEXT("%s has a move without Action"), *Owner));
			}
			CheckAction(Move.Action, Owner);
			if (Move.MaxRange <= Move.MinRange)
			{
				Errors.Add(FString::Printf(TEXT("%s move '%s' has MaxRange <= MinRange"), *Owner, *Move.Action.ToString()));
			}
		}
		float PreviousThreshold = 1.f;
		for (const FOSAIPhaseDef& Phase : Def.Phases)
		{
			CheckAction(Phase.EnterAction, Owner);
			if (Phase.HealthBelow >= PreviousThreshold)
			{
				Errors.Add(FString::Printf(TEXT("%s phases must have strictly decreasing HealthBelow"), *Owner));
			}
			PreviousThreshold = Phase.HealthBelow;
		}
	}

	return Errors;
}
