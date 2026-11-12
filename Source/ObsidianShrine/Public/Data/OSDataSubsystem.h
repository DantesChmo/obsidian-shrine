#pragma once

#include "CoreMinimal.h"
#include "Data/OSDataTypes.h"
#include "Subsystems/EngineSubsystem.h"

#include "OSDataSubsystem.generated.h"

/**
 * Registry of every gameplay definition. Loads all *.json under Content/Data at startup.
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

	/** Drops everything and reloads Content/Data. */
	void Reload();
	void Reset();

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

	/** Load errors plus broken references and malformed timelines. Empty when the data is consistent. */
	TArray<FString> Validate() const;

private:
	TMap<FName, FOSActionDef> Actions;
	TMap<FName, FOSWeaponDef> Weapons;
	TMap<FName, FOSCharacterDef> Characters;
	TMap<FName, FOSAIProfileDef> AIProfiles;

	TArray<FString> LoadErrors;

	void ReportLoadError(const FString& Message);
};
