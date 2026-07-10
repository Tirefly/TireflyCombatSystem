// Copyright Tirefly. All Rights Reserved.

using UnrealBuildTool;

public class TireflyCombatSystemEditor : ModuleRules
{
	public TireflyCombatSystemEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AssetDefinition",
				"EditorSubsystem",
				"UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"DeveloperSettings",
				"GameplayStateTreeModule",
				"MainFrame",
				"Slate",
				"SlateCore",
				"StateTreeEditorModule",
				"ToolMenus",
				"TireflyCombatSystem"
			}
		);
	}
}
