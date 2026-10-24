using UnrealBuildTool;

public class Rogue10mEditor : ModuleRules
{
	public Rogue10mEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "Rogue10m", "UnrealEd", "EditorFramework",
			"Slate", "SlateCore", "InputCore", "ToolMenus", "LevelEditor", "ContentBrowser",
			"AssetRegistry", "Landscape"
		});
	}
}
