// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PharusLasertracking : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }
    
    public PharusLasertracking(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

        // Add Pharus Headers/Include folders
        // assuming "<Project Folder>/Plugins/PharusLasertracking/Source/PharusLasertracking/...");
        PublicIncludePaths.Add(Path.Combine(ModulePath, "ThirdParty"));

        PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core", "CoreUObject", "Engine", "InputCore", 
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Engine", "Projects",
			
				// ... add private dependencies that you statically link with here ...	
			}
			);

        // Add Pharus required Build Libraries
        PublicAdditionalLibraries.Add("ws2_32.lib"); // ID NOTE: required for UDPManager.cpp
        // PublicAdditionalLibraries.Add("C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/x64/ws2_32.lib"); // ID NOTE: required for UDPManager.cpp

        PublicDefinitions.Add("NO_TRACELOG=1"); // ID NOTE: skips compiling of all logging bits in 'UDPManager.cpp'
        //PublicDefinitions.Add("VERBOSE_AELOG=1");

        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
