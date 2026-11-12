#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/OSAIBrainComponent.h"
#include "Characters/OSCharacterBase.h"
#include "Combat/OSLockOnComponent.h"
#include "Core/OSProgress.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "World/OSWorldActors.h"

// ---------------------------------------------------------------------------------------------- AI

namespace
{
	FOSAIMoveDef MakeMove(FName Action, float MinRange, float MaxRange, int32 MinPhase = 0)
	{
		FOSAIMoveDef Move;
		Move.Action = Action;
		Move.MinRange = MinRange;
		Move.MaxRange = MaxRange;
		Move.MinPhase = MinPhase;
		return Move;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSAIPickMoveTest, "ObsidianShrine.AI.PickMoveFilters", OS_TEST_FLAGS)

bool FOSAIPickMoveTest::RunTest(const FString& Parameters)
{
	FOSAIProfileDef Profile;
	Profile.Moves.Add(MakeMove(TEXT("melee"), 0.f, 200.f));
	Profile.Moves.Add(MakeMove(TEXT("lunge"), 400.f, 800.f));
	Profile.Moves.Add(MakeMove(TEXT("enraged"), 0.f, 200.f, 1));

	FRandomStream Random(1234);
	TMap<FName, float> Cooldowns;

	TestEqual(TEXT("close range picks melee"), UOSAIBrainComponent::PickMove(Profile, 100.f, 0, Cooldowns, Random), 0);
	TestEqual(TEXT("long range picks lunge"), UOSAIBrainComponent::PickMove(Profile, 600.f, 0, Cooldowns, Random), 1);
	TestEqual(TEXT("nothing reaches the gap"), UOSAIBrainComponent::PickMove(Profile, 300.f, 0, Cooldowns, Random), static_cast<int32>(INDEX_NONE));

	Cooldowns.Add(TEXT("melee"), 2.f);
	TestEqual(TEXT("cooldown blocks the move"), UOSAIBrainComponent::PickMove(Profile, 100.f, 0, Cooldowns, Random), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("phase 1 unlocks the enraged move"), UOSAIBrainComponent::PickMove(Profile, 100.f, 1, Cooldowns, Random), 2);

	Cooldowns.Add(TEXT("melee"), -0.1f);
	bool bSawMelee = false, bSawEnraged = false;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		const int32 Picked = UOSAIBrainComponent::PickMove(Profile, 100.f, 1, Cooldowns, Random);
		bSawMelee |= Picked == 0;
		bSawEnraged |= Picked == 2;
		if (Picked != 0 && Picked != 2)
		{
			AddError(FString::Printf(TEXT("picked an unusable move %d"), Picked));
			break;
		}
	}
	TestTrue(TEXT("weighted choice uses every usable move"), bSawMelee && bSawEnraged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSAIPhaseTest, "ObsidianShrine.AI.PhaseFollowsHealth", OS_TEST_FLAGS)

bool FOSAIPhaseTest::RunTest(const FString& Parameters)
{
	FOSAIProfileDef Profile;
	Profile.Phases.AddDefaulted(2);
	Profile.Phases[0].HealthBelow = 0.6f;
	Profile.Phases[1].HealthBelow = 0.25f;

	TestEqual(TEXT("full health"), UOSAIBrainComponent::ComputePhase(Profile, 1.f), 0);
	TestEqual(TEXT("above the first threshold"), UOSAIBrainComponent::ComputePhase(Profile, 0.61f), 0);
	TestEqual(TEXT("at the first threshold"), UOSAIBrainComponent::ComputePhase(Profile, 0.6f), 1);
	TestEqual(TEXT("below the second threshold"), UOSAIBrainComponent::ComputePhase(Profile, 0.1f), 2);
	TestEqual(TEXT("no phases defined"), UOSAIBrainComponent::ComputePhase(FOSAIProfileDef(), 0.f), 0);
	return true;
}

// ---------------------------------------------------------------------------------------------- Progress

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSProgressDeathLoopTest, "ObsidianShrine.Progress.DeathDropsAndRecoversCurrency", OS_TEST_FLAGS)

bool FOSProgressDeathLoopTest::RunTest(const FString& Parameters)
{
	// The classic rules, spelled out so the test does not depend on the shipped settings.
	FOSDeathDef Rules;
	Rules.CurrencyLossFraction = 1.f;
	Rules.bKeepPreviousDrop = false;

	FOSProgressState State;
	State.AddCurrency(300);
	State.HandleDeath(FVector(10.f, 20.f, 30.f), TEXT("L_Test"), Rules);
	TestEqual(TEXT("nothing in hand after death"), State.Currency, 0);
	TestEqual(TEXT("everything dropped"), State.DroppedCurrency, 300);
	TestEqual(TEXT("drop location"), State.DroppedLocation, FVector(10.f, 20.f, 30.f));

	State.AddCurrency(50);
	TestEqual(TEXT("recovered amount"), State.RecoverDropped(), 300);
	TestEqual(TEXT("recovered on top of new earnings"), State.Currency, 350);
	TestEqual(TEXT("nothing left on the ground"), State.DroppedCurrency, 0);

	State.HandleDeath(FVector::ZeroVector, TEXT("L_Test"), Rules);
	State.AddCurrency(20);
	State.HandleDeath(FVector::ZeroVector, TEXT("L_Test"), Rules);
	TestEqual(TEXT("dying again loses the earlier drop"), State.DroppedCurrency, 20);

	State.MarkDefeated(TEXT("boss_a"));
	State.MarkDefeated(TEXT("boss_a"));
	State.MarkDefeated(NAME_None);
	TestEqual(TEXT("defeated list has no duplicates or None"), State.Defeated.Num(), 1);
	TestTrue(TEXT("boss_a defeated"), State.IsDefeated(TEXT("boss_a")));
	TestFalse(TEXT("None is never defeated"), State.IsDefeated(NAME_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSProgressDeathRulesTest, "ObsidianShrine.Progress.DeathRulesComeFromSettings", OS_TEST_FLAGS)

bool FOSProgressDeathRulesTest::RunTest(const FString& Parameters)
{
	FOSDeathDef Half;
	Half.CurrencyLossFraction = 0.5f;
	Half.bKeepPreviousDrop = true;

	FOSProgressState State;
	State.AddCurrency(300);
	State.HandleDeath(FVector(1.f, 0.f, 0.f), TEXT("L_A"), Half);
	TestEqual(TEXT("half stays in hand"), State.Currency, 150);
	TestEqual(TEXT("half is dropped"), State.DroppedCurrency, 150);

	State.HandleDeath(FVector(2.f, 0.f, 0.f), TEXT("L_B"), Half);
	TestEqual(TEXT("half of the rest stays in hand"), State.Currency, 75);
	TestEqual(TEXT("the earlier drop is carried to the new spot"), State.DroppedCurrency, 225);
	TestEqual(TEXT("the drop moved"), State.DroppedLocation, FVector(2.f, 0.f, 0.f));
	TestEqual(TEXT("the drop moved to the new map"), State.DroppedMap, FString(TEXT("L_B")));

	FOSDeathDef Free;
	Free.CurrencyLossFraction = 0.f;
	Free.bKeepPreviousDrop = false;
	State.HandleDeath(FVector(3.f, 0.f, 0.f), TEXT("L_C"), Free);
	TestEqual(TEXT("a free death costs nothing in hand"), State.Currency, 75);
	TestEqual(TEXT("but the earlier drop is lost"), State.DroppedCurrency, 0);
	TestTrue(TEXT("no map holds a drop"), State.DroppedMap.IsEmpty());

	FOSDeathDef OutOfRange;
	OutOfRange.CurrencyLossFraction = 7.f;
	State.HandleDeath(FVector::ZeroVector, TEXT("L_C"), OutOfRange);
	TestEqual(TEXT("a fraction above 1 cannot take more than there is"), State.Currency, 0);
	TestEqual(TEXT("everything dropped"), State.DroppedCurrency, 75);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSProgressSaveTest, "ObsidianShrine.Progress.SaveRoundTrip", OS_TEST_FLAGS)

bool FOSProgressSaveTest::RunTest(const FString& Parameters)
{
	UOSSaveGame* Saved = NewObject<UOSSaveGame>();
	Saved->State.Currency = 777;
	Saved->State.LastCheckpoint = TEXT("shrine_x");
	Saved->State.LastCheckpointMap = TEXT("/Game/Maps/L_Test");
	Saved->State.DroppedCurrency = 55;
	Saved->State.DroppedLocation = FVector(1.f, 2.f, 3.f);
	Saved->State.DroppedMap = TEXT("L_Test");
	Saved->State.MarkDefeated(TEXT("boss_a"));

	// In memory, so the test never touches the real save slot.
	TArray<uint8> Bytes;
	TestTrue(TEXT("serialises"), UGameplayStatics::SaveGameToMemory(Saved, Bytes));

	const UOSSaveGame* Loaded = Cast<UOSSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("deserialises"), Loaded))
	{
		return false;
	}
	TestEqual(TEXT("Currency"), Loaded->State.Currency, 777);
	TestEqual(TEXT("LastCheckpoint"), Loaded->State.LastCheckpoint, FName(TEXT("shrine_x")));
	TestEqual(TEXT("LastCheckpointMap"), Loaded->State.LastCheckpointMap, FString(TEXT("/Game/Maps/L_Test")));
	TestEqual(TEXT("DroppedCurrency"), Loaded->State.DroppedCurrency, 55);
	TestEqual(TEXT("DroppedLocation"), Loaded->State.DroppedLocation, FVector(1.f, 2.f, 3.f));
	TestEqual(TEXT("DroppedMap"), Loaded->State.DroppedMap, FString(TEXT("L_Test")));
	TestTrue(TEXT("Defeated"), Loaded->State.IsDefeated(TEXT("boss_a")));
	return true;
}

// ---------------------------------------------------------------------------------------------- Travel

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSTravelRespawnMapTest, "ObsidianShrine.Travel.RespawnMapFollowsLastShrine", OS_TEST_FLAGS)

bool FOSTravelRespawnMapTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a path and its short name are the same map"), FOSProgressState::IsSameMap(TEXT("/Game/Maps/L_A"), TEXT("L_A")));
	TestTrue(TEXT("map names ignore case"), FOSProgressState::IsSameMap(TEXT("/Game/Maps/L_A"), TEXT("/Game/Maps/l_a")));
	TestFalse(TEXT("different maps"), FOSProgressState::IsSameMap(TEXT("/Game/Maps/L_A"), TEXT("/Game/Maps/L_B")));
	TestFalse(TEXT("no map is no map"), FOSProgressState::IsSameMap(TEXT(""), TEXT("")));

	FOSProgressState State;
	TestTrue(TEXT("never rested: respawn where you are"), State.RespawnMapIfElsewhere(TEXT("/Game/Maps/L_A")).IsEmpty());

	State.RestAt(TEXT("shrine_b"), TEXT("/Game/Maps/L_B"));
	TestEqual(TEXT("checkpoint stored"), State.LastCheckpoint, FName(TEXT("shrine_b")));
	TestEqual(TEXT("dying on another map goes to the shrine's map"),
		State.RespawnMapIfElsewhere(TEXT("/Game/Maps/L_A")), FString(TEXT("/Game/Maps/L_B")));
	TestTrue(TEXT("dying on the shrine's map stays"), State.RespawnMapIfElsewhere(TEXT("/Game/Maps/L_B")).IsEmpty());

	// A save written before map travel existed has a checkpoint but no map.
	FOSProgressState OldSave;
	OldSave.LastCheckpoint = TEXT("shrine_b");
	TestTrue(TEXT("an old save never travels"), OldSave.RespawnMapIfElsewhere(TEXT("/Game/Maps/L_A")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSTravelStateTest, "ObsidianShrine.Travel.StateIsTakenOnce", OS_TEST_FLAGS)

bool FOSTravelStateTest::RunTest(const FString& Parameters)
{
	FOSTravelState Travel;
	float Health = -1.f;
	TestTrue(TEXT("no travel: no arrival"), Travel.TakeArrival().IsNone());
	TestFalse(TEXT("no travel: no health"), Travel.TakeHealth(Health));

	Travel.Begin(TEXT("exit_b"), 42.f);
	TestEqual(TEXT("arrival"), Travel.TakeArrival(), FName(TEXT("exit_b")));
	TestTrue(TEXT("arrival is taken once"), Travel.TakeArrival().IsNone());
	TestTrue(TEXT("health carried"), Travel.TakeHealth(Health));
	TestEqual(TEXT("carried amount"), Health, 42.f);
	TestFalse(TEXT("health is taken once"), Travel.TakeHealth(Health));

	Travel.Begin(TEXT("exit_c"), 10.f);
	Travel.Clear();
	TestTrue(TEXT("death clears the arrival"), Travel.TakeArrival().IsNone());
	TestFalse(TEXT("death clears the health"), Travel.TakeHealth(Health));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSTravelHealthTest, "ObsidianShrine.Travel.CarriedHealthNeverKills", OS_TEST_FLAGS)

bool FOSTravelHealthTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig(100.f);
	Rig.Stats->SetHealth(37.f);
	TestEqual(TEXT("health set"), Rig.Stats->GetHealth(), 37.f);
	Rig.Stats->SetHealth(500.f);
	TestEqual(TEXT("clamped to max"), Rig.Stats->GetHealth(), 100.f);
	Rig.Stats->SetHealth(0.f);
	TestEqual(TEXT("never below 1"), Rig.Stats->GetHealth(), 1.f);
	TestFalse(TEXT("not killed"), Rig.Stats->IsDead());

	Rig.Stats->Kill();
	Rig.Stats->SetHealth(50.f);
	TestTrue(TEXT("not revived"), Rig.Stats->IsDead());
	TestEqual(TEXT("dead stays at zero"), Rig.Stats->GetHealth(), 0.f);
	return true;
}

// ---------------------------------------------------------------------------------------------- Lock-on

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSLockOnScoreTest, "ObsidianShrine.LockOn.ScorePrefersCentreOfView", OS_TEST_FLAGS)

bool FOSLockOnScoreTest::RunTest(const FString& Parameters)
{
	const FVector Eye = FVector::ZeroVector;
	const FVector Forward = FVector::ForwardVector;
	auto Score = [&](const FVector& Location) { return UOSLockOnComponent::ScoreCandidate(Eye, Forward, Location, 2000.f, 55.f); };

	TestTrue(TEXT("behind the camera is rejected"), Score(FVector(-500.f, 0.f, 0.f)) < 0.f);
	TestTrue(TEXT("too far is rejected"), Score(FVector(2500.f, 0.f, 0.f)) < 0.f);
	TestTrue(TEXT("outside the cone is rejected"), Score(FVector(100.f, 500.f, 0.f)) < 0.f);

	const float Centre = Score(FVector(1000.f, 0.f, 0.f));
	const float OffCentre = Score(FVector(1000.f, 500.f, 0.f));
	const float CentreFar = Score(FVector(1500.f, 0.f, 0.f));
	TestTrue(TEXT("valid candidates score >= 0"), Centre >= 0.f && OffCentre >= 0.f && CentreFar >= 0.f);
	TestTrue(TEXT("centre beats off-centre at the same depth"), Centre < OffCentre);
	TestTrue(TEXT("nearer beats farther on the same line"), Centre < CentreFar);
	TestTrue(TEXT("a far target in the centre beats a nearer one at the edge"), CentreFar < OffCentre);
	return true;
}

// ---------------------------------------------------------------------------------------------- World

namespace
{
	/** A minimal game world with physics, no game mode and no player. */
	struct FOSTestWorld
	{
		UWorld* World = nullptr;

		FOSTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			// Without a game mode nobody flips the world into "begun play", and spawned actors would never BeginPlay.
			World->GetWorldSettings()->NotifyBeginPlay();
		}

		~FOSTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		AOSCharacterBase* Spawn(FName CharacterId, EOSFaction Faction, const FVector& Location) const
		{
			const FTransform Transform(Location);
			AOSCharacterBase* Character = World->SpawnActorDeferred<AOSCharacterBase>(AOSCharacterBase::StaticClass(), Transform,
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Character->CharacterId = CharacterId;
			Character->Faction = Faction;
			Character->FinishSpawning(Transform);
			return Character;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSWorldAttackTest, "ObsidianShrine.World.AttackHitsOnlyEnemiesInFront", OS_TEST_FLAGS)

bool FOSWorldAttackTest::RunTest(const FString& Parameters)
{
	FOSActionDef Swing = OSTest::Action(TEXT("test_world_swing"), 1.f);
	Swing.DamageScale = 2.f;
	Swing.Windows.Add(OSTest::Window(EOSWindowType::Hit, 0.2f, 0.4f));
	OSTest::Register(Swing);

	FOSWeaponDef Blade;
	Blade.Id = TEXT("test_world_blade");
	Blade.Damage = 10.f;
	Blade.PoiseDamage = 0.f;
	Blade.Reach = 200.f;
	Blade.Radius = 50.f;
	Blade.Actions.Add(OSSlots::Light, TEXT("test_world_swing"));
	OSTest::Register(Blade);

	FOSCharacterDef Dummy;
	Dummy.Id = TEXT("test_world_dummy");
	Dummy.MaxHealth = 100.f;
	Dummy.MaxPoise = 1000.f;
	Dummy.Weapon = TEXT("test_world_blade");
	OSTest::Register(Dummy);

	FOSTestWorld TestWorld;
	AOSCharacterBase* Attacker = TestWorld.Spawn(TEXT("test_world_dummy"), EOSFaction::Player, FVector::ZeroVector);
	AOSCharacterBase* InFront = TestWorld.Spawn(TEXT("test_world_dummy"), EOSFaction::Enemy, FVector(150.f, 0.f, 0.f));
	AOSCharacterBase* Behind = TestWorld.Spawn(TEXT("test_world_dummy"), EOSFaction::Enemy, FVector(-150.f, 0.f, 0.f));
	AOSCharacterBase* OutOfReach = TestWorld.Spawn(TEXT("test_world_dummy"), EOSFaction::Enemy, FVector(400.f, 0.f, 0.f));
	AOSCharacterBase* Ally = TestWorld.Spawn(TEXT("test_world_dummy"), EOSFaction::Player, FVector(120.f, 60.f, 0.f));

	if (!TestTrue(TEXT("spawned characters have begun play"), Attacker->HasActorBegunPlay()))
	{
		return false;
	}
	TestEqual(TEXT("definition applied: health"), InFront->GetStats()->GetMaxHealth(), 100.f);
	TestEqual(TEXT("definition applied: weapon"), Attacker->GetCombat()->GetWeapon().Damage, 10.f);

	int32 HitsDealt = 0;
	Attacker->GetCombat()->OnHitDealt.AddLambda([&HitsDealt](const FOSHitResult&, AActor*) { ++HitsDealt; });

	TestTrue(TEXT("attack starts from the weapon's slot"), Attacker->GetAction()->RequestSlot(OSSlots::Light));
	TestEqual(TEXT("no damage before the hit window"), InFront->GetStats()->GetHealth(), 100.f);

	// Drive the timeline by hand; the window opening triggers the sweep.
	Attacker->GetAction()->Advance(0.25f);

	TestEqual(TEXT("enemy in front takes weapon damage x action scale"), InFront->GetStats()->GetHealth(), 80.f);
	TestEqual(TEXT("enemy behind is untouched"), Behind->GetStats()->GetHealth(), 100.f);
	TestEqual(TEXT("enemy out of reach is untouched"), OutOfReach->GetStats()->GetHealth(), 100.f);
	TestEqual(TEXT("ally is untouched"), Ally->GetStats()->GetHealth(), 100.f);
	TestEqual(TEXT("one hit dealt"), HitsDealt, 1);

	// Sweeping again within the same window must not hit the same target twice.
	Attacker->GetCombat()->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("a target is hit once per window"), InFront->GetStats()->GetHealth(), 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSWorldReapplyTest, "ObsidianShrine.World.ReappliedDefinitionKeepsState", OS_TEST_FLAGS)

bool FOSWorldReapplyTest::RunTest(const FString& Parameters)
{
	// What os.ReloadData does to a living character: new numbers, same fight.
	FOSCharacterDef Before;
	Before.Id = TEXT("test_reapply_before");
	Before.MaxHealth = 100.f;
	Before.WalkSpeed = 300.f;
	Before.TurnRate = 200.f;
	Before.Acceleration = 1000.f;
	Before.BrakingDeceleration = 900.f;
	OSTest::Register(Before);

	FOSCharacterDef After = Before;
	After.Id = TEXT("test_reapply_after");
	After.MaxHealth = 60.f;
	After.WalkSpeed = 500.f;
	After.TurnRate = 450.f;
	After.Acceleration = 2000.f;
	After.BrakingDeceleration = 1800.f;
	OSTest::Register(After);

	FOSTestWorld TestWorld;
	AOSCharacterBase* Character = TestWorld.Spawn(TEXT("test_reapply_before"), EOSFaction::Enemy, FVector::ZeroVector);
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TestEqual(TEXT("walk speed from the definition"), Movement->MaxWalkSpeed, 300.f);
	TestEqual(TEXT("turn rate from the definition"), (float)Movement->RotationRate.Yaw, 200.f);
	TestEqual(TEXT("acceleration from the definition"), Movement->MaxAcceleration, 1000.f);
	TestEqual(TEXT("braking from the definition"), Movement->BrakingDecelerationWalking, 900.f);

	Character->GetStats()->ApplyDamage(70.f);
	TestTrue(TEXT("the other definition applies"), Character->ApplyDefinition(TEXT("test_reapply_after"), true));
	TestEqual(TEXT("new limit"), Character->GetStats()->GetMaxHealth(), 60.f);
	TestEqual(TEXT("the wound is kept"), Character->GetStats()->GetHealth(), 30.f);
	TestEqual(TEXT("new walk speed"), Movement->MaxWalkSpeed, 500.f);
	TestEqual(TEXT("new turn rate"), (float)Movement->RotationRate.Yaw, 450.f);
	TestEqual(TEXT("new acceleration"), Movement->MaxAcceleration, 2000.f);
	TestEqual(TEXT("new braking"), Movement->BrakingDecelerationWalking, 1800.f);

	TestTrue(TEXT("a fresh apply refills"), Character->ApplyDefinition(TEXT("test_reapply_before"), false));
	TestEqual(TEXT("full health"), Character->GetStats()->GetHealth(), 100.f);

	AddExpectedError(TEXT("unknown character definition"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("an unknown definition is refused"), Character->ApplyDefinition(TEXT("test_reapply_missing"), true));
	TestEqual(TEXT("and changes nothing"), Character->GetStats()->GetMaxHealth(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSWorldHitStopTest, "ObsidianShrine.World.HitStopFreezesThenReleases", OS_TEST_FLAGS)

bool FOSWorldHitStopTest::RunTest(const FString& Parameters)
{
	FOSCharacterDef Dummy;
	Dummy.Id = TEXT("test_hitstop_dummy");
	OSTest::Register(Dummy);

	FOSTestWorld TestWorld;
	AOSCharacterBase* Character = TestWorld.Spawn(TEXT("test_hitstop_dummy"), EOSFaction::Enemy, FVector::ZeroVector);
	TestFalse(TEXT("runs normally"), Character->IsInHitStop());

	Character->ApplyHitStop(0.1f);
	TestTrue(TEXT("frozen"), Character->IsInHitStop());
	Character->ApplyHitStop(0.05f);

	TestWorld.World->Tick(LEVELTICK_All, 0.08f);
	TestTrue(TEXT("a shorter hit-stop does not cut the longer one short"), Character->IsInHitStop());
	TestWorld.World->Tick(LEVELTICK_All, 0.05f);
	TestFalse(TEXT("released after the longest"), Character->IsInHitStop());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSWorldMapExitTest, "ObsidianShrine.World.MapExitArrivalAndSeal", OS_TEST_FLAGS)

bool FOSWorldMapExitTest::RunTest(const FString& Parameters)
{
	FOSTestWorld TestWorld;
	AOSMapExit* Exit = TestWorld.World->SpawnActor<AOSMapExit>(AOSMapExit::StaticClass(), FVector(100.f, 200.f, 150.f), FRotator(0.f, 90.f, 0.f));
	if (!TestNotNull(TEXT("exit spawned"), Exit))
	{
		return false;
	}
	Exit->ArrivalDistance = 200.f;
	Exit->ArrivalHeight = 10.f;

	const FTransform Arrival = Exit->GetArrivalTransform();
	TestTrue(TEXT("arrive in front of the exit"), Arrival.GetLocation().Equals(FVector(100.f, 400.f, 160.f), 0.1f));
	TestTrue(TEXT("facing away from the exit"), FMath::IsNearlyEqual(Arrival.Rotator().Yaw, 90.f, 0.1f));

	FOSProgressState State;
	TestFalse(TEXT("an exit without a requirement is open"), Exit->IsSealed(State));
	Exit->RequiresDefeated = TEXT("test_boss");
	TestTrue(TEXT("sealed while the boss lives"), Exit->IsSealed(State));
	State.MarkDefeated(TEXT("test_boss"));
	TestFalse(TEXT("open once the boss is defeated"), Exit->IsSealed(State));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
