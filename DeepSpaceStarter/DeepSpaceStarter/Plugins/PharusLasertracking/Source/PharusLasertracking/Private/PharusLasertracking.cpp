/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2020
  ========================================================================*/

#include "PharusLasertracking.h"

#define LOCTEXT_NAMESPACE "FPharusLasertrackingModule"

DEFINE_LOG_CATEGORY(AELog);

void FPharusLasertrackingModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	///UE_LOG(AELog, Warning, TEXT("FPharusLasertrackingModule :: StartupModule BUILD %s"), *PharusLasertrackingBuildNumber); // ID DEBUG ONLY
}

void FPharusLasertrackingModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPharusLasertrackingModule, PharusLasertracking)