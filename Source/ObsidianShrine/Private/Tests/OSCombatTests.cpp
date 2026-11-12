#include "Tests/OSTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---------------------------------------------------------------------------------------------- Stats

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSStatsStaminaTest, "ObsidianShrine.Stats.StaminaRegenWaitsForDelay", OS_TEST_FLAGS)

bool FOSStatsStaminaTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig; // regen 10/s after a 1 s delay
	Rig.Stats->SpendStamina(40.f);
	TestEqual(TEXT("spent"), Rig.Stats->GetStamina(), 60.f);

	Rig.Stats->Advance(0.5f);
	TestEqual(TEXT("no regen during the delay"), Rig.Stats->GetStamina(), 60.f);

	Rig.Stats->Advance(0.6f); // delay over
	Rig.Stats->Advance(1.f);
	TestEqual(TEXT("regen after the delay"), Rig.Stats->GetStamina(), 70.f);

	Rig.Stats->SpendStamina(500.f);
	TestEqual(TEXT("clamped at zero"), Rig.Stats->GetStamina(), 0.f);
	TestFalse(TEXT("HasStamina at zero"), Rig.Stats->HasStamina());

	Rig.Stats->Advance(100.f);
	Rig.Stats->Advance(100.f);
	TestEqual(TEXT("clamped at max"), Rig.Stats->GetStamina(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSStatsDeathTest, "ObsidianShrine.Stats.DeathFiresOnce", OS_TEST_FLAGS)

bool FOSStatsDeathTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig;
	int32 Deaths = 0;
	Rig.Stats->OnDeath.AddLambda([&Deaths]() { ++Deaths; });

	TestEqual(TEXT("applied damage"), Rig.Stats->ApplyDamage(30.f), 30.f);
	TestFalse(TEXT("alive at 70"), Rig.Stats->IsDead());

	TestEqual(TEXT("overkill is clamped to remaining health"), Rig.Stats->ApplyDamage(500.f), 70.f);
	TestTrue(TEXT("dead"), Rig.Stats->IsDead());

	Rig.Stats->ApplyDamage(10.f);
	Rig.Stats->Kill();
	TestEqual(TEXT("OnDeath fired once"), Deaths, 1);

	Rig.Stats->RestoreAll();
	TestFalse(TEXT("alive after restore"), Rig.Stats->IsDead());
	TestEqual(TEXT("full health after restore"), Rig.Stats->GetHealth(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSStatsPoiseTest, "ObsidianShrine.Stats.PoiseBreaksAndResets", OS_TEST_FLAGS)

bool FOSStatsPoiseTest::RunTest(const FString& Parameters)
{
	FOSTestRig Armoured(100.f, 100.f, 50.f); // poise resets after 2 s
	TestFalse(TEXT("first hit does not break"), Armoured.Stats->ApplyPoiseDamage(30.f));
	TestTrue(TEXT("second hit breaks"), Armoured.Stats->ApplyPoiseDamage(30.f));
	TestEqual(TEXT("poise refilled after breaking"), Armoured.Stats->GetPoise(), 50.f);

	Armoured.Stats->ApplyPoiseDamage(30.f);
	Armoured.Stats->Advance(2.5f);
	TestEqual(TEXT("poise resets after the delay"), Armoured.Stats->GetPoise(), 50.f);

	FOSTestRig Unarmoured(100.f, 100.f, 0.f);
	TestTrue(TEXT("no poise: every hit breaks"), Unarmoured.Stats->ApplyPoiseDamage(1.f));
	return true;
}

// ---------------------------------------------------------------------------------------------- Actions

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionTimelineTest, "ObsidianShrine.Action.WindowsFollowTimeline", OS_TEST_FLAGS)

bool FOSActionTimelineTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig;
	FOSActionDef Attack = OSTest::Action(TEXT("test_attack"), 1.f);
	Attack.Windows.Add(OSTest::Window(EOSWindowType::Hit, 0.3f, 0.5f));
	Attack.Windows.Add(OSTest::Window(EOSWindowType::Cancel, 0.7f)); // until the end

	int32 Started = 0, Ended = 0, HitBegins = 0, HitEnds = 0;
	Rig.Action->OnActionStarted.AddLambda([&Started](const FOSActionDef&) { ++Started; });
	Rig.Action->OnActionEnded.AddLambda([&Ended](const FOSActionDef&, bool) { ++Ended; });
	Rig.Action->OnWindowChanged.AddLambda([&HitBegins, &HitEnds](EOSWindowType Type, bool bBegin)
	{
		if (Type == EOSWindowType::Hit) { bBegin ? ++HitBegins : ++HitEnds; }
	});

	TestTrue(TEXT("starts"), Rig.Action->StartActionDef(Attack));
	TestTrue(TEXT("busy"), Rig.Action->IsBusy());
	TestFalse(TEXT("hit closed at 0"), Rig.Action->IsWindowActive(EOSWindowType::Hit));

	Rig.Action->Advance(0.35f);
	TestTrue(TEXT("hit open at 0.35"), Rig.Action->IsWindowActive(EOSWindowType::Hit));

	Rig.Action->Advance(0.25f);
	TestFalse(TEXT("hit closed at 0.6"), Rig.Action->IsWindowActive(EOSWindowType::Hit));
	TestFalse(TEXT("cancel closed at 0.6"), Rig.Action->IsWindowActive(EOSWindowType::Cancel));

	Rig.Action->Advance(0.2f);
	TestTrue(TEXT("open-ended cancel open at 0.8"), Rig.Action->IsWindowActive(EOSWindowType::Cancel));

	Rig.Action->Advance(0.3f);
	TestFalse(TEXT("idle after the duration"), Rig.Action->IsBusy());
	TestFalse(TEXT("no windows when idle"), Rig.Action->IsWindowActive(EOSWindowType::Cancel));

	TestEqual(TEXT("started once"), Started, 1);
	TestEqual(TEXT("ended once"), Ended, 1);
	TestEqual(TEXT("hit began once"), HitBegins, 1);
	TestEqual(TEXT("hit ended once"), HitEnds, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionSkippedWindowTest, "ObsidianShrine.Action.LongFrameDoesNotSkipWindow", OS_TEST_FLAGS)

bool FOSActionSkippedWindowTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig;
	FOSActionDef Attack = OSTest::Action(TEXT("test_quick"), 1.f);
	Attack.Windows.Add(OSTest::Window(EOSWindowType::Hit, 0.3f, 0.35f));

	int32 HitBegins = 0, HitEnds = 0;
	Rig.Action->OnWindowChanged.AddLambda([&HitBegins, &HitEnds](EOSWindowType, bool bBegin) { bBegin ? ++HitBegins : ++HitEnds; });

	Rig.Action->StartActionDef(Attack);
	Rig.Action->Advance(0.2f);
	Rig.Action->Advance(0.3f); // 0.2 -> 0.5 steps over the whole window

	TestEqual(TEXT("window opened"), HitBegins, 1);
	TestEqual(TEXT("window closed"), HitEnds, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionStaminaTest, "ObsidianShrine.Action.StaminaGate", OS_TEST_FLAGS)

bool FOSActionStaminaTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig;
	const FOSActionDef Costly = OSTest::Action(TEXT("test_costly"), 0.2f, 60.f);

	TestTrue(TEXT("first start"), Rig.Action->StartActionDef(Costly));
	TestEqual(TEXT("cost spent"), Rig.Stats->GetStamina(), 40.f);
	TestFalse(TEXT("cannot start while busy"), Rig.Action->StartActionDef(Costly));
	Rig.Action->Advance(0.3f);

	TestTrue(TEXT("any stamina above zero is enough"), Rig.Action->StartActionDef(Costly));
	TestEqual(TEXT("stamina bottoms out at zero"), Rig.Stats->GetStamina(), 0.f);
	Rig.Action->Advance(0.3f);

	TestFalse(TEXT("blocked at zero stamina"), Rig.Action->StartActionDef(Costly));
	TestTrue(TEXT("free actions still start"), Rig.Action->StartActionDef(OSTest::Action(TEXT("test_free"), 0.2f)));
	TestTrue(TEXT("forced start ignores stamina"), Rig.Action->StartActionDef(Costly, NAME_None, true));

	Rig.Stats->Kill();
	Rig.Action->Interrupt();
	TestFalse(TEXT("the dead start nothing"), Rig.Action->StartActionDef(OSTest::Action(TEXT("test_free"), 0.2f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionComboTest, "ObsidianShrine.Action.BufferedInputChainsCombo", OS_TEST_FLAGS)

bool FOSActionComboTest::RunTest(const FString& Parameters)
{
	FOSActionDef First = OSTest::Action(TEXT("test_combo_1"), 1.f);
	First.ComboNext = TEXT("test_combo_2");
	First.Windows.Add(OSTest::Window(EOSWindowType::Cancel, 0.6f));
	OSTest::Register(First);
	OSTest::Register(OSTest::Action(TEXT("test_combo_2"), 1.f));
	OSTest::Register(OSTest::Action(TEXT("test_other"), 1.f));

	TMap<FName, FName> Slots;
	Slots.Add(OSSlots::Light, TEXT("test_combo_1"));
	Slots.Add(OSSlots::Dodge, TEXT("test_other"));

	{
		FOSTestRig Rig;
		Rig.Action->SetSlotMap(Slots);

		TestTrue(TEXT("first press starts the opener"), Rig.Action->RequestSlot(OSSlots::Light));
		Rig.Action->Advance(0.4f);
		TestFalse(TEXT("press before the cancel window is buffered"), Rig.Action->RequestSlot(OSSlots::Light));
		TestEqual(TEXT("still the opener"), Rig.Action->GetCurrent().Id, FName(TEXT("test_combo_1")));

		Rig.Action->Advance(0.25f); // 0.65: cancel window open, buffer is 0.25 s old
		TestEqual(TEXT("buffered press became the follow-up"), Rig.Action->GetCurrent().Id, FName(TEXT("test_combo_2")));
	}
	{
		FOSTestRig Rig;
		Rig.Action->SetSlotMap(Slots);

		Rig.Action->RequestSlot(OSSlots::Light);
		Rig.Action->Advance(0.1f);
		Rig.Action->RequestSlot(OSSlots::Light);
		Rig.Action->Advance(0.55f); // 0.65: the press is 0.55 s old, older than the 0.4 s buffer
		TestEqual(TEXT("a stale press is dropped"), Rig.Action->GetCurrent().Id, FName(TEXT("test_combo_1")));

		TestTrue(TEXT("another slot can cancel in the window"), Rig.Action->RequestSlot(OSSlots::Dodge));
		TestEqual(TEXT("cancelled into the other action"), Rig.Action->GetCurrent().Id, FName(TEXT("test_other")));
	}
	{
		FOSTestRig Rig;
		Rig.Action->SetSlotMap(Slots);

		Rig.Action->RequestSlot(OSSlots::Light);
		Rig.Action->Advance(1.1f);
		Rig.Action->RequestSlot(OSSlots::Light);
		TestEqual(TEXT("after the action ended the combo restarts"), Rig.Action->GetCurrent().Id, FName(TEXT("test_combo_1")));
		TestFalse(TEXT("unmapped slot does nothing"), Rig.Action->StartSlot(OSSlots::Heavy, true));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionHeldTest, "ObsidianShrine.Action.HeldActionAndInterrupts", OS_TEST_FLAGS)

bool FOSActionHeldTest::RunTest(const FString& Parameters)
{
	FOSActionDef Guard = OSTest::Action(TEXT("test_guard"), 0.f);
	Guard.Windows.Add(OSTest::Window(EOSWindowType::PerfectGuard, 0.f, 0.2f));
	Guard.Windows.Add(OSTest::Window(EOSWindowType::Guard, 0.f));
	OSTest::Register(Guard);

	FOSTestRig Rig;
	TMap<FName, FName> Slots;
	Slots.Add(OSSlots::Guard, TEXT("test_guard"));
	Rig.Action->SetSlotMap(Slots);

	Rig.Action->RequestSlot(OSSlots::Guard);
	TestTrue(TEXT("perfect guard on the first frame"), Rig.Action->IsWindowActive(EOSWindowType::PerfectGuard));
	TestTrue(TEXT("guard on the first frame"), Rig.Action->IsWindowActive(EOSWindowType::Guard));

	Rig.Action->Advance(30.f);
	TestTrue(TEXT("held action never times out"), Rig.Action->IsBusy());
	TestFalse(TEXT("perfect guard expired"), Rig.Action->IsWindowActive(EOSWindowType::PerfectGuard));
	TestTrue(TEXT("still guarding"), Rig.Action->IsWindowActive(EOSWindowType::Guard));

	Rig.Action->StopSlot(OSSlots::Light);
	TestTrue(TEXT("releasing another slot changes nothing"), Rig.Action->IsBusy());
	Rig.Action->StopSlot(OSSlots::Guard);
	TestFalse(TEXT("released"), Rig.Action->IsBusy());

	bool bInterrupted = false;
	Rig.Action->OnActionEnded.AddLambda([&bInterrupted](const FOSActionDef&, bool bWasInterrupted) { bInterrupted = bWasInterrupted; });
	Rig.Action->RequestSlot(OSSlots::Guard);
	TestTrue(TEXT("forced action interrupts"), Rig.Action->StartActionDef(OSTest::Action(TEXT("test_stagger"), 0.5f), NAME_None, true));
	TestTrue(TEXT("the guard was reported as interrupted"), bInterrupted);
	TestFalse(TEXT("guard window closed by the interrupt"), Rig.Action->IsWindowActive(EOSWindowType::Guard));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSActionAutoNextTest, "ObsidianShrine.Action.AutoNextChains", OS_TEST_FLAGS)

bool FOSActionAutoNextTest::RunTest(const FString& Parameters)
{
	FOSActionDef First = OSTest::Action(TEXT("test_string_1"), 0.5f);
	First.AutoNext = TEXT("test_string_2");
	First.MoveDistance = 100.f;
	First.MoveStart = 0.1f;
	First.MoveEnd = 0.3f;
	OSTest::Register(First);
	OSTest::Register(OSTest::Action(TEXT("test_string_2"), 0.5f));

	FOSTestRig Rig;
	Rig.Action->StartAction(TEXT("test_string_1"));
	TestEqual(TEXT("no displacement before MoveStart"), Rig.Action->GetMoveSpeedNow(), 0.f);
	Rig.Action->Advance(0.2f);
	TestEqual(TEXT("displacement speed = distance / time"), Rig.Action->GetMoveSpeedNow(), 500.f);
	Rig.Action->Advance(0.4f);
	TestEqual(TEXT("chained into the next action"), Rig.Action->GetCurrent().Id, FName(TEXT("test_string_2")));
	TestTrue(TEXT("still busy"), Rig.Action->IsBusy());
	Rig.Action->Advance(0.6f);
	TestFalse(TEXT("the string ends"), Rig.Action->IsBusy());
	return true;
}

// ---------------------------------------------------------------------------------------------- Damage

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCombatResolveTest, "ObsidianShrine.Combat.ResolveHitRules", OS_TEST_FLAGS)

bool FOSCombatResolveTest::RunTest(const FString& Parameters)
{
	FOSHitInfo Hit;
	Hit.Damage = 40.f;
	Hit.PoiseDamage = 25.f;

	FOSDefenseState Open;
	Open.Stamina = 100.f;
	FOSHitResult Result = OSCombat::ResolveHit(Hit, Open);
	TestTrue(TEXT("open: hit"), Result.Outcome == EOSHitOutcome::Hit);
	TestEqual(TEXT("open: full damage"), Result.HealthDamage, 40.f);
	TestEqual(TEXT("open: poise damage"), Result.PoiseDamage, 25.f);

	FOSDefenseState Armoured = Open;
	Armoured.bHyperArmor = true;
	Result = OSCombat::ResolveHit(Hit, Armoured);
	TestEqual(TEXT("hyper armour: full damage"), Result.HealthDamage, 40.f);
	TestEqual(TEXT("hyper armour: no poise damage"), Result.PoiseDamage, 0.f);

	FOSDefenseState Rolling = Open;
	Rolling.bInvulnerable = true;
	Rolling.bGuarding = true;
	TestTrue(TEXT("i-frames beat everything"), OSCombat::ResolveHit(Hit, Rolling).Outcome == EOSHitOutcome::Ignored);

	FOSDefenseState Guarding = Open;
	Guarding.bGuarding = true;
	Guarding.GuardDamageReduction = 0.75f;
	Guarding.GuardStaminaScale = 0.5f;
	Result = OSCombat::ResolveHit(Hit, Guarding);
	TestTrue(TEXT("guard: guarded"), Result.Outcome == EOSHitOutcome::Guarded);
	TestEqual(TEXT("guard: chip damage"), Result.HealthDamage, 10.f);
	TestEqual(TEXT("guard: stamina damage"), Result.StaminaDamage, 20.f);
	TestEqual(TEXT("guard: no poise damage"), Result.PoiseDamage, 0.f);

	FOSDefenseState Tired = Guarding;
	Tired.Stamina = 15.f;
	TestTrue(TEXT("guard with too little stamina breaks"), OSCombat::ResolveHit(Hit, Tired).Outcome == EOSHitOutcome::GuardBroken);

	FOSDefenseState Backstabbed = Guarding;
	Backstabbed.bFacingAttacker = false;
	Backstabbed.bPerfectGuard = true;
	TestTrue(TEXT("guard does not cover the back"), OSCombat::ResolveHit(Hit, Backstabbed).Outcome == EOSHitOutcome::Hit);

	FOSDefenseState Parrying = Guarding;
	Parrying.bPerfectGuard = true;
	Result = OSCombat::ResolveHit(Hit, Parrying);
	TestTrue(TEXT("perfect guard"), Result.Outcome == EOSHitOutcome::PerfectGuarded);
	TestEqual(TEXT("perfect guard: no damage"), Result.HealthDamage + Result.StaminaDamage, 0.f);

	Hit.bUnblockable = true;
	TestTrue(TEXT("unblockable goes through guards"), OSCombat::ResolveHit(Hit, Parrying).Outcome == EOSHitOutcome::Hit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCombatGuardArcTest, "ObsidianShrine.Combat.GuardArcFollowsWeaponAngle", OS_TEST_FLAGS)

bool FOSCombatGuardArcTest::RunTest(const FString& Parameters)
{
	const FVector Forward = FVector::ForwardVector;
	const FVector Front(100.f, 0.f, 0.f);
	const FVector FrontSide(100.f, 100.f, 0.f);    // 45 degrees off
	const FVector Side(0.f, 100.f, 0.f);           // 90 degrees off
	const FVector BackSide(-100.f, 100.f, 0.f);    // 135 degrees off
	const FVector Back(-100.f, 0.f, 0.f);

	TestTrue(TEXT("180: front"), OSCombat::IsInGuardArc(Forward, Front, 180.f));
	TestTrue(TEXT("180: 45 degrees off"), OSCombat::IsInGuardArc(Forward, FrontSide, 180.f));
	TestFalse(TEXT("180: behind the shoulder"), OSCombat::IsInGuardArc(Forward, BackSide, 180.f));
	TestFalse(TEXT("180: back"), OSCombat::IsInGuardArc(Forward, Back, 180.f));

	TestTrue(TEXT("60: front"), OSCombat::IsInGuardArc(Forward, Front, 60.f));
	TestFalse(TEXT("60: 45 degrees off is outside a 30 degree half-angle"), OSCombat::IsInGuardArc(Forward, FrontSide, 60.f));

	TestTrue(TEXT("270: behind the shoulder is covered"), OSCombat::IsInGuardArc(Forward, BackSide, 270.f));
	TestFalse(TEXT("270: straight back is not"), OSCombat::IsInGuardArc(Forward, Back, 270.f));

	TestTrue(TEXT("360: all around"), OSCombat::IsInGuardArc(Forward, Back, 360.f));
	TestFalse(TEXT("0: guards nothing"), OSCombat::IsInGuardArc(Forward, Front, 0.f));

	TestTrue(TEXT("height does not matter"), OSCombat::IsInGuardArc(Forward, FVector(100.f, 0.f, 500.f), 60.f));
	TestTrue(TEXT("an attacker standing on the defender is covered"), OSCombat::IsInGuardArc(Forward, FVector::ZeroVector, 60.f));
	TestTrue(TEXT("exactly on the edge of 180 counts as covered"), OSCombat::IsInGuardArc(Forward, Side, 180.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSStatsUpdateTest, "ObsidianShrine.Stats.UpdateFromDefKeepsState", OS_TEST_FLAGS)

bool FOSStatsUpdateTest::RunTest(const FString& Parameters)
{
	FOSTestRig Rig(100.f, 100.f, 50.f);
	Rig.Stats->ApplyDamage(40.f);
	Rig.Stats->SpendStamina(30.f);

	FOSCharacterDef Bigger;
	Bigger.MaxHealth = 200.f;
	Bigger.MaxStamina = 150.f;
	Bigger.MaxPoise = 80.f;
	Rig.Stats->UpdateFromDef(Bigger);
	TestEqual(TEXT("limit raised"), Rig.Stats->GetMaxHealth(), 200.f);
	TestEqual(TEXT("health untouched"), Rig.Stats->GetHealth(), 60.f);
	TestEqual(TEXT("stamina untouched"), Rig.Stats->GetStamina(), 70.f);

	FOSCharacterDef Smaller;
	Smaller.MaxHealth = 50.f;
	Smaller.MaxStamina = 40.f;
	Smaller.MaxPoise = 0.f;
	Rig.Stats->UpdateFromDef(Smaller);
	TestEqual(TEXT("health clamped to the new limit"), Rig.Stats->GetHealth(), 50.f);
	TestEqual(TEXT("stamina clamped to the new limit"), Rig.Stats->GetStamina(), 40.f);
	TestEqual(TEXT("poise clamped to the new limit"), Rig.Stats->GetPoise(), 0.f);
	TestFalse(TEXT("a smaller limit never kills"), Rig.Stats->IsDead());

	Rig.Stats->Kill();
	Rig.Stats->UpdateFromDef(Bigger);
	TestTrue(TEXT("the dead stay dead"), Rig.Stats->IsDead());
	TestEqual(TEXT("with no health"), Rig.Stats->GetHealth(), 0.f);

	Rig.Stats->InitFromDef(Bigger);
	TestFalse(TEXT("InitFromDef starts fresh"), Rig.Stats->IsDead());
	TestEqual(TEXT("at full health"), Rig.Stats->GetHealth(), 200.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSCombatReceiveTest, "ObsidianShrine.Combat.ReceiveHitDrivesReactions", OS_TEST_FLAGS)

bool FOSCombatReceiveTest::RunTest(const FString& Parameters)
{
	OSTest::Register(OSTest::Action(TEXT("test_react_stagger"), 0.5f));
	OSTest::Register(OSTest::Action(TEXT("test_react_break"), 1.f));
	FOSActionDef Guard = OSTest::Action(TEXT("test_guard_plain"), 0.f);
	Guard.Windows.Add(OSTest::Window(EOSWindowType::Guard, 0.f));
	OSTest::Register(Guard);
	FOSActionDef Roll = OSTest::Action(TEXT("test_roll"), 1.f);
	Roll.Windows.Add(OSTest::Window(EOSWindowType::Invulnerable, 0.f, 0.5f));

	TMap<FName, FName> Slots;
	Slots.Add(OSSlots::Stagger, TEXT("test_react_stagger"));
	Slots.Add(OSSlots::GuardBreak, TEXT("test_react_break"));
	Slots.Add(OSSlots::Guard, TEXT("test_guard_plain"));

	FOSWeaponDef Shield;
	Shield.GuardDamageReduction = 1.f;
	Shield.GuardStaminaScale = 1.f;

	FOSHitInfo Hit;
	Hit.Damage = 30.f;
	Hit.PoiseDamage = 30.f;

	{
		FOSTestRig Rig(100.f, 100.f, 50.f);
		Rig.Action->SetSlotMap(Slots);

		TestTrue(TEXT("first hit: poise holds"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Hit);
		TestFalse(TEXT("no reaction yet"), Rig.Action->IsBusy());
		TestTrue(TEXT("second hit: staggered"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Staggered);
		TestEqual(TEXT("stagger action plays"), Rig.Action->GetCurrent().Id, FName(TEXT("test_react_stagger")));
		TestEqual(TEXT("health after two hits"), Rig.Stats->GetHealth(), 40.f);

		Rig.Action->StartActionDef(Roll, NAME_None, true);
		TestTrue(TEXT("i-frames ignore the hit"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Ignored);
		TestEqual(TEXT("health untouched by the ignored hit"), Rig.Stats->GetHealth(), 40.f);

		Rig.Action->Advance(0.6f);
		Rig.Combat->ReceiveHit(Hit);
		TestTrue(TEXT("lethal hit: killed"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Killed);
		TestTrue(TEXT("hits on a corpse are ignored"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Ignored);
	}
	{
		FOSTestRig Rig(100.f, 50.f, 0.f);
		Rig.Action->SetSlotMap(Slots);
		Rig.Combat->SetWeapon(Shield);
		Rig.Action->RequestSlot(OSSlots::Guard);

		TestTrue(TEXT("guarded"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::Guarded);
		TestEqual(TEXT("guard costs stamina"), Rig.Stats->GetStamina(), 20.f);
		TestEqual(TEXT("full reduction: no damage"), Rig.Stats->GetHealth(), 100.f);
		TestTrue(TEXT("still guarding"), Rig.Action->IsWindowActive(EOSWindowType::Guard));

		TestTrue(TEXT("guard broken when stamina runs out"), Rig.Combat->ReceiveHit(Hit).Outcome == EOSHitOutcome::GuardBroken);
		TestEqual(TEXT("guard break action plays"), Rig.Action->GetCurrent().Id, FName(TEXT("test_react_break")));
		TestEqual(TEXT("stamina emptied"), Rig.Stats->GetStamina(), 0.f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
