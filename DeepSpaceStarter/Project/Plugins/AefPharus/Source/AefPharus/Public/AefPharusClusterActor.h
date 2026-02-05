/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Cluster Actor Class

   nDisplay-optimized actor class for Pharus-tracked objects.
   Inherits from AAefPharusActor and uses DisplayClusterSceneComponentSyncThis
   for automatic transform synchronization across cluster nodes.

   Usage:
   1. Use this class (or derive from it) when deploying with nDisplay
   2. Set as SpawnClass in Pharus instance configuration
   3. Transforms are automatically synchronized to all cluster nodes

   Example in INI:
   [Pharus.Floor]
   SpawnClass=/Script/AefPharus.AefPharusClusterActor

   Or use a Blueprint derived from this class:
   SpawnClass=/Game/MyProject/BP_PharusClusterActor.BP_PharusClusterActor_C

   Benefits:
   - Automatic transform replication across cluster nodes
   - Only spawns on primary node (handled by AefPharusInstance)
   - Compatible with all nDisplay configurations
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "AefPharusActor.h"
#include "AefPharusClusterActor.generated.h"

/**
 * nDisplay Cluster-Optimized Pharus Actor
 *
 * Extends AAefPharusActor with automatic cluster synchronization.
 * Uses DisplayClusterSceneComponentSyncThis as root component to automatically
 * replicate transform changes from primary node to all secondary nodes.
 *
 * Key Features:
 * - Spawns only on primary cluster node (via AefPharusInstance check)
 * - Automatic transform synchronization to secondary/backup nodes
 * - Drop-in replacement for AAefPharusActor in nDisplay environments
 * - Works seamlessly in standalone mode (no cluster overhead)
 *
 * Inherit from this class to:
 * - Add visual components (mesh, particles, etc.)
 * - Override SetActorTrackInfo to customize initialization
 * - Add custom logic based on tracking data
 * - Use IsMyTrackActive() to self-validate track status
 *
 * The tracker instance will automatically:
 * - Spawn this actor when a new track is detected (primary node only)
 * - Call SetActorTrackInfo with track ID AND instance name (full context)
 * - Update actor transform every frame (synced to all nodes)
 * - Destroy actor when track is lost or times out (if configured)
 *
 * Cluster actors automatically receive:
 * - TrackID: Which Pharus track this actor represents
 * - InstanceName: Which tracker instance spawned it (e.g., "Floor", "Wall")
 * - Self-validation capability via IsMyTrackActive() (no manual parameters needed)
 */
UCLASS(Blueprintable)
class AEFPHARUS_API AAefPharusClusterActor : public AAefPharusActor
{
	GENERATED_BODY()

public:
	/**
	 * Constructor
	 * Replaces default SceneComponent with DisplayClusterSceneComponentSyncThis
	 */
	AAefPharusClusterActor();

protected:
	/**
	 * DisplayCluster sync component for automatic cluster replication
	 * This component automatically synchronizes transform changes from the
	 * primary node to all secondary/backup nodes in the cluster.
	 * 
	 * Note: If DisplayCluster is not available, this will be nullptr and
	 * a standard USceneComponent will be used as RootComponent instead.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AefXR|Pharus|Cluster")
	class USceneComponent* ClusterSyncComponent;
};
