/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2026

   AefUtility - Blueprint Function Library Implementation

   Resolves and caches the path of the active AefConfig.ini.
  ========================================================================*/

#include "AefUtilityBPLibrary.h"
#include "AefUtility.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace AefUtilityConfigCache
{
	// The config is only read at startup and changes require a restart, so the path is
	// resolved once. It is keyed by the command line so a changed command line is picked up.
	FCriticalSection Lock;
	FString CachedCommandLine;
	FAefConfigResolution Resolution;
	bool bValid = false;

	void LogResolution(const FAefConfigResolution& Result)
	{
		switch (Result.Source)
		{
		case EAefConfigSource::Override:
			UE_LOG(LogAefUtility, Display, TEXT("Using config override: %s"), *Result.Path);
			break;
		case EAefConfigSource::OverrideMissingFallback:
			UE_LOG(LogAefUtility, Error, TEXT("dsConfig override '%s' not found, falling back to default config: %s"),
				*Result.RequestedOverride, *Result.Path);
			break;
		case EAefConfigSource::DefaultMissing:
			UE_LOG(LogAefUtility, Warning, TEXT("No AefConfig.ini found at %s, plugins use their built-in defaults"), *Result.Path);
			break;
		default:
			UE_LOG(LogAefUtility, Log, TEXT("Using default config: %s"), *Result.Path);
			break;
		}
	}
}

UAefUtilityBPLibrary::UAefUtilityBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FString UAefUtilityBPLibrary::GetConfigFilePath()
{
	return GetActiveResolution().Path;
}

EAefConfigSource UAefUtilityBPLibrary::GetConfigSource()
{
	return GetActiveResolution().Source;
}

FString UAefUtilityBPLibrary::GetRequestedOverridePath()
{
	return GetActiveResolution().RequestedOverride;
}

bool UAefUtilityBPLibrary::IsConfigWritable(FString& OutReason)
{
	const FAefConfigResolution Resolution = GetActiveResolution();
	if (Resolution.Source == EAefConfigSource::OverrideMissingFallback)
	{
		OutReason = FString::Printf(TEXT("dsConfig override '%s' does not exist. Writing is blocked so the default config '%s' is not modified by mistake."),
			*Resolution.RequestedOverride, *Resolution.Path);
		return false;
	}

	OutReason.Empty();
	return true;
}

FString UAefUtilityBPLibrary::ResolveConfigFilePath(const TCHAR* CommandLineOverride)
{
	return ResolveConfig(CommandLineOverride).Path;
}

FAefConfigResolution UAefUtilityBPLibrary::ResolveConfig(const TCHAR* CommandLineOverride)
{
	const FString CommandLine = CommandLineOverride ? FString(CommandLineOverride) : FCommandLine::Get();
	const FString DefaultPath = FConfigCacheIni::NormalizeConfigIniPath(FPaths::ProjectConfigDir() / TEXT("AefConfig.ini"));

	FAefConfigResolution Result;
	Result.Path = DefaultPath;
	Result.Source = FPaths::FileExists(DefaultPath) ? EAefConfigSource::Default : EAefConfigSource::DefaultMissing;

	FString CustomConfigPath;
	if (FParse::Value(*CommandLine, TEXT("dsConfig="), CustomConfigPath))
	{
		if (CustomConfigPath.IsEmpty())
		{
			// An empty dsConfig= is a misconfiguration, not a request for the default.
			Result.Source = EAefConfigSource::OverrideMissingFallback;
			return Result;
		}

		if (FPaths::IsRelative(CustomConfigPath))
		{
			CustomConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), CustomConfigPath);
		}

		CustomConfigPath = FConfigCacheIni::NormalizeConfigIniPath(CustomConfigPath);
		Result.RequestedOverride = CustomConfigPath;

		if (FPaths::FileExists(CustomConfigPath))
		{
			Result.Path = CustomConfigPath;
			Result.Source = EAefConfigSource::Override;
		}
		else
		{
			Result.Source = EAefConfigSource::OverrideMissingFallback;
		}
	}

	return Result;
}

FAefConfigResolution UAefUtilityBPLibrary::GetActiveResolution()
{
	using namespace AefUtilityConfigCache;

	FScopeLock ScopeLock(&Lock);

	const TCHAR* CurrentCommandLine = FCommandLine::Get();
	if (!bValid || !CachedCommandLine.Equals(CurrentCommandLine, ESearchCase::CaseSensitive))
	{
		CachedCommandLine = CurrentCommandLine;
		Resolution = ResolveConfig(CurrentCommandLine);
		bValid = true;
		LogResolution(Resolution);
	}

	return Resolution;
}
