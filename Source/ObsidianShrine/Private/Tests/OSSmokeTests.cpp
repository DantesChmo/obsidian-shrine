#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/OSGameMode.h"
#include "Data/OSDataSubsystem.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeRequiredModulesTest, "ObsidianShrine.Smoke.RequiredModulesLoaded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeRequiredModulesTest::RunTest(const FString& Parameters)
{
	const FModuleManager& Modules = FModuleManager::Get();
	TestTrue(TEXT("ObsidianShrine module loaded"), Modules.IsModuleLoaded(TEXT("ObsidianShrine")));
	TestTrue(TEXT("EnhancedInput module loaded"), Modules.IsModuleLoaded(TEXT("EnhancedInput")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeDefaultGameModeTest, "ObsidianShrine.Smoke.DefaultGameModeIsOSGameMode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeDefaultGameModeTest::RunTest(const FString& Parameters)
{
	const FString Configured = UGameMapsSettings::GetGlobalDefaultGameMode();
	TestEqual(TEXT("GlobalDefaultGameMode"), Configured, AOSGameMode::StaticClass()->GetPathName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeStartMapTest, "ObsidianShrine.Smoke.StartMapExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeStartMapTest::RunTest(const FString& Parameters)
{
	// StartMap comes from Game/game.yaml; the data compiler copies it into DefaultEngine.ini.
	const FString StartMap = UOSDataSubsystem::Settings().StartMap;
	TestTrue(FString::Printf(TEXT("the engine starts on StartMap (%s), got %s"), *StartMap, *UGameMapsSettings::GetGameDefaultMap()),
		UGameMapsSettings::GetGameDefaultMap().StartsWith(StartMap));

	// Produced by Content/Python/build_content.py; fails if the content pipeline has not been run.
	TestTrue(FString::Printf(TEXT("%s exists"), *StartMap), FPackageName::DoesPackageExist(StartMap));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
