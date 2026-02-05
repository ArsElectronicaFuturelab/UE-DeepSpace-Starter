/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Cluster Actor Implementation
  ========================================================================*/

#include "AefPharusClusterActor.h"
#include "AefPharus.h"

// Check if DisplayCluster components are available
#if __has_include("Components/DisplayClusterSceneComponentSyncThis.h")
	#include "Components/DisplayClusterSceneComponentSyncThis.h"
	#define AefPharus_HAS_DISPLAYCLUSTER_COMPONENTS 1
#else
	#define AefPharus_HAS_DISPLAYCLUSTER_COMPONENTS 0
	#include "Components/SceneComponent.h"
#endif

//--------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------

AAefPharusClusterActor::AAefPharusClusterActor()
{
	// Replace default SceneComponent root with DisplayCluster sync component
	// This enables automatic transform synchronization across cluster nodes
	if (RootComponent)
	{
		RootComponent->DestroyComponent();
		RootComponent = nullptr;
	}

#if AefPharus_HAS_DISPLAYCLUSTER_COMPONENTS
	// Create DisplayCluster sync component as new root (if available)
	ClusterSyncComponent = CreateDefaultSubobject<UDisplayClusterSceneComponentSyncThis>(TEXT("ClusterSyncRoot"));
	RootComponent = ClusterSyncComponent;
	
	UE_LOG(LogAefPharus, Verbose, TEXT("AefPharusClusterActor created with DisplayCluster sync component"));
#else
	// Fallback to standard SceneComponent if DisplayCluster is not available
	ClusterSyncComponent = nullptr;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	UE_LOG(LogAefPharus, Warning, TEXT("AefPharusClusterActor: DisplayCluster not available, using standard SceneComponent"));
#endif
}
