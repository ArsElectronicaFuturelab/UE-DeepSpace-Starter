/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Module Implementation

   Main plugin module implementation.
   The GameInstance subsystem handles automatic initialization.
  ========================================================================*/

#include "AefPharus.h"

//--------------------------------------------------------------------------------
// Log Category
//--------------------------------------------------------------------------------

DEFINE_LOG_CATEGORY(LogAefPharus);

//--------------------------------------------------------------------------------
// Module Lifecycle
//--------------------------------------------------------------------------------

void FAefPharusModule::StartupModule()
{
	UE_LOG(LogAefPharus, Log, TEXT("AefPharus module starting..."));

	// Note: The UAefPharusSubsystem will automatically initialize
	// when the GameInstance is created, loading configuration from
	// Config/AefConfig.ini
}

void FAefPharusModule::ShutdownModule()
{
	UE_LOG(LogAefPharus, Log, TEXT("AefPharus module shutting down..."));

	// Note: The UAefPharusSubsystem will automatically deinitialize
	// when the GameInstance is destroyed
}

//--------------------------------------------------------------------------------
// Module Registration
//--------------------------------------------------------------------------------

IMPLEMENT_MODULE(FAefPharusModule, AefPharus)
