/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2026

   AefUtility - Blueprint Function Library

   Resolves the active AefConfig.ini: <Project>/Config/AefConfig.ini by default,
   or the file given with the command-line argument -aefConfig=<path>.
  ========================================================================*/

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AefUtilityBPLibrary.generated.h"

/** Where the active AefConfig.ini path came from. */
UENUM(BlueprintType)
enum class EAefConfigSource : uint8
{
	/** No aefConfig= argument; <Project>/Config/AefConfig.ini is used. */
	Default,
	/** aefConfig= was given and the file exists. */
	Override,
	/** aefConfig= was given but the file does not exist; the default path is used instead. */
	OverrideMissingFallback,
	/** No aefConfig= argument and the default file does not exist either. */
	DefaultMissing
};

/** Result of resolving the active AefConfig.ini. */
struct FAefConfigResolution
{
	/** Normalized path of the file to read (the default path on fallback). */
	FString Path;
	/** aefConfig= value resolved to a full path; empty when no override was given. */
	FString RequestedOverride;
	EAefConfigSource Source = EAefConfigSource::Default;
};

UCLASS()
class AEFUTILITY_API UAefUtilityBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	// GetConfigFilePath: path of the active AefConfig.ini. The result is cached and only
	// resolved again (and logged once) when the process command line changes.
	UFUNCTION(BlueprintPure, Category = "AEF|Utility")
	static FString GetConfigFilePath();

	// GetConfigSource: how the active path was chosen (see EAefConfigSource).
	UFUNCTION(BlueprintPure, Category = "AEF|Utility")
	static EAefConfigSource GetConfigSource();

	// GetRequestedOverridePath: full path requested via aefConfig=, empty when none was given.
	UFUNCTION(BlueprintPure, Category = "AEF|Utility")
	static FString GetRequestedOverridePath();

	// IsConfigWritable: false when aefConfig= points to a missing file. Writing would then
	// modify the default file instead of the requested one. OutReason explains why.
	static bool IsConfigWritable(FString& OutReason);

	// ResolveConfigFilePath: same resolution as GetConfigFilePath, but reads the
	// aefConfig= argument from the given command line instead of FCommandLine::Get().
	// Relative paths are resolved against FPaths::ProjectDir(). Pass nullptr to
	// use the real command line. Not cached and does not log.
	static FString ResolveConfigFilePath(const TCHAR* CommandLineOverride);

	// ResolveConfig: like ResolveConfigFilePath, but also returns the source and the requested override.
	static FAefConfigResolution ResolveConfig(const TCHAR* CommandLineOverride);

private:

	static FAefConfigResolution GetActiveResolution();
};
