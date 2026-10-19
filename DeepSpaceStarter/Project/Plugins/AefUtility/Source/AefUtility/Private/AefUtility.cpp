/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2026

   AefUtility - Module Implementation
  ========================================================================*/

#include "AefUtility.h"
#include "AefUtilityBPLibrary.h"

//--------------------------------------------------------------------------------
// Log Category
//--------------------------------------------------------------------------------

DEFINE_LOG_CATEGORY(LogAefUtility);

//--------------------------------------------------------------------------------
// Module Lifecycle
//--------------------------------------------------------------------------------

void FAefUtilityModule::StartupModule()
{
	// Resolve the active AefConfig.ini once at startup so the log always shows which file is
	// used (default or -aefConfig= override), in the editor as well as in packaged builds.
	// The result is cached; later calls do not log again unless the command line changes.
	UAefUtilityBPLibrary::GetConfigFilePath();
}

void FAefUtilityModule::ShutdownModule()
{
}

//--------------------------------------------------------------------------------
// Module Registration
//--------------------------------------------------------------------------------

IMPLEMENT_MODULE(FAefUtilityModule, AefUtility)
