// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EditorLiveStreaming : ModuleRules
	{
		public EditorLiveStreaming( ReadOnlyTargetRules Target ) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] 
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"UnrealEd",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] 
				{
					"Settings",
					"MainFrame",
					"GameLiveStreaming"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] 
				{
					"Settings",
					"MainFrame"
				}
			);
		}
	}
}
