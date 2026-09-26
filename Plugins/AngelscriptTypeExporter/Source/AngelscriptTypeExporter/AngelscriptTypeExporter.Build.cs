using UnrealBuildTool;

public class AngelscriptTypeExporter : ModuleRules
{
	public AngelscriptTypeExporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// AngelscriptCode publicly exposes the angelscript ThirdParty include
			// paths (both the public angelscript.h and the internal asC classes),
			// which is what the debug server itself uses to enumerate types.
			"AngelscriptCode",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"SlateCore",
			"Projects",
			"ToolMenus",
		});
	}
}
