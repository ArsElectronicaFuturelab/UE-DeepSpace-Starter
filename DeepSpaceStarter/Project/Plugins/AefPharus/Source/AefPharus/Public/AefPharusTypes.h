/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Data Structures and Types

   This file defines all core data structures for the Pharus floor tracking system:
   - Instance configuration
   - Track data structures
   - Error codes and delegates
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "AefPharus.h"
#include "AefPharusTypes.generated.h"

//--------------------------------------------------------------------------------
// DATA STRUCTURES
//--------------------------------------------------------------------------------

/**
 * Pharus Track Data
 *
 * Runtime tracking data for a single track.
 * Contains position, velocity, and state information.
 */
USTRUCT(BlueprintType)
struct AEFPHARUS_API FAefPharusTrackData
{
	GENERATED_BODY()

	/** Unique track ID */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	int32 TrackID = -1;

	/** Current world position (cm) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	FVector WorldPosition = FVector::ZeroVector;

	/** Current velocity (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	FVector Velocity = FVector::ZeroVector;

	/** Current speed (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	float Speed = 0.0f;

	/** Movement orientation (normalized direction vector) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	FVector2D Orientation = FVector2D::ZeroVector;

	/** Raw tracking position (normalized 0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	FVector2D RawPosition = FVector2D::ZeroVector;

	/** Last time this track received an update (for timeout detection) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	double LastUpdateTime = 0.0;

	/** Is this track currently inside valid tracking bounds? 
	 *  When true: Actor is spawned and visible
	 *  When false: Track is known but actor is hidden/not spawned (outside boundary)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	bool bIsInsideBoundary = true;
};

/**
 * Pharus Instance Configuration
 *
 * Complete configuration for a single Pharus tracker instance.
 * Can be loaded from INI file or configured at runtime.
 */
USTRUCT(BlueprintType)
struct AEFPHARUS_API FAefPharusInstanceConfig
{
	GENERATED_BODY()

	//--------------------------------------------------------------------------------
	// Identity
	//--------------------------------------------------------------------------------

	/** Unique name for this tracker instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Identity")
	FName InstanceName = "Floor";

	/** Enable this tracker instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Identity")
	bool bEnable = true;

	//--------------------------------------------------------------------------------
	// Network Configuration
	//--------------------------------------------------------------------------------

	/** Network interface IP to bind to (e.g., "192.168.101.55") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Network")
	FString BindNIC = "127.0.0.1";

	/** UDP port for receiving tracking data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Network")
	int32 UDPPort = 44345;

	/** Use multicast (true) or unicast (false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Network")
	bool bIsMulticast = true;

	/** Multicast group address (if bIsMulticast = true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Network")
	FString MulticastGroup = "239.1.1.1";

	//--------------------------------------------------------------------------------
	// Mapping Configuration
	//--------------------------------------------------------------------------------

	// NOTE: Origin was removed - use [PharusSubsystem].GlobalOrigin in AefConfig.ini 
	// or place AAefPharusRootOriginActor in your level for a global 3D origin point

	/** Scale factors (typically 100.0 for m→cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	FVector2D SimpleScale = FVector2D(100.0f, 100.0f);

	/** Floor Z height offset (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	float FloorZ = 0.0f;

	/** Floor rotation around Z-axis (degrees, 0=forward is +X, 90=forward is +Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	float FloorRotation = 0.0f;

	/** Invert Y-axis (fixes left/right swap) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	bool bInvertY = false;

	/** Physical dimensions of tracking surface in meters (for normalizing absolute coordinates) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping", meta = (ClampMin = "0.1"))
	FVector2D TrackingSurfaceDimensions = FVector2D(10.0f, 15.0f);

	/** Whether incoming coordinates are normalized (0-1) or absolute world dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	bool bUseNormalizedCoordinates = false;

	//--------------------------------------------------------------------------------
	// Actor Spawning
	//--------------------------------------------------------------------------------

	/** How to handle spawn collisions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Spawning")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	/** Automatically destroy actors when tracking is lost */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Spawning")
	bool bAutoDestroyOnTrackLost = true;

	//--------------------------------------------------------------------------------
	// Transform & Performance
	//--------------------------------------------------------------------------------

	/** Use local space (relative to parent actor) instead of world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Transform")
	bool bUseLocalSpace = false;

	/** Apply rotation based on movement direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Transform")
	bool bApplyOrientationFromMovement = true;

	/**
	 * DEPRECATED: UseRelativeSpawning is now controlled globally in [PharusSubsystem].
	 * This per-instance setting is ignored. The global setting applies to ALL instances.
	 * 
	 * When UseRelativeSpawning=true in [PharusSubsystem] AND UsePharusRootOriginActor=true:
	 * - Actors are attached as children to the RootOriginActor
	 * - Eliminates per-frame transform calculations when origin moves
	 * - Recommended for nDisplay setups with moving camera/stage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Transform", meta = (DeprecatedProperty, DeprecationMessage = "UseRelativeSpawning is now controlled globally in [PharusSubsystem]"))
	bool bUseRelativeSpawning = false;

	//--------------------------------------------------------------------------------
	// Actor Pool (Performance & Cluster Synchronization)
	//--------------------------------------------------------------------------------
	// Actor pooling provides two benefits:
	// 1. PERFORMANCE: Pre-spawned actors avoid runtime spawn/destroy overhead and GC pressure
	// 2. NDISPLAY CLUSTER: Deterministic actor indices ensure consistent references across cluster nodes
	// Pooling is recommended for ALL deployments, not just nDisplay configurations!

	/** 
	 * Use pre-spawned actor pool instead of dynamic spawning.
	 * Benefits: Better performance (no spawn/destroy overhead), required for nDisplay cluster sync.
	 * Recommended: true for most deployments.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|ActorPool")
	bool bUseActorPool = true;

	/** Number of actors to pre-spawn in pool (should match max expected concurrent trackers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|ActorPool", meta = (EditCondition = "bUseActorPool", ClampMin = "1", ClampMax = "200"))
	int32 ActorPoolSize = 50;

	/** Location where pooled actors are spawned and returned when inactive (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|ActorPool", meta = (EditCondition = "bUseActorPool"))
	FVector PoolSpawnLocation = FVector::ZeroVector;

	/** Rotation for pooled actors when inactive (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|ActorPool", meta = (EditCondition = "bUseActorPool"))
	FRotator PoolSpawnRotation = FRotator::ZeroRotator;

	/** Offset each pooled actor by index to prevent visual clustering (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|ActorPool", meta = (EditCondition = "bUseActorPool"))
	FVector PoolIndexOffset = FVector(0.0f, 10.0f, 0.0f);

	//--------------------------------------------------------------------------------
	// Performance
	//--------------------------------------------------------------------------------

	/** Allow live adjustments without restart */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Performance")
	bool bLiveAdjustments = true;

	/**
	 * Track lost timeout in seconds - tracks without UDP updates for this duration are treated as lost.
	 * Detects when simulator stops sending data (crash/close) vs. person standing still.
	 * Recommended: 2-5 seconds. Set to 0 to disable timeout detection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Performance", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float TrackLostTimeout = 3.0f;

	//--------------------------------------------------------------------------------
	// Logging & Debug
	//--------------------------------------------------------------------------------

	/** Log when tracks are spawned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogTrackerSpawned = true;

	/** Log track updates (VERY VERBOSE!) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogTrackerUpdated = false;

	/** Log when tracks are removed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogTrackerRemoved = true;

	/** Log network packet statistics */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogNetworkStats = false;

	/** Log rejected tracks (positions outside valid bounds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogRejectedTracks = true;

	/** Enable debug visualization in editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Debug")
	bool bDebugVisualization = false;

	/** Draw tracking bounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Debug")
	bool bDebugDrawBounds = false;

	/** Draw origin point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Debug")
	bool bDebugDrawOrigin = false;
};

//--------------------------------------------------------------------------------
// ERROR HANDLING
//--------------------------------------------------------------------------------

/**
 * Pharus Instance Creation Error Codes
 *
 * Describes all possible errors that can occur during tracker instance creation.
 */
UENUM(BlueprintType)
enum class EAefPharusCreateInstanceError : uint8
{
	/** Operation succeeded */
	Success						UMETA(DisplayName = "Success"),

	/** Instance is disabled in configuration (bEnable = false) */
	InstanceDisabled			UMETA(DisplayName = "Instance Disabled"),

	/** Instance with this name already exists */
	InstanceAlreadyExists		UMETA(DisplayName = "Instance Already Exists"),

	/** SpawnClass does not implement IAefPharusActorInterface */
	SpawnClassMissingInterface	UMETA(DisplayName = "SpawnClass Missing Interface"),

	/** Failed to create UAefPharusInstance object */
	InstanceCreationFailed		UMETA(DisplayName = "Instance Creation Failed"),

	/** No valid World context available */
	NoValidWorld				UMETA(DisplayName = "No Valid World"),

	/** Instance->Initialize() failed */
	InitializationFailed		UMETA(DisplayName = "Initialization Failed"),

	/** SpawnClass parameter is nullptr */
	SpawnClassIsNull			UMETA(DisplayName = "SpawnClass Is Null"),

	/** Invalid World context parameter */
	InvalidWorldContext			UMETA(DisplayName = "Invalid World Context"),

	/** Failed to create UDP socket */
	NetworkSocketFailed			UMETA(DisplayName = "Network Socket Failed"),

	/** Failed to join multicast group */
	MulticastJoinFailed			UMETA(DisplayName = "Multicast Join Failed"),

	/** Failed to create TrackLink client */
	TrackLinkClientFailed		UMETA(DisplayName = "TrackLink Client Failed"),

	/** UDP port is invalid (0, negative, or > 65535) */
	InvalidUDPPort				UMETA(DisplayName = "Invalid UDP Port"),

	/** Network configuration is invalid (empty BindNIC) */
	InvalidNetworkConfig		UMETA(DisplayName = "Invalid Network Config")
};

/**
 * Pharus Instance Creation Result
 *
 * Contains success/failure status and detailed error information.
 * Returned by CreateTrackerInstance functions.
 */
USTRUCT(BlueprintType)
struct AEFPHARUS_API FAefPharusCreateInstanceResult
{
	GENERATED_BODY()

	/** Was the operation successful? */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Result")
	bool bSuccess = false;

	/** Error code (Success if bSuccess = true) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Result")
	EAefPharusCreateInstanceError ErrorCode = EAefPharusCreateInstanceError::Success;

	/** Human-readable error message with context */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Result")
	FString ErrorMessage;

	/** Helper: Create success result */
	static FAefPharusCreateInstanceResult MakeSuccess()
	{
		FAefPharusCreateInstanceResult Result;
		Result.bSuccess = true;
		Result.ErrorCode = EAefPharusCreateInstanceError::Success;
		Result.ErrorMessage = TEXT("Success");
		return Result;
	}

	/** Helper: Create error result */
	static FAefPharusCreateInstanceResult MakeError(EAefPharusCreateInstanceError InErrorCode, const FString& InErrorMessage)
	{
		FAefPharusCreateInstanceResult Result;
		Result.bSuccess = false;
		Result.ErrorCode = InErrorCode;
		Result.ErrorMessage = InErrorMessage;
		return Result;
	}
};

//--------------------------------------------------------------------------------
// DELEGATES
//--------------------------------------------------------------------------------

/** Delegate called when a track is spawned */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAefPharusTrackSpawnedDelegate, int32, TrackID, AActor*, SpawnedActor);

/** Delegate called when a track is updated */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAefPharusTrackUpdatedDelegate, int32, TrackID, const FAefPharusTrackData&, TrackData);

/** Delegate called when a track is lost */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAefPharusTrackLostDelegate, int32, TrackID);

