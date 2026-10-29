#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/OSGameMode.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeRequiredModulesTest, "ObsidianShrine.Smoke.RequiredModulesLoaded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeRequiredModulesTest::RunTest(const FString& Parameters)
{
	const FModuleManager& Modules = FModuleManager::Get();
	TestTrue(TEXT("ObsidianShrine module loaded"), Modules.IsModuleLoaded(TEXT("ObsidianShrine")));
	TestTrue(TEXT("GameplayAbilities module loaded"), Modules.IsModuleLoaded(TEXT("GameplayAbilities")));
	TestTrue(TEXT("EnhancedInput module loaded"), Modules.IsModuleLoaded(TEXT("EnhancedInput")));
	TestTrue(TEXT("MotionWarping module loaded"), Modules.IsModuleLoaded(TEXT("MotionWarping")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeDefaultGameModeTest, "ObsidianShrine.Smoke.DefaultGameModeIsOSGameMode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeDefaultGameModeTest::RunTest(const FString& Parameters)
{
	const FString Configured = UGameMapsSettings::GetGlobalDefaultGameMode();
	TestEqual(TEXT("GlobalDefaultGameMode"), Configured, AOSGameMode::StaticClass()->GetPathName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOSSmokeSandboxMapTest, "ObsidianShrine.Smoke.SandboxMapExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOSSmokeSandboxMapTest::RunTest(const FString& Parameters)
{
	// Produced by Content/Python/build_content.py; fails if the content pipeline has not been run.
	TestTrue(TEXT("/Game/Maps/L_Sandbox exists"), FPackageName::DoesPackageExist(TEXT("/Game/Maps/L_Sandbox")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
