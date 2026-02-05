/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Tracker Instance Class

   Represents a single Pharus tracking system instance.
   Handles network connection, track management, and actor spawning.

   Key features:
   - Multiple mapping modes (Simple, Regions)
   - Thread-safe track management
   - Blueprint-accessible API
   - Automatic actor lifecycle management
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AefPharusTypes.h"
#include "TrackLink.h"
#include "AefPharusInstance.generated.h"

/**
 * Pharus Tracker Instance
 *
 * Manages a single Pharus laser tracking system.
 * Multiple instances can run simultaneously (e.g., Floor + Wall).
 *
 * Usage:
 *   auto* Instance = NewObject<UAefPharusInstance>();
 *   Instance->Initialize(Config, GetWorld());
 */
UCLASS(BlueprintType)
class AEFPHARUS_API UAefPharusInstance : public UObject, public pharus::ITrackReceiver
{
	GENERATED_BODY()

public:
	UAefPharusInstance();
	virtual ~UAefPharusInstance() override;

	//--------------------------------------------------------------------------------
	// Lifecycle
	//--------------------------------------------------------------------------------

	/**
	 * Initialize this tracker instance
	 * @param InConfig Configuration settings
	 * @param InWorld World context for actor spawning
	 * @param InSpawnClass Actor class to spawn for tracks (optional)
	 * @return true if initialization succeeded
	 */
	bool Initialize(const FPharusInstanceConfig& InConfig, UWorld* InWorld, TSubclassOf<AActor> InSpawnClass = nullptr);

	/**
	 * Shutdown and cleanup this tracker instance
	 */
	void Shutdown();

	/**
	 * Check if this instance is currently running
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	bool IsRunning() const { return bIsRunning; }

	//--------------------------------------------------------------------------------
	// pharus::ITrackReceiver Interface
	//--------------------------------------------------------------------------------

	virtual void onTrackNew(const pharus::TrackRecord& Track) override;
	virtual void onTrackUpdate(const pharus::TrackRecord& Track) override;
	virtual void onTrackLost(const pharus::TrackRecord& Track) override;

	//--------------------------------------------------------------------------------
	// Track Data Access
	//--------------------------------------------------------------------------------

	/**
	 * Get current world position and rotation for a track
	 * @param TrackID Track ID to query
	 * @param OutPosition World position (if found)
	 * @param OutRotation World rotation (if found)
	 * @param bOutIsInsideBoundary True if track is inside valid bounds (actor visible), false if outside (actor hidden)
	 * @return true if track exists
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	bool GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation, bool& bOutIsInsideBoundary);

	/**
	 * Get all active track IDs
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	TArray<int32> GetActiveTrackIDs() const;

	/**
	 * Get number of currently active tracks
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	int32 GetActiveTrackCount() const;

	/**
	 * Get the spawned actor for a specific track
	 * @param TrackID Track ID to query
	 * @return Spawned actor, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	AActor* GetSpawnedActor(int32 TrackID);

	/**
	 * Check if a track ID is currently active (receiving UDP updates)
	 * Useful for actors to self-validate if their track still exists
	 * @param TrackID Track ID to check
	 * @return true if track is active in the system
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	bool IsTrackActive(int32 TrackID) const;

	//--------------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------------

	/**
	 * Get current configuration
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	const FPharusInstanceConfig& GetConfig() const { return Config; }

	/**
	 * Update configuration at runtime (if bLiveAdjustments enabled)
	 * NOTE: Network settings (BindNIC, UDPPort) cannot be changed - requires restart
	 * @param NewConfig New configuration to apply
	 * @return true if update succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
	bool UpdateConfig(const FPharusInstanceConfig& NewConfig);

	/**
	 * Update Floor tracker settings at runtime (Blueprint-friendly)
	 * Updates all relevant floor settings in memory.
	 * @param NewConfig Complete configuration with updated floor settings
	 * @return true if update succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Floor")
	bool UpdateFloorSettings(const FPharusInstanceConfig& NewConfig);

	/**
	 * Update Floor tracker settings at runtime (Simple parameters - DEPRECATED, use full config version)
	 * @param OriginX X origin offset
	 * @param OriginY Y origin offset
	 * @param ScaleX X scale factor
	 * @param ScaleY Y scale factor
	 * @param FloorZ Z height for floor
	 * @param FloorRotation Rotation around Z-axis (degrees, 0=forward is +X, 90=forward is +Y)
	 * @param bInvertY Invert Y-axis (fixes left/right swap)
	 * @return true if update succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Floor", meta = (DeprecatedFunction, DeprecationMessage = "Use UpdateFloorSettings(FPharusInstanceConfig) instead"))
	bool UpdateFloorSettingsSimple(float OriginX, float OriginY, float ScaleX, float ScaleY, float FloorZ, float FloorRotation, bool bInvertY);

	/**
	 * Update Wall tracker settings at runtime (Blueprint-friendly)
	 * Updates all relevant wall settings in memory.
	 * @param NewConfig Complete configuration with updated wall settings
	 * @return true if update succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Wall")
	bool UpdateWallSettings(const FPharusInstanceConfig& NewConfig);

	/**
	 * Restart tracker with new network settings (requires LiveAdjustments enabled)
	 * WARNING: Will disconnect, destroy all actors, and reconnect!
	 * @param NewBindNIC New network interface IP
	 * @param NewUDPPort New UDP port
	 * @return true if restart succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
	bool RestartWithNewNetwork(const FString& NewBindNIC, int32 NewUDPPort);

	/**
	 * Set the SpawnClass for this tracker instance at runtime.
	 * New tracks will use this class for spawning. Existing tracks are not affected.
	 * @param NewSpawnClass Actor class to spawn (must implement IAefPharusActorInterface)
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
	void SetSpawnClass(UPARAM(meta = (MustImplement = "/Script/AefPharus.AefPharusActorInterface")) TSubclassOf<AActor> NewSpawnClass);

	/**
	 * Get the current SpawnClass for this tracker instance.
	 * @return Current actor class used for spawning
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
	TSubclassOf<AActor> GetSpawnClass() const { return SpawnClass; }

	//--------------------------------------------------------------------------------
	// Events
	//--------------------------------------------------------------------------------

	/** Called when a new track is spawned */
	UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
	FPharusTrackSpawnedDelegate OnTrackSpawned;

	/** Called when a track is updated */
	UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
	FPharusTrackUpdatedDelegate OnTrackUpdated;

	/** Called when a track is lost */
	UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
	FPharusTrackLostDelegate OnTrackLost;

	//--------------------------------------------------------------------------------
	// Debug
	//--------------------------------------------------------------------------------

	/**
	 * Debug: Inject a test track at specified position
	 * @param NormalizedX X position (0-1)
	 * @param NormalizedY Y position (0-1)
	 * @param TrackID Track ID to use (default: auto-generate)
	 */
	UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Debug")
	void DebugInjectTrack(float NormalizedX, float NormalizedY, int32 TrackID = -1);

private:
	//--------------------------------------------------------------------------------
	// Network & Threading
	//--------------------------------------------------------------------------------

	/** TrackLink client for network communication */
	TUniquePtr<pharus::TrackLinkClient> TrackLinkClient;

	/** Persistent storage for BindNIC string (TrackLinkClient stores pointer to this) */
	TArray<ANSICHAR> BindNICStorage;

	/** Persistent storage for MulticastGroup string (TrackLinkClient stores pointer to this) */
	TArray<ANSICHAR> MulticastGroupStorage;

	/** Is this instance currently running? */
	bool bIsRunning;

	//--------------------------------------------------------------------------------
	// Configuration & Context
	//--------------------------------------------------------------------------------

	/** Current configuration */
	FPharusInstanceConfig Config;

	/** Actor class to spawn for tracks */
	TSubclassOf<AActor> SpawnClass;

	/** World context for actor spawning */
	UWorld* WorldContext;

	//--------------------------------------------------------------------------------
	// Track Management
	//--------------------------------------------------------------------------------

	/** Actor pool for nDisplay cluster synchronization (optional) */
	UPROPERTY()
	class UAefPharusActorPool* ActorPool;

	/** Track ID to pool index mapping (for actor pool mode) */
	TMap<int32, int32> TrackToPoolIndex;

	/** Spawned actors mapped by track ID */
	UPROPERTY()
	TMap<int32, AActor*> SpawnedActors;

	/** Track data cache for Blueprint access */
	TMap<int32, FPharusTrackData> TrackDataCache;

	/** Mutex for thread-safe actor spawning */
	mutable FCriticalSection ActorSpawnMutex;

	/** Pending spawn operations (processed on game thread) */
	TArray<int32> PendingSpawns;
	TArray<int32> PendingUpdates;
	TArray<int32> PendingRemovals;

	/** Tracks that are known but currently outside valid bounds (no actor spawned) */
	TSet<int32> TracksOutsideBounds;

	/** Mutex for pending operation lists */
	mutable FCriticalSection PendingOperationsMutex;

	//--------------------------------------------------------------------------------
	// Coordinate Transformation
	//--------------------------------------------------------------------------------

	/**
	 * Transform 2D tracking position to 3D world position
	 * Dispatches to appropriate mapping mode handler
	 */
	FVector TrackToWorld(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const;

	/**
	 * Floor mapping: Direct 2D→3D with scale and offset
	 * Uses global root origin from subsystem
	 */
	FVector TrackToWorldFloor(const FVector2D& TrackPos) const;

	/**
	 * Get the global root origin from the owning subsystem
	 * @return Global origin in world space (cm)
	 */
	FVector GetRootOrigin() const;

	/**
	 * Get the global root origin rotation from the owning subsystem
	 * @return Global rotation in world space
	 */
	FRotator GetRootOriginRotation() const;

	/**
	 * Regions mapping: Determine wall and transform to 3D
	 */
	FVector TrackToWorldRegions(const FVector2D& TrackPos, EPharusWallSide& OutWall) const;

	/**
	 * Find which wall region contains the given track point
	 */
	const FPharusWallRegion* FindWallRegion(const FVector2D& TrackPos) const;

	/**
	 * Transform 2D tracking position to LOCAL 3D position (relative to RootOriginActor)
	 * Used when bUseRelativeSpawning=true - skips RootOrigin/RootRotation application
	 * @param TrackPos Tracking position
	 * @param Track Track record for wall region lookup
	 * @return Local position relative to RootOriginActor
	 */
	FVector TrackToLocal(const FVector2D& TrackPos, const pharus::TrackRecord& Track) const;

	/**
	 * Floor mapping: Calculate LOCAL position (no RootOrigin/RootRotation)
	 */
	FVector TrackToLocalFloor(const FVector2D& TrackPos) const;

	/**
	 * Regions mapping: Calculate LOCAL position (no RootOrigin/RootRotation)
	 */
	FVector TrackToLocalRegions(const FVector2D& TrackPos, EPharusWallSide& OutWall) const;

	//--------------------------------------------------------------------------------
	// Bounds Validation
	//--------------------------------------------------------------------------------

	/**
	 * Validate if track position is within valid bounds
	 * Rejects tracks from simulators sending invalid coordinates
	 * @param InputPos Raw input position from tracker
	 * @return true if position is valid and should be processed
	 */
	bool IsTrackPositionValid(const FVector2D& InputPos) const;

	/**
	 * Normalize input coordinates based on config settings
	 * @param InputPos Raw input position
	 * @return Normalized position (0-1 range)
	 */
	FVector2D NormalizeTrackPosition(const FVector2D& InputPos) const;

	//--------------------------------------------------------------------------------
	// Actor Management
	//--------------------------------------------------------------------------------

	/**
	 * Spawn an actor for a new track (game thread only)
	 */
	void SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track);

	/**
	 * Update actor transform for an existing track (game thread only)
	 */
	void UpdateActorForTrack(int32 TrackID, const pharus::TrackRecord& Track);

	/**
	 * Destroy actor for a lost track (game thread only)
	 * @param TrackID The track ID to destroy
	 * @param Reason Why the track was lost ("Removed", "Timeout", "Destroyed")
	 */
	void DestroyActorForTrack(int32 TrackID, const FString& Reason = TEXT("Removed"));

	/**
	 * Process pending operations on game thread
	 * Called via Tick delegate
	 * @param DeltaTime Time since last tick
	 * @return true to keep ticking
	 */
	bool ProcessPendingOperations(float DeltaTime);

	/** Tick delegate handle */
	FTSTicker::FDelegateHandle TickDelegateHandle;

	//--------------------------------------------------------------------------------
	// Helpers
	//--------------------------------------------------------------------------------

	/**
	 * Convert Pharus TrackRecord to FPharusTrackData
	 * @param Track The pharus track record
	 * @param WorldPos Calculated world position
	 * @param InputPos Input tracking coordinates (will be normalized if needed for RawPosition)
	 */
	FPharusTrackData ConvertTrackData(const pharus::TrackRecord& Track, const FVector& WorldPos, const FVector2D& InputPos) const;

	/**
	 * Get FRotator from movement direction (Floor mode - Yaw rotation around World Z)
	 */
	FRotator GetRotationFromDirection(const FVector2D& Direction) const;

	/**
	 * Get FRotator for Wall actor orientation (Wall mode - rotation around Wall Normal)
	 * Projects 2D tracking orientation onto the wall plane so actors rotate
	 * "within" the wall surface instead of around World Z.
	 * @param Direction Movement orientation from tracker (2D vector)
	 * @param WallRegion The wall region for this actor
	 * @return Rotation with actors facing movement direction, aligned to wall surface
	 */
	FRotator GetWallActorRotation(const FVector2D& Direction, const FPharusWallRegion& WallRegion) const;

	/**
	 * Find an existing actor by the expected Pharus track name
	 * Used in dynamic spawn mode to detect and reuse actors that weren't destroyed
	 * (when bAutoDestroyOnTrackLost=false and track re-enters bounds)
	 * @param TrackID The track ID to search for
	 * @return Existing actor if found, nullptr otherwise
	 */
	AActor* FindExistingActorByName(int32 TrackID) const;
};
