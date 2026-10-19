/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2026

   AefUtility - Build Configuration

   Unreal Build Tool configuration for AefUtility plugin.
   Stages Config/AefConfig.ini into packaged builds.
  ========================================================================*/

using System.IO;
using UnrealBuildTool;

public class AefUtility : ModuleRules
{
	public AefUtility(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public dependencies
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		// Private dependencies
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
		);

		// Stage the project's AefConfig.ini as a loose, editable file so packaged builds find it at
		// FPaths::ProjectConfigDir(). UAT does not stage it by itself because it is not an engine config file.
		if (Target.Type != TargetType.Editor && Target.ProjectFile != null)
		{
			string AefConfigPath = Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "AefConfig.ini");
			if (File.Exists(AefConfigPath))
			{
				RuntimeDependencies.Add("$(ProjectDir)/Config/AefConfig.ini", StagedFileType.NonUFS);
			}
		}
	}
}
