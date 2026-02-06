/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - GameInstance Subsystem Implementation

   Loads configuration from AefConfig.ini and manages tracker instances.
  ========================================================================*/

#include "AefPharusSubsystem.h"
#include "AefPharus.h"
#include "AefPharusActorInterface.h"
#include "AefPharusRootOriginActor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

//--------------------------------------------------------------------------------
// USubsystem Interface
//--------------------------------------------------------------------------------

void UAefPharusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogAefPharus, Log, TEXT("AefPharus Subsystem initializing..."));

	// Load configuration
	LoadConfigurationFromIni();

	// Read AutoStartSystem flag from config (default: true for backward compatibility)
	FString ConfigPath = GetConfigFilePath();
	bAutoStartSystem = true;  // Default to auto-start
	if (FPaths::FileExists(ConfigPath))
	{
		GConfig->GetBool(TEXT("PharusSubsystem"), TEXT("AutoStartSystem"), bAutoStartSystem, ConfigPath);
	}

	// Delay auto-initialization to ensure network is ready (especially for nDisplay)
	// This fixes the issue where UDP sockets bind too early before the network stack is fully initialized
	UWorld* World = GetWorld();
	if (World)
	{
		// Read delay from config (default: 0.5 seconds)
		float AutoCreateDelay = 0.5f;
		if (FPaths::FileExists(ConfigPath))
		{
			GConfig->GetFloat(TEXT("PharusSubsystem"), TEXT("AutoCreateDelay"), AutoCreateDelay, ConfigPath);
			// Clamp to reasonable range (0.1 - 10.0 seconds)
			AutoCreateDelay = FMath::Clamp(AutoCreateDelay, 0.1f, 10.0f);
		}
		
		// Use weak pointer to safely capture 'this' in lambda
		TWeakObjectPtr<UAefPharusSubsystem> WeakThis(this);
		
		World->GetTimerManager().SetTimer(
			DelayedInitTimerHandle,
			[WeakThis]()
			{
				// Check if subsystem is still valid
				if (UAefPharusSubsystem* StrongThis = WeakThis.Get())
				{
					if (!StrongThis->bDelayedInitExecuted)
					{
						// Only auto-start if enabled in config
						if (StrongThis->bAutoStartSystem)
						{
							UE_LOG(LogAefPharus, Log, TEXT("Executing delayed auto-initialization..."));
							StrongThis->CreateInstancesFromConfig();
							StrongThis->bDelayedInitExecuted = true;
							UE_LOG(LogAefPharus, Log, TEXT("AefPharus Subsystem initialized with %d instance(s)"), StrongThis->TrackerInstances.Num());
						}
						else
						{
							UE_LOG(LogAefPharus, Log, TEXT("Auto-start disabled - call StartPharusSystem() to manually start tracking"));
							StrongThis->bDelayedInitExecuted = true;
						}
					}
				}
				else
				{
					UE_LOG(LogAefPharus, Warning, TEXT("Delayed initialization aborted - Subsystem was destroyed"));
				}
			},
			AutoCreateDelay,
			false
		);
		
		UE_LOG(LogAefPharus, Log, TEXT("AefPharus Subsystem: Scheduled delayed initialization (%.1fs)"), AutoCreateDelay);
	}
	else
	{
		// Fallback: Initialize immediately if no world context (only if auto-start enabled)
		if (bAutoStartSystem)
		{
			UE_LOG(LogAefPharus, Warning, TEXT("No World context - initializing immediately"));
			CreateInstancesFromConfig();
			UE_LOG(LogAefPharus, Log, TEXT("AefPharus Subsystem initialized with %d instance(s)"), TrackerInstances.Num());
		}
		else
		{
			UE_LOG(LogAefPharus, Log, TEXT("Auto-start disabled - call StartPharusSystem() to manually start tracking"));
		}
	}

	bIsPharusDebug = false;
}

bool UAefPharusSubsystem::GetIsPharusDebug()
{
	return bIsPharusDebug;
}

void UAefPharusSubsystem::SetIsPharusDebug(bool bNewValue)
{
	bIsPharusDebug = bNewValue;
}

void UAefPharusSubsystem::Deinitialize()
{
	UE_LOG(LogAefPharus, Log, TEXT("AefPharus Subsystem deinitializing..."));

	// Clear delayed init timer if still pending
	UWorld* World = GetWorld();
	if (World && DelayedInitTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(DelayedInitTimerHandle);
	}

	// Shutdown all instances
	for (auto& Pair : TrackerInstances)
	{
		if (Pair.Value)
		{
			Pair.Value->Shutdown();
		}
	}
	TrackerInstances.Empty();

	Super::Deinitialize();
}

//--------------------------------------------------------------------------------
// Instance Management
//--------------------------------------------------------------------------------

FAefPharusCreateInstanceResult UAefPharusSubsystem::CreateTrackerInstance(const FAefPharusInstanceConfig& Config, TSubclassOf<AActor> SpawnClass)
{
	// Debug logging - detailed input parameters
	if (bIsPharusDebug)
	{
		UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance CALLED"));
		UE_LOG(LogAefPharus, Log, TEXT("  InstanceName: %s"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Log, TEXT("  bEnable: %s"), Config.bEnable ? TEXT("true") : TEXT("false"));
		UE_LOG(LogAefPharus, Log, TEXT("  BindNIC: '%s'"), *Config.BindNIC);
		UE_LOG(LogAefPharus, Log, TEXT("  UDPPort: %d"), Config.UDPPort);
		UE_LOG(LogAefPharus, Log, TEXT("  bIsMulticast: %s"), Config.bIsMulticast ? TEXT("true") : TEXT("false"));
		UE_LOG(LogAefPharus, Log, TEXT("  MulticastGroup: %s"), *Config.MulticastGroup);
		UE_LOG(LogAefPharus, Log, TEXT("  MappingMode: %s"), *UEnum::GetValueAsString(Config.MappingMode));
		UE_LOG(LogAefPharus, Log, TEXT("  SpawnClass: %s"), SpawnClass ? *SpawnClass->GetName() : TEXT("None"));
		UE_LOG(LogAefPharus, Log, TEXT("========================================"));
	}

	// Check if instance is enabled
	if (!Config.bEnable)
	{
		FString ErrorMsg = FString::Printf(TEXT("Instance '%s' is disabled (bEnable = false)"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Warning, TEXT("%s"), *ErrorMsg);
		FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::InstanceDisabled, ErrorMsg);
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		}
		return Result;
	}

	// Check if instance already exists
	if (TrackerInstances.Contains(Config.InstanceName))
	{
		FString ErrorMsg = FString::Printf(TEXT("Instance '%s' already exists"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Warning, TEXT("%s"), *ErrorMsg);
		FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::InstanceAlreadyExists, ErrorMsg);
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		}
		return Result;
	}

	// Validate SpawnClass implements interface (optional but recommended)
	if (SpawnClass)
	{
		bool bImplementsInterface = SpawnClass->ImplementsInterface(UAefPharusActorInterface::StaticClass());
		if (!bImplementsInterface)
		{
			FString ErrorMsg = FString::Printf(TEXT("SpawnClass '%s' does not implement IAefPharusActorInterface - track IDs will not be set"),
				*SpawnClass->GetName());
			UE_LOG(LogAefPharus, Warning, TEXT("%s"), *ErrorMsg);
			FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::SpawnClassMissingInterface, ErrorMsg);
			if (bIsPharusDebug)
			{
				UE_LOG(LogAefPharus, Log, TEXT("========================================"));
				UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
				UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			}
			return Result;
		}
	}

	// Create instance
	UAefPharusInstance* Instance = NewObject<UAefPharusInstance>(this);
	if (!Instance)
	{
		FString ErrorMsg = FString::Printf(TEXT("Failed to create UAefPharusInstance object for '%s'"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Error, TEXT("%s"), *ErrorMsg);
		FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::InstanceCreationFailed, ErrorMsg);
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		}
		return Result;
	}

	// Initialize instance
	UWorld* World = GetWorld();
	if (!World)
	{
		FString ErrorMsg = FString::Printf(TEXT("Cannot initialize instance '%s' - no valid World context"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Error, TEXT("%s"), *ErrorMsg);
		FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::NoValidWorld, ErrorMsg);
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		}
		return Result;
	}

	if (!Instance->Initialize(Config, World, SpawnClass))
	{
		FString ErrorMsg = FString::Printf(TEXT("Instance->Initialize() failed for '%s'"), *Config.InstanceName.ToString());
		UE_LOG(LogAefPharus, Error, TEXT("%s"), *ErrorMsg);
		FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::InitializationFailed, ErrorMsg);
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
			UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT: FAILED - %s"), *UEnum::GetValueAsString(Result.ErrorCode));
			UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		}
		return Result;
	}

	// Store instance
	TrackerInstances.Add(Config.InstanceName, Instance);

	// Compact success log
	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' created: %s:%d, Mode=%s, Pool=%s, SpawnClass=%s"),
		*Config.InstanceName.ToString(),
		*Config.BindNIC,
		Config.UDPPort,
		*UEnum::GetValueAsString(Config.MappingMode),
		Config.bUseActorPool ? TEXT("Yes") : TEXT("No"),
		SpawnClass ? *SpawnClass->GetName() : TEXT("None"));

	// Debug logging - detailed result
	FAefPharusCreateInstanceResult Result = FAefPharusCreateInstanceResult::MakeSuccess();
	if (bIsPharusDebug)
	{
		UE_LOG(LogAefPharus, Log, TEXT("========================================"));
		UE_LOG(LogAefPharus, Log, TEXT("CreateTrackerInstance RESULT"));
		UE_LOG(LogAefPharus, Log, TEXT("  bSuccess: %s"), Result.bSuccess ? TEXT("true") : TEXT("false"));
		UE_LOG(LogAefPharus, Log, TEXT("  ErrorCode: %s"), *UEnum::GetValueAsString(Result.ErrorCode));
		UE_LOG(LogAefPharus, Log, TEXT("  ErrorMessage: %s"), *Result.ErrorMessage);
		UE_LOG(LogAefPharus, Log, TEXT("========================================"));
	}
	return Result;
}

FAefPharusCreateInstanceResult UAefPharusSubsystem::CreateTrackerInstanceSimple(
	FName InstanceName,
	FString BindNIC,
	int32 UDPPort,
	TSubclassOf<AActor> SpawnClass,
	EAefPharusMappingMode MappingMode)
{
	// Validate SpawnClass
	if (!SpawnClass)
	{
		FString ErrorMsg = FString::Printf(TEXT("Cannot create instance '%s' - SpawnClass is null"), *InstanceName.ToString());
		UE_LOG(LogAefPharus, Error, TEXT("%s"), *ErrorMsg);
		return FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::SpawnClassIsNull, ErrorMsg);
	}

	// Verify interface implementation (strict check for Simple variant)
	if (!SpawnClass->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
	{
		FString ErrorMsg = FString::Printf(TEXT("Cannot create instance '%s' - SpawnClass '%s' does not implement IAefPharusActorInterface"),
			*InstanceName.ToString(), *SpawnClass->GetName());
		UE_LOG(LogAefPharus, Error, TEXT("%s"), *ErrorMsg);
		return FAefPharusCreateInstanceResult::MakeError(EAefPharusCreateInstanceError::SpawnClassMissingInterface, ErrorMsg);
	}

	// Create config with sensible defaults
	FAefPharusInstanceConfig Config;
	Config.InstanceName = InstanceName;
	Config.bEnable = true;
	Config.BindNIC = BindNIC;
	Config.UDPPort = UDPPort;
	Config.bIsMulticast = true;
	Config.MulticastGroup = TEXT("239.0.0.1");
	Config.MappingMode = MappingMode;

	// Simple mode defaults (Origin is now global via subsystem)
	Config.SimpleScale = FVector2D(100.0f, 100.0f);
	Config.FloorZ = 0.0f;

	// Performance defaults
	Config.bAutoDestroyOnTrackLost = true;

	// Debug defaults
	Config.bLogTrackerSpawned = true;
	Config.bLogTrackerRemoved = true;

	return CreateTrackerInstance(Config, SpawnClass);
}

UAefPharusInstance* UAefPharusSubsystem::GetTrackerInstance(FName InstanceName)
{
	UAefPharusInstance** InstancePtr = TrackerInstances.Find(InstanceName);
	return InstancePtr ? *InstancePtr : nullptr;
}

bool UAefPharusSubsystem::RemoveTrackerInstance(FName InstanceName)
{
	UAefPharusInstance** InstancePtr = TrackerInstances.Find(InstanceName);
	if (!InstancePtr)
	{
		return false;
	}

	// Shutdown instance
	if (*InstancePtr)
	{
		(*InstancePtr)->Shutdown();
	}

	TrackerInstances.Remove(InstanceName);

	UE_LOG(LogAefPharus, Log, TEXT("Removed tracker instance '%s'"), *InstanceName.ToString());
	return true;
}

TArray<FName> UAefPharusSubsystem::GetAllInstanceNames() const
{
	TArray<FName> Names;
	TrackerInstances.GetKeys(Names);
	return Names;
}

int32 UAefPharusSubsystem::GetInstanceCount() const
{
	return TrackerInstances.Num();
}

bool UAefPharusSubsystem::HasInstance(FName InstanceName) const
{
	return TrackerInstances.Contains(InstanceName);
}

bool UAefPharusSubsystem::IsTrackActive(FName InstanceName, int32 TrackID) const
{
	UAefPharusInstance* const* InstancePtr = TrackerInstances.Find(InstanceName);
	if (!InstancePtr || !*InstancePtr)
	{
		return false;
	}

	return (*InstancePtr)->IsTrackActive(TrackID);
}

void UAefPharusSubsystem::SetSpawnClassOverride(FName InstanceName, TSubclassOf<AActor> SpawnClass)
{
	if (TrackerInstances.Num() > 0)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("SetSpawnClassOverride called after StartPharusSystem() - override will not take effect for '%s'"), *InstanceName.ToString());
	}

	if (SpawnClass)
	{
		SpawnClassOverrides.Add(InstanceName, SpawnClass);
		UE_LOG(LogAefPharus, Log, TEXT("SpawnClass override set for '%s': %s"), *InstanceName.ToString(), *SpawnClass->GetName());
	}
	else
	{
		SpawnClassOverrides.Remove(InstanceName);
		UE_LOG(LogAefPharus, Log, TEXT("SpawnClass override cleared for '%s'"), *InstanceName.ToString());
	}
}

bool UAefPharusSubsystem::StartPharusSystem()
{
	if (TrackerInstances.Num() > 0)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Pharus system is already running with %d instance(s)"), TrackerInstances.Num());
		return false;
	}

	UE_LOG(LogAefPharus, Log, TEXT("Starting Pharus system..."));
	CreateInstancesFromConfig();
	
	bool bSuccess = TrackerInstances.Num() > 0;
	if (bSuccess)
	{
		UE_LOG(LogAefPharus, Log, TEXT("Pharus system started successfully with %d instance(s)"), TrackerInstances.Num());
	}
	else
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Pharus system start failed - no instances were created"));
	}
	
	return bSuccess;
}

bool UAefPharusSubsystem::StopPharusSystem()
{
	if (TrackerInstances.Num() == 0)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Pharus system is not running"));
		return false;
	}

	UE_LOG(LogAefPharus, Log, TEXT("Stopping Pharus system (%d instance(s))..."), TrackerInstances.Num());
	
	// Shutdown all instances
	TArray<FName> InstanceNames;
	TrackerInstances.GetKeys(InstanceNames);
	
	for (const FName& InstanceName : InstanceNames)
	{
		RemoveTrackerInstance(InstanceName);
	}
	
	UE_LOG(LogAefPharus, Log, TEXT("Pharus system stopped successfully"));
	return true;
}

bool UAefPharusSubsystem::IsPharusSystemRunning() const
{
	return TrackerInstances.Num() > 0;
}

//--------------------------------------------------------------------------------
// Root Origin Management
//--------------------------------------------------------------------------------

void UAefPharusSubsystem::SetRootOrigin(FVector Origin)
{
	RootOrigin = Origin;
	UE_LOG(LogAefPharus, Log, TEXT("Root origin set to: %s"), *RootOrigin.ToString());
}

FVector UAefPharusSubsystem::GetRootOrigin() const
{
	// If using dynamic actor and actor is valid, return its position
	if (bUsePharusRootOriginActor && PharusRootOriginActor.IsValid())
	{
		FVector ActorOrigin = PharusRootOriginActor->GetOriginLocation();
		UE_LOG(LogAefPharus, Verbose, TEXT("GetRootOrigin: Using actor origin = %s"), *ActorOrigin.ToString());
		return ActorOrigin;
	}

	// Return static origin (from config or SetRootOrigin)
	UE_LOG(LogAefPharus, Verbose, TEXT("GetRootOrigin: Using static origin = %s, bUsePharusRootOriginActor=%s"), 
		*RootOrigin.ToString(), bUsePharusRootOriginActor ? TEXT("true") : TEXT("false"));
	return RootOrigin;
}

bool UAefPharusSubsystem::HasValidRootOrigin() const
{
	if (bUsePharusRootOriginActor)
	{
		// When using actor mode, we need a valid actor
		return PharusRootOriginActor.IsValid();
	}
	// Static origin is always "valid" (even if zero)
	return true;
}

void UAefPharusSubsystem::SetRootOriginRotation(FRotator Rotation)
{
	RootRotation = Rotation;
	UE_LOG(LogAefPharus, Log, TEXT("Root origin rotation set to: %s"), *RootRotation.ToString());
}

FRotator UAefPharusSubsystem::GetRootOriginRotation() const
{
	// If using dynamic actor and actor is valid, return its rotation
	if (bUsePharusRootOriginActor && PharusRootOriginActor.IsValid())
	{
		FRotator ActorRotation = PharusRootOriginActor->GetOriginRotation();
		UE_LOG(LogAefPharus, Verbose, TEXT("GetRootOriginRotation: Using actor rotation = %s"), *ActorRotation.ToString());
		return ActorRotation;
	}

	// Return static rotation (from config or SetRootOriginRotation)
	UE_LOG(LogAefPharus, Verbose, TEXT("GetRootOriginRotation: Using static rotation = %s"), *RootRotation.ToString());
	return RootRotation;
}

void UAefPharusSubsystem::RegisterRootOriginActor(AAefPharusRootOriginActor* RootActor)
{
	if (!RootActor)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("RegisterRootOriginActor: Null actor provided"));
		return;
	}

	// Enforce singleton pattern - only ONE root origin actor allowed
	if (PharusRootOriginActor.IsValid() && PharusRootOriginActor.Get() != RootActor)
	{
		UE_LOG(LogAefPharus, Error, 
			TEXT("DUPLICATE AefPharusRootOriginActor detected! Only ONE root origin actor is allowed per world. ")
			TEXT("Existing: '%s', New: '%s'. The new actor will be IGNORED."),
			*PharusRootOriginActor->GetName(), *RootActor->GetName());
		return;
	}

	// Always register the actor reference (for IsValid() checks)
	PharusRootOriginActor = RootActor;
	
	// Only update RootOrigin from actor if bUsePharusRootOriginActor is true!
	// If false, the user wants to use the static GlobalOrigin from config
	if (bUsePharusRootOriginActor)
	{
		RootOrigin = RootActor->GetOriginLocation();
		RootRotation = RootActor->GetOriginRotation();
		UE_LOG(LogAefPharus, Log, TEXT("PharusRootOriginActor registered: '%s' - DYNAMIC origin at %s, rotation %s"),
			*RootActor->GetName(), *RootOrigin.ToString(), *RootRotation.ToString());
	}
	else
	{
		// Actor is in scene but UsePharusRootOriginActor=false 
		// Keep the static GlobalOrigin/GlobalRotation from config, but log a helpful message
		UE_LOG(LogAefPharus, Log, TEXT("PharusRootOriginActor '%s' is in scene, but UsePharusRootOriginActor=FALSE in config."),
			*RootActor->GetName());
		UE_LOG(LogAefPharus, Log, TEXT("  -> Using STATIC GlobalOrigin=%s, GlobalRotation=%s from AefConfig.ini (actor transform IGNORED)"),
			*RootOrigin.ToString(), *RootRotation.ToString());
		UE_LOG(LogAefPharus, Log, TEXT("  -> Set UsePharusRootOriginActor=true in config to use this actor's transform."));
	}
}

void UAefPharusSubsystem::UnregisterRootOriginActor(AAefPharusRootOriginActor* RootActor)
{
	if (!RootActor)
	{
		return;
	}

	// Only unregister if this is the registered actor
	if (PharusRootOriginActor.Get() == RootActor)
	{
		PharusRootOriginActor.Reset();
		UE_LOG(LogAefPharus, Log, TEXT("PharusRootOriginActor '%s' unregistered"), *RootActor->GetName());
	}
}

bool UAefPharusSubsystem::IsUsingRootOriginActor() const
{
	return bUsePharusRootOriginActor;
}

AAefPharusRootOriginActor* UAefPharusSubsystem::GetRootOriginActor() const
{
	return PharusRootOriginActor.Get();
}

bool UAefPharusSubsystem::IsRelativeSpawningActive() const
{
	return bUseRelativeSpawning && bUsePharusRootOriginActor && PharusRootOriginActor.IsValid();
}

//--------------------------------------------------------------------------------
// Configuration Loading
//--------------------------------------------------------------------------------

void UAefPharusSubsystem::LoadConfigurationFromIni()
{
	FString ConfigPath = GetConfigFilePath();

	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Configuration file not found: %s"), *ConfigPath);
		UE_LOG(LogAefPharus, Warning, TEXT("No tracker instances will be created automatically."));
		UE_LOG(LogAefPharus, Warning, TEXT("You can create instances manually via Blueprint or copy a config template from Plugins/AefPharus/Config/Examples/"));
		return;
	}

	UE_LOG(LogAefPharus, Log, TEXT("Loading configuration from: %s"), *ConfigPath);

	//--------------------------------------------------------------------------------
	// Load Global Root Origin Configuration from [PharusSubsystem]
	//--------------------------------------------------------------------------------

	// UsePharusRootOriginActor flag (default: false)
	GConfig->GetBool(TEXT("PharusSubsystem"), TEXT("UsePharusRootOriginActor"), bUsePharusRootOriginActor, ConfigPath);

	// GlobalOrigin - Vector3 format: (X=...,Y=...,Z=...)
	FString GlobalOriginStr;
	if (GConfig->GetString(TEXT("PharusSubsystem"), TEXT("GlobalOrigin"), GlobalOriginStr, ConfigPath))
	{
		FVector ParsedOrigin;
		if (ParsedOrigin.InitFromString(GlobalOriginStr))
		{
			RootOrigin = ParsedOrigin;
			UE_LOG(LogAefPharus, Log, TEXT("GlobalOrigin loaded from config: %s"), *RootOrigin.ToString());
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse GlobalOrigin: '%s' - expected format: (X=...,Y=...,Z=...) - using default (0,0,0)"), *GlobalOriginStr);
			RootOrigin = FVector::ZeroVector;
		}
	}

	// GlobalRotation - Rotator format: (Pitch=...,Yaw=...,Roll=...)
	FString GlobalRotationStr;
	if (GConfig->GetString(TEXT("PharusSubsystem"), TEXT("GlobalRotation"), GlobalRotationStr, ConfigPath))
	{
		FRotator ParsedRotation;
		if (ParsedRotation.InitFromString(GlobalRotationStr))
		{
			RootRotation = ParsedRotation;
			UE_LOG(LogAefPharus, Log, TEXT("GlobalRotation loaded from config: %s"), *RootRotation.ToString());
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse GlobalRotation: '%s' - expected format: (Pitch=...,Yaw=...,Roll=...) - using default (0,0,0)"), *GlobalRotationStr);
			RootRotation = FRotator::ZeroRotator;
		}
	}

	// UseRelativeSpawning flag (default: false)
	// When true AND UsePharusRootOriginActor=true, actors are attached as children to RootOriginActor
	GConfig->GetBool(TEXT("PharusSubsystem"), TEXT("UseRelativeSpawning"), bUseRelativeSpawning, ConfigPath);
	
	// Validate: UseRelativeSpawning requires UsePharusRootOriginActor
	if (bUseRelativeSpawning && !bUsePharusRootOriginActor)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("UseRelativeSpawning=true requires UsePharusRootOriginActor=true! Disabling relative spawning."));
		bUseRelativeSpawning = false;
	}

	// Log origin mode
	if (bUsePharusRootOriginActor)
	{
		UE_LOG(LogAefPharus, Log, TEXT("Root origin mode: Dynamic (AefPharusRootOriginActor - position AND rotation from actor)"));
		if (bUseRelativeSpawning)
		{
			UE_LOG(LogAefPharus, Log, TEXT("  -> UseRelativeSpawning=true: Actors will be attached as children to RootOriginActor"));
		}
	}
	else
	{
		UE_LOG(LogAefPharus, Log, TEXT("Root origin mode: Static (GlobalOrigin=%s, GlobalRotation=%s)"), *RootOrigin.ToString(), *RootRotation.ToString());
	}
}

void UAefPharusSubsystem::CreateInstancesFromConfig()
{
	FString ConfigPath = GetConfigFilePath();

	// Read AutoCreateInstances from [PharusSubsystem] section
	FString AutoCreateInstancesStr;
	if (!GConfig->GetString(TEXT("PharusSubsystem"), TEXT("AutoCreateInstances"), AutoCreateInstancesStr, ConfigPath))
	{
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT("No AutoCreateInstances setting found in [PharusSubsystem]"));
		}
		return;
	}

	// Parse comma-separated list
	TArray<FString> InstanceNames;
	AutoCreateInstancesStr.ParseIntoArray(InstanceNames, TEXT(","), true);

	UE_LOG(LogAefPharus, Log, TEXT("Auto-creating %d instance(s): %s"), InstanceNames.Num(), *AutoCreateInstancesStr);

	// Create each instance
	int32 SuccessCount = 0;
	int32 FailCount = 0;

	for (const FString& InstanceName : InstanceNames)
	{
		FString TrimmedName = InstanceName.TrimStartAndEnd();
		FString SectionName = FString::Printf(TEXT("Pharus.%s"), *TrimmedName);

		// Debug logging - detailed processing info
		if (bIsPharusDebug)
		{
			UE_LOG(LogAefPharus, Log, TEXT(""));
			UE_LOG(LogAefPharus, Log, TEXT("Processing instance: '%s'"), *TrimmedName);
			UE_LOG(LogAefPharus, Log, TEXT("  Section: [%s]"), *SectionName);
		}

		// Check for SpawnClass override first, then fall back to INI DefaultSpawnClass
		FName InstanceFName = FName(*TrimmedName);
		TSubclassOf<AActor> InstanceSpawnClass = nullptr;

		if (TSubclassOf<AActor>* OverrideClass = SpawnClassOverrides.Find(InstanceFName))
		{
			// Use override set via SetSpawnClassOverride()
			InstanceSpawnClass = *OverrideClass;
			UE_LOG(LogAefPharus, Log, TEXT("  Using SpawnClass override for '%s': %s"), *TrimmedName, *InstanceSpawnClass->GetName());
		}
		else
		{
			// Load instance-specific DefaultSpawnClass from INI
			FString InstanceSpawnClassPath;
			if (GConfig->GetString(*SectionName, TEXT("DefaultSpawnClass"), InstanceSpawnClassPath, ConfigPath))
			{
				if (bIsPharusDebug)
				{
					UE_LOG(LogAefPharus, Log, TEXT("  DefaultSpawnClass: %s"), *InstanceSpawnClassPath);
				}
				UClass* LoadedClass = LoadObject<UClass>(nullptr, *InstanceSpawnClassPath);
				if (LoadedClass)
				{
					InstanceSpawnClass = LoadedClass;
					if (bIsPharusDebug)
					{
						UE_LOG(LogAefPharus, Log, TEXT("  ✓ SpawnClass loaded: %s"), *LoadedClass->GetName());
					}
				}
				else
				{
					UE_LOG(LogAefPharus, Error, TEXT("Failed to load SpawnClass for '%s': %s"), *TrimmedName, *InstanceSpawnClassPath);
				}
			}
			else
			{
				UE_LOG(LogAefPharus, Warning, TEXT("No DefaultSpawnClass found for '%s' - actors will not spawn!"), *TrimmedName);
			}
		}

		// Parse instance configuration
		FAefPharusInstanceConfig Config = ParseInstanceConfigFromIni(SectionName);
		Config.InstanceName = FName(*TrimmedName);

		// Create instance with SpawnClass
		FAefPharusCreateInstanceResult Result = CreateTrackerInstance(Config, InstanceSpawnClass);
		if (Result.bSuccess)
		{
			SuccessCount++;
		}
		else
		{
			FailCount++;
			UE_LOG(LogAefPharus, Error, TEXT("Instance '%s' creation failed: %s"), *TrimmedName, *Result.ErrorMessage);
		}
	}

	// Compact summary log
	UE_LOG(LogAefPharus, Log, TEXT("Auto-create complete: %d/%d instances created successfully"),
		SuccessCount, InstanceNames.Num());
}

/**
 * Parse Instance Configuration from INI File
 *
 * CONFIGURATION FORMAT SPECIFICATION:
 * ==================================
 *
 * This function parses configuration values from AefConfig.ini using ONLY Unreal Engine vector formats:
 * - Vector3 (FVector):   (X=...,Y=...,Z=...)
 * - Vector2D (FVector2D): (X=...,Y=...)
 * - Rotator (FRotator):  (P=...,Y=...,R=...)  [P=Pitch, Y=Yaw, R=Roll]
 *
 * DEPRECATED FORMATS:
 * - Individual component properties (e.g., WorldPositionX, WorldPositionY, WorldPositionZ) are NO LONGER SUPPORTED
 * - Component-wise parsing has been removed to ensure configuration clarity and prevent parsing errors
 *
 * SUPPORTED CONFIGURATION PROPERTIES:
 *
 * [Pharus.Floor] / [Pharus.Wall] - Instance Settings:
 * --------------------------------------------------
 * TrackingSurfaceDimensions=(X=50.0,Y=6.0)    - Physical dimensions of tracking surface (meters) [Vector2D]
 * Origin=(X=0.0,Y=0.0)                         - Origin offset for coordinate transformation [Vector2D]
 * Scale=(X=100.0,Y=100.0)                      - Scale factors (typically 100.0 for meter→cm) [Vector2D]
 *
 * Wall.{WallName}.WorldPosition=(X=...,Y=...,Z=...)    - World position of wall center (cm) [Vector3]
 * Wall.{WallName}.WorldRotation=(P=...,Y=...,R=...)    - World rotation of wall (degrees) [Rotator]
 * Wall.{WallName}.WorldSize=(X=...,Y=...,Z=...)        - Physical dimensions of wall (cm) [Vector3]
 * Wall.{WallName}.Scale=(X=...,Y=...)                  - Scale factors for X/Z transformation [Vector2D]
 * Wall.{WallName}.Origin=(X=...,Y=...)                 - Origin offset for normalized coordinates [Vector2D]
 *
 * ERROR HANDLING:
 * - Missing properties: Warning logged, default value used
 * - Invalid format: Warning logged with expected format, default value used
 * - Parse failure: Clear error message indicating expected format
 *
 * @param SectionName - INI section name (e.g., "Pharus.Floor", "Pharus.Wall")
 * @return Parsed configuration with defaults for missing/invalid properties
 */
FAefPharusInstanceConfig UAefPharusSubsystem::ParseInstanceConfigFromIni(const FString& SectionName)
{
	FString ConfigPath = GetConfigFilePath();
	FAefPharusInstanceConfig Config;

	// Identity
	GConfig->GetBool(*SectionName, TEXT("Enable"), Config.bEnable, ConfigPath);

	// Network
	GConfig->GetString(*SectionName, TEXT("BindNIC"), Config.BindNIC, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("UDPPort"), Config.UDPPort, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("IsMulticast"), Config.bIsMulticast, ConfigPath);
	GConfig->GetString(*SectionName, TEXT("MulticastGroup"), Config.MulticastGroup, ConfigPath);

	// Mapping mode
	FString MappingModeStr;
	if (GConfig->GetString(*SectionName, TEXT("MappingMode"), MappingModeStr, ConfigPath))
	{
		if (MappingModeStr.Equals(TEXT("Simple"), ESearchCase::IgnoreCase))
		{
			Config.MappingMode = EAefPharusMappingMode::Simple;
		}
		else if (MappingModeStr.Equals(TEXT("Regions"), ESearchCase::IgnoreCase))
		{
			Config.MappingMode = EAefPharusMappingMode::Regions;
			Config.WallRegions = ParseWallRegionsFromIni(SectionName);
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Unknown MappingMode '%s', defaulting to Simple"), *MappingModeStr);
			Config.MappingMode = EAefPharusMappingMode::Simple;
		}
	}

	// Coordinate system settings - Vector2D format: TrackingSurfaceDimensions=(X=...,Y=...)
	FString TrackingSurfaceDimensionsStr;
	if (GConfig->GetString(*SectionName, TEXT("TrackingSurfaceDimensions"), TrackingSurfaceDimensionsStr, ConfigPath))
	{
		FVector2D Dims;
		if (Dims.InitFromString(TrackingSurfaceDimensionsStr))
		{
			Config.TrackingSurfaceDimensions = Dims;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse TrackingSurfaceDimensions: '%s' - expected format: (X=...,Y=...) - using default: %s"), *TrackingSurfaceDimensionsStr, *Config.TrackingSurfaceDimensions.ToString());
		}
	}
	else
	{
		// Default: 10m x 15m
		Config.TrackingSurfaceDimensions = FVector2D(10.0f, 15.0f);
	}

	GConfig->GetBool(*SectionName, TEXT("UseNormalizedCoordinates"), Config.bUseNormalizedCoordinates, ConfigPath);

	// NOTE: Per-instance Origin was removed - use [PharusSubsystem].GlobalOrigin instead

	// Scale - Vector2D format: Scale=(X=...,Y=...)
	FString ScaleStr;
	if (GConfig->GetString(*SectionName, TEXT("Scale"), ScaleStr, ConfigPath))
	{
		FVector2D Scale;
		if (Scale.InitFromString(ScaleStr))
		{
			Config.SimpleScale = Scale;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse Scale: '%s' - expected format: (X=...,Y=...) - using default: %s"), *ScaleStr, *Config.SimpleScale.ToString());
		}
	}
	else
	{
		// Default: 100.0 scale (typical for meter to cm conversion)
		Config.SimpleScale = FVector2D(100.0f, 100.0f);
	}

	GConfig->GetFloat(*SectionName, TEXT("FloorZ"), Config.FloorZ, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("FloorRotation"), Config.FloorRotation, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("WallRotation"), Config.WallRotation, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("InvertY"), Config.bInvertY, ConfigPath);

	// Spawning (SpawnClass is now passed to CreateTrackerInstance directly, not loaded from INI)
	GConfig->GetBool(*SectionName, TEXT("AutoDestroyOnTrackLost"), Config.bAutoDestroyOnTrackLost, ConfigPath);

	// nDisplay Actor Pool Configuration
	GConfig->GetBool(*SectionName, TEXT("UseActorPool"), Config.bUseActorPool, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("ActorPoolSize"), Config.ActorPoolSize, ConfigPath);

	// Pool spawn location - Vector format: PoolSpawnLocation=(X=...,Y=...,Z=...)
	FString PoolSpawnLocationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnLocation"), PoolSpawnLocationStr, ConfigPath))
	{
		FVector PoolSpawnLocation;
		if (PoolSpawnLocation.InitFromString(PoolSpawnLocationStr))
		{
			Config.PoolSpawnLocation = PoolSpawnLocation;
		}
	}

	// Pool spawn rotation - Rotator format: PoolSpawnRotation=(Pitch=...,Yaw=...,Roll=...)
	FString PoolSpawnRotationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnRotation"), PoolSpawnRotationStr, ConfigPath))
	{
		FRotator PoolSpawnRotation;
		if (PoolSpawnRotation.InitFromString(PoolSpawnRotationStr))
		{
			Config.PoolSpawnRotation = PoolSpawnRotation;
		}
	}

	// Pool index offset - Vector format: PoolIndexOffset=(X=...,Y=...,Z=...)
	FString PoolIndexOffsetStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolIndexOffset"), PoolIndexOffsetStr, ConfigPath))
	{
		FVector PoolIndexOffset;
		if (PoolIndexOffset.InitFromString(PoolIndexOffsetStr))
		{
			Config.PoolIndexOffset = PoolIndexOffset;
		}
	}

	// Transform & Performance
	GConfig->GetBool(*SectionName, TEXT("UseLocalSpace"), Config.bUseLocalSpace, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("ApplyOrientationFromMovement"), Config.bApplyOrientationFromMovement, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("UseRelativeSpawning"), Config.bUseRelativeSpawning, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LiveAdjustments"), Config.bLiveAdjustments, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("TrackLostTimeout"), Config.TrackLostTimeout, ConfigPath);

	// Logging & Debug
	GConfig->GetBool(*SectionName, TEXT("LogTrackerSpawned"), Config.bLogTrackerSpawned, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerUpdated"), Config.bLogTrackerUpdated, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerRemoved"), Config.bLogTrackerRemoved, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogNetworkStats"), Config.bLogNetworkStats, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogRegionAssignment"), Config.bLogRegionAssignment, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogRejectedTracks"), Config.bLogRejectedTracks, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugVisualization"), Config.bDebugVisualization, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawBounds"), Config.bDebugDrawBounds, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawOrigin"), Config.bDebugDrawOrigin, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawWallPlanes"), Config.bDebugDrawWallPlanes, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawRegionBoundaries"), Config.bDebugDrawRegionBoundaries, ConfigPath);

	return Config;
}

TArray<FAefPharusWallRegion> UAefPharusSubsystem::ParseWallRegionsFromIni(const FString& BaseSectionName) const
{
	TArray<FAefPharusWallRegion> Regions;
	TArray<FString> WallNames = {TEXT("Front"), TEXT("Left"), TEXT("Back"), TEXT("Right")};

	for (const FString& WallName : WallNames)
	{
		FAefPharusWallRegion Region = ParseWallRegionFromIni(BaseSectionName, WallName);
		Regions.Add(Region);
	}

	UE_LOG(LogAefPharus, Log, TEXT("Parsed %d wall regions from [%s]"), Regions.Num(), *BaseSectionName);

	return Regions;
}

FAefPharusWallRegion UAefPharusSubsystem::ParseWallRegionFromIni(const FString& SectionName, const FString& WallName) const
{
	FString ConfigPath = GetConfigFilePath();
	FAefPharusWallRegion Region;

	// Determine wall side
	if (WallName.Equals(TEXT("Front")))
		Region.WallSide = EAefPharusWallSide::Front;
	else if (WallName.Equals(TEXT("Left")))
		Region.WallSide = EAefPharusWallSide::Left;
	else if (WallName.Equals(TEXT("Back")))
		Region.WallSide = EAefPharusWallSide::Back;
	else if (WallName.Equals(TEXT("Right")))
		Region.WallSide = EAefPharusWallSide::Right;

	// Build key prefix
	FString Prefix = FString::Printf(TEXT("Wall.%s."), *WallName);

	// Tracking bounds
	float MinX = 0.0f, MaxX = 0.25f, MinY = 0.0f, MaxY = 1.0f;
	GConfig->GetFloat(*SectionName, *(Prefix + TEXT("TrackingMinX")), MinX, ConfigPath);
	GConfig->GetFloat(*SectionName, *(Prefix + TEXT("TrackingMaxX")), MaxX, ConfigPath);
	GConfig->GetFloat(*SectionName, *(Prefix + TEXT("TrackingMinY")), MinY, ConfigPath);
	GConfig->GetFloat(*SectionName, *(Prefix + TEXT("TrackingMaxY")), MaxY, ConfigPath);
	Region.TrackingBounds = FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));

	// World position - Vector3 format only: WorldPosition=(X=...,Y=...,Z=...)
	FString PosStr;
	if (GConfig->GetString(*SectionName, *(Prefix + TEXT("WorldPosition")), PosStr, ConfigPath))
	{
		FVector Pos;
		if (Pos.InitFromString(PosStr))
		{
			Region.WorldPosition = Pos;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse %sWorldPosition: '%s' - expected format: (X=...,Y=...,Z=...)"), *Prefix, *PosStr);
		}
	}

	// World rotation - Rotator format: WorldRotation=(Pitch=...,Yaw=...,Roll=...)
	FString RotStr;
	if (GConfig->GetString(*SectionName, *(Prefix + TEXT("WorldRotation")), RotStr, ConfigPath))
	{
		FRotator Rot;
		// Try standard UE format first: (P=...,Y=...,R=...)
		if (Rot.InitFromString(RotStr))
		{
			Region.WorldRotation = Rot;
		}
		else
		{
			// Parse long-form format: (Pitch=...,Yaw=...,Roll=...)
			float Pitch = 0.0f, Yaw = 0.0f, Roll = 0.0f;
			FString TrimmedStr = RotStr.TrimStartAndEnd();

			// Remove parentheses
			if (TrimmedStr.StartsWith(TEXT("("))) TrimmedStr.RightChopInline(1);
			if (TrimmedStr.EndsWith(TEXT(")"))) TrimmedStr.LeftChopInline(1);

			// Split by comma
			TArray<FString> Components;
			TrimmedStr.ParseIntoArray(Components, TEXT(","), true);

			for (const FString& Component : Components)
			{
				FString Key, Value;
				if (Component.Split(TEXT("="), &Key, &Value))
				{
					Key = Key.TrimStartAndEnd();
					Value = Value.TrimStartAndEnd();

					if (Key.Equals(TEXT("Pitch"), ESearchCase::IgnoreCase))
						Pitch = FCString::Atof(*Value);
					else if (Key.Equals(TEXT("Yaw"), ESearchCase::IgnoreCase))
						Yaw = FCString::Atof(*Value);
					else if (Key.Equals(TEXT("Roll"), ESearchCase::IgnoreCase))
						Roll = FCString::Atof(*Value);
				}
			}

			Region.WorldRotation = FRotator(Pitch, Yaw, Roll);
		}
	}

	// World size - Vector3 format only: WorldSize=(X=...,Y=...,Z=...)
	FString SizeStr;
	if (GConfig->GetString(*SectionName, *(Prefix + TEXT("WorldSize")), SizeStr, ConfigPath))
	{
		FVector Size;
		if (Size.InitFromString(SizeStr))
		{
			Region.WorldSize = Size;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse %sWorldSize: '%s' - expected format: (X=...,Y=...,Z=...)"), *Prefix, *SizeStr);
		}
	}
	else
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Missing %sWorldSize in config - using default: %s"), *Prefix, *Region.WorldSize.ToString());
	}

	// Scale - Vector2D format: Scale=(X=...,Y=...)
	FString ScaleStr;
	if (GConfig->GetString(*SectionName, *(Prefix + TEXT("Scale")), ScaleStr, ConfigPath))
	{
		FVector2D Scale;
		if (Scale.InitFromString(ScaleStr))
		{
			Region.Scale = Scale;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse %sScale: '%s' - expected format: (X=...,Y=...) - using default: %s"), *Prefix, *ScaleStr, *Region.Scale.ToString());
		}
	}
	else
	{
		// Default: 100.0 scale (typical for meter to cm conversion)
		Region.Scale = FVector2D(100.0f, 100.0f);
	}

	// Origin offset - Vector2D format: Origin=(X=...,Y=...)
	// This shifts the coordinate system so (0,0) maps to bottom-left corner of wall
	FString OriginStr;
	if (GConfig->GetString(*SectionName, *(Prefix + TEXT("Origin")), OriginStr, ConfigPath))
	{
		FVector2D OriginVec;
		if (OriginVec.InitFromString(OriginStr))
		{
			Region.Origin = OriginVec;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Failed to parse %sOrigin: '%s' - expected format: (X=...,Y=...) - using default: %s"), *Prefix, *OriginStr, *Region.Origin.ToString());
		}
	}
	else
	{
		// Default: Zero origin (center-based mapping)
		Region.Origin = FVector2D::ZeroVector;
	}

	// InvertY (like Floor mapping)
	GConfig->GetBool(*SectionName, *(Prefix + TEXT("InvertY")), Region.bInvertY, ConfigPath);

	// Rotation2D (like FloorRotation)
	GConfig->GetFloat(*SectionName, *(Prefix + TEXT("Rotation2D")), Region.Rotation2D, ConfigPath);

	UE_LOG(LogAefPharus, Verbose, TEXT("Parsed wall region '%s': Bounds=(%.3f-%.3f, %.3f-%.3f), Pos=%s, Rot=%s, Scale=%s, Origin=%s, InvertY=%s, Rot2D=%.1f°"),
		*WallName,
		MinX, MaxX, MinY, MaxY,
		*Region.WorldPosition.ToString(),
		*Region.WorldRotation.ToString(),
		*Region.Scale.ToString(),
		*Region.Origin.ToString(),
		Region.bInvertY ? TEXT("true") : TEXT("false"),
		Region.Rotation2D);

	return Region;
}

//--------------------------------------------------------------------------------
// Helper Functions
//--------------------------------------------------------------------------------

FString UAefPharusSubsystem::GetConfigFilePath() const
{
	FString Path = FPaths::ProjectConfigDir() / TEXT("AefConfig.ini");
	return FConfigCacheIni::NormalizeConfigIniPath(Path);
}

//--------------------------------------------------------------------------------
// Configuration Reading from Disk
//--------------------------------------------------------------------------------

bool UAefPharusSubsystem::GetFloorSettingsFromDisk(FName InstanceName, FAefPharusInstanceConfig& OutConfig) const
{
	// Build config file path
	FString ConfigPath = GetConfigFilePath();

	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogAefPharus, Error, TEXT("Config file not found: %s"), *ConfigPath);
		return false;
	}

	FString SectionName = FString::Printf(TEXT("Pharus.%s"), *InstanceName.ToString());

	// Read Floor settings from disk (only settings that are in AefConfig.ini)
	FAefPharusInstanceConfig DiskConfig;
	DiskConfig.InstanceName = InstanceName;

	// Basic Settings
	GConfig->GetBool(*SectionName, TEXT("Enable"), DiskConfig.bEnable, ConfigPath);

	FString MappingModeStr;
	if (GConfig->GetString(*SectionName, TEXT("MappingMode"), MappingModeStr, ConfigPath))
	{
		if (MappingModeStr.Equals(TEXT("Simple"), ESearchCase::IgnoreCase))
			DiskConfig.MappingMode = EAefPharusMappingMode::Simple;
	}

	// Network Configuration
	GConfig->GetString(*SectionName, TEXT("BindNIC"), DiskConfig.BindNIC, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("UDPPort"), DiskConfig.UDPPort, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("IsMulticast"), DiskConfig.bIsMulticast, ConfigPath);
	GConfig->GetString(*SectionName, TEXT("MulticastGroup"), DiskConfig.MulticastGroup, ConfigPath);

	// Coordinate System Settings (Origin is now global via [PharusSubsystem].GlobalOrigin)
	FString ScaleStr;
	if (GConfig->GetString(*SectionName, TEXT("Scale"), ScaleStr, ConfigPath))
	{
		FVector2D Scale;
		if (Scale.InitFromString(ScaleStr))
			DiskConfig.SimpleScale = Scale;
	}

	GConfig->GetFloat(*SectionName, TEXT("FloorZ"), DiskConfig.FloorZ, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("FloorRotation"), DiskConfig.FloorRotation, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("InvertY"), DiskConfig.bInvertY, ConfigPath);

	// Tracking Surface Configuration
	FString TrackingSurfaceDimensionsStr;
	if (GConfig->GetString(*SectionName, TEXT("TrackingSurfaceDimensions"), TrackingSurfaceDimensionsStr, ConfigPath))
	{
		FVector2D Dims;
		if (Dims.InitFromString(TrackingSurfaceDimensionsStr))
			DiskConfig.TrackingSurfaceDimensions = Dims;
	}

	GConfig->GetBool(*SectionName, TEXT("UseNormalizedCoordinates"), DiskConfig.bUseNormalizedCoordinates, ConfigPath);

	// Actor Spawning
	FString SpawnCollisionStr;
	if (GConfig->GetString(*SectionName, TEXT("SpawnCollisionHandling"), SpawnCollisionStr, ConfigPath))
	{
		if (SpawnCollisionStr.Equals(TEXT("AlwaysSpawn"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		else if (SpawnCollisionStr.Equals(TEXT("AdjustIfPossibleButAlwaysSpawn"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		else if (SpawnCollisionStr.Equals(TEXT("AdjustIfPossibleButDontSpawnIfColliding"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		else if (SpawnCollisionStr.Equals(TEXT("DontSpawnIfColliding"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	}
	GConfig->GetBool(*SectionName, TEXT("AutoDestroyOnTrackLost"), DiskConfig.bAutoDestroyOnTrackLost, ConfigPath);

	// nDisplay Actor Pool Configuration
	GConfig->GetBool(*SectionName, TEXT("UseActorPool"), DiskConfig.bUseActorPool, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("ActorPoolSize"), DiskConfig.ActorPoolSize, ConfigPath);

	// Pool spawn location - Vector format: PoolSpawnLocation=(X=...,Y=...,Z=...)
	FString PoolSpawnLocationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnLocation"), PoolSpawnLocationStr, ConfigPath))
	{
		FVector PoolSpawnLocation;
		if (PoolSpawnLocation.InitFromString(PoolSpawnLocationStr))
			DiskConfig.PoolSpawnLocation = PoolSpawnLocation;
	}

	// Pool spawn rotation - Rotator format: PoolSpawnRotation=(Pitch=...,Yaw=...,Roll=...)
	FString PoolSpawnRotationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnRotation"), PoolSpawnRotationStr, ConfigPath))
	{
		FRotator PoolSpawnRotation;
		if (PoolSpawnRotation.InitFromString(PoolSpawnRotationStr))
			DiskConfig.PoolSpawnRotation = PoolSpawnRotation;
	}

	// Pool index offset - Vector format: PoolIndexOffset=(X=...,Y=...,Z=...)
	FString PoolIndexOffsetStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolIndexOffset"), PoolIndexOffsetStr, ConfigPath))
	{
		FVector PoolIndexOffset;
		if (PoolIndexOffset.InitFromString(PoolIndexOffsetStr))
			DiskConfig.PoolIndexOffset = PoolIndexOffset;
	}

	// Transform & Performance (was MISSING - added to match Wall settings)
	GConfig->GetBool(*SectionName, TEXT("UseLocalSpace"), DiskConfig.bUseLocalSpace, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("ApplyOrientationFromMovement"), DiskConfig.bApplyOrientationFromMovement, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LiveAdjustments"), DiskConfig.bLiveAdjustments, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("TrackLostTimeout"), DiskConfig.TrackLostTimeout, ConfigPath);

	// Logging & Debug
	GConfig->GetBool(*SectionName, TEXT("LogTrackerSpawned"), DiskConfig.bLogTrackerSpawned, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerUpdated"), DiskConfig.bLogTrackerUpdated, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerRemoved"), DiskConfig.bLogTrackerRemoved, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogNetworkStats"), DiskConfig.bLogNetworkStats, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogRegionAssignment"), DiskConfig.bLogRegionAssignment, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugVisualization"), DiskConfig.bDebugVisualization, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawBounds"), DiskConfig.bDebugDrawBounds, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawOrigin"), DiskConfig.bDebugDrawOrigin, ConfigPath);

	OutConfig = DiskConfig;

	UE_LOG(LogAefPharus, Log, TEXT("Floor settings loaded from disk for '%s': Scale=%s, FloorZ=%.2f, Rotation=%.2f, InvertY=%s"),
		*InstanceName.ToString(),
		*OutConfig.SimpleScale.ToString(),
		OutConfig.FloorZ,
		OutConfig.FloorRotation,
		OutConfig.bInvertY ? TEXT("true") : TEXT("false"));

	return true;
}

bool UAefPharusSubsystem::GetWallSettingsFromDisk(FName InstanceName, FAefPharusInstanceConfig& OutConfig) const
{
	// Build config file path
	FString ConfigPath = GetConfigFilePath();

	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogAefPharus, Error, TEXT("Config file not found: %s"), *ConfigPath);
		return false;
	}

	FString SectionName = FString::Printf(TEXT("Pharus.%s"), *InstanceName.ToString());

	// Read Wall settings from disk (only settings that are in AefConfig.ini)
	FAefPharusInstanceConfig DiskConfig;
	DiskConfig.InstanceName = InstanceName;

	// Basic Settings
	GConfig->GetBool(*SectionName, TEXT("Enable"), DiskConfig.bEnable, ConfigPath);

	FString MappingModeStr;
	if (GConfig->GetString(*SectionName, TEXT("MappingMode"), MappingModeStr, ConfigPath))
	{
		if (MappingModeStr.Equals(TEXT("Regions"), ESearchCase::IgnoreCase))
		{
			DiskConfig.MappingMode = EAefPharusMappingMode::Regions;
			// Parse wall regions for proper 3D normalization
			DiskConfig.WallRegions = ParseWallRegionsFromIni(SectionName);
		}
	}

	// Network Configuration
	GConfig->GetString(*SectionName, TEXT("BindNIC"), DiskConfig.BindNIC, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("UDPPort"), DiskConfig.UDPPort, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("IsMulticast"), DiskConfig.bIsMulticast, ConfigPath);
	GConfig->GetString(*SectionName, TEXT("MulticastGroup"), DiskConfig.MulticastGroup, ConfigPath);

	// Tracking Surface Configuration
	FString TrackingSurfaceDimensionsStr;
	if (GConfig->GetString(*SectionName, TEXT("TrackingSurfaceDimensions"), TrackingSurfaceDimensionsStr, ConfigPath))
	{
		FVector2D Dims;
		if (Dims.InitFromString(TrackingSurfaceDimensionsStr))
			DiskConfig.TrackingSurfaceDimensions = Dims;
	}

	GConfig->GetBool(*SectionName, TEXT("UseNormalizedCoordinates"), DiskConfig.bUseNormalizedCoordinates, ConfigPath);

	// Actor Spawning
	FString SpawnCollisionStr;
	if (GConfig->GetString(*SectionName, TEXT("SpawnCollisionHandling"), SpawnCollisionStr, ConfigPath))
	{
		if (SpawnCollisionStr.Equals(TEXT("AlwaysSpawn"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		else if (SpawnCollisionStr.Equals(TEXT("AdjustIfPossibleButAlwaysSpawn"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		else if (SpawnCollisionStr.Equals(TEXT("AdjustIfPossibleButDontSpawnIfColliding"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
		else if (SpawnCollisionStr.Equals(TEXT("DontSpawnIfColliding"), ESearchCase::IgnoreCase))
			DiskConfig.SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	}
	GConfig->GetBool(*SectionName, TEXT("AutoDestroyOnTrackLost"), DiskConfig.bAutoDestroyOnTrackLost, ConfigPath);

	// nDisplay Actor Pool Configuration
	GConfig->GetBool(*SectionName, TEXT("UseActorPool"), DiskConfig.bUseActorPool, ConfigPath);
	GConfig->GetInt(*SectionName, TEXT("ActorPoolSize"), DiskConfig.ActorPoolSize, ConfigPath);

	// Pool spawn location - Vector format: PoolSpawnLocation=(X=...,Y=...,Z=...)
	FString PoolSpawnLocationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnLocation"), PoolSpawnLocationStr, ConfigPath))
	{
		FVector PoolSpawnLocation;
		if (PoolSpawnLocation.InitFromString(PoolSpawnLocationStr))
			DiskConfig.PoolSpawnLocation = PoolSpawnLocation;
	}

	// Pool spawn rotation - Rotator format: PoolSpawnRotation=(Pitch=...,Yaw=...,Roll=...)
	FString PoolSpawnRotationStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolSpawnRotation"), PoolSpawnRotationStr, ConfigPath))
	{
		FRotator PoolSpawnRotation;
		if (PoolSpawnRotation.InitFromString(PoolSpawnRotationStr))
			DiskConfig.PoolSpawnRotation = PoolSpawnRotation;
	}

	// Pool index offset - Vector format: PoolIndexOffset=(X=...,Y=...,Z=...)
	FString PoolIndexOffsetStr;
	if (GConfig->GetString(*SectionName, TEXT("PoolIndexOffset"), PoolIndexOffsetStr, ConfigPath))
	{
		FVector PoolIndexOffset;
		if (PoolIndexOffset.InitFromString(PoolIndexOffsetStr))
			DiskConfig.PoolIndexOffset = PoolIndexOffset;
	}

	// Transform & Performance
	GConfig->GetBool(*SectionName, TEXT("UseLocalSpace"), DiskConfig.bUseLocalSpace, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("ApplyOrientationFromMovement"), DiskConfig.bApplyOrientationFromMovement, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LiveAdjustments"), DiskConfig.bLiveAdjustments, ConfigPath);
	GConfig->GetFloat(*SectionName, TEXT("TrackLostTimeout"), DiskConfig.TrackLostTimeout, ConfigPath);

	// Logging & Debug
	GConfig->GetBool(*SectionName, TEXT("LogTrackerSpawned"), DiskConfig.bLogTrackerSpawned, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerUpdated"), DiskConfig.bLogTrackerUpdated, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogTrackerRemoved"), DiskConfig.bLogTrackerRemoved, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogNetworkStats"), DiskConfig.bLogNetworkStats, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogRegionAssignment"), DiskConfig.bLogRegionAssignment, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("LogRejectedTracks"), DiskConfig.bLogRejectedTracks, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugVisualization"), DiskConfig.bDebugVisualization, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawBounds"), DiskConfig.bDebugDrawBounds, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawOrigin"), DiskConfig.bDebugDrawOrigin, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawWallPlanes"), DiskConfig.bDebugDrawWallPlanes, ConfigPath);
	GConfig->GetBool(*SectionName, TEXT("DebugDrawRegionBoundaries"), DiskConfig.bDebugDrawRegionBoundaries, ConfigPath);

	OutConfig = DiskConfig;

	UE_LOG(LogAefPharus, Log, TEXT("Wall settings loaded from disk for '%s': MappingMode=%s, UDPPort=%d, TrackingSurfaceDimensions=%s, UseNormalizedCoords=%s, WallRegions=%d"),
		*InstanceName.ToString(),
		*UEnum::GetValueAsString(OutConfig.MappingMode),
		OutConfig.UDPPort,
		*OutConfig.TrackingSurfaceDimensions.ToString(),
		OutConfig.bUseNormalizedCoordinates ? TEXT("true") : TEXT("false"),
		OutConfig.WallRegions.Num());

	return true;
}
