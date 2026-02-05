/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Actor Pool Class

   Pre-spawned actor pool for performance optimization and nDisplay cluster synchronization.
   
   Purpose:
   - Spawn actors at level start (on ALL cluster nodes in nDisplay mode)
   - Actors are initially hidden and inactive
   - Primary node (or single-player mode) activates/deactivates actors as needed
   - Transform sync handled automatically by DisplayClusterSceneComponentSync (nDisplay)
   
   Benefits:
   PERFORMANCE (ALL deployments):
   - Zero spawn/destroy overhead at runtime
   - Reduced garbage collection pressure
   - Consistent memory footprint
   - Better frame-time stability
   
   NDISPLAY CLUSTER:
   - Deterministic: All nodes have identical actors
   - Zero latency: No network events needed
   - Reliable: No event loss possible
   - Simple: Minimal complexity
   
   IMPORTANT: Actor pooling is recommended for ALL deployments, not just nDisplay!
   
   Usage:
   1. Initialize pool at level start (all nodes)
   2. Primary node acquires actors from pool
   3. Primary node releases actors back to pool
   4. (nDisplay) Transforms automatically sync via DisplayClusterSceneComponentSync
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AefPharusActorPool.generated.h"

/**
 * Actor Pool for Performance & nDisplay Cluster Synchronization
 *
 * Manages a pool of pre-spawned actors that exist on all cluster nodes.
 * The primary node controls actor visibility and activation, while
 * transform synchronization happens automatically via DisplayClusterSceneComponentSync.
 *
 * Key Features:
 * - Performance: Avoids spawn/destroy overhead at runtime
 * - Deterministic: All nodes spawn identical actors in identical order
 * - Pool-based: Actors are reused, not destroyed
 * - Hidden by default: Actors are invisible until activated
 * - Index-based access: Ensures same actor on all nodes
 *
 * Thread Safety:
 * - All methods should be called from game thread only
 * - Primary node check happens in caller (AefPharusInstance)
 */
UCLASS()
class AEFPHARUS_API UAefPharusActorPool : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the actor pool
	 * Called on ALL cluster nodes at level start
	 *
	 * @param InWorld World context for spawning actors
	 * @param InActorClass Class of actors to spawn
	 * @param InPoolSize Number of actors to pre-spawn
	 * @param InInstanceName Name of owning tracker instance (e.g., "Floor", "Wall")
	 * @param InSpawnLocation Location where pooled actors spawn/rest (cm)
	 * @param InSpawnRotation Rotation for pooled actors when inactive (degrees)
	 * @param InIndexOffset Offset per actor index to prevent clustering (cm)
	 * @return true if initialization succeeded
	 */
	bool Initialize(
		UWorld* InWorld,
		TSubclassOf<AActor> InActorClass,
		int32 InPoolSize,
		FName InInstanceName,
		const FVector& InSpawnLocation = FVector::ZeroVector,
		const FRotator& InSpawnRotation = FRotator::ZeroRotator,
		const FVector& InIndexOffset = FVector::ZeroVector
	);

	/**
	 * Shutdown and cleanup the pool
	 * Destroys all pooled actors
	 */
	void Shutdown();

	/**
	 * Acquire a free actor from the pool
	 * Should only be called on PRIMARY NODE
	 * 
	 * @param OutPoolIndex Index of acquired actor (for deterministic access)
	 * @return Acquired actor, or nullptr if pool is exhausted
	 */
	AActor* AcquireActor(int32& OutPoolIndex);

	/**
	 * Release an actor back to the pool
	 * Should only be called on PRIMARY NODE
	 * 
	 * @param PoolIndex Index of actor to release
	 * @return true if actor was successfully released
	 */
	bool ReleaseActor(int32 PoolIndex);

	/**
	 * Get actor by pool index (deterministic access)
	 * Can be called on ANY node
	 * 
	 * @param PoolIndex Index of actor in pool
	 * @return Actor at index, or nullptr if invalid index
	 */
	AActor* GetActorByIndex(int32 PoolIndex) const;

	/**
	 * Check if pool has free actors available
	 * 
	 * @return true if at least one actor is available
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Pool")
	bool HasFreeActors() const { return FreeIndices.Num() > 0; }

	/**
	 * Get number of free actors in pool
	 * 
	 * @return Number of available actors
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Pool")
	int32 GetFreeActorCount() const { return FreeIndices.Num(); }

	/**
	 * Get total pool size
	 * 
	 * @return Total number of actors in pool
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Pool")
	int32 GetPoolSize() const { return PooledActors.Num(); }

	/**
	 * Get number of active (acquired) actors
	 * 
	 * @return Number of actors currently in use
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Pool")
	int32 GetActiveActorCount() const { return PooledActors.Num() - FreeIndices.Num(); }

	/**
	 * Check if pool is initialized
	 * 
	 * @return true if pool is ready to use
	 */
	UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Pool")
	bool IsInitialized() const { return bIsInitialized; }

private:
	/**
	 * Spawn a single actor for the pool
	 * Called during initialization
	 * 
	 * @param Index Pool index for this actor
	 * @return Spawned actor, or nullptr on failure
	 */
	AActor* SpawnPooledActor(int32 Index);

	/**
	 * Activate an actor (make visible and enable tick)
	 * 
	 * @param Actor Actor to activate
	 */
	void ActivateActor(AActor* Actor);

	/**
	 * Deactivate an actor (hide and disable tick)
	 * 
	 * @param Actor Actor to deactivate
	 * @param PoolIndex Index in the pool (to reset to correct slot position)
	 */
	void DeactivateActor(AActor* Actor, int32 PoolIndex);

private:
	/** All pooled actors (spawned on all nodes) */
	UPROPERTY()
	TArray<AActor*> PooledActors;

	/** Indices of free (available) actors */
	TArray<int32> FreeIndices;

	/** World context for spawning */
	UPROPERTY()
	UWorld* WorldContext;

	/** Actor class to spawn */
	UPROPERTY()
	TSubclassOf<AActor> ActorClass;

	/** Unique pool ID for naming (prevents collisions between multiple pools) */
	FString PoolUniqueID;

	/** Name of the tracker instance that owns this pool (e.g., "Floor", "Wall") */
	FName OwningInstanceName;

	/** Location where pooled actors spawn and rest when inactive */
	FVector PoolSpawnLocation;

	/** Rotation for pooled actors when inactive */
	FRotator PoolSpawnRotation;

	/** Offset applied per actor index (prevents visual clustering) */
	FVector PoolIndexOffset;

	/** Is pool initialized? */
	bool bIsInitialized;
};
