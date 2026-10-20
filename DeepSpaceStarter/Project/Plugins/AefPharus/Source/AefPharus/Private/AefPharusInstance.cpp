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
		#define AEFPHARUS_NDISPLAY_SUPPORT 1
	#else
		#define AEFPHARUS_NDISPLAY_SUPPORT 0
	#endif
#else
	#define AEFPHARUS_NDISPLAY_SUPPORT 0
#endif

//--------------------------------------------------------------------------------
// Constructor / Destructor
//--------------------------------------------------------------------------------

UAefPharusInstance::UAefPharusInstance()
	: bIsRunning(false)
	, ActorPool(nullptr)
{
}

//--------------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------------

UWorld* UAefPharusInstance::GetWorld() const
{
	const UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
	return Subsystem ? Subsystem->GetWorld() : nullptr;
}

void UAefPharusInstance::BeginDestroy()
{
	// Only release non-UObject resources here: during garbage collection the pool and
	// the actors may already be unreachable and must not be touched (Shutdown() does that).
	ShutdownNetwork();
	bIsRunning = false;

	Super::BeginDestroy();
}

void UAefPharusInstance::ShutdownNetwork()
{
	// Unregister tick delegate
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}

	if (WorldTearDownHandle.IsValid())
	{
		FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
		WorldTearDownHandle.Reset();
	}

	// Unregister from TrackLinkClient (destructor joins the receive thread)
	if (TrackLinkClient)
	{
		TrackLinkClient->unregisterTrackReceiver(this);
		TrackLinkClient.Reset();
	}
}

UWorld* UAefPharusInstance::GetUsableWorld() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->bIsTearingDown || !World->IsGameWorld() || !World->HasBegunPlay())
	{
		return nullptr;
	}
	return World;
}

bool UAefPharusInstance::RefreshWorldBinding()
{
	UWorld* CurrentWorld = GetUsableWorld();
	if (CurrentWorld && CurrentWorld == BoundWorld.Get())
	{
		return true;
	}

	// Bound world changed, is being torn down or was garbage collected
	if (!BoundWorld.IsExplicitlyNull())
	{
		ReleaseWorldActors();
	}

	if (!CurrentWorld)
	{
		return false;
	}

	BindToWorld(CurrentWorld);
	return true;
}

void UAefPharusInstance::BindToWorld(UWorld* World)
{
	BoundWorld = World;

	// (Re)build the actor pool in this world (runs on ALL cluster nodes)
	if (Config.bUseActorPool && SpawnClass)
	{
		if (!ActorPool)
		{
			ActorPool = NewObject<UAefPharusActorPool>(this);
		}

		const bool bPoolInitialized = ActorPool->Initialize(
			World,
			SpawnClass,
			Config.ActorPoolSize,
			Config.InstanceName,
			Config.PoolSpawnLocation,
			Config.PoolSpawnRotation,
			Config.PoolIndexOffset
		);
		if (bPoolInitialized)
		{
			UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': Actor pool initialized with %d actors in world '%s'"),
				*Config.InstanceName.ToString(), Config.ActorPoolSize, *World->GetName());
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("Instance '%s': Actor pool initialization failed - using dynamic spawning"),
				*Config.InstanceName.ToString());
			ActorPool = nullptr;
		}
	}

	// Tracks that were active before a level change get an actor in the new world
	int32 RequeuedCount = 0;
	for (const TPair<int32, FAefPharusTrackData>& Pair : TrackDataCache)
	{
		if (!TracksOutsideBounds.Contains(Pair.Key))
		{
			PendingSpawns.AddUnique(Pair.Key);
			++RequeuedCount;
		}
	}

	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': bound to world '%s' (%d active track(s) queued for spawn)"),
		*Config.InstanceName.ToString(), *World->GetName(), RequeuedCount);
}

void UAefPharusInstance::ReleaseWorldActors()
{
	// Actors in a world that keeps running are cleaned up. In a world that is being torn
	// down they die with their level and must not be touched anymore.
	UWorld* OldWorld = BoundWorld.Get();
	const bool bDestroyActors = OldWorld && !OldWorld->bIsTearingDown;

	if (ActorPool)
	{
		ActorPool->Shutdown(bDestroyActors);
	}

	// Pooled actors are already gone at this point; this catches dynamically spawned
	// ones (also when the pool failed to initialize and spawning fell back to dynamic)
	if (bDestroyActors)
	{
		for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : SpawnedActors)
		{
			if (AActor* Actor = Pair.Value.Get())
			{
				Actor->Destroy();
			}
		}
	}
	SpawnedActors.Empty();
	TrackToPoolIndex.Empty();
	bHasAppliedRoot = false;
	AppliedRootActor.Reset();

	if (!BoundWorld.IsExplicitlyNull())
	{
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': released actors of world '%s'"),
			*Config.InstanceName.ToString(), OldWorld ? *OldWorld->GetName() : TEXT("<destroyed>"));
	}
	BoundWorld.Reset();
}

void UAefPharusInstance::HandleWorldBeginTearDown(UWorld* World)
{
	if (World && World == BoundWorld.Get())
	{
		ReleaseWorldActors();
	}
}

void UAefPharusInstance::EnqueueTrackEvent(FTrackEvent::EType Type, const pharus::TrackRecord& Track)
{
	FTrackEvent Event;
	Event.Type = Type;
	Event.Record = Track;
	Event.ReceiveTime = FPlatformTime::Seconds();

	FScopeLock Lock(&TrackEventMutex);
	PendingTrackEvents.Add(MoveTemp(Event));
}

void UAefPharusInstance::ApplyTrackNew(const pharus::TrackRecord& Track, double ReceiveTime)
{
	// Use relPos (TUIO-normalized 0-1 coordinates) instead of currentPos (absolute meters)
	// TUIO has origin top-left (Y=0 at top), flip Y to match UE coordinate system
	const FVector2D InputPos = FVector2D(Track.relPos.x, 1.0f - Track.relPos.y);
	const bool bIsValid = IsTrackPositionValid(InputPos);

	if (!bIsValid)
	{
		// Track starts outside valid bounds - remember it but don't spawn
		// Still add to TrackDataCache for timeout tracking!
		TracksOutsideBounds.Add(Track.trackID);

		FAefPharusTrackData TrackData;
		TrackData.TrackID = Track.trackID;
		TrackData.LastUpdateTime = ReceiveTime;
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
	const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos, ReceiveTime);

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

void UAefPharusInstance::ApplyTrackUpdate(const pharus::TrackRecord& Track, double ReceiveTime)
{
	// Use relPos (TUIO-normalized 0-1 coordinates) instead of currentPos (absolute meters)
	// TUIO has origin top-left (Y=0 at top), flip Y to match UE coordinate system
	const FVector2D InputPos = FVector2D(Track.relPos.x, 1.0f - Track.relPos.y);
	const bool bIsValid = IsTrackPositionValid(InputPos);

	const bool bWasOutside = TracksOutsideBounds.Contains(Track.trackID);

	if (!bIsValid)
	{
		// Track is outside valid bounds - always update timestamp for timeout tracking
		FAefPharusTrackData* ExistingData = TrackDataCache.Find(Track.trackID);
		if (ExistingData)
		{
			ExistingData->LastUpdateTime = ReceiveTime;
			ExistingData->bIsInsideBoundary = false;  // Track is outside bounds
		}
		else
		{
			// Track not in cache yet - add it for timeout tracking
			FAefPharusTrackData TrackData;
			TrackData.TrackID = Track.trackID;
			TrackData.LastUpdateTime = ReceiveTime;
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
		const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos, ReceiveTime);
		
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
		const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos, ReceiveTime);
		
		PendingSpawns.AddUnique(Track.trackID);
		TrackDataCache.Add(Track.trackID, TrackData);
		
		if (Config.bLogTrackerSpawned)
		{
			UE_LOG(LogAefPharus, Log, TEXT("[%s] Track %d spawned (recovery) at %s"),
				*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString());
		}
		return;
	}

	// Recalculate world position with the current RootOrigin/RootRotation
	const FVector WorldPos = TrackToWorld(InputPos, Track);
	const FAefPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, InputPos, ReceiveTime);

	// Update the cached data (so actor always has correct position relative to RootOrigin)
	*ExistingData = TrackData;

	// Queue actor update (origin movement between packets is handled per frame in ProcessPendingOperations)
	PendingUpdates.AddUnique(Track.trackID);

	if (Config.bLogTrackerUpdated)
	{
		UE_LOG(LogAefPharus, VeryVerbose, TEXT("[%s] Track %d updated at %s"),
			*Config.InstanceName.ToString(), Track.trackID, *WorldPos.ToString());
	}
}

void UAefPharusInstance::ApplyTrackLost(const pharus::TrackRecord& Track)
{
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

	WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UAefPharusInstance::HandleWorldBeginTearDown);
	if (!Config.bUseActorPool)
	{
		UE_LOG(LogAefPharus, Log, TEXT("Instance '%s': Actor pool disabled, using dynamic spawning"),
			*Config.InstanceName.ToString());
	}

	bIsRunning = true;
	RefreshWorldBinding();

	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' initialized successfully (%s:%d)"),
		*Config.InstanceName.ToString(),
		*Config.BindNIC,
		Config.UDPPort);

	return true;
}

void UAefPharusInstance::Shutdown()
{
	if (!bIsRunning)
	{
		return;
	}

	// Stop network thread first so no new track operations get queued
	ShutdownNetwork();

	// Destroy pooled and spawned actors (skipped if their world is being torn down)
	ReleaseWorldActors();
	ActorPool = nullptr;

	// Network thread is stopped, so the event queue is no longer written
	PendingTrackEvents.Empty();
	TrackDataCache.Empty();
	TracksOutsideBounds.Empty();
	PendingSpawns.Empty();
	PendingUpdates.Empty();
	PendingRemovals.Empty();
	bHasAppliedRoot = false;

	bIsRunning = false;

	UE_LOG(LogAefPharus, Log, TEXT("Instance '%s' shutdown complete"), *Config.InstanceName.ToString());
}

//--------------------------------------------------------------------------------
// ITrackReceiver Interface (Network Thread)
//--------------------------------------------------------------------------------

void UAefPharusInstance::onTrackNew(const pharus::TrackRecord& Track)
{
	EnqueueTrackEvent(FTrackEvent::EType::New, Track);
}

void UAefPharusInstance::onTrackUpdate(const pharus::TrackRecord& Track)
{
	EnqueueTrackEvent(FTrackEvent::EType::Update, Track);
}

void UAefPharusInstance::onTrackLost(const pharus::TrackRecord& Track)
{
	EnqueueTrackEvent(FTrackEvent::EType::Lost, Track);
}

//--------------------------------------------------------------------------------
// Track Data Access (Game Thread)
//--------------------------------------------------------------------------------

bool UAefPharusInstance::GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation, bool& bOutIsInsideBoundary)
{
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
	TArray<int32> TrackIDs;
	TrackDataCache.GetKeys(TrackIDs);
	return TrackIDs;
}

int32 UAefPharusInstance::GetActiveTrackCount() const
{
	return TrackDataCache.Num();
}

AActor* UAefPharusInstance::GetSpawnedActor(int32 TrackID)
{
	const TWeakObjectPtr<AActor>* ActorPtr = SpawnedActors.Find(TrackID);
	return ActorPtr ? ActorPtr->Get() : nullptr;
}

bool UAefPharusInstance::IsTrackActive(int32 TrackID) const
{
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
	// Enable and network settings cannot be changed at runtime

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
	FAefPharusInstanceConfig SavedConfig = Config;

	// Update network settings in config
	SavedConfig.BindNIC = NewBindNIC;
	SavedConfig.UDPPort = NewUDPPort;

	// Shutdown current connection (will destroy all actors)
	Shutdown();

	// Reinitialize with new network settings
	bool bSuccess = Initialize(SavedConfig, GetWorld(), SavedSpawnClass);

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
// Coordinate Transformation (Game Thread)
//--------------------------------------------------------------------------------

FVector UAefPharusInstance::TrackToWorld(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const
{
	// Step 1: Handle coordinate normalization based on explicit flag
	return NormalizedToWorld(NormalizeTrackPosition(TrackPos));
}

FVector UAefPharusInstance::NormalizedToWorld(const FVector2D& NormalizedPos) const
{
	return TrackToWorldFloor(NormalizedPos);
}

FVector UAefPharusInstance::TrackToWorldFloor(const FVector2D& NormalizedPos) const
{
	FVector2D AdjustedPos = NormalizedPos;

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

//--------------------------------------------------------------------------------
// Local Coordinate Transformation (for Relative Spawning)
//--------------------------------------------------------------------------------

FVector UAefPharusInstance::TrackToLocal(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const
{
	return TrackToLocalFloor(TrackPos);
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

	// Check if normalized position is within 0-1 bounds
	return NormalizedPos.X >= 0.0f && NormalizedPos.X <= 1.0f &&
	       NormalizedPos.Y >= 0.0f && NormalizedPos.Y <= 1.0f;
}

//--------------------------------------------------------------------------------
// Actor Management (Game Thread)
//--------------------------------------------------------------------------------

void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
	UWorld* World = BoundWorld.Get();
	if (!World || !SpawnClass)
	{
		return;
	}

	//--------------------------------------------------------------------------------
	// nDisplay Cluster Check: Only activate actors on Primary Node
	//--------------------------------------------------------------------------------
	// If nDisplay is active in cluster mode, only the primary node activates actors.
	// Actor pool exists on ALL nodes, but only primary controls visibility.
#if AEFPHARUS_NDISPLAY_SUPPORT
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

	// A destroyed actor must not block a replacement for the same track.
	if (const TWeakObjectPtr<AActor>* Existing = SpawnedActors.Find(TrackID))
	{
		if (Existing->IsValid())
		{
			return;
		}
		SpawnedActors.Remove(TrackID);
	}
	if (const int32* StalePoolIndex = TrackToPoolIndex.Find(TrackID))
	{
		if (ActorPool)
		{
			ActorPool->ReleaseActor(*StalePoolIndex);
		}
		TrackToPoolIndex.Remove(TrackID);
	}
	const FAefPharusTrackData* TrackData = TrackDataCache.Find(TrackID);
	if (!TrackData) return;
	const FAefPharusTrackData TrackDataCopy = *TrackData;

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
			SpawnedActor = World->SpawnActor(SpawnClass, &SpawnLocation, &SpawnRotation, SpawnParams);

			if (!SpawnedActor)
			{
				// Spawn failed - likely because name already exists
				// Try to find the existing actor by iterating all actors with matching class
				UE_LOG(LogAefPharus, Warning, 
					TEXT("[%s] Spawn failed for track %d with name '%s' - searching for existing actor by name..."),
					*Config.InstanceName.ToString(), TrackID, *ActorName);
				
				// More aggressive search: find by exact name using FindObject
				AActor* ExistingActor = FindObject<AActor>(World->GetCurrentLevel(), *ActorName);
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
					SpawnedActor = World->SpawnActor(SpawnClass, &SpawnLocation, &SpawnRotation, SpawnParams);
					
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
				pharus::TrackRecord DummyTrack;
				const FVector LocalPos = TrackToLocal(TrackDataCopy.RawPosition, DummyTrack);

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
					// Yaw rotation around World Z
					SpawnedActor->SetActorRelativeRotation(GetRotationFromDirection(TrackDataCopy.Orientation));
				}
				else
				{
					SpawnedActor->SetActorRelativeRotation(FRotator::ZeroRotator);
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
				// Combine root rotation with movement orientation (Yaw around World Z)
				FRotator CombinedRotation = RootRotation + GetRotationFromDirection(TrackDataCopy.Orientation);
				SpawnedActor->SetActorRotation(CombinedRotation);
			}
			else
			{
				SpawnedActor->SetActorRotation(RootRotation);
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
		SpawnedActors.Add(TrackID, SpawnedActor);

		// Broadcast event
		OnTrackSpawned.Broadcast(TrackID, SpawnedActor);
	}
}

void UAefPharusInstance::UpdateActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
	const TWeakObjectPtr<AActor>* ActorPtr = SpawnedActors.Find(TrackID);
	AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
	if (!Actor) return;
	FAefPharusTrackData* TrackData = TrackDataCache.Find(TrackID);
	if (!TrackData) return;
	TrackData->WorldPosition = NormalizedToWorld(TrackData->RawPosition);
	const FAefPharusTrackData TrackDataCopy = *TrackData;

	// Get the owning subsystem to check for relative spawning mode
	// UseRelativeSpawning in [PharusSubsystem] is the master switch
	UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
	const bool bUseRelativeSpawning = Subsystem && Subsystem->IsRelativeSpawningActive();

	if (bUseRelativeSpawning)
	{
		//--------------------------------------------------------------------------------
		// RELATIVE SPAWNING MODE: Use local coordinates (actor is attached to RootOriginActor)
		//--------------------------------------------------------------------------------
		AAefPharusRootOriginActor* RootActor = Subsystem->GetRootOriginActor();
		if (RootActor && Actor->GetAttachParentActor() != RootActor)
		{
			Actor->AttachToActor(RootActor, FAttachmentTransformRules::KeepRelativeTransform);
		}
		// Calculate LOCAL position from raw tracking coordinates
		pharus::TrackRecord DummyTrack;
		FVector LocalPos = TrackToLocal(TrackDataCopy.RawPosition, DummyTrack);

		// Update LOCAL position (relative to RootOriginActor)
		Actor->SetActorRelativeLocation(LocalPos);

		// Update LOCAL rotation
		if (Config.bApplyOrientationFromMovement && !TrackDataCopy.Orientation.IsNearlyZero())
		{
			// Yaw rotation around World Z
			Actor->SetActorRelativeRotation(GetRotationFromDirection(TrackDataCopy.Orientation));
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
			// Combine root rotation with movement orientation (Yaw around World Z)
			FRotator CombinedRotation = RootRotation + GetRotationFromDirection(TrackDataCopy.Orientation);
			
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
			// No movement orientation - use root rotation
			if (Config.bUseLocalSpace)
			{
				Actor->SetActorRelativeRotation(RootRotation);
			}
			else
			{
				Actor->SetActorRotation(RootRotation);
			}
		}
	}

	// Broadcast update event
	OnTrackUpdated.Broadcast(TrackID, TrackDataCopy);
}

void UAefPharusInstance::DestroyActorForTrack(int32 TrackID, const FString& Reason)
{
	const TWeakObjectPtr<AActor>* ActorPtr = SpawnedActors.Find(TrackID);
	AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;

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
	SpawnedActors.Remove(TrackID);
	TrackDataCache.Remove(TrackID);
	// Keep TracksOutsideBounds until ApplyTrackLost(), so a track can re-enter.

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

	// Follow the current world: after a level change the actors and the pool are rebuilt
	// in the new world. While no usable world exists (transition), actor operations pause.
	const bool bHasWorld = RefreshWorldBinding();

	// Take over the events queued by the network thread (the only cross-thread hand-off)
	TArray<FTrackEvent> Events;
	{
		FScopeLock Lock(&TrackEventMutex);
		Events = MoveTemp(PendingTrackEvents);
	}

	// Apply them in arrival order: bounds checks and coordinate transformation run here,
	// on the game thread, with the current config and root origin
	for (const FTrackEvent& Event : Events)
	{
		switch (Event.Type)
		{
			case FTrackEvent::EType::New:
				ApplyTrackNew(Event.Record, Event.ReceiveTime);
				break;
			case FTrackEvent::EType::Update:
				ApplyTrackUpdate(Event.Record, Event.ReceiveTime);
				break;
			case FTrackEvent::EType::Lost:
				ApplyTrackLost(Event.Record);
				break;
		}
	}

	TArray<int32> LocalSpawns = MoveTemp(PendingSpawns);
	TArray<int32> LocalUpdates = MoveTemp(PendingUpdates);
	TArray<int32> LocalRemovals = MoveTemp(PendingRemovals);
	// Replace an externally destroyed actor even if its track has not moved.
	if (bHasWorld)
	{
		for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : SpawnedActors)
		{
			if (!Pair.Value.IsValid() && TrackDataCache.Contains(Pair.Key) && !TracksOutsideBounds.Contains(Pair.Key))
			{
				LocalSpawns.AddUnique(Pair.Key);
			}
		}
	}

	// Absolute positions depend on the root origin. When it moves (moving stage, tracked
	// origin) every actor follows in the same frame, not only when its next packet arrives.
	// Attached actors (relative spawning) follow their parent on their own.
	if (bHasWorld)
	{
		const FVector RootOrigin = GetRootOrigin();
		const FRotator RootRotation = GetRootOriginRotation();
		const bool bRootMoved = !bHasAppliedRoot
			|| !RootOrigin.Equals(AppliedRootOrigin)
			|| !RootRotation.Equals(AppliedRootRotation);
		AppliedRootOrigin = RootOrigin;
		AppliedRootRotation = RootRotation;
		bHasAppliedRoot = true;

		UAefPharusSubsystem* Subsystem = Cast<UAefPharusSubsystem>(GetOuter());
		const bool bUseRelativeSpawning = Subsystem && Subsystem->IsRelativeSpawningActive();
		AAefPharusRootOriginActor* CurrentRootActor = bUseRelativeSpawning ? Subsystem->GetRootOriginActor() : nullptr;
		const bool bRootActorChanged = AppliedRootActor.Get() != CurrentRootActor;
		AppliedRootActor = CurrentRootActor;
		if ((bRootMoved && !bUseRelativeSpawning) || bRootActorChanged)
		{
			for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : SpawnedActors)
			{
				LocalUpdates.AddUnique(Pair.Key);
			}
		}
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

	// Process spawns (SpawnActorForTrack skips tracks that are no longer in TrackDataCache).
	// Without a world the spawns are dropped: BindToWorld() re-queues every active track.
	if (bHasWorld)
	{
		for (int32 TrackID : LocalSpawns)
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
		if (bHasWorld && !LocalSpawns.Contains(TrackID))
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

FAefPharusTrackData UAefPharusInstance::ConvertTrackData(const pharus::TrackRecord& Track, const FVector& WorldPos, const FVector2D& InputPos, double ReceiveTime) const
{
	FAefPharusTrackData Data;
	Data.TrackID = Track.trackID;
	Data.WorldPosition = WorldPos;
	Data.Speed = Track.speed * 100.0f; // m/s → cm/s

	// Apply floor transformations to orientation
	FVector2D TrackOrientation(Track.orientation.x, Track.orientation.y);

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

	Data.Orientation = TrackOrientation;
	Data.Velocity = FVector(TrackOrientation.X, TrackOrientation.Y, 0.0f) * Data.Speed;

	// RawPosition is stored normalized (0-1), used by TrackToLocal for relative spawning
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
	Data.LastUpdateTime = ReceiveTime;

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

	// Calculate yaw from 2D direction vector (rotation around World Z)
	float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	return FRotator(0.0f, Yaw, 0.0f);
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
	UWorld* World = BoundWorld.Get();
	if (!World)
	{
		return nullptr;
	}

	// Construct the expected actor name prefix (same pattern as in SpawnActorForTrack)
	// Note: UE may add suffixes like "_0", "_1" etc. when name already exists
	FString ExpectedNamePrefix = FString::Printf(TEXT("PharusTrack_%s_%d"), 
		*Config.InstanceName.ToString(), TrackID);

	// Search for existing actor with this name (or name with UE suffix) in the world
	for (TActorIterator<AActor> It(World); It; ++It)
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
