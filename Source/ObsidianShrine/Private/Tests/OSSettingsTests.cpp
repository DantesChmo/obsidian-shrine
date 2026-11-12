#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSettingsParseTest, "ObsidianShrine.Settings.ParsesGameInputAndTexts", OS_TEST_FLAGS)

bool FOSSettingsParseTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	TestFalse(TEXT("no Game section before loading"), Data->HasGame());
	TestFalse(TEXT("no Input section before loading"), Data->HasInput());
	TestFalse(TEXT("no Texts section before loading"), Data->HasTexts());

	TestTrue(TEXT("a definitions file loads"), Data->LoadFromJsonString(TEXT(R"({
		"Characters": [ { "Id": "hero" } ]
	})"), TEXT("defs")));
	TestFalse(TEXT("a file without the section does not count as having it"), Data->HasGame());

	TestTrue(TEXT("game settings load"), Data->LoadFromJsonString(TEXT(R"({
		"Game": {
			"PlayerCharacter": "hero",
			"InteractRadius": 300,
			"Camera": { "ArmLength": 600, "MinPitch": -40 },
			"LockOn": { "BreakDistance": 3500 },
			"Controls": { "InputBuffer": 0.25 },
			"Death": { "RespawnDelay": 1.5, "CurrencyLossFraction": 0.5, "bKeepPreviousDrop": true },
			"Messages": { "RestedDuration": 9 }
		}
	})"), TEXT("game")));
	TestTrue(TEXT("Game section seen"), Data->HasGame());

	const FOSGameDef& Game = Data->GetGame();
	const FOSGameDef Defaults;
	TestEqual(TEXT("PlayerCharacter"), Game.PlayerCharacter, FName(TEXT("hero")));
	TestEqual(TEXT("InteractRadius"), Game.InteractRadius, 300.f);
	TestEqual(TEXT("Camera.ArmLength"), Game.Camera.ArmLength, 600.f);
	TestEqual(TEXT("Camera.MinPitch"), Game.Camera.MinPitch, -40.f);
	TestEqual(TEXT("an unwritten camera field keeps its default"), Game.Camera.MaxPitch, Defaults.Camera.MaxPitch);
	TestEqual(TEXT("LockOn.BreakDistance"), Game.LockOn.BreakDistance, 3500.f);
	TestEqual(TEXT("an unwritten lock-on field keeps its default"), Game.LockOn.AcquireDistance, Defaults.LockOn.AcquireDistance);
	TestEqual(TEXT("Controls.InputBuffer"), Game.Controls.InputBuffer, 0.25f);
	TestEqual(TEXT("Death.RespawnDelay"), Game.Death.RespawnDelay, 1.5f);
	TestEqual(TEXT("Death.CurrencyLossFraction"), Game.Death.CurrencyLossFraction, 0.5f);
	TestTrue(TEXT("Death.bKeepPreviousDrop"), Game.Death.bKeepPreviousDrop);
	TestEqual(TEXT("Messages.RestedDuration"), Game.Messages.RestedDuration, 9.f);
	TestEqual(TEXT("an unwritten section keeps its defaults"), Game.Death.KillHeight, Defaults.Death.KillHeight);

	TestTrue(TEXT("bindings and texts load from one file"), Data->LoadFromJsonString(TEXT(R"({
		"Input": {
			"MoveForward": [ "W", "Up" ],
			"MoveStick": [ "Gamepad_Left2D" ],
			"Interact": [ "E", "Gamepad_FaceButton_Bottom" ],
			"Dodge": []
		},
		"Texts": {
			"Currency": "Souls: {Amount}",
			"KeySeparator": " or ",
			"KeyLabels": { "Gamepad_FaceButton_Bottom": "A" }
		}
	})"), TEXT("ui")));
	TestTrue(TEXT("Input section seen"), Data->HasInput());
	TestTrue(TEXT("Texts section seen"), Data->HasTexts());
	TestEqual(TEXT("two keys move forward"), Data->GetInput().MoveForward.Num(), 2);
	TestEqual(TEXT("second key"), Data->GetInput().MoveForward.Last(), FName(TEXT("Up")));
	TestEqual(TEXT("an empty list unbinds"), Data->GetInput().Dodge.Num(), 0);
	TestEqual(TEXT("a text"), Data->GetTexts().Currency, FString(TEXT("Souls: {Amount}")));
	TestEqual(TEXT("an unwritten text keeps its default"), Data->GetTexts().MessageDied, FOSTextsDef().MessageDied);
	TestEqual(TEXT("key label"), Data->GetTexts().KeyLabels.FindRef(TEXT("Gamepad_FaceButton_Bottom")), FString(TEXT("A")));

	TestEqual(TEXT("consistent data validates"), Data->Validate().Num(), 0);

	// A second file must not silently replace settings that are already loaded.
	Data->LoadFromJsonString(TEXT(R"({ "Game": { "InteractRadius": 1 } })"), TEXT("game_again"));
	TestEqual(TEXT("the first Game section wins"), Data->GetGame().InteractRadius, 300.f);
	TestTrue(TEXT("and the clash is reported"), FString::Join(Data->Validate(), TEXT("\n")).Contains(TEXT("game_again")));

	Data->Reset();
	TestFalse(TEXT("Reset forgets the sections"), Data->HasGame() || Data->HasInput() || Data->HasTexts());
	TestEqual(TEXT("Reset restores the defaults"), Data->GetGame().InteractRadius, Defaults.InteractRadius);
	TestEqual(TEXT("Reset clears the bindings"), Data->GetInput().Interact.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSettingsValidationTest, "ObsidianShrine.Settings.ValidationCatchesMistakes", OS_TEST_FLAGS)

bool FOSSettingsValidationTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	Data->LoadFromJsonString(TEXT(R"({
		"Characters": [ { "Id": "hero" } ],
		"Game": { "PlayerCharacter": "nobody" },
		"Input": { "Light": [ "LeftMouseButton", "LeftMouseButon" ], "Interact": [ "E" ] }
	})"), TEXT("test"));

	const FString Report = FString::Join(Data->Validate(), TEXT("\n"));
	TestTrue(TEXT("unknown player character reported"), Report.Contains(TEXT("nobody")));
	TestTrue(TEXT("unknown key reported"), Report.Contains(TEXT("LeftMouseButon")));
	TestTrue(TEXT("with the action it is bound to"), Report.Contains(TEXT("Light")));
	TestEqual(TEXT("valid keys are not reported"), Data->Validate().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSettingsReloadTest, "ObsidianShrine.Settings.ReloadNotifiesAndReplaces", OS_TEST_FLAGS)

bool FOSSettingsReloadTest::RunTest(const FString& Parameters)
{
	// A private registry, so the live one keeps what other tests registered.
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	int32 Notified = 0;
	Data->OnReloaded.AddLambda([&Notified]() { ++Notified; });

	FOSGameDef Edited;
	Edited.InteractRadius = 12345.f;
	Data->SetGame(Edited);
	FOSActionDef Stray = OSTest::Action(TEXT("test_reload_stray"), 1.f);
	Data->AddAction(Stray, TEXT("test"));

	Data->Reload();
	TestEqual(TEXT("listeners are told once"), Notified, 1);
	TestNull(TEXT("what was only in memory is gone"), Data->FindAction(TEXT("test_reload_stray")));
	TestTrue(TEXT("settings come from the files again"), Data->GetGame().InteractRadius != 12345.f);
	TestTrue(TEXT("the shipped definitions are back"), Data->NumActions() > 0 && Data->NumCharacters() > 0);
	TestTrue(TEXT("the shipped bindings are back"), Data->HasInput());

	Data->Reload();
	TestEqual(TEXT("every reload notifies"), Notified, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSettingsTextTest, "ObsidianShrine.Settings.TextsAndKeyLabels", OS_TEST_FLAGS)

bool FOSSettingsTextTest::RunTest(const FString& Parameters)
{
	TMap<FString, FString> Values;
	Values.Add(TEXT("Keys"), TEXT("E / A"));
	Values.Add(TEXT("Prompt"), TEXT("Rest"));
	TestEqual(TEXT("placeholders are filled"), UOSDataSubsystem::FormatText(TEXT("[{Keys}]  {Prompt}"), Values), FString(TEXT("[E / A]  Rest")));
	TestEqual(TEXT("a placeholder may repeat"), UOSDataSubsystem::FormatText(TEXT("{Prompt}, {Prompt}"), Values), FString(TEXT("Rest, Rest")));
	TestEqual(TEXT("unknown placeholders are left alone"), UOSDataSubsystem::FormatText(TEXT("{Other} {Prompt}"), Values), FString(TEXT("{Other} Rest")));
	TestEqual(TEXT("text without placeholders is unchanged"), UOSDataSubsystem::FormatText(TEXT("YOU DIED"), Values), FString(TEXT("YOU DIED")));
	TestEqual(TEXT("placeholder names are case-sensitive"), UOSDataSubsystem::FormatText(TEXT("{keys}"), Values), FString(TEXT("{keys}")));

	FOSTextsDef Texts;
	Texts.KeySeparator = TEXT(" / ");
	Texts.KeyLabels.Add(TEXT("Gamepad_FaceButton_Bottom"), TEXT("A"));

	TArray<FName> Keys;
	TestEqual(TEXT("no keys, no text"), UOSDataSubsystem::DescribeKeys(Keys, Texts), FString());
	Keys.Add(TEXT("E"));
	TestEqual(TEXT("one key has no separator"), UOSDataSubsystem::DescribeKeys(Keys, Texts), FString(TEXT("E")));
	Keys.Add(TEXT("Gamepad_FaceButton_Bottom"));
	TestEqual(TEXT("labels where given, engine names otherwise"), UOSDataSubsystem::DescribeKeys(Keys, Texts), FString(TEXT("E / A")));

	int32 Actions = 0;
	FOSInputDef Input;
	Input.Interact.Add(TEXT("E"));
	bool bSawInteract = false;
	UOSDataSubsystem::ForEachBinding(Input, [&Actions, &bSawInteract](const TCHAR* Action, const TArray<FName>& Bound)
	{
		++Actions;
		if (FCString::Strcmp(Action, TEXT("Interact")) == 0)
		{
			bSawInteract = Bound.Num() == 1;
		}
	});
	TestEqual(TEXT("every input action is visited"), Actions, 17);
	TestTrue(TEXT("with its keys"), bSawInteract);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSettingsBufferTest, "ObsidianShrine.Settings.InputBufferComesFromSettings", OS_TEST_FLAGS)

bool FOSSettingsBufferTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Data = UOSDataSubsystem::Get();
	if (!TestNotNull(TEXT("data subsystem"), Data))
	{
		return false;
	}

	// The action component reads the live settings; put them back whatever happens below.
	const FOSGameDef Original = Data->GetGame();
	const bool bHadGame = Data->HasGame();
	ON_SCOPE_EXIT
	{
		Data->SetGame(Original);
		if (!bHadGame) { Data->Reload(); }
	};

	OSTest::Register(OSTest::Action(TEXT("test_buffer_long"), 1.f));
	TMap<FName, FName> Slots;
	Slots.Add(OSSlots::Light, TEXT("test_buffer_long"));

	auto PressDuringActionAndWait = [&Slots](float Wait)
	{
		FOSTestRig Rig;
		Rig.Action->SetSlotMap(Slots);
		Rig.Action->RequestSlot(OSSlots::Light);
		Rig.Action->Advance(0.5f);
		Rig.Action->RequestSlot(OSSlots::Light);   // busy: goes to the buffer
		Rig.Action->Advance(Wait);                 // the first action ends at 1.0
		return Rig.Action->IsBusy();
	};

	FOSGameDef Short = Original;
	Short.Controls.InputBuffer = 0.2f;
	Data->SetGame(Short);
	TestFalse(TEXT("a 0.2 s buffer forgets a press made 0.5 s before the action ends"), PressDuringActionAndWait(0.6f));

	FOSGameDef Long = Original;
	Long.Controls.InputBuffer = 0.8f;
	Data->SetGame(Long);
	TestTrue(TEXT("a 0.8 s buffer remembers it and starts the action again"), PressDuringActionAndWait(0.6f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
