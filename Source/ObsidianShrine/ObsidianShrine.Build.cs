using UnrealBuildTool;

public class ObsidianShrine : ModuleRules
{
	public ObsidianShrine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"MotionWarping"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EngineSettings"
		});
	}
}
