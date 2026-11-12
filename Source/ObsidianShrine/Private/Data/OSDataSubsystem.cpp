#include "Data/OSDataSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObsidianShrine.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static FAutoConsoleCommand OSReloadDataCommand(
	TEXT("os.ReloadData"),
	TEXT("Reloads Content/Data (definitions, settings, key bindings, texts) and applies it to the running game."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (UOSDataSubsystem* Data = UOSDataSubsystem::Get())
		{
			Data->Reload();
		}
	}));

UOSDataSubsystem* UOSDataSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UOSDataSubsystem>() : nullptr;
}

FString UOSDataSubsystem::GetDataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
}

const FOSGameDef& UOSDataSubsystem::Settings()
{
	static const FOSGameDef Defaults;
	const UOSDataSubsystem* Data = Get();
	return Data ? Data->GameDef : Defaults;
}

const FOSInputDef& UOSDataSubsystem::Bindings()
{
	static const FOSInputDef Defaults;
	const UOSDataSubsystem* Data = Get();
	return Data ? Data->InputDef : Defaults;
}

const FOSTextsDef& UOSDataSubsystem::Texts()
{
	static const FOSTextsDef Defaults;
	const UOSDataSubsystem* Data = Get();
	return Data ? Data->TextsDef : Defaults;
}

FString UOSDataSubsystem::FormatText(const FString& Pattern, const TMap<FString, FString>& Values)
{
	FString Result = Pattern;
	for (const TPair<FString, FString>& Value : Values)
	{
		Result.ReplaceInline(*FString::Printf(TEXT("{%s}"), *Value.Key), *Value.Value, ESearchCase::CaseSensitive);
	}
	return Result;
}

FString UOSDataSubsystem::DescribeKeys(const TArray<FName>& Keys, const FOSTextsDef& InTexts)
{
	TArray<FString> Labels;
	for (const FName Key : Keys)
	{
		const FString* Label = InTexts.KeyLabels.Find(Key);
		Labels.Add(Label ? *Label : Key.ToString());
	}
	return FString::Join(Labels, *InTexts.KeySeparator);
}

void UOSDataSubsystem::ForEachBinding(const FOSInputDef& Input, TFunctionRef<void(const TCHAR*, const TArray<FName>&)> Visit)
{
	Visit(TEXT("MoveForward"), Input.MoveForward);
	Visit(TEXT("MoveBackward"), Input.MoveBackward);
	Visit(TEXT("MoveLeft"), Input.MoveLeft);
	Visit(TEXT("MoveRight"), Input.MoveRight);
	Visit(TEXT("MoveStick"), Input.MoveStick);
	Visit(TEXT("LookMouse"), Input.LookMouse);
	Visit(TEXT("LookStick"), Input.LookStick);
	Visit(TEXT("Light"), Input.Light);
	Visit(TEXT("Heavy"), Input.Heavy);
	Visit(TEXT("Guard"), Input.Guard);
	Visit(TEXT("Dodge"), Input.Dodge);
	Visit(TEXT("Sprint"), Input.Sprint);
	Visit(TEXT("DodgeOrSprint"), Input.DodgeOrSprint);
	Visit(TEXT("LockOn"), Input.LockOn);
	Visit(TEXT("TargetLeft"), Input.TargetLeft);
	Visit(TEXT("TargetRight"), Input.TargetRight);
	Visit(TEXT("Interact"), Input.Interact);
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
	if (!bHasInput)
	{
		UE_LOG(LogObsidian, Error, TEXT("Data: no Input section in %s, so no key is bound to anything"), *GetDataDir());
	}
	UE_LOG(LogObsidian, Log, TEXT("Data: %d actions, %d weapons, %d characters, %d AI profiles, %d animation sets, %d problems"),
		Actions.Num(), Weapons.Num(), Characters.Num(), AIProfiles.Num(), AnimationSets.Num(), Errors.Num());

	OnReloaded.Broadcast();
}

void UOSDataSubsystem::Reset()
{
	Actions.Reset();
	Weapons.Reset();
	Characters.Reset();
	AIProfiles.Reset();
	AnimationSets.Reset();
	Cosmetics.Reset();
	GameDef = FOSGameDef();
	InputDef = FOSInputDef();
	TextsDef = FOSTextsDef();
	bHasGame = false;
	bHasInput = false;
	bHasTexts = false;
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
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		ReportLoadError(FString::Printf(TEXT("%s: invalid JSON: %s"), *SourceName, *Reader->GetErrorMessage()));
		return false;
	}

	FOSDataFile File;
	FText Reason;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), &File, 0, 0, false, &Reason))
	{
		ReportLoadError(FString::Printf(TEXT("%s: invalid JSON: %s"), *SourceName, *Reason.ToString()));
		return false;
	}

	// The struct always has these three, so ask the JSON whether the file really wrote them.
	auto TakeSection = [this, &Object, &SourceName](const TCHAR* Section, bool& bHas) -> bool
	{
		if (!Object->HasField(Section))
		{
			return false;
		}
		if (bHas)
		{
			ReportLoadError(FString::Printf(TEXT("%s: section '%s' is already defined by another file"), *SourceName, Section));
			return false;
		}
		bHas = true;
		return true;
	};
	if (TakeSection(TEXT("Game"), bHasGame)) { GameDef = File.Game; }
	if (TakeSection(TEXT("Input"), bHasInput)) { InputDef = File.Input; }
	if (TakeSection(TEXT("Texts"), bHasTexts)) { TextsDef = File.Texts; }

	for (const FOSActionDef& Def : File.Actions) { AddAction(Def, SourceName); }
	for (const FOSWeaponDef& Def : File.Weapons) { AddWeapon(Def, SourceName); }
	for (const FOSCharacterDef& Def : File.Characters) { AddCharacter(Def, SourceName); }
	for (const FOSAIProfileDef& Def : File.AIProfiles) { AddAIProfile(Def, SourceName); }
	for (const FOSAnimationSetDef& Def : File.AnimationSets) { AddAnimationSet(Def, SourceName); }
	for (const FOSCosmeticsDef& Def : File.Cosmetics) { AddCosmetics(Def, SourceName); }
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

void UOSDataSubsystem::AddAnimationSet(const FOSAnimationSetDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(AnimationSets, Def, TEXT("animation set"), SourceName, Error)) { ReportLoadError(Error); }
}

void UOSDataSubsystem::AddCosmetics(const FOSCosmeticsDef& Def, const FString& SourceName)
{
	FString Error;
	if (!AddDefinition(Cosmetics, Def, TEXT("cosmetics"), SourceName, Error)) { ReportLoadError(Error); }
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
		if (!Def.AnimationSet.IsNone() && !AnimationSets.Contains(Def.AnimationSet))
		{
			Errors.Add(FString::Printf(TEXT("%s references unknown animation set '%s'"), *Owner, *Def.AnimationSet.ToString()));
		}
		if (!Def.Cosmetics.IsNone() && !Cosmetics.Contains(Def.Cosmetics))
		{
			Errors.Add(FString::Printf(TEXT("%s references unknown cosmetics '%s'"), *Owner, *Def.Cosmetics.ToString()));
		}
	}

	for (const TPair<FName, FOSAnimationSetDef>& Pair : AnimationSets)
	{
		const FString Owner = FString::Printf(TEXT("animation set '%s'"), *Pair.Key.ToString());
		for (const TPair<FName, FString>& Animation : Pair.Value.Actions)
		{
			CheckAction(Animation.Key, Owner);
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

	if (bHasGame && !Characters.Contains(GameDef.PlayerCharacter))
	{
		Errors.Add(FString::Printf(TEXT("game settings: PlayerCharacter '%s' is not a known character"), *GameDef.PlayerCharacter.ToString()));
	}

	ForEachBinding(InputDef, [&Errors](const TCHAR* Action, const TArray<FName>& Keys)
	{
		for (const FName Key : Keys)
		{
			if (!FKey(Key).IsValid())
			{
				Errors.Add(FString::Printf(TEXT("input: %s is bound to unknown key '%s'"), Action, *Key.ToString()));
			}
		}
	});

	return Errors;
}
