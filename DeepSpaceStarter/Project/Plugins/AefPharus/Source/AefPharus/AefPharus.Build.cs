/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Build Configuration

   Unreal Build Tool configuration for AefPharus plugin.
  ========================================================================*/

using System.IO;
using UnrealBuildTool;

public class AefPharus : ModuleRules
{
	public AefPharus(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true; // Enable C++ exceptions for try/catch blocks
		bUseUnity = false; // Disable Unity Build to ensure flags are applied correctly

		// Public include paths for ThirdParty networking code
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "ThirdParty")
			}
		);

		// Public dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",           // For UDP networking (UDPManager)
				"Networking",        // For network utilities (InetAddr)
				"DisplayCluster"     // For nDisplay cluster API
			}
		);

		// Private dependencies
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AefUtility"         // Resolves the active AefConfig.ini (default or -dsConfig= override)
			}
		);
	}
}
