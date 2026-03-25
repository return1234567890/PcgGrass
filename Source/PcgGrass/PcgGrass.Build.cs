// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PcgGrass : ModuleRules
{
	public PcgGrass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MeshConversion",
			"MeshDescription",
			"StaticMeshDescription",
		});
	}
}
