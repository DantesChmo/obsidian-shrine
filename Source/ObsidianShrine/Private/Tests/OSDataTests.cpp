#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSDataShippedTest, "ObsidianShrine.Data.ShippedDataIsValid", OS_TEST_FLAGS)

bool FOSDataShippedTest::RunTest(const FString& Parameters)
{
	// A private registry: the live one may hold definitions registered by other tests.
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	TestEqual(TEXT("files that failed to load"), Data->LoadFromDirectory(UOSDataSubsystem::GetDataDir()), 0);

	for (const FString& Error : Data->Validate())
	{
		AddError(Error);
	}

	const FOSCharacterDef* Player = Data->FindCharacter(TEXT("player"));
	if (!TestNotNull(TEXT("character 'player'"), Player))
	{
		return false;
	}
	const FOSWeaponDef* Weapon = Data->FindWeapon(Player->Weapon);
	if (!TestNotNull(TEXT("player weapon"), Weapon))
	{
		return false;
	}

	TMap<FName, FName> Slots = Weapon->Actions;
	Slots.Append(Player->Actions);
	for (const FName Slot : { OSSlots::Light, OSSlots::Heavy, OSSlots::Guard, OSSlots::Dodge, OSSlots::Stagger, OSSlots::GuardBreak, OSSlots::Parried, OSSlots::Death })
	{
		TestTrue(FString::Printf(TEXT("player has an action for slot %s"), *Slot.ToString()), Slots.Contains(Slot));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSDataParseTest, "ObsidianShrine.Data.ParsesJson", OS_TEST_FLAGS)

bool FOSDataParseTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"({
		"Actions": [
			{ "Id": "a", "Duration": 1.5, "StaminaCost": 7, "bSnapFacing": true, "MoveDirection": "Input", "ComboNext": "a",
			  "Windows": [ { "Type": "Invulnerable", "Start": 0.1, "End": 0.4 }, { "Type": "Cancel", "Start": 1.0 } ] }
		],
		"Weapons": [ { "Id": "w", "Damage": 12, "Actions": { "Light": "a" } } ],
		"Characters": [ { "Id": "c", "MaxHealth": 50, "Weapon": "w", "bBoss": true } ]
	})");

	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	TestTrue(TEXT("valid JSON loads"), Data->LoadFromJsonString(Json, TEXT("test")));
	TestEqual(TEXT("no validation errors"), Data->Validate().Num(), 0);

	const FOSActionDef* Action = Data->FindAction(TEXT("a"));
	if (TestNotNull(TEXT("action parsed"), Action))
	{
		TestEqual(TEXT("Duration"), Action->Duration, 1.5f);
		TestEqual(TEXT("StaminaCost"), Action->StaminaCost, 7.f);
		TestTrue(TEXT("bSnapFacing"), Action->bSnapFacing);
		TestTrue(TEXT("MoveDirection enum"), Action->MoveDirection == EOSMoveDirection::Input);
		TestEqual(TEXT("window count"), Action->Windows.Num(), 2);
		if (Action->Windows.Num() == 2)
		{
			TestTrue(TEXT("window type enum"), Action->Windows[0].Type == EOSWindowType::Invulnerable);
			TestEqual(TEXT("window end"), Action->Windows[0].End, 0.4f);
			TestTrue(TEXT("second window type"), Action->Windows[1].Type == EOSWindowType::Cancel);
		}
	}

	const FOSWeaponDef* Weapon = Data->FindWeapon(TEXT("w"));
	if (TestNotNull(TEXT("weapon parsed"), Weapon))
	{
		TestEqual(TEXT("weapon slot map"), Weapon->Actions.FindRef(OSSlots::Light), FName(TEXT("a")));
	}

	const FOSCharacterDef* Character = Data->FindCharacter(TEXT("c"));
	TestTrue(TEXT("character parsed"), Character && Character->bBoss && Character->MaxHealth == 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSDataValidationTest, "ObsidianShrine.Data.ValidationCatchesMistakes", OS_TEST_FLAGS)

bool FOSDataValidationTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Broken = NewObject<UOSDataSubsystem>(GEngine);
	TestFalse(TEXT("malformed JSON is rejected"), Broken->LoadFromJsonString(TEXT("{ \"Actions\": [ "), TEXT("test")));
	TestTrue(TEXT("malformed JSON is reported"), Broken->Validate().Num() > 0);

	UOSDataSubsystem* Dangling = NewObject<UOSDataSubsystem>(GEngine);
	Dangling->LoadFromJsonString(TEXT(R"({
		"Actions": [ { "Id": "a", "Duration": 1, "ComboNext": "missing_action" }, { "Id": "a", "Duration": 2 } ],
		"Characters": [ { "Id": "c", "Weapon": "missing_weapon", "AIProfile": "missing_profile" } ]
	})"), TEXT("test"));

	const FString Report = FString::Join(Dangling->Validate(), TEXT("\n"));
	TestTrue(TEXT("unknown action reported"), Report.Contains(TEXT("missing_action")));
	TestTrue(TEXT("unknown weapon reported"), Report.Contains(TEXT("missing_weapon")));
	TestTrue(TEXT("unknown AI profile reported"), Report.Contains(TEXT("missing_profile")));
	TestTrue(TEXT("duplicate id reported"), Report.Contains(TEXT("duplicate")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
