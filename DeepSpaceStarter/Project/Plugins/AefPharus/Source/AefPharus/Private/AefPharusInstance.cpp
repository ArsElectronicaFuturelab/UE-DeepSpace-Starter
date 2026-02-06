/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Tracker Instance Implementation

   Complete implementation of a single Pharus tracking system instance.
   Handles network communication, track management, and actor spawning.
  ========================================================================*/

#include "AefPharusInstance.h"
#include "AefPharus.h"
#include "AefPharusActorInterface.h"
#include "AefPharusActorPool.h"
#include "AefPharusSubsystem.h"
#include "AefPharusRootOriginActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"  // For TActorIterator (used in FindExistingActorByName)
#include "GameFramework/Actor.h"
#include "Containers/Ticker.h"

// nDisplay support (optional - only if DisplayCluster module is available)
#if WITH_EDITOR || UE_BUILD_SHIPPING || UE_BUILD_DEVELOPMENT
	#include "Modules/ModuleManager.h"
	#if __has_include("DisplayClusterBlueprintLib.h")
		#include "DisplayClusterBlueprintLib.h"
		#include "DisplayClusterEnums.h"
		#define AefPharus_NDISPLAY_SUPPORT 1
	#else
		#define AefPharus_NDISPLAY_SUPPORT 0
	#endif
#else
	#define AefPharus_NDISPLAY_SUPPORT 0
#endif

//--------------------------------------------------------------------------------
// Constructor / Destructor
//--------------------------------------------------------------------------------

UAefPharusInstance::UAefPharusInstance()
	: bIsRunning(false)
	, WorldContext(nullptr)
	, ActorPool(nullptr)
{
}

UAefPharusInstance::~UAefPharusInstance()
{
	Shutdown();
}

//--------------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------------

bool UAefPharusInstance::Initialize(const FAefPharusInstanceConfig& InConfig, UWorld* InWorld, TSubclassOf<AActor> InSpawnClass)
{
	if (bIsRunning)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Instance '%s' already running"), *InConfig.InstanceName.ToString());
		return false;
	}

	if (!InWorld)
	{
		UE_LOG(LogAefPharus, Error, TEXT("Cannot initialize instance '%s' - invalid World context"), *InConfig.InstanceName.ToString());
		return false;
	}

	Config = InConfig;
	SpawnClass = InSpawnClass;
	WorldContext = InWorld;

	// Create TrackLinkClient
	try
	{
		// Prepare BindNIC string for TrackLinkClient
		// IMPORTANT: TrackLinkClient stores a pointer to this string, so we must keep it alive
		const char* BindNICPtr = nullptr;

		FString TrimmedBindNIC = Config.BindNIC.TrimStartAndEnd();
		if (!TrimmedBindNIC.IsEmpty())
		{
			// Use specific network interface
			// Convert FString to ANSI and store in persistent TArray
			// +1 for null terminator
			BindNICStorage.SetNumUninitialized(TrimmedBindNIC.Len() + 1);
			FCStringAnsi::Strncpy(BindNICStorage.GetData(), TCHAR_TO_ANSI(*TrimmedBindNIC), BindNICStorage.Num());
			BindNICPtr = BindNICStorage.GetData();
		}
		else
		{
			// Use INADDR_ANY (bind to all interfaces)
			// IMPORTANT: Must use "0.0.0.0" string, NOT nullptr (inet_addr cannot handle nullptr!)
			BindNICStorage.SetNumUninitialized(8);  // "0.0.0.0" + null terminator
			FCStringAnsi::Strncpy(BindNICStorage.GetData(), "0.0.0.0", BindNICStorage.Num());
			BindNICPtr = BindNICStorage.GetData();
		}

		// Prepare MulticastGroup string for TrackLinkClient
		// IMPORTANT: TrackLinkClient stores a pointer to this string, so we must keep it alive
		const char* MulticastGroupPtr = nullptr;

		FString TrimmedMulticastGroup = Config.MulticastGroup.TrimStartAndEnd();
		if (!TrimmedMulticastGroup.IsEmpty())
		{
			// Convert FString to ANSI and store in persistent TArray
			MulticastGroupStorage.SetNumUninitialized(TrimmedMulticastGroup.Len() + 1);
			FCStringAnsi::Strncpy(MulticastGroupStorage.GetData(), TCHAR_TO_ANSI(*TrimmedMulticastGroup), MulticastGroupStorage.Num());
			MulticastGroupPtr = MulticastGroupStorage.GetData();
		}
		else
		{
			// Use default multicast group
			const char* DefaultMulticast = "239.1.1.1";
			MulticastGroupStorage.SetNumUninitialized(FCStringAnsi::Strlen(DefaultMulticast) + 1);
			FCStringAnsi::Strncpy(MulticastGroupStorage.GetData(), DefaultMulticast, MulticastGroupStorage.Num());
			MulticastGroupPtr = MulticastGroupStorage.GetData();
		}

		TrackLinkClient = MakeUnique<pharus::TrackLinkClient>(
			Config.bIsMulticast,
			BindNICPtr,
			Config.UDPPort,
			MulticastGroupPtr
		);
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogAefPharus, Error, TEXT("Failed to create TrackLinkClient for instance '%s': %hs"),
			*Config.InstanceName.ToString(), e.what());
		return false;
	}

	// Register as receiver
	if (TrackLinkClient)
	{
		TrackLinkClient->registerTrackReceiver(this);
	}

	// Register tick delegate for processing pending operations
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UAefPharusInstance::ProcessPendingOperations),
		0.0f // Every frame
	);

	// Initialize actor pool if enabled (runs on ALL cluster nodes)
	if (Config.bUseActorPool && InSpawnClass)
	{
		ActorPool = NewObject<UAefPharusActorPool>(this);
		if (ActorPool)
		{
			bool bPoolInitialized = ActorPool->Initialize(
				InWorld,
				InSpawnClass,
				Config.ActorPoolSize,
				Config.InstanceName,
				Config.PoolSpawnLocation,
				Config.PoolSpawnRotation,
				Config.PoolIndexOffset
			);
			if (bPoolInitialized)
			{
				UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': Actor pool initialized with %d actors"),
					*Config.InstanceName.ToString(), Config.ActorPoolSize);
			}
			else
			{
				UE_LOG(LogAefPharus, Warning, TEXT("Instance '%s': Actor pool initialization failed"),
					*Config.InstanceName.ToString());
				ActorPool = nullptr;
			}
		}
	}
	else if (!Config.bUseActorPool)
	{
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': Actor pool disabled, using dynamic spawning"),
			*Config.InstanceName.ToString());
	}

	bIsRunning = true;

	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' initialized successfully (%s:%d, MappingMode: %s)"),
		*Config.InstanceName.ToString(),
		*Config.BindNIC,
		Config.UDPPort,
		*UEnum::GetValueAsString(Config.MappingMode));

	return true;
}

void UAefPharusInstance::Shutdown()
{
	if (!bIsRunning)
	{
		return;
	}

	// Unregister tick delegate
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}

	// Unregister from TrackLinkClient
	if (TrackLinkClient)
	{
		TrackLinkClient->unregisterTrackReceiver(this);
		TrackLinkClient.Reset();
	}

	// Shutdown actor pool if exists
	if (ActorPool)
	{
		ActorPool->Shutdown();
		ActorPool = nullptr;
	}

	// Destroy all spawned actors (only if not using pool)
	if (!Config.bUseActorPool)
	{
		for (auto& Pair : SpawnedActors)
		{
			if (Pair.Value && IsValid(Pair.Value))
			{
				Pair.Value->Destroy();
			}
		}
	}
	
	SpawnedActors.Empty();
	TrackToPoolIndex.Empty();
	TrackDataCache.Empty();

	bIsRunning = false;

	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' shutdown complete"), *Config.InstanceName.ToString());
}

//--------------------------------------------------------------------------------
// ITrackReceiver Interface (Network Thread)
//--------------------------------------------------------------------------------

void UAefPharusInstance::onTrackNew(const pharus::TrackRecord& Track)
{
	// Use relPos (TUIO-normalized 0-1 coordinates) instead of currentPos (absolute meters)
	// TUIO has origin top-left (Y=0 at top), flip Y to match UE coordinate system
	const FVector2D InputPos = FVector2D(Track.relPos.x, 1.0f - Track.relPos.y);
	const bool bIsValid = IsTrackPositionValid(InputPos);

	FScopeLock Lock(&PendingOperationsMutex);

	if (!bIsValid)
	{
		// Track starts outside valid bounds - remember it but don't spawn
		// Still add to TrackDataCache for timeout tracking!
		TracksOutsideBounds.Add(Track.trackID);
		
		FAefPharusTrackData TrackData;
		TrackData.TrackID = Track.trackID;
		TrackData.LastUpdateTime = FPlatformTime::Seconds();
		TrackData.bIsInsideBoundary = false;  // Outside bounds
		TrackDataCache.Add(Track.trackID, TrackData);
		
		if (Config.bLogRejectedTracks)
		{
			const FVector2D NormalizedPos = NormalizeTrackPosition(InputPos);
			UE_LOG(LogAefPharus, Warning, 
				TEXT("[%s] Track %d outside bounds (new) - position (%.3f, %.3f) normalized (%.3f, %.3f) - waiting for valid position"),
				*Config.InstanceName.ToString(), Track.trackID, 
				InputPos.X, InputPos.Y, NormalizedPos.X, NormalizedPos.Y);
		}
		return;
	}

	// Track is valid - spawn actor
	const FVector WorldPos = TrackToWorld(InputPos, Track);
	const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos);

	PendingSpawns.AddUnique(Track.trackID);
	TrackDataCache.Add(Track.trackID, TrackData);

	if (Config.bLogTrackerSpawned)
	{
		// Debug: Show raw TUIO coordinates and transformed world position for mirroring diagnosis
		UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d spawned at %s (Speed: %.2f cm/s) | RAW TUIO: (%.3f, %.3f) -> InputPos: (%.3f, %.3f)"),
			*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString(), Track.speed * 100.0f,
			Track.relPos.x, Track.relPos.y, InputPos.X, InputPos.Y);
	}
}

void UAefPharusInstance::onTrackUpdate(const pharus::TrackRecord& Track)
{
	// Use relPos (TUIO-normalized 0-1 coordinates) instead of currentPos (absolute meters)
	// TUIO has origin top-left (Y=0 at top), flip Y to match UE coordinate system
	const FVector2D InputPos = FVector2D(Track.relPos.x, 1.0f - Track.relPos.y);
	const bool bIsValid = IsTrackPositionValid(InputPos);

	FScopeLock Lock(&PendingOperationsMutex);

	const bool bWasOutside = TracksOutsideBounds.Contains(Track.trackID);

	if (!bIsValid)
	{
		// Track is outside valid bounds - always update timestamp for timeout tracking
		FAefPharusTrackData* ExistingData = TrackDataCache.Find(Track.trackID);
		if (ExistingData)
		{
			ExistingData->LastUpdateTime = FPlatformTime::Seconds();
			ExistingData->bIsInsideBoundary = false;  // Track is outside bounds
		}
		else
		{
			// Track not in cache yet - add it for timeout tracking
			FAefPharusTrackData TrackData;
			TrackData.TrackID = Track.trackID;
			TrackData.LastUpdateTime = FPlatformTime::Seconds();
			TrackData.bIsInsideBoundary = false;  // Outside bounds
			TrackDataCache.Add(Track.trackID, TrackData);
		}
		
		if (!bWasOutside)
		{
			// Track moved OUT of bounds - mark and remove actor
			TracksOutsideBounds.Add(Track.trackID);
			PendingRemovals.AddUnique(Track.trackID);
			
			if (Config.bLogRejectedTracks)
			{
				const FVector2D NormalizedPos = NormalizeTrackPosition(InputPos);
				UE_LOG(LogAefPharus, Warning, 
					TEXT("[%s] Track %d LEFT valid bounds - position (%.3f, %.3f) normalized (%.3f, %.3f) - removing actor"),
					*Config.InstanceName.ToString(), Track.trackID, 
					InputPos.X, InputPos.Y, NormalizedPos.X, NormalizedPos.Y);
			}
		}
		// else: already outside, just updated timestamp
		return;
	}

	// Track is inside valid bounds
	if (bWasOutside)
	{
		// Track moved INTO bounds - spawn actor
		TracksOutsideBounds.Remove(Track.trackID);
		
		const FVector WorldPos = TrackToWorld(InputPos, Track);
		const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos);
		
		PendingSpawns.AddUnique(Track.trackID);
		TrackDataCache.Add(Track.trackID, TrackData);
		
		if (Config.bLogTrackerSpawned)
		{
			UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d ENTERED valid bounds - spawning at %s"),
				*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString());
		}
		return;
	}

	// Track is inside and was inside - normal update path
	// Update LastUpdateTime to prevent timeout on static tracks
	FAefPharusTrackData* ExistingData = TrackDataCache.Find(Track.trackID);
	if (!ExistingData)
	{
		// Edge case: Track was never properly spawned, spawn it now
		const FVector WorldPos = TrackToWorld(InputPos, Track);
		const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos);
		
		PendingSpawns.AddUnique(Track.trackID);
		TrackDataCache.Add(Track.trackID, TrackData);
		
		if (Config.bLogTrackerSpawned)
		{
			UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d spawned (recovery) at %s"),
				*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString());
		}
		return;
	}

	ExistingData->LastUpdateTime = FPlatformTime::Seconds();

	// ALWAYS recalculate world position (RootOrigin/RootRotation may have changed)
	const FVector WorldPos = TrackToWorld(InputPos, Track);
	const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos);
	
	// Update the cached data (so actor always has correct position relative to RootOrigin)
	TrackDataCache.FindOrAdd(Track.trackID) = TrackData;
	
	// Always queue update - even for static trackers we need to apply RootOrigin changes
	PendingUpdates.AddUnique(Track.trackID);

	if (Config.bLogTrackerUpdated)
	{
		UE_LOG(LogAefPharus, VeryVerbose, TEXT("[%s] Track %d updated at %s"),
			*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString());
	}
}

void UAefPharusInstance::onTrackLost(const pharus::TrackRecord& Track)
{
	FScopeLock Lock(&PendingOperationsMutex);
	
	// Clean up tracking state
	const bool bWasOutside = TracksOutsideBounds.Contains(Track.trackID);
	TracksOutsideBounds.Remove(Track.trackID);
	
	// Only queue removal if track had an actor (was inside bounds)
	if (!bWasOutside)
	{
		PendingRemovals.AddUnique(Track.trackID);
	}

	if (Config.bLogTrackerRemoved)
	{
		UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d lost%s"),
			*Config.InstanceName.ToString(), Track.trackID,
			bWasOutside ? TEXT(" (was outside bounds)") : TEXT(""));
	}
}

//--------------------------------------------------------------------------------
// Track Data Access (Game Thread)
//--------------------------------------------------------------------------------

bool UAefPharusInstance::GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation, bool& bOutIsInsideBoundary)
{
	FScopeLock Lock(&PendingOperationsMutex);
	FAefPharusTrackData* Data = TrackDataCache.Find(TrackID);
	if (!Data)
	{
		return false;
	}

	OutPosition = Data->WorldPosition;
	OutRotation = GetRotationFromDirection(Data->Orientation);
	
	// Check if track is inside valid bounds (NOT in TracksOutsideBounds set)
	// This also updates the cached value for consistency
	bOutIsInsideBoundary = !TracksOutsideBounds.Contains(TrackID);
	
	return true;
}

TArray<int32> UAefPharusInstance::GetActiveTrackIDs() const
{
	FScopeLock Lock(&PendingOperationsMutex);
	TArray<int32> TrackIDs;
	TrackDataCache.GetKeys(TrackIDs);
	return TrackIDs;
}

int32 UAefPharusInstance::GetActiveTrackCount() const
{
	FScopeLock Lock(&PendingOperationsMutex);
	return TrackDataCache.Num();
}

AActor* UAefPharusInstance::GetSpawnedActor(int32 TrackID)
{
	FScopeLock Lock(&ActorSpawnMutex);
	AActor** ActorPtr = SpawnedActors.Find(TrackID);
	return ActorPtr ? *ActorPtr : nullptr;
}

bool UAefPharusInstance::IsTrackActive(int32 TrackID) const
{
	FScopeLock Lock(&PendingOperationsMutex);
	return TrackDataCache.Contains(TrackID);
}

//--------------------------------------------------------------------------------
// Configuration
//--------------------------------------------------------------------------------

bool UAefPharusInstance::UpdateConfig(const FAefPharusInstanceConfig& NewConfig)
{
	if (!Config.bLiveAdjustments)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Live adjustments disabled for instance '%s'"),
			*Config.InstanceName.ToString());
		return false;
	}

	// Update config (network settings cannot be changed at runtime)
	// NOTE: Origin is now global via UAefPharusSubsystem::SetRootOrigin()
	Config.SimpleScale = NewConfig.SimpleScale;
	Config.FloorZ = NewConfig.FloorZ;
	Config.FloorRotation = NewConfig.FloorRotation;
	Config.bInvertY = NewConfig.bInvertY;
	Config.WallRegions = NewConfig.WallRegions;
	Config.bDebugVisualization = NewConfig.bDebugVisualization;

	UE_LOG(LogAefPharus, Log, TEXT("Configuration updated for instance '%s'"),
		*Config.InstanceName.ToString());

	return true;
}

bool UAefPharusInstance::UpdateFloorSettings(const FAefPharusInstanceConfig& NewConfig)
{
	if (!Config.bLiveAdjustments)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Live adjustments disabled for instance '%s'"),
			*Config.InstanceName.ToString());
		return false;
	}

	// Update Floor settings (only settings that are in AefConfig.ini)

	// Basic Settings (Enable cannot be changed at runtime)
	Config.MappingMode = NewConfig.MappingMode;

	// Coordinate System Settings (Origin is now global via subsystem)
	Config.SimpleScale = NewConfig.SimpleScale;
	Config.FloorZ = NewConfig.FloorZ;
	Config.FloorRotation = NewConfig.FloorRotation;
	Config.bInvertY = NewConfig.bInvertY;

	// Tracking Surface Configuration
	Config.TrackingSurfaceDimensions = NewConfig.TrackingSurfaceDimensions;
	Config.bUseNormalizedCoordinates = NewConfig.bUseNormalizedCoordinates;

	// Actor Spawning
	Config.SpawnCollisionHandling = NewConfig.SpawnCollisionHandling;
	Config.bAutoDestroyOnTrackLost = NewConfig.bAutoDestroyOnTrackLost;

	UE_LOG(LogAefPharus, Log, TEXT("Floor settings updated: Scale=%s, FloorZ=%.2f, Rotation=%.2f, InvertY=%s"),
		*Config.SimpleScale.ToString(),
		Config.FloorZ,
		Config.FloorRotation,
		Config.bInvertY ? TEXT("true") : TEXT("false"));

	return true;
}

bool UAefPharusInstance::UpdateFloorSettingsSimple(float OriginX, float OriginY, float ScaleX, float ScaleY, float FloorZ, float FloorRotation, bool bInvertY)
{
	if (!Config.bLiveAdjustments)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Live adjustments disabled for instance '%s'"),
			*Config.InstanceName.ToString());
		return false;
	}

	// NOTE: OriginX/OriginY parameters are now IGNORED - use UAefPharusSubsystem::SetRootOrigin() instead
	UE_LOG(LogAefPharus, Warning, TEXT("UpdateFloorSettingsSimple: OriginX/OriginY are deprecated and ignored. Use UAefPharusSubsystem::SetRootOrigin() for global origin."));

	// Update non-origin settings
	Config.SimpleScale = FVector2D(ScaleX, ScaleY);
	Config.FloorZ = FloorZ;
	Config.FloorRotation = FloorRotation;
	Config.bInvertY = bInvertY;

	UE_LOG(LogAefPharus, Log, TEXT("Floor settings updated for instance '%s': Scale=(%.2f, %.2f), FloorZ=%.2f, Rotation=%.2f, InvertY=%s"),
		*Config.InstanceName.ToString(), ScaleX, ScaleY, FloorZ, FloorRotation, bInvertY ? TEXT("true") : TEXT("false"));

	return true;
}

bool UAefPharusInstance::UpdateWallSettings(const FAefPharusInstanceConfig& NewConfig)
{
	if (!Config.bLiveAdjustments)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Live adjustments disabled for instance '%s'"),
			*Config.InstanceName.ToString());
		return false;
	}

	// Update Wall settings (only settings that are in AefConfig.ini)

	// Basic Settings (Enable cannot be changed at runtime)
	Config.MappingMode = NewConfig.MappingMode;

	// Tracking Surface Configuration
	Config.TrackingSurfaceDimensions = NewConfig.TrackingSurfaceDimensions;
	Config.bUseNormalizedCoordinates = NewConfig.bUseNormalizedCoordinates;

	// Actor Spawning
	Config.SpawnCollisionHandling = NewConfig.SpawnCollisionHandling;
	Config.bAutoDestroyOnTrackLost = NewConfig.bAutoDestroyOnTrackLost;

	UE_LOG(LogAefPharus, Log, TEXT("Wall settings updated: TrackingSurfaceDimensions=%s, UseNormalizedCoords=%s"),
		*Config.TrackingSurfaceDimensions.ToString(),
		Config.bUseNormalizedCoordinates ? TEXT("true") : TEXT("false"));

	return true;
}

void UAefPharusInstance::SetSpawnClass(TSubclassOf<AActor> NewSpawnClass)
{
	if (NewSpawnClass)
	{
		// Validate interface implementation
		if (!NewSpawnClass->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Instance '%s': SpawnClass '%s' does not implement IAefPharusActorInterface - track IDs will not be set"),
				*Config.InstanceName.ToString(), *NewSpawnClass->GetName());
		}
		
		SpawnClass = NewSpawnClass;
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': SpawnClass changed to '%s'"),
			*Config.InstanceName.ToString(), *NewSpawnClass->GetName());
	}
	else
	{
		SpawnClass = nullptr;
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': SpawnClass cleared (no actors will spawn for new tracks)"),
			*Config.InstanceName.ToString());
	}
}

bool UAefPharusInstance::RestartWithNewNetwork(const FString& NewBindNIC, int32 NewUDPPort)
{
	if (!Config.bLiveAdjustments)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Live adjustments disabled for instance '%s'"),
			*Config.InstanceName.ToString());
		return false;
	}

	if (!bIsRunning)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Instance '%s' is not running - cannot restart"),
			*Config.InstanceName.ToString());
		return false;
	}

	// Save important state before shutdown
	TSubclassOf<AActor> SavedSpawnClass = SpawnClass;
	UWorld* SavedWorldContext = WorldContext;
	FAefPharusInstanceConfig SavedConfig = Config;

	// Update network settings in config
	SavedConfig.BindNIC = NewBindNIC;
	SavedConfig.UDPPort = NewUDPPort;

	// Shutdown current connection (will destroy all actors)
	Shutdown();

	// Reinitialize with new network settings
	bool bSuccess = Initialize(SavedConfig, SavedWorldContext, SavedSpawnClass);

	if (bSuccess)
	{
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' restarted: %s:%d"),
			*Config.InstanceName.ToString(), *NewBindNIC, NewUDPPort);
	}
	else
	{
		UE_LOG(LogAefPharus, Error, TEXT("Failed to restart instance '%s' with %s:%d"),
			*SavedConfig.InstanceName.ToString(), *NewBindNIC, NewUDPPort);
	}

	return bSuccess;
}

//--------------------------------------------------------------------------------
// Coordinate Transformation (Network Thread)
//--------------------------------------------------------------------------------

FVector UAefPharusInstance::TrackToWorld(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const
{
	switch (Config.MappingMode)
	{
		case EAefPharusMappingMode::Simple:
			return TrackToWorldFloor(TrackPos);

		case EAefPharusMappingMode::Regions:
		{
			EAefPharusWallSide Wall;
			return TrackToWorldRegions(TrackPos, Wall);
		}

		default:
			UE_LOG(LogAefPharus, Warning, TEXT("Unknown mapping mode, falling back to Floor"));
			return TrackToWorldFloor(TrackPos);
	}
}

FVector UAefPharusInstance::TrackToWorldFloor(const FVector2D& TrackPos) const
{
	// Step 1: Handle coordinate normalization based on explicit flag
	FVector2D AdjustedPos = TrackPos;

	if (Config.bUseNormalizedCoordinates)
	{
		// Input is already normalized (0-1 range)
		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Using normalized floor coordinates (%.3f, %.3f)"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y);
	}
	else
	{
		// Input is absolute world coordinates - normalize them
		AdjustedPos.X = TrackPos.X / Config.TrackingSurfaceDimensions.X;
		AdjustedPos.Y = TrackPos.Y / Config.TrackingSurfaceDimensions.Y;

		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Normalized floor coordinates (%.3f, %.3f) → (%.3f, %.3f)"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y, AdjustedPos.X, AdjustedPos.Y);
	}

	// Step 2: Invert Y if needed
	// For normalized coordinates (0-1, TUIO-style with origin top-left):
	//   Use 1.0-Y to flip within the 0-1 range
	// For absolute coordinates (meters):
	//   Use -Y to mirror around Y=0
	if (Config.bInvertY)
	{
		if (Config.bUseNormalizedCoordinates)
		{
			AdjustedPos.Y = 1.0f - AdjustedPos.Y;  // Flip within 0-1 range
		}
		else
		{
			AdjustedPos.Y = -AdjustedPos.Y;  // Mirror around Y=0
		}
	}

	// Step 3: Apply scale
	FVector2D ScaledPos = AdjustedPos * Config.SimpleScale;

	// Step 4: Apply floor-specific 2D rotation (convert degrees to radians)
	if (!FMath::IsNearlyZero(Config.FloorRotation))
	{
		float RotationRad = FMath::DegreesToRadians(Config.FloorRotation);
		float CosRot = FMath::Cos(RotationRad);
		float SinRot = FMath::Sin(RotationRad);

		// Rotate 2D point around origin
		FVector2D RotatedPos;
		RotatedPos.X = ScaledPos.X * CosRot - ScaledPos.Y * SinRot;
		RotatedPos.Y = ScaledPos.X * SinRot + ScaledPos.Y * CosRot;
		ScaledPos = RotatedPos;
	}

	// Step 5: Get global root origin and rotation from subsystem
	FVector RootOrigin = GetRootOrigin();
	FRotator RootRotation = GetRootOriginRotation();

	// Step 6: Build local position (relative to root origin, before global rotation)
	// FloorZ is a height offset relative to the local coordinate system
	FVector LocalPos(ScaledPos.X, ScaledPos.Y, Config.FloorZ);

	// Step 7: Apply global root rotation to transform local position
	// This rotates the entire tracking coordinate system around the root origin
	// Full 3D rotation is applied to support tilted stages and complex setups
	FVector RotatedPos = RootRotation.RotateVector(LocalPos);

	// Step 8: Add root origin to get final world position
	FVector WorldPos = RootOrigin + RotatedPos;

	UE_LOG(LogAefPharus, Verbose, TEXT("[%s] TrackToWorldFloor: Local=%s, RootRotation=%s, Rotated=%s, Final=%s"),
		*Config.InstanceName.ToString(), *LocalPos.ToString(), *RootRotation.ToString(), 
		*RotatedPos.ToString(), *WorldPos.ToString());

	return WorldPos;
}

FVector UAefPharusInstance::GetRootOrigin() const
{
	// Get the owning subsystem (this UObject is owned by the subsystem)
	UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
	if (Subsystem)
	{
		return Subsystem->GetRootOrigin();
	}

	// Fallback (should never happen in normal operation)
	UE_LOG(LogAefPharus, Warning, TEXT("[%s] GetRootOrigin: No owning subsystem found - using zero origin"),
		*Config.InstanceName.ToString());
	return FVector::ZeroVector;
}

FRotator UAefPharusInstance::GetRootOriginRotation() const
{
	// Get the owning subsystem (this UObject is owned by the subsystem)
	UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
	if (Subsystem)
	{
		return Subsystem->GetRootOriginRotation();
	}

	// Fallback (should never happen in normal operation)
	UE_LOG(LogAefPharus, Warning, TEXT("[%s] GetRootOriginRotation: No owning subsystem found - using zero rotation"),
		*Config.InstanceName.ToString());
	return FRotator::ZeroRotator;
}

FVector UAefPharusInstance::TrackToWorldRegions(const FVector2D& TrackPos, EAefPharusWallSide& OutWall) const
{
	// Handle coordinate normalization based on explicit flag
	FVector2D NormalizedPos = TrackPos;

	if (Config.bUseNormalizedCoordinates)
	{
		// Input is already normalized (0-1 range)
		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Using normalized coordinates (%.3f, %.3f)"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y);
	}
	else
	{
		// Input is absolute world coordinates - normalize them
		NormalizedPos.X = TrackPos.X / Config.TrackingSurfaceDimensions.X;
		NormalizedPos.Y = TrackPos.Y / Config.TrackingSurfaceDimensions.Y;

		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Normalized absolute coordinates (%.3f, %.3f) → (%.3f, %.3f)"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y, NormalizedPos.X, NormalizedPos.Y);
	}

	// Find matching wall region
	// NOTE: IsTrackPositionValid() should have already rejected invalid positions,
	//       so this should always find a region. This is a safety fallback.
	const FAefPharusWallRegion* Region = FindWallRegion(NormalizedPos);
	if (!Region)
	{
		// This should never happen if IsTrackPositionValid() was called first
		UE_LOG(LogAefPharus, Error, TEXT("[%s] Track position (%.3f, %.3f) outside all wall regions - this should have been rejected earlier!"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y);
		OutWall = EAefPharusWallSide::Floor;
		return FVector::ZeroVector;
	}

	OutWall = Region->WallSide;

	if (Config.bLogRegionAssignment)
	{
		UE_LOG(LogAefPharus, Log, TEXT("[%s] Track assigned to %s wall (NormalizedPos: %.3f, %.3f)"),
			*Config.InstanceName.ToString(),
			*UEnum::GetValueAsString(OutWall),
			NormalizedPos.X, NormalizedPos.Y);
	}

	// Use region's transformation method with normalized coordinates, global root origin, rotation, and WallRotation
	FVector RootOrigin = GetRootOrigin();
	FRotator RootRotation = GetRootOriginRotation();
	return Region->TrackToWorld(NormalizedPos, RootOrigin, RootRotation, Config.WallRotation);
}

const FAefPharusWallRegion* UAefPharusInstance::FindWallRegion(const FVector2D& TrackPos) const
{
	// Debug: Log number of wall regions
	UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Checking %d wall regions for position (%.3f, %.3f)"),
		*Config.InstanceName.ToString(), Config.WallRegions.Num(), TrackPos.X, TrackPos.Y);

	// Check all regions and collect matches (due to overlapping regions)
	TArray<const FAefPharusWallRegion*> MatchingRegions;

	for (int32 i = 0; i < Config.WallRegions.Num(); ++i)
	{
		const FAefPharusWallRegion& Region = Config.WallRegions[i];
		bool bContains = Region.ContainsTrackPoint(TrackPos);

		UE_LOG(LogAefPharus, Verbose, TEXT("  Region %d (%s): Bounds=(%.3f-%.3f, %.3f-%.3f) Contains=%s"),
			i, *UEnum::GetValueAsString(Region.WallSide),
			Region.TrackingBounds.Min.X, Region.TrackingBounds.Max.X,
			Region.TrackingBounds.Min.Y, Region.TrackingBounds.Max.Y,
			bContains ? TEXT("YES") : TEXT("NO"));

		if (bContains)
		{
			MatchingRegions.Add(&Region);
		}
	}

	// If no regions match, return null
	if (MatchingRegions.Num() == 0)
	{
		return nullptr;
	}

	// If only one region matches, return it
	if (MatchingRegions.Num() == 1)
	{
		return MatchingRegions[0];
	}

	// Multiple regions match (corner case) - use priority logic
	// Priority: Front > Right > Back > Left (clockwise priority)
	for (const FAefPharusWallRegion* Region : MatchingRegions)
	{
		if (Region->WallSide == EAefPharusWallSide::Front)
			return Region;
	}

	for (const FAefPharusWallRegion* Region : MatchingRegions)
	{
		if (Region->WallSide == EAefPharusWallSide::Right)
			return Region;
	}

	for (const FAefPharusWallRegion* Region : MatchingRegions)
	{
		if (Region->WallSide == EAefPharusWallSide::Back)
			return Region;
	}

	for (const FAefPharusWallRegion* Region : MatchingRegions)
	{
		if (Region->WallSide == EAefPharusWallSide::Left)
			return Region;
	}

	// Fallback to first match
	return MatchingRegions[0];
}

//--------------------------------------------------------------------------------
// Local Coordinate Transformation (for Relative Spawning)
//--------------------------------------------------------------------------------

FVector UAefPharusInstance::TrackToLocal(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const
{
	switch (Config.MappingMode)
	{
		case EAefPharusMappingMode::Simple:
			return TrackToLocalFloor(TrackPos);

		case EAefPharusMappingMode::Regions:
		{
			EAefPharusWallSide Wall;
			return TrackToLocalRegions(TrackPos, Wall);
		}

		default:
			UE_LOG(LogAefPharus, Warning, TEXT("Unknown mapping mode, falling back to Floor"));
			return TrackToLocalFloor(TrackPos);
	}
}

FVector UAefPharusInstance::TrackToLocalFloor(const FVector2D& TrackPos) const
{
	// NOTE: TrackPos (from RawPosition) is ALWAYS normalized (0-1 range)
	// The normalization was already done when storing RawPosition in ConvertTrackData()
	// So we do NOT apply TrackingSurfaceDimensions normalization here!
	
	FVector2D AdjustedPos = TrackPos;

	// Step 1: Invert Y if needed
	// RawPosition is ALWAYS normalized (0-1), so use 1.0-Y for correct flip
	if (Config.bInvertY)
	{
		AdjustedPos.Y = 1.0f - AdjustedPos.Y;  // Flip within 0-1 range (TUIO origin top-left)
	}

	// Step 2: Apply scale (converts normalized 0-1 to actual world size in cm)
	FVector2D ScaledPos = AdjustedPos * Config.SimpleScale;

	// Step 3: Apply floor-specific 2D rotation (convert degrees to radians)
	if (!FMath::IsNearlyZero(Config.FloorRotation))
	{
		float RotationRad = FMath::DegreesToRadians(Config.FloorRotation);
		float CosRot = FMath::Cos(RotationRad);
		float SinRot = FMath::Sin(RotationRad);

		// Rotate 2D point around origin
		FVector2D RotatedPos;
		RotatedPos.X = ScaledPos.X * CosRot - ScaledPos.Y * SinRot;
		RotatedPos.Y = ScaledPos.X * SinRot + ScaledPos.Y * CosRot;
		ScaledPos = RotatedPos;
	}

	// Step 4: Build LOCAL position (relative to RootOriginActor)
	// NO RootOrigin/RootRotation applied - actor is attached as child
	FVector LocalPos(ScaledPos.X, ScaledPos.Y, Config.FloorZ);

	UE_LOG(LogAefPharus, Verbose, TEXT("[%s] TrackToLocalFloor: Input=%s, LocalPos=%s"),
		*Config.InstanceName.ToString(), *TrackPos.ToString(), *LocalPos.ToString());

	return LocalPos;
}

FVector UAefPharusInstance::TrackToLocalRegions(const FVector2D& TrackPos, EAefPharusWallSide& OutWall) const
{
	// NOTE: TrackPos (from RawPosition) is ALWAYS normalized (0-1 range)
	// The normalization was already done when storing RawPosition in ConvertTrackData()
	// So we use TrackPos directly for region lookup and transformation

	// Find matching wall region
	const FAefPharusWallRegion* Region = FindWallRegion(TrackPos);
	if (!Region)
	{
		UE_LOG(LogAefPharus, Error, TEXT("[%s] Track position (%.3f, %.3f) outside all wall regions!"),
			*Config.InstanceName.ToString(), TrackPos.X, TrackPos.Y);
		OutWall = EAefPharusWallSide::Floor;
		return FVector::ZeroVector;
	}

	OutWall = Region->WallSide;

	// Use region's local transformation method (no RootOrigin/RootRotation)
	return Region->TrackToLocal(TrackPos);
}

//--------------------------------------------------------------------------------
// Bounds Validation
//--------------------------------------------------------------------------------

FVector2D UAefPharusInstance::NormalizeTrackPosition(const FVector2D& InputPos) const
{
	if (Config.bUseNormalizedCoordinates)
	{
		// Input is already normalized (0-1 range)
		return InputPos;
	}

	// Normalize absolute coordinates using tracking surface dimensions
	return FVector2D(
		InputPos.X / Config.TrackingSurfaceDimensions.X,
		InputPos.Y / Config.TrackingSurfaceDimensions.Y
	);
}

bool UAefPharusInstance::IsTrackPositionValid(const FVector2D& InputPos) const
{
	const FVector2D NormalizedPos = NormalizeTrackPosition(InputPos);

	// For Floor mode (Simple): Check if normalized position is within 0-1 bounds
	if (Config.MappingMode == EAefPharusMappingMode::Simple)
	{
		const bool bIsValid = NormalizedPos.X >= 0.0f && NormalizedPos.X <= 1.0f &&
		                      NormalizedPos.Y >= 0.0f && NormalizedPos.Y <= 1.0f;
		return bIsValid;
	}

	// For Regions mode: Check if position falls within any defined wall region
	if (Config.MappingMode == EAefPharusMappingMode::Regions)
	{
		return FindWallRegion(NormalizedPos) != nullptr;
	}

	// Simple mode: Allow all normalized positions
	return true;
}

//--------------------------------------------------------------------------------
// Actor Management (Game Thread)
//--------------------------------------------------------------------------------

void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
	if (!WorldContext || !SpawnClass)
	{
		return;
	}

	//--------------------------------------------------------------------------------
	// nDisplay Cluster Check: Only activate actors on Primary Node
	//--------------------------------------------------------------------------------
	// If nDisplay is active in cluster mode, only the primary node activates actors.
	// Actor pool exists on ALL nodes, but only primary controls visibility.
#if AefPharus_NDISPLAY_SUPPORT
	if (UDisplayClusterBlueprintLib::IsModuleInitialized())
	{
		EDisplayClusterOperationMode OpMode = UDisplayClusterBlueprintLib::GetOperationMode();
		
		if (OpMode == EDisplayClusterOperationMode::Cluster)
		{
			// Check if we're on the primary node
			if (!UDisplayClusterBlueprintLib::IsPrimary())
			{
				// Skip actor activation on secondary/backup nodes
				UE_LOG(LogAefPharus, Verbose, 
					TEXT("[%s] Skipping actor activation on secondary cluster node for track %d"),
					*Config.InstanceName.ToString(), TrackID);
				return;
			}
			
			// Log activation on primary node
			UE_LOG(LogAefPharus, Verbose, 
				TEXT("[%s] Activating actor on primary cluster node for track %d"),
				*Config.InstanceName.ToString(), TrackID);
		}
	}
#endif
	//--------------------------------------------------------------------------------

	// Check if actor already exists
	{
		FScopeLock Lock(&ActorSpawnMutex);
		if (SpawnedActors.Contains(TrackID))
		{
			return;
		}
	}

	FAefPharusTrackData TrackDataCopy;
	{
		FScopeLock Lock(&PendingOperationsMutex);
		FAefPharusTrackData* TrackData = TrackDataCache.Find(TrackID);
		if (!TrackData)
		{
			return;
		}
		TrackDataCopy = *TrackData;
	}

	AActor* SpawnedActor = nullptr;
	int32 PoolIndex = INDEX_NONE;

	//--------------------------------------------------------------------------------
	// Actor Pool Mode: Acquire actor from pool
	//--------------------------------------------------------------------------------
	if (Config.bUseActorPool && ActorPool)
	{
		SpawnedActor = ActorPool->AcquireActor(PoolIndex);
		
		if (!SpawnedActor)
		{
			UE_LOG(LogAefPharus, Warning, TEXT("[%s] Actor pool exhausted for track %d (consider increasing ActorPoolSize)"),
				*Config.InstanceName.ToString(), TrackID);
			return;
		}

		// Store pool index mapping
		TrackToPoolIndex.Add(TrackID, PoolIndex);

		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Acquired actor from pool (index %d) for track %d"),
			*Config.InstanceName.ToString(), PoolIndex, TrackID);
	}
	//--------------------------------------------------------------------------------
	// Dynamic Spawn Mode: Spawn new actor or reuse existing
	//--------------------------------------------------------------------------------
	else
	{
		// First, check if an actor with this name already exists in the level
		// This can happen when bAutoDestroyOnTrackLost=false and a track re-enters bounds
		SpawnedActor = FindExistingActorByName(TrackID);

		if (SpawnedActor)
		{
			// Actor was found - reuse it
			UE_LOG(LogAefPharus, Log, 
				TEXT("[%s] Reusing existing actor for track %d (actor was not destroyed on previous exit)"),
				*Config.InstanceName.ToString(), TrackID);
			
			// Make sure it's visible and enabled
			SpawnedActor->SetActorHiddenInGame(false);
			SpawnedActor->SetActorEnableCollision(true);
			SpawnedActor->SetActorTickEnabled(true);
		}
		else
		{
			// No existing actor found - spawn new one
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = Config.SpawnCollisionHandling;
			
			// Construct expected name
			FString ActorName = FString::Printf(TEXT("PharusTrack_%s_%d"), 
				*Config.InstanceName.ToString(), TrackID);
			SpawnParams.Name = FName(*ActorName);
			
			// CRITICAL: Use NameMode to return nullptr on duplicate name instead of crashing
			// This happens when an actor exists but wasn't found by FindExistingActorByName
			// (e.g., actor created by level, different class, or TActorIterator timing issue)
			SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;

			FVector SpawnLocation = TrackDataCopy.WorldPosition;
			FRotator SpawnRotation = FRotator::ZeroRotator;
			SpawnedActor = WorldContext->SpawnActor(SpawnClass, &SpawnLocation, &SpawnRotation, SpawnParams);

			if (!SpawnedActor)
			{
				// Spawn failed - likely because name already exists
				// Try to find the existing actor by iterating all actors with matching class
				UE_LOG(LogAefPharus, Warning, 
					TEXT("[%s] Spawn failed for track %d with name '%s' - searching for existing actor by name..."),
					*Config.InstanceName.ToString(), TrackID, *ActorName);
				
				// More aggressive search: find by exact name using FindObject
				AActor* ExistingActor = FindObject<AActor>(WorldContext->GetCurrentLevel(), *ActorName);
				if (ExistingActor && IsValid(ExistingActor))
				{
					UE_LOG(LogAefPharus, Log, 
						TEXT("[%s] Found existing actor '%s' via FindObject for track %d - reusing"),
						*Config.InstanceName.ToString(), *ActorName, TrackID);
					
					SpawnedActor = ExistingActor;
					SpawnedActor->SetActorHiddenInGame(false);
					SpawnedActor->SetActorEnableCollision(true);
					SpawnedActor->SetActorTickEnabled(true);
				}
				else
				{
					// Still couldn't find it - spawn with auto-generated name as fallback
					UE_LOG(LogAefPharus, Warning, 
						TEXT("[%s] Could not find existing actor - spawning with auto-generated name"),
						*Config.InstanceName.ToString());
					
					SpawnParams.Name = NAME_None;  // Let UE generate unique name
					SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_Fatal;
					SpawnedActor = WorldContext->SpawnActor(SpawnClass, &SpawnLocation, &SpawnRotation, SpawnParams);
					
					if (!SpawnedActor)
					{
						UE_LOG(LogAefPharus, Error, TEXT("[%s] Failed to spawn actor for track %d"),
							*Config.InstanceName.ToString(), TrackID);
						return;
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------------
	// Configure Actor
	//--------------------------------------------------------------------------------
	if (SpawnedActor)
	{
		// Get the owning subsystem to check for relative spawning mode
		// UseRelativeSpawning in [PharusSubsystem] is the master switch
		// Per-instance bUseRelativeSpawning=false can override to disable for specific instances
		UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
		const bool bUseRelativeSpawning = Subsystem && Subsystem->IsRelativeSpawningActive();

		if (bUseRelativeSpawning)
		{
			//--------------------------------------------------------------------------------
			// RELATIVE SPAWNING MODE: Attach to RootOriginActor, use local coordinates
			//--------------------------------------------------------------------------------
			AAefPharusRootOriginActor* RootActor = Subsystem->GetRootOriginActor();
			if (RootActor)
			{
				// Calculate LOCAL position (no RootOrigin/RootRotation applied)
				const FVector2D InputPos = FVector2D(TrackDataCopy.RawPosition.X, TrackDataCopy.RawPosition.Y);
				
				// Get input position from cache (use original tracking coordinates)
				FVector LocalPos;
				{
					FScopeLock Lock(&PendingOperationsMutex);
					FAefPharusTrackData* CachedData = TrackDataCache.Find(TrackID);
					if (CachedData)
					{
						// Recalculate LOCAL position from raw coordinates
						pharus::TrackRecord DummyTrack;
						LocalPos = TrackToLocal(CachedData->RawPosition, DummyTrack);
					}
					else
					{
						LocalPos = FVector::ZeroVector;
					}
				}

				// Attach to RootOriginActor if not already attached
				if (SpawnedActor->GetAttachParentActor() != RootActor)
				{
					SpawnedActor->AttachToActor(RootActor, FAttachmentTransformRules::KeepRelativeTransform);
					UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d attached to RootOriginActor (relative spawning)"),
						*Config.InstanceName.ToString(), TrackID);
				}

				// Set LOCAL transform (relative to RootOriginActor)
				SpawnedActor->SetActorRelativeLocation(LocalPos);

				if (Config.bApplyOrientationFromMovement)
				{
					FRotator OrientationRotation;
					if (Config.MappingMode == EAefPharusMappingMode::Regions)
					{
						// Wall mode: Rotate around wall normal
						const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
						if (Region)
						{
							OrientationRotation = GetWallActorRotation(TrackDataCopy.Orientation, *Region);
						}
						else
						{
							OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
						}
					}
					else
					{
						// Floor mode: Yaw rotation around World Z
						OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
					}
					SpawnedActor->SetActorRelativeRotation(OrientationRotation);
				}
				else
				{
					// No orientation - use wall's base rotation for walls, zero for floor
					if (Config.MappingMode == EAefPharusMappingMode::Regions)
					{
						const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
						SpawnedActor->SetActorRelativeRotation(Region ? Region->WorldRotation : FRotator::ZeroRotator);
					}
					else
					{
						SpawnedActor->SetActorRelativeRotation(FRotator::ZeroRotator);
					}
				}
			}
			else
			{
				UE_LOG(LogAefPharus, Warning, TEXT("[%s] RelativeSpawning enabled but RootOriginActor is null - using absolute position"),
					*Config.InstanceName.ToString());
				SpawnedActor->SetActorLocation(TrackDataCopy.WorldPosition);
			}
		}
		else
		{
			//--------------------------------------------------------------------------------
			// ABSOLUTE SPAWNING MODE: Use world coordinates (original behavior)
			//--------------------------------------------------------------------------------
			SpawnedActor->SetActorLocation(TrackDataCopy.WorldPosition);
			
			// Get global root rotation
			// Actors are oriented RELATIVE TO the RootOriginActor (like being attached to a camera)
			// Full 3D rotation ensures actors stay correctly aligned when the origin moves/rotates
			FRotator RootRotation = GetRootOriginRotation();
			
			if (Config.bApplyOrientationFromMovement)
			{
				FRotator OrientationRotation;
				if (Config.MappingMode == EAefPharusMappingMode::Regions)
				{
					// Wall mode: Rotate around wall normal
					const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
					if (Region)
					{
						OrientationRotation = GetWallActorRotation(TrackDataCopy.Orientation, *Region);
					}
					else
					{
						OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
					}
				}
				else
				{
					// Floor mode: Yaw rotation around World Z
					OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
				}
				// Combine root rotation with orientation
				FRotator CombinedRotation = RootRotation + OrientationRotation;
				SpawnedActor->SetActorRotation(CombinedRotation);
			}
			else
			{
				// No orientation - use wall's base rotation for walls, root rotation for floor
				if (Config.MappingMode == EAefPharusMappingMode::Regions)
				{
					const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
					FRotator WallBaseRotation = Region ? Region->WorldRotation : FRotator::ZeroRotator;
					SpawnedActor->SetActorRotation(RootRotation + WallBaseRotation);
				}
				else
				{
					SpawnedActor->SetActorRotation(RootRotation);
				}
			}
		}

		// Call SetActorTrackInfo if actor implements IAefPharusActorInterface
		// This provides full context: TrackID + InstanceName
		if (SpawnedActor->GetClass()->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
		{
			IAefPharusActorInterface::Execute_SetActorTrackInfo(SpawnedActor, TrackID, Config.InstanceName);

			// Fire OnTrackConnected event for valid tracks (>= 0)
			IAefPharusActorInterface::Execute_OnTrackConnected(SpawnedActor, TrackID, Config.InstanceName);
		}

		// Store actor reference
		{
			FScopeLock Lock(&ActorSpawnMutex);
			SpawnedActors.Add(TrackID, SpawnedActor);
		}

		// Broadcast event
		OnTrackSpawned.Broadcast(TrackID, SpawnedActor);
	}
}

void UAefPharusInstance::UpdateActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
	AActor* Actor = nullptr;
	{
		FScopeLock Lock(&ActorSpawnMutex);
		AActor** ActorPtr = SpawnedActors.Find(TrackID);
		if (!ActorPtr || !IsValid(*ActorPtr))
		{
			return;
		}
		Actor = *ActorPtr;
	}

	FAefPharusTrackData TrackDataCopy;
	{
		FScopeLock Lock(&PendingOperationsMutex);
		FAefPharusTrackData* TrackData = TrackDataCache.Find(TrackID);
		if (!TrackData)
		{
			return;
		}
		TrackDataCopy = *TrackData;
	}

	// Get the owning subsystem to check for relative spawning mode
	// UseRelativeSpawning in [PharusSubsystem] is the master switch
	UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
	const bool bUseRelativeSpawning = Subsystem && Subsystem->IsRelativeSpawningActive();

	if (bUseRelativeSpawning)
	{
		//--------------------------------------------------------------------------------
		// RELATIVE SPAWNING MODE: Use local coordinates (actor is attached to RootOriginActor)
		//--------------------------------------------------------------------------------
		// Calculate LOCAL position from raw tracking coordinates
		pharus::TrackRecord DummyTrack;
		FVector LocalPos = TrackToLocal(TrackDataCopy.RawPosition, DummyTrack);

		// Update LOCAL position (relative to RootOriginActor)
		Actor->SetActorRelativeLocation(LocalPos);

		// Update LOCAL rotation
		if (Config.bApplyOrientationFromMovement && !TrackDataCopy.Orientation.IsNearlyZero())
		{
			FRotator OrientationRotation;
			if (Config.MappingMode == EAefPharusMappingMode::Regions)
			{
				// Wall mode: Rotate around wall normal
				const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
				if (Region)
				{
					OrientationRotation = GetWallActorRotation(TrackDataCopy.Orientation, *Region);
				}
				else
				{
					OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
				}
			}
			else
			{
				// Floor mode: Yaw rotation around World Z
				OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
			}
			Actor->SetActorRelativeRotation(OrientationRotation);
		}
		// else: keep current relative rotation (no change needed for stationary actors)
	}
	else
	{
		//--------------------------------------------------------------------------------
		// ABSOLUTE SPAWNING MODE: Use world coordinates (original behavior)
		//--------------------------------------------------------------------------------
		if (Config.bUseLocalSpace)
		{
			Actor->SetActorRelativeLocation(TrackDataCopy.WorldPosition);
		}
		else
		{
			Actor->SetActorLocation(TrackDataCopy.WorldPosition);
		}

		// Update rotation - actors are oriented RELATIVE TO the RootOriginActor
		// Full 3D rotation ensures actors stay correctly aligned when the origin moves/rotates
		FRotator RootRotation = GetRootOriginRotation();
		
		if (Config.bApplyOrientationFromMovement && !TrackDataCopy.Orientation.IsNearlyZero())
		{
			FRotator OrientationRotation;
			if (Config.MappingMode == EAefPharusMappingMode::Regions)
			{
				// Wall mode: Rotate around wall normal
				const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
				if (Region)
				{
					OrientationRotation = GetWallActorRotation(TrackDataCopy.Orientation, *Region);
				}
				else
				{
					OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
				}
			}
			else
			{
				// Floor mode: Yaw rotation around World Z
				OrientationRotation = GetRotationFromDirection(TrackDataCopy.Orientation);
			}
			// Combine root rotation with orientation
			FRotator CombinedRotation = RootRotation + OrientationRotation;
			
			if (Config.bUseLocalSpace)
			{
				Actor->SetActorRelativeRotation(CombinedRotation);
			}
			else
			{
				Actor->SetActorRotation(CombinedRotation);
			}
		}
		else
		{
			// No movement orientation - use wall's base rotation for walls, root rotation for floor
			FRotator BaseRotation = RootRotation;
			if (Config.MappingMode == EAefPharusMappingMode::Regions)
			{
				const FAefPharusWallRegion* Region = FindWallRegion(TrackDataCopy.RawPosition);
				if (Region)
				{
					BaseRotation = RootRotation + Region->WorldRotation;
				}
			}
			
			if (Config.bUseLocalSpace)
			{
				Actor->SetActorRelativeRotation(BaseRotation);
			}
			else
			{
				Actor->SetActorRotation(BaseRotation);
			}
		}
	}

	// Broadcast update event
	OnTrackUpdated.Broadcast(TrackID, TrackDataCopy);
}

void UAefPharusInstance::DestroyActorForTrack(int32 TrackID, const FString& Reason)
{
	AActor* Actor = nullptr;
	{
		FScopeLock Lock(&ActorSpawnMutex);
		AActor** ActorPtr = SpawnedActors.Find(TrackID);
		Actor = (ActorPtr && IsValid(*ActorPtr)) ? *ActorPtr : nullptr;
	}

	// Call OnTrackLost on actor BEFORE releasing/destroying
	if (Actor && Actor->GetClass()->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
	{
		IAefPharusActorInterface::Execute_OnTrackLost(Actor, TrackID, Config.InstanceName, Reason);
	}

	// Check if this track had an actor (was inside bounds)
	bool bHadActor = false;
	
	//--------------------------------------------------------------------------------
	// Actor Pool Mode: Release actor back to pool
	//--------------------------------------------------------------------------------
	if (Config.bUseActorPool && ActorPool)
	{
		int32* PoolIndexPtr = TrackToPoolIndex.Find(TrackID);
		if (PoolIndexPtr)
		{
			bHadActor = true;
			int32 PoolIndex = *PoolIndexPtr;

			// Release actor back to pool (this will also call OnTrackLost via SetActorTrackInfo)
			if (ActorPool->ReleaseActor(PoolIndex))
			{
				UE_LOG(LogAefPharus, Log, TEXT("[%s] Released actor (pool index %d) for track %d (Reason: %s)"),
					*Config.InstanceName.ToString(), PoolIndex, TrackID, *Reason);
			}
			else
			{
				UE_LOG(LogAefPharus, Warning, TEXT("[%s] Failed to release actor (pool index %d) for track %d (Reason: %s)"),
					*Config.InstanceName.ToString(), PoolIndex, TrackID, *Reason);
			}

			TrackToPoolIndex.Remove(TrackID);
		}
		// else: No pool index = track was outside bounds, no actor to release (this is normal)
	}
	//--------------------------------------------------------------------------------
	// Dynamic Spawn Mode: Destroy or hide actor
	//--------------------------------------------------------------------------------
	else
	{
		if (Actor)
		{
			bHadActor = true;
			if (Config.bAutoDestroyOnTrackLost)
			{
				UE_LOG(LogAefPharus, Log, TEXT("[%s] Destroying actor for track %d (Reason: %s)"),
					*Config.InstanceName.ToString(), TrackID, *Reason);
				Actor->Destroy();
			}
			else
			{
				// Actor will persist - hide it but keep it for reuse
				// The actor can be reused when the track re-enters bounds
				UE_LOG(LogAefPharus, Log, 
					TEXT("[%s] Hiding actor for track %d (Reason: %s, bAutoDestroyOnTrackLost=false)"),
					*Config.InstanceName.ToString(), TrackID, *Reason);
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorEnableCollision(false);
				Actor->SetActorTickEnabled(false);
			}
		}
		// else: No actor = track was outside bounds (this is normal)
	}
	
	// Log cleanup for tracks that never had an actor (were always outside bounds)
	if (!bHadActor)
	{
		UE_LOG(LogAefPharus, Verbose, TEXT("[%s] Cleaned up track %d (Reason: %s) - no actor was spawned (track was outside bounds)"),
			*Config.InstanceName.ToString(), TrackID, *Reason);
	}

	// Remove from spawned actors map
	{
		FScopeLock Lock(&ActorSpawnMutex);
		SpawnedActors.Remove(TrackID);
	}

	{
		FScopeLock Lock(&PendingOperationsMutex);
		TrackDataCache.Remove(TrackID);
		// NOTE: Do NOT remove from TracksOutsideBounds here!
		// If track left bounds, we want to keep tracking that state
		// so we can re-spawn when it comes back.
		// TracksOutsideBounds is only cleaned up in onTrackLost().
	}

	// Broadcast lost event
	OnTrackLost.Broadcast(TrackID);
}

//--------------------------------------------------------------------------------
// Pending Operations Processing (Game Thread, called by FTSTicker)
//--------------------------------------------------------------------------------

bool UAefPharusInstance::ProcessPendingOperations(float DeltaTime)
{
	if (!bIsRunning)
	{
		return true; // Keep ticking
	}

	TArray<int32> LocalSpawns, LocalUpdates, LocalRemovals;
	TMap<int32, pharus::TrackRecord> LocalTrackData;

	// Copy pending operations under lock
	{
		FScopeLock Lock(&PendingOperationsMutex);
		LocalSpawns = PendingSpawns;
		LocalUpdates = PendingUpdates;
		LocalRemovals = PendingRemovals;

		// Clear pending arrays
		PendingSpawns.Empty();
		PendingUpdates.Empty();
		PendingRemovals.Empty();
	}

	// Process removals FIRST to avoid conflicts with spawns
	// (e.g., track leaves and re-enters bounds in same frame)
	for (int32 TrackID : LocalRemovals)
	{
		// Skip if track is also in spawns (re-entered bounds)
		if (!LocalSpawns.Contains(TrackID))
		{
			DestroyActorForTrack(TrackID, TEXT("LeftBounds"));
		}
	}

	// Process spawns
	for (int32 TrackID : LocalSpawns)
	{
		FAefPharusTrackData* TrackData = TrackDataCache.Find(TrackID);
		if (TrackData)
		{
			pharus::TrackRecord DummyTrack;
			DummyTrack.trackID = TrackID;
			SpawnActorForTrack(TrackID, DummyTrack);
		}
	}

	// Process updates
	for (int32 TrackID : LocalUpdates)
	{
		// Skip if track was just spawned (already has latest data)
		if (!LocalSpawns.Contains(TrackID))
		{
			pharus::TrackRecord DummyTrack;
			DummyTrack.trackID = TrackID;
			UpdateActorForTrack(TrackID, DummyTrack);
		}
	}

	// Check for timed-out tracks (no UDP packets received)
	// This detects when simulator stops sending (crash/close) vs. person standing still
	if (Config.TrackLostTimeout > 0.0f)
	{
		double CurrentTime = FPlatformTime::Seconds();
		TArray<int32> TimedOutTracks;

		{
			FScopeLock Lock(&PendingOperationsMutex);
			// Check all active tracks for timeout
			for (const auto& Pair : TrackDataCache)
			{
				double TimeSinceLastUpdate = CurrentTime - Pair.Value.LastUpdateTime;
				if (TimeSinceLastUpdate > Config.TrackLostTimeout)
				{
					TimedOutTracks.Add(Pair.Key);

					if (Config.bLogTrackerRemoved)
					{
						UE_LOG(LogAefPharus, Warning, TEXT("[%s] Track %d timed out (no UDP updates for %.1fs) - treating as lost"),
							*Config.InstanceName.ToString(), Pair.Key, TimeSinceLastUpdate);
					}
				}
			}
			
			// Also clean up TracksOutsideBounds for timed-out tracks
			for (int32 TrackID : TimedOutTracks)
			{
				TracksOutsideBounds.Remove(TrackID);
			}
		}

		// Process timed-out tracks as if they were lost
		for (int32 TrackID : TimedOutTracks)
		{
			DestroyActorForTrack(TrackID, TEXT("Timeout"));
		}
	}

	return true; // Keep ticking
}

//--------------------------------------------------------------------------------
// Helper Functions
//--------------------------------------------------------------------------------

FAefPharusTrackData UAefPharusInstance::ConvertTrackData(const pharus::TrackRecord& Track, const FVector& WorldPos, const FVector2D& InputPos) const
{
	FAefPharusTrackData Data;
	Data.TrackID = Track.trackID;
	Data.WorldPosition = WorldPos;
	Data.Speed = Track.speed * 100.0f; // m/s → cm/s

	// Apply transformations to orientation for Simple mode
	FVector2D TrackOrientation(Track.orientation.x, Track.orientation.y);
	if (Config.MappingMode == EAefPharusMappingMode::Simple)
	{
		// Invert Y if needed (fixes left/right swap in orientation)
		if (Config.bInvertY)
		{
			TrackOrientation.Y = -TrackOrientation.Y;
		}

		// Apply rotation
		if (!FMath::IsNearlyZero(Config.FloorRotation))
		{
			float RotationRad = FMath::DegreesToRadians(Config.FloorRotation);
			float CosRot = FMath::Cos(RotationRad);
			float SinRot = FMath::Sin(RotationRad);

			// Rotate orientation vector
			FVector2D RotatedOrientation;
			RotatedOrientation.X = TrackOrientation.X * CosRot - TrackOrientation.Y * SinRot;
			RotatedOrientation.Y = TrackOrientation.X * SinRot + TrackOrientation.Y * CosRot;
			TrackOrientation = RotatedOrientation;
		}
	}

	Data.Orientation = TrackOrientation;
	Data.Velocity = FVector(TrackOrientation.X, TrackOrientation.Y, 0.0f) * Data.Speed;

	// RawPosition should be normalized (0-1) for FindWallRegion
	// If input is already normalized, use it directly; otherwise normalize it
	if (Config.bUseNormalizedCoordinates)
	{
		Data.RawPosition = InputPos; // Already normalized
	}
	else
	{
		// Normalize absolute coordinates
		Data.RawPosition.X = InputPos.X / Config.TrackingSurfaceDimensions.X;
		Data.RawPosition.Y = InputPos.Y / Config.TrackingSurfaceDimensions.Y;
	}

	// Update timestamp for timeout detection (UDP packet received)
	Data.LastUpdateTime = FPlatformTime::Seconds();

	// Determine assigned wall for Regions mode
	if (Config.MappingMode == EAefPharusMappingMode::Regions)
	{
		const FAefPharusWallRegion* Region = FindWallRegion(Data.RawPosition);
		Data.AssignedWall = Region ? Region->WallSide : EAefPharusWallSide::Floor;
	}
	else
	{
		Data.AssignedWall = EAefPharusWallSide::Floor;
	}

	// ConvertTrackData is only called for tracks inside valid bounds
	Data.bIsInsideBoundary = true;

	return Data;
}

FRotator UAefPharusInstance::GetRotationFromDirection(const FVector2D& Direction) const
{
	if (Direction.IsNearlyZero())
	{
		return FRotator::ZeroRotator;
	}

	// Calculate yaw from 2D direction vector (Floor mode - rotation around World Z)
	float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	return FRotator(0.0f, Yaw, 0.0f);
}

FRotator UAefPharusInstance::GetWallActorRotation(const FVector2D& Direction, const FAefPharusWallRegion& WallRegion) const
{
	// Wall mode: Rotation around the wall's normal vector
	// The 2D orientation from tracking represents movement on the wall plane:
	// - Direction.X = horizontal movement on wall
	// - Direction.Y = vertical movement on wall (up/down)
	
	if (Direction.IsNearlyZero())
	{
		// No movement - use wall's base rotation
		return WallRegion.WorldRotation;
	}

	// Build 3D direction vector in wall-local space
	// Wall local: X = along wall (horizontal), Y = out of wall (normal), Z = up on wall (vertical)
	FVector LocalDir(Direction.X, 0.0f, Direction.Y);
	LocalDir.Normalize();

	// Transform to world space using wall rotation
	FVector WorldDir = WallRegion.WorldRotation.RotateVector(LocalDir);

	// Wall normal (perpendicular to wall, pointing into room)
	// In wall-local space, Y+ is the normal direction
	FVector WallNormal = WallRegion.WorldRotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));

	// Create rotation where:
	// - Forward (X) = movement direction on wall
	// - Up (Z) = wall normal (actor faces into room)
	FRotator Result = FRotationMatrix::MakeFromXZ(WorldDir, WallNormal).Rotator();

	return Result;
}

//--------------------------------------------------------------------------------
// Debug
//--------------------------------------------------------------------------------

void UAefPharusInstance::DebugInjectTrack(float NormalizedX, float NormalizedY, int32 TrackID)
{
	if (TrackID < 0)
	{
		TrackID = FMath::Rand();
	}

	pharus::TrackRecord Track;
	Track.trackID = TrackID;
	Track.currentPos.x = NormalizedX;
	Track.currentPos.y = NormalizedY;
	Track.relPos.x = NormalizedX;
	Track.relPos.y = NormalizedY;
	Track.speed = 1.0f;
	Track.orientation.x = 1.0f;
	Track.orientation.y = 0.0f;
	Track.state = pharus::TS_NEW;

	UE_LOG(LogAefPharus, Log, TEXT("[%s] DEBUG: Injecting track %d at (%.3f, %.3f)"),
		*Config.InstanceName.ToString(), TrackID, NormalizedX, NormalizedY);

	onTrackNew(Track);
}

//--------------------------------------------------------------------------------
// Actor Reuse Helper (Dynamic Spawn Mode)
//--------------------------------------------------------------------------------

AActor* UAefPharusInstance::FindExistingActorByName(int32 TrackID) const
{
	if (!WorldContext)
	{
		return nullptr;
	}

	// Construct the expected actor name prefix (same pattern as in SpawnActorForTrack)
	// Note: UE may add suffixes like "_0", "_1" etc. when name already exists
	FString ExpectedNamePrefix = FString::Printf(TEXT("PharusTrack_%s_%d"), 
		*Config.InstanceName.ToString(), TrackID);

	// Search for existing actor with this name (or name with UE suffix) in the world
	for (TActorIterator<AActor> It(WorldContext); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !IsValid(Actor))
		{
			continue;
		}

		// Get actor's FName string representation
		FString ActorNameStr = Actor->GetFName().ToString();
		
		// Check if actor name starts with our expected prefix
		// This handles both exact matches and UE-suffixed names like "PharusTrack_Floor_49_0"
		if (ActorNameStr.StartsWith(ExpectedNamePrefix))
		{
			// Additional check: make sure it's not a different track (e.g., Track 490 vs Track 49)
			// The name after prefix should either be empty or start with underscore (UE suffix)
			FString Remainder = ActorNameStr.Mid(ExpectedNamePrefix.Len());
			if (Remainder.IsEmpty() || Remainder.StartsWith(TEXT("_")))
			{
				// Verify it's the right class type (or derived from it)
				if (!SpawnClass || Actor->IsA(SpawnClass))
				{
					UE_LOG(LogAefPharus, Log, 
						TEXT("[%s] Found existing actor '%s' for track %d - will reuse"),
						*Config.InstanceName.ToString(), *ActorNameStr, TrackID);
					return Actor;
				}
				else
				{
					UE_LOG(LogAefPharus, Warning, 
						TEXT("[%s] Found actor '%s' but wrong class (expected %s, got %s) - destroying and respawning"),
						*Config.InstanceName.ToString(), *ActorNameStr, 
						*SpawnClass->GetName(), *Actor->GetClass()->GetName());
					// Wrong class - destroy it so we can spawn the correct one
					Actor->Destroy();
					return nullptr;
				}
			}
		}
	}

	return nullptr;
}
