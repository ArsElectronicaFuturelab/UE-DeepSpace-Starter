/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Module Header

   Main plugin module for the AefPharus laser tracking system.
   Provides GameInstance subsystem for multi-instance tracker management.
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Log category for AefPharus plugin
 * Usage: UE_LOG(LogAefPharus, Log, TEXT("Message"));
 */
DECLARE_LOG_CATEGORY_EXTERN(LogAefPharus, Log, All);

/**
 * Aef Pharus Module
 *
 * Plugin module that provides Pharus laser tracking integration.
 * Features:
 * - GameInstance subsystem for automatic initialization
 * - Multi-instance support (Floor + Wall tracking)
 * - INI-based configuration
 * - Blueprint-accessible API
 */
class FAefPharusModule : public IModuleInterface
{
public:
	/**
	 * Called when the module is loaded into memory
	 */
	virtual void StartupModule() override;

	/**
	 * Called when the module is unloaded from memory
	 */
	virtual void ShutdownModule() override;
};
