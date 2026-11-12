#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/OSPlayerCharacter.h"
#include "Combat/OSCombatComponent.h"
#include "Engine/Engine.h"
#include "Presentation/OSCosmeticsComponent.h"

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

// ---------------------------------------------------------------------------------------------- Cosmetics

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCosmeticsCueTest, "ObsidianShrine.Cosmetics.HitOutcomesPickCues", OS_TEST_FLAGS)

bool FOSCosmeticsCueTest::RunTest(const FString& Parameters)
{
	EOSCue Cue = EOSCue::Death;
	TestFalse(TEXT("an ignored hit shows nothing"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::Ignored, Cue));
	TestTrue(TEXT("perfect guard"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::PerfectGuarded, Cue) && Cue == EOSCue::PerfectGuard);
	TestTrue(TEXT("guard"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::Guarded, Cue) && Cue == EOSCue::Guard);
	TestTrue(TEXT("a broken guard still sounds like a guard"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::GuardBroken, Cue) && Cue == EOSCue::Guard);
	TestTrue(TEXT("hit"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::Hit, Cue) && Cue == EOSCue::Hit);
	TestTrue(TEXT("stagger is a hit"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::Staggered, Cue) && Cue == EOSCue::Hit);
	TestTrue(TEXT("a killing blow is a hit"), UOSCosmeticsComponent::CueForHit(EOSHitOutcome::Killed, Cue) && Cue == EOSCue::Hit);

	FOSCosmeticsDef Def;
	Def.Hit.HitStop = 0.1f;
	Def.PhaseChange.Shake = 9.f;
	TestEqual(TEXT("Hit cue"), UOSCosmeticsComponent::GetCue(Def, EOSCue::Hit).HitStop, 0.1f);
	TestEqual(TEXT("PhaseChange cue"), UOSCosmeticsComponent::GetCue(Def, EOSCue::PhaseChange).Shake, 9.f);
	TestEqual(TEXT("other cues untouched"), UOSCosmeticsComponent::GetCue(Def, EOSCue::Guard).HitStop, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCosmeticsFootstepTest, "ObsidianShrine.Cosmetics.FootstepsFollowDistance", OS_TEST_FLAGS)

bool FOSCosmeticsFootstepTest::RunTest(const FString& Parameters)
{
	UOSCosmeticsComponent* Cosmetics = NewObject<UOSCosmeticsComponent>(GetTransientPackage());
	FOSCosmeticsDef Def;
	Def.FootstepDistance = 100.f;
	Cosmetics->SetDefinition(Def);

	TestEqual(TEXT("less than a stride: no step"), Cosmetics->AdvanceFootsteps(60.f), 0);
	TestEqual(TEXT("the rest of the stride completes a step"), Cosmetics->AdvanceFootsteps(50.f), 1);
	TestEqual(TEXT("the remainder carries over"), Cosmetics->AdvanceFootsteps(90.f), 1);
	TestEqual(TEXT("a long frame covers several strides"), Cosmetics->AdvanceFootsteps(300.f), 3);
	TestEqual(TEXT("walking backwards in distance does nothing"), Cosmetics->AdvanceFootsteps(-500.f), 0);

	Def.FootstepDistance = 0.f;
	Cosmetics->SetDefinition(Def);
	TestTrue(TEXT("a zero stride does not explode"), Cosmetics->AdvanceFootsteps(100.f) <= 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCosmeticsShakeTest, "ObsidianShrine.Cosmetics.ShakeFadesOut", OS_TEST_FLAGS)

bool FOSCosmeticsShakeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("full strength at the start"), AOSPlayerCharacter::ShakeAmplitude(10.f, 0.5f, 0.f), 10.f);
	TestEqual(TEXT("half way, half strength"), AOSPlayerCharacter::ShakeAmplitude(10.f, 0.5f, 0.25f), 5.f);
	TestEqual(TEXT("over at the end"), AOSPlayerCharacter::ShakeAmplitude(10.f, 0.5f, 0.5f), 0.f);
	TestEqual(TEXT("no duration, no shake"), AOSPlayerCharacter::ShakeAmplitude(10.f, 0.f, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCosmeticsDataTest, "ObsidianShrine.Cosmetics.LoadAndValidate", OS_TEST_FLAGS)

bool FOSCosmeticsDataTest::RunTest(const FString& Parameters)
{
	UOSDataSubsystem* Data = NewObject<UOSDataSubsystem>(GEngine);
	TestTrue(TEXT("loads"), Data->LoadFromJsonString(TEXT(R"({
		"Characters": [ { "Id": "c", "Cosmetics": "flesh" }, { "Id": "d", "Cosmetics": "missing_cosmetics" } ],
		"Cosmetics": [ { "Id": "flesh", "FootstepDistance": 120,
		                 "Hit": { "Sound": "/Game/Hit.Hit", "HitStop": 0.05, "Shake": 4 } } ]
	})"), TEXT("test")));

	const FOSCosmeticsDef* Def = Data->FindCosmetics(TEXT("flesh"));
	if (TestNotNull(TEXT("cosmetics parsed"), Def))
	{
		TestEqual(TEXT("FootstepDistance"), Def->FootstepDistance, 120.f);
		TestEqual(TEXT("nested cue: sound"), Def->Hit.Sound, FString(TEXT("/Game/Hit.Hit")));
		TestEqual(TEXT("nested cue: hit-stop"), Def->Hit.HitStop, 0.05f);
		TestEqual(TEXT("unwritten cue field keeps its default"), Def->Hit.Volume, 1.f);
	}
	TestTrue(TEXT("unknown cosmetics reported"), FString::Join(Data->Validate(), TEXT("\n")).Contains(TEXT("missing_cosmetics")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
