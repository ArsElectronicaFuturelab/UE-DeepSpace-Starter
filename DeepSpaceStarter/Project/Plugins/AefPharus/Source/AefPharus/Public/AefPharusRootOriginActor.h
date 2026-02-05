/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Root Origin Reference Actor

   A singleton actor that defines the global origin point for ALL Pharus
   tracking transformations. When placed in a level and enabled via config,
   this actor's world position is used as the reference origin for ALL
   coordinate calculations (Floor, Wall, and any other tracking instances).

   Features:
   - Cluster-synchronized via DisplayClusterSceneComponentSyncThis
   - Singleton pattern: Only ONE root origin actor allowed per world
   - Auto-registers with AefPharusSubsystem on BeginPlay
   - Position is read every frame for dynamic origin support (moving stage, etc.)

   Usage:
   1. Place ONE AAefPharusRootOriginActor in your level
   2. Set UsePharusRootOriginActor=true in AefConfig.ini [PharusSubsystem]
   3. Position the actor at your desired tracking origin (e.g., center of stage)
   4. All tracking coordinates will be calculated relative to this point

   Configuration (AefConfig.ini):
   [PharusSubsystem]
   UsePharusRootOriginActor=true   ; Enable this actor as origin source
   ; OR use static origin:
   UsePharusRootOriginActor=false
   GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AefPharusRootOriginActor.generated.h"

// Forward declarations
class UAefPharusSubsystem;

/**
 * Root Origin Reference Actor for Pharus Tracking
 *
 * Singleton actor that provides the global origin point for all tracking
 * coordinate transformations. When placed in a level and enabled, this
 * actor's world position replaces any per-instance origin settings.
 *
 * The actor uses DisplayClusterSceneComponentSyncThis as its root component
 * to ensure position synchronization across all nDisplay cluster nodes.
 *
 * Key Concepts:
 * - GlobalOrigin: The 3D world position used as reference for all tracking
 * - Floor tracking: Positions calculated as GlobalOrigin + (ScaledX, ScaledY, FloorZ)
 * - Wall tracking: Positions calculated as GlobalOrigin + WallTransform
 *
 * Singleton Enforcement:
 * - Only ONE AAefPharusRootOriginActor can be active per world
 * - If multiple actors exist, only the first registered one is used
 * - Duplicate actors log an error and are ignored
 *
 * Dynamic Origin:
 * - Actor position is queried every frame via GetRootOrigin()
 * - Useful for moving stages, tracking on vehicles, etc.
 * - For static origin, use SetRootOrigin() API or GlobalOrigin config instead
 */
UCLASS(Blueprintable)
class AEFPHARUS_API AAefPharusRootOriginActor : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * Constructor
	 * Creates DisplayClusterSceneComponentSyncThis as root for cluster sync
	 */
	AAefPharusRootOriginActor();

protected:
	/**
	 * Called when the game starts
	 * Registers this actor with AefPharusSubsystem as the origin source
	 */
	virtual void BeginPlay() override;

	/**
	 * Called when the actor is destroyed or level ends
	 * Unregisters from AefPharusSubsystem
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	//--------------------------------------------------------------------------------
	// Public API
	//--------------------------------------------------------------------------------

	/**
	 * Get the current origin position in world space
	 * @return World position of this root origin actor (cm)
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
	FVector GetOriginLocation() const;

	/**
	 * Get the current origin rotation in world space
	 * @return World rotation of this root origin actor
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
	FRotator GetOriginRotation() const;

	/**
	 * Check if this actor is currently registered as the active origin
	 * @return true if registered with subsystem
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
	bool IsRegisteredAsOrigin() const;

protected:
	//--------------------------------------------------------------------------------
	// Components
	//--------------------------------------------------------------------------------

	/**
	 * DisplayCluster sync component for automatic cluster replication
	 * Ensures the root origin is synchronized across all nDisplay cluster nodes.
	 * Falls back to standard SceneComponent if DisplayCluster is not available.
	 * 
	 * When DisplayCluster is active:
	 * - Primary node sets the actor position
	 * - Secondary nodes automatically receive the synchronized transform
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AefXR|Pharus|Cluster")
	class USceneComponent* ClusterSyncRoot;

#if WITH_EDITORONLY_DATA
	/**
	 * Billboard component for editor visibility
	 * Shows a visual marker at the origin position in the editor
	 */
	UPROPERTY(VisibleAnywhere, Category = "AefXR|Pharus|Debug")
	class UBillboardComponent* EditorBillboard;

	/**
	 * Arrow component showing forward direction (X-axis)
	 * Helps visualize the coordinate system orientation
	 */
	UPROPERTY(VisibleAnywhere, Category = "AefXR|Pharus|Debug")
	class UArrowComponent* ForwardArrow;
#endif

private:
	/** Cached registration status */
	bool bIsRegistered = false;
};
