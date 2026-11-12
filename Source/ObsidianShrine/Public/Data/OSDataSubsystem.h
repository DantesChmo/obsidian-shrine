#pragma once

#include "CoreMinimal.h"
#include "Data/OSDataTypes.h"
#include "Subsystems/EngineSubsystem.h"

#include "OSDataSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOSOnDataReloaded);

/**
 * Registry of every gameplay definition and of the game-wide settings, key bindings and texts.
 * Loads all *.json under Content/Data at startup; the console command os.ReloadData loads them again.
 * An engine subsystem so it is available to automation tests that have no game instance.
 */
UCLASS()
class OBSIDIANSHRINE_API UOSDataSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UOSDataSubsystem* Get();
	static FString GetDataDir();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Game-wide settings, key bindings and texts as currently loaded; the built-in defaults when there is
	 * no engine. Read them where they are used instead of copying, so a reload takes effect at once.
	 */
	static const FOSGameDef& Settings();
	static const FOSInputDef& Bindings();
	static const FOSTextsDef& Texts();

	/** Replaces every {Name} in Pattern that has a value. */
	static FString FormatText(const FString& Pattern, const TMap<FString, FString>& Values);
	/** "E / A": the keys as the player should read them, using Texts.KeyLabels where a key has one. */
	static FString DescribeKeys(const TArray<FName>& Keys, const FOSTextsDef& InTexts);
	/** Calls Visit for every input action with its name and keys. */
	static void ForEachBinding(const FOSInputDef& Input, TFunctionRef<void(const TCHAR* /*Action*/, const TArray<FName>& /*Keys*/)> Visit);

	/** Drops everything, reloads Content/Data and tells the listeners of OnReloaded. */
	void Reload();
	void Reset();

	/** Fired after Reload. Whoever keeps a copy of a definition refreshes it here. */
	FOSOnDataReloaded OnReloaded;

	/** Returns the number of files that failed to load. */
	int32 LoadFromDirectory(const FString& Directory);
	bool LoadFromJsonString(const FString& Json, const FString& SourceName);

	void AddAction(const FOSActionDef& Def, const FString& SourceName = FString());
	void AddWeapon(const FOSWeaponDef& Def, const FString& SourceName = FString());
	void AddCharacter(const FOSCharacterDef& Def, const FString& SourceName = FString());
	void AddAIProfile(const FOSAIProfileDef& Def, const FString& SourceName = FString());

	/** Returned pointers are invalidated by any Add or Reload: copy the struct if it must outlive the call. */
	const FOSActionDef* FindAction(FName Id) const { return Actions.Find(Id); }
	const FOSWeaponDef* FindWeapon(FName Id) const { return Weapons.Find(Id); }
	const FOSCharacterDef* FindCharacter(FName Id) const { return Characters.Find(Id); }
	const FOSAIProfileDef* FindAIProfile(FName Id) const { return AIProfiles.Find(Id); }

	int32 NumActions() const { return Actions.Num(); }
	int32 NumCharacters() const { return Characters.Num(); }

	/** False until some file had the section; the game then runs on the built-in defaults (no keys at all). */
	bool HasGame() const { return bHasGame; }
	bool HasInput() const { return bHasInput; }
	bool HasTexts() const { return bHasTexts; }

	void SetGame(const FOSGameDef& Def) { GameDef = Def; bHasGame = true; }
	void SetInput(const FOSInputDef& Def) { InputDef = Def; bHasInput = true; }
	void SetTexts(const FOSTextsDef& Def) { TextsDef = Def; bHasTexts = true; }

	const FOSGameDef& GetGame() const { return GameDef; }
	const FOSInputDef& GetInput() const { return InputDef; }
	const FOSTextsDef& GetTexts() const { return TextsDef; }

	/** Load errors plus broken references and malformed timelines. Empty when the data is consistent. */
	TArray<FString> Validate() const;

private:
	TMap<FName, FOSActionDef> Actions;
	TMap<FName, FOSWeaponDef> Weapons;
	TMap<FName, FOSCharacterDef> Characters;
	TMap<FName, FOSAIProfileDef> AIProfiles;

	FOSGameDef GameDef;
	FOSInputDef InputDef;
	FOSTextsDef TextsDef;
	bool bHasGame = false;
	bool bHasInput = false;
	bool bHasTexts = false;

	TArray<FString> LoadErrors;

	void ReportLoadError(const FString& Message);
};
