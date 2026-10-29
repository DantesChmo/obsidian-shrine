using UnrealBuildTool;

public class ObsidianShrineTarget : TargetRules
{
	public ObsidianShrineTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ObsidianShrine");
	}
}
