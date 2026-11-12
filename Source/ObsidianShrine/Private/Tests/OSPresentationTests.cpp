#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"

// ---------------------------------------------------------------------------------------------- Animation

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSAnimationLocomotionTest, "ObsidianShrine.Animation.LocomotionFollowsSpeed", OS_TEST_FLAGS)

bool FOSAnimationLocomotionTest::RunTest(const FString& Parameters)
{
	FOSAnimationSetDef Set;
	Set.Idle = TEXT("/Game/Idle.Idle");
	Set.Walk = TEXT("/Game/Walk.Walk");
	Set.Run = TEXT("/Game/Run.Run");
	Set.WalkSpeed = 400.f;
	Set.RunSpeed = 600.f;

	float Rate = 0.f;
	TestEqual(TEXT("standing plays Idle"), Set.PickLocomotion(0.f, false, Rate), Set.Idle);
	TestEqual(TEXT("Idle at its own rate"), Rate, 1.f);
	TestEqual(TEXT("walking plays Walk"), Set.PickLocomotion(200.f, false, Rate), Set.Walk);
	TestEqual(TEXT("half the authored speed, half the rate"), Rate, 0.5f);
	TestEqual(TEXT("sprinting plays Run"), Set.PickLocomotion(900.f, true, Rate), Set.Run);
	TestEqual(TEXT("Run matched to its own speed"), Rate, 1.5f);
	Set.PickLocomotion(5000.f, false, Rate);
	TestEqual(TEXT("rate is clamped"), Rate, 3.f);

	Set.Run.Reset();
	TestEqual(TEXT("no Run: sprinting plays Walk"), Set.PickLocomotion(600.f, true, Rate), Set.Walk);
	TestEqual(TEXT("at Walk's speed"), Rate, 1.5f);

	Set.WalkSpeed = 0.f;
	Set.PickLocomotion(200.f, false, Rate);
	TestEqual(TEXT("no authored speed: as authored"), Rate, 1.f);

	Set.Walk.Reset();
	TestEqual(TEXT("no Walk: moving keeps Idle"), Set.PickLocomotion(200.f, false, Rate), Set.Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSAnimationDataTest, "ObsidianShrine.Animation.SetsLoadAndValidate", OS_TEST_FLAGS)

bool FOSAnimationDataTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	TestTrue(TEXT("loads"), Data->LoadFromJsonString(TEXT(R"({
		"Actions": [ { "Id": "swing", "Duration": 1 } ],
		"Characters": [ { "Id": "c", "Mesh": "/Game/C.C", "AnimationSet": "c_anims" },
		                { "Id": "d", "AnimationSet": "missing_set" } ],
		"AnimationSets": [ { "Id": "c_anims", "Idle": "/Game/Idle.Idle", "WalkSpeed": 350,
		                     "Actions": { "swing": "/Game/Swing.Swing", "missing_action": "/Game/X.X" } } ]
	})"), TEXT("test")));

	const FOSAnimationSetDef* Set = Data->FindAnimationSet(TEXT("c_anims"));
	if (TestNotNull(TEXT("set parsed"), Set))
	{
		TestEqual(TEXT("Idle"), Set->Idle, FString(TEXT("/Game/Idle.Idle")));
		TestEqual(TEXT("WalkSpeed"), Set->WalkSpeed, 350.f);
		TestEqual(TEXT("action animation"), Set->Actions.FindRef(TEXT("swing")), FString(TEXT("/Game/Swing.Swing")));
	}

	const FString Report = FString::Join(Data->Validate(), TEXT("\n"));
	TestTrue(TEXT("unknown set reported"), Report.Contains(TEXT("missing_set")));
	TestTrue(TEXT("unknown action in a set reported"), Report.Contains(TEXT("missing_action")));
	TestFalse(TEXT("a known set is fine"), Report.Contains(TEXT("unknown animation set 'c_anims'")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
