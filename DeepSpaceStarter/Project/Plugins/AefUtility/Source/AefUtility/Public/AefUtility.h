/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2026

   AefUtility - Module Interface

   Shared utilities for the Ars Electronica Futurelab plugins.
  ========================================================================*/

#pragma once

#include "Modules/ModuleManager.h"

AEFUTILITY_API DECLARE_LOG_CATEGORY_EXTERN(LogAefUtility, Log, All);

class FAefUtilityModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
