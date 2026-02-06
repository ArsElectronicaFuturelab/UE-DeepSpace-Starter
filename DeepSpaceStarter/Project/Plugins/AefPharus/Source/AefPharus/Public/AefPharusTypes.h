/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Data Structures and Types

   This file defines all core data structures for the Pharus tracking system:
   - Mapping modes (Simple, Regions)
   - Wall definitions and regions
   - Instance configuration
   - Track data structures
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "AefPharus.h"
#include "AefPharusTypes.generated.h"

//--------------------------------------------------------------------------------
// ENUMS
//--------------------------------------------------------------------------------

/** Mapping mode defines how 2D tracking data is transformed to 3D world space */
UENUM(BlueprintType)
enum class EAefPharusMappingMode : uint8
{
	/** Simple 2D→3D mapping for horizontal floor tracking */
	Simple		UMETA(DisplayName = "Simple (Floor)"),

	/** Region-based mapping for 4-wall tracking from single planar surface */
	Regions		UMETA(DisplayName = "Regions (4 Walls)")
};

/** Wall side identification for region-based mapping */
UENUM(BlueprintType)
enum class EAefPharusWallSide : uint8
{
	Front		UMETA(DisplayName = "Front Wall"),
	Left		UMETA(DisplayName = "Left Wall"),
	Back		UMETA(DisplayName = "Back Wall"),
	Right		UMETA(DisplayName = "Right Wall"),
	Floor		UMETA(DisplayName = "Floor"),
	Ceiling		UMETA(DisplayName = "Ceiling")
};

//--------------------------------------------------------------------------------
// DATA STRUCTURES
//--------------------------------------------------------------------------------

/**
 * Pharus Wall Region Definition
 *
 * Defines mapping from a 2D tracking region to a 3D wall plane.
 * Used in Regions mapping mode to split one planar tracking surface into multiple 3D planes.
 *
 * Example: Front wall occupies region 0.0-0.25 of tracking surface
 */
USTRUCT(BlueprintType)
struct AEFPHARUS_API FAefPharusWallRegion
{
	GENERATED_BODY()

	/** Which wall this region represents */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	EAefPharusWallSide WallSide = EAefPharusWallSide::Front;

	/** Tracking bounds in normalized coordinates (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FBox2D TrackingBounds = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(0.25f, 1.0f));

	/** World position of wall center (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FVector WorldPosition = FVector::ZeroVector;

	/** World rotation of wall (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/** Physical size of wall in world units (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FVector WorldSize = FVector(1000.0f, 0.0f, 400.0f);

	/** Scale factors for coordinate transformation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FVector2D Scale = FVector2D(100.0f, 100.0f);

	/** Origin offset for coordinate transformation (like Floor SimpleOrigin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	FVector2D Origin = FVector2D::ZeroVector;

	/** Invert Y coordinate (like Floor mapping) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	bool bInvertY = false;

	/** 2D rotation angle in degrees (like FloorRotation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Wall")
	float Rotation2D = 0.0f;

	/** Check if tracking point is within this region's bounds */
	bool ContainsTrackPoint(const FVector2D& TrackPos) const
	{
		return TrackingBounds.IsInside(TrackPos);
	}

	/** 
	 * Transform 2D tracking position to 3D world position on this wall
	 * 
	 * COORDINATE SYSTEM (consistent with Floor mapping):
	 * - Input: Normalized tracking position within this region (0-1)
	 * - (0,0) = bottom-left corner of wall
	 * - (1,1) = top-right corner of wall
	 * - Origin offset shifts the coordinate system (like Floor SimpleOrigin)
	 * 
	 * @param TrackPos Normalized tracking position (0-1 within full tracking surface)
	 * @param RootOrigin Global root origin from subsystem
	 * @param RootRotation Global root rotation from subsystem
	 * @param GlobalWallRotation Global wall rotation from config (applied before per-region Rotation2D)
	 * @return World position in 3D space
	 */
	FVector TrackToWorld(const FVector2D& TrackPos, const FVector& RootOrigin, const FRotator& RootRotation, float GlobalWallRotation = 0.0f) const
	{
		// Step 1: Normalize position within region bounds (0-1)
		FVector2D RegionSize = TrackingBounds.GetSize();
		FVector2D LocalPos = (TrackPos - TrackingBounds.Min) / RegionSize;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 1] LocalPos within region: %s (TrackPos=%s, Bounds.Min=%s, RegionSize=%s)"),
			*LocalPos.ToString(), *TrackPos.ToString(), *TrackingBounds.Min.ToString(), *RegionSize.ToString());

		// Step 2: Apply InvertY if enabled
		if (bInvertY)
		{
			LocalPos.Y = 1.0f - LocalPos.Y;  // Flip Y: 0→1, 1→0
			UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 2] After InvertY: %s"), *LocalPos.ToString());
		}

		// Step 3: Apply scale FIRST (converts 0-1 to actual wall dimensions in cm)
		// This must happen BEFORE rotation to match Floor behavior
		FVector2D ScaledPos = LocalPos * Scale;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 3] After Scale: %s (Scale=%s)"),
			*ScaledPos.ToString(), *Scale.ToString());

		// Step 4: Apply GLOBAL wall rotation AFTER scale (from Config.WallRotation)
		// This is analogous to FloorRotation for floor tracking
		// Applied on scaled coordinates (cm), consistent with Floor behavior
		float TotalRotation = GlobalWallRotation + Rotation2D;
		if (!FMath::IsNearlyZero(TotalRotation))
		{
			float RotationRad = FMath::DegreesToRadians(TotalRotation);
			float CosRot = FMath::Cos(RotationRad);
			float SinRot = FMath::Sin(RotationRad);

			FVector2D RotatedPos;
			RotatedPos.X = ScaledPos.X * CosRot - ScaledPos.Y * SinRot;
			RotatedPos.Y = ScaledPos.X * SinRot + ScaledPos.Y * CosRot;
			ScaledPos = RotatedPos;

			UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 4] After 2D Rotation (Global=%.1f° + Region=%.1f° = %.1f°): %s"), 
				GlobalWallRotation, Rotation2D, TotalRotation, *ScaledPos.ToString());
		}

		// Step 5: Apply Origin offset (consistent with Floor mapping)
		// Origin shifts the coordinate system relative to wall anchor point
		// For corner-based mapping: Origin=(0,0) means (0,0) maps to wall corner
		// For center-based mapping: Origin=(-Scale/2) shifts (0,0) to corner
		FVector2D OffsetPos = ScaledPos + Origin;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 5] After Origin offset: %s (Origin=%s)"),
			*OffsetPos.ToString(), *Origin.ToString());

		// Step 6: Build 3D position on wall plane
		// Wall local space: X=horizontal along wall, Y=0 (wall surface), Z=vertical
		FVector LocalWallPos(OffsetPos.X, 0.0f, OffsetPos.Y);

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 6] LocalWallPos (wall-local): %s"), *LocalWallPos.ToString());

		// Step 7: Apply wall's own rotation to get wall-relative world position
		// WorldRotation defines how this specific wall is oriented
		FVector WallRotatedPos = WorldRotation.RotateVector(LocalWallPos);
		
		// Step 8: Add wall's WorldPosition to get position in wall's local coordinate system
		FVector WallLocalWorld = WorldPosition + WallRotatedPos;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 7-8] WallRotatedPos=%s, WallLocalWorld=%s"),
			*WallRotatedPos.ToString(), *WallLocalWorld.ToString());

		// Step 9: Apply global root rotation to entire wall system
		// This rotates the wall position around the root origin
		// Full 3D rotation is applied to support tilted stages and complex setups
		FVector GlobalRotatedPos = RootRotation.RotateVector(WallLocalWorld);

		// Step 10: Add root origin to get final world position
		FVector WorldPos = RootOrigin + GlobalRotatedPos;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall Transform Step 9-10] RootRotation=%s, GlobalRotated=%s, Final=%s"),
			*RootRotation.ToString(), *GlobalRotatedPos.ToString(), *WorldPos.ToString());

		return WorldPos;
	}

	/**
	 * Transform 2D tracking position to LOCAL 3D position (relative to RootOriginActor)
	 * 
	 * Used when bUseRelativeSpawning=true - actors are attached as children to RootOriginActor.
	 * This skips the RootOrigin/RootRotation application since that's handled by the parent attachment.
	 * 
	 * @param TrackPos Normalized tracking position (0-1 within full tracking surface)
	 * @return Local position relative to RootOriginActor
	 */
	FVector TrackToLocal(const FVector2D& TrackPos, float GlobalWallRotation = 0.0f) const
	{
		// Steps 1-8 are identical to TrackToWorld (region normalization, wall transform)
		
		// Step 1: Normalize position within region bounds (0-1)
		FVector2D RegionSize = TrackingBounds.GetSize();
		FVector2D LocalPos = (TrackPos - TrackingBounds.Min) / RegionSize;

		// Step 2: Apply InvertY if enabled
		if (bInvertY)
		{
			LocalPos.Y = 1.0f - LocalPos.Y;
		}

		// Step 3: Apply scale FIRST (converts 0-1 to actual wall dimensions in cm)
		// This must happen BEFORE rotation to match Floor behavior
		FVector2D ScaledPos = LocalPos * Scale;

		// Step 4: Apply global + per-region 2D rotation AFTER scale
		float TotalRotation = GlobalWallRotation + Rotation2D;
		if (!FMath::IsNearlyZero(TotalRotation))
		{
			float RotationRad = FMath::DegreesToRadians(TotalRotation);
			float CosRot = FMath::Cos(RotationRad);
			float SinRot = FMath::Sin(RotationRad);

			FVector2D RotatedPos;
			RotatedPos.X = ScaledPos.X * CosRot - ScaledPos.Y * SinRot;
			RotatedPos.Y = ScaledPos.X * SinRot + ScaledPos.Y * CosRot;
			ScaledPos = RotatedPos;
		}

		// Step 5: Apply Origin offset
		FVector2D OffsetPos = ScaledPos + Origin;

		// Step 6: Build 3D position on wall plane
		FVector LocalWallPos(OffsetPos.X, 0.0f, OffsetPos.Y);

		// Step 7: Apply wall's own rotation
		FVector WallRotatedPos = WorldRotation.RotateVector(LocalWallPos);
		
		// Step 8: Add wall's WorldPosition to get LOCAL position
		// This is the position relative to RootOriginActor (NO RootRotation applied!)
		FVector RelativePos = WorldPosition + WallRotatedPos;

		UE_LOG(LogAefPharus, Verbose, TEXT("  [Wall TrackToLocal] WallPos=%s, WallRotated=%s, RelativePos=%s"),
			*WorldPosition.ToString(), *WallRotatedPos.ToString(), *RelativePos.ToString());

		return RelativePos;
	}
};

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

	/** Assigned wall (for Regions mode) */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus|Track")
	EAefPharusWallSide AssignedWall = EAefPharusWallSide::Floor;

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

	/** Mapping mode (Simple for floors, Regions for walls) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	EAefPharusMappingMode MappingMode = EAefPharusMappingMode::Simple;

	/** Wall region definitions (for Regions mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	TArray<FAefPharusWallRegion> WallRegions;

	// NOTE: Origin was removed - use [PharusSubsystem].GlobalOrigin in AefConfig.ini 
	// or place AAefPharusRootOriginActor in your level for a global 3D origin point

	/** Scale factors (typically 100.0 for m→cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	FVector2D SimpleScale = FVector2D(100.0f, 100.0f);

	/** Floor Z height for Simple mode (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	float FloorZ = 0.0f;

	/** Floor rotation around Z-axis for Simple mode (degrees, 0=forward is +X, 90=forward is +Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	float FloorRotation = 0.0f;

	/** 
	 * Wall rotation around local axis for Regions mode (degrees)
	 * Applied to all wall regions before individual Rotation2D
	 * Use this to align wall tracking with floor tracking when FloorRotation is used
	 * Example: If FloorRotation=90, set WallRotation=90 for consistent coordinate alignment
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	float WallRotation = 0.0f;

	/** Invert Y-axis for Simple mode (fixes left/right swap) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	bool bInvertY = false;

	/** Physical dimensions of tracking surface in meters (for normalizing absolute coordinates) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping", meta = (ClampMin = "0.1"))
	FVector2D TrackingSurfaceDimensions = FVector2D(10.0f, 15.0f);

	/** Whether incoming coordinates are normalized (0-1) or absolute world dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Mapping")
	bool bUseNormalizedCoordinates = false;
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

	/** Log wall region assignment (for debugging Regions mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Logging")
	bool bLogRegionAssignment = false;

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

	/** Draw wall planes (for Regions mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Debug")
	bool bDebugDrawWallPlanes = false;

	/** Draw region boundaries (for Regions mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AEF|Pharus|Debug")
	bool bDebugDrawRegionBoundaries = false;
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

