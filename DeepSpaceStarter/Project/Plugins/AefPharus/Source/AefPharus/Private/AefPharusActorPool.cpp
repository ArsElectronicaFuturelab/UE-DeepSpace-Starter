/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Actor Pool Implementation
  ========================================================================*/

#include "AefPharusActorPool.h"
#include "AefPharus.h"
#include "AefPharusActorInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

//--------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------

bool UAefPharusActorPool::Initialize(
	UWorld* InWorld,
	TSubclassOf<AActor> InActorClass,
	int32 InPoolSize,
	FName InInstanceName,
	const FVector& InSpawnLocation,
	const FRotator& InSpawnRotation,
	const FVector& InIndexOffset)
{
	if (!InWorld)
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Cannot initialize - invalid World context"));
		return false;
	}

	if (!InActorClass)
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Cannot initialize - invalid Actor class"));
		return false;
	}

	if (InPoolSize <= 0)
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Cannot initialize - invalid pool size %d"), InPoolSize);
		return false;
	}

	if (bIsInitialized)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("ActorPool: Already initialized, shutting down first"));
		Shutdown();
	}

	WorldContext = InWorld;
	ActorClass = InActorClass;
	OwningInstanceName = InInstanceName;
	PoolSpawnLocation = InSpawnLocation;
	PoolSpawnRotation = InSpawnRotation;
	PoolIndexOffset = InIndexOffset;

	// Generate unique pool ID for naming (prevents collisions between multiple pools)
	PoolUniqueID = FGuid::NewGuid().ToString().Left(8);

	// Pre-allocate arrays
	PooledActors.Reserve(InPoolSize);
	FreeIndices.Reserve(InPoolSize);

	UE_LOG(LogAefPharus, Log, TEXT("ActorPool [%s]: Spawning %d actors of class %s..."),
		*InInstanceName.ToString(), InPoolSize, *InActorClass->GetName());

	// Spawn all actors
	int32 SuccessCount = 0;
	for (int32 i = 0; i < InPoolSize; ++i)
	{
		AActor* Actor = SpawnPooledActor(i);
		if (Actor)
		{
			PooledActors.Add(Actor);
			FreeIndices.Add(i);
			SuccessCount++;
		}
		else
		{
			UE_LOG(LogAefPharus, Warning, TEXT("ActorPool: Failed to spawn actor %d/%d"), i + 1, InPoolSize);
		}
	}

	bIsInitialized = (SuccessCount > 0);

	if (bIsInitialized)
	{
		UE_LOG(LogAefPharus, Log, TEXT("ActorPool [%s]: Successfully initialized with %d/%d actors at location %s"),
			*InInstanceName.ToString(), SuccessCount, InPoolSize, *PoolSpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Initialization failed - no actors spawned"));
	}

	return bIsInitialized;
}

void UAefPharusActorPool::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogAefPharus, Log, TEXT("ActorPool: Shutting down, destroying %d actors"), PooledActors.Num());

	// Destroy all pooled actors
	for (AActor* Actor : PooledActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}

	PooledActors.Empty();
	FreeIndices.Empty();
	WorldContext = nullptr;
	ActorClass = nullptr;
	bIsInitialized = false;

	UE_LOG(LogAefPharus, Log, TEXT("ActorPool: Shutdown complete"));
}

//--------------------------------------------------------------------------------
// Pool Management
//--------------------------------------------------------------------------------

AActor* UAefPharusActorPool::AcquireActor(int32& OutPoolIndex)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Cannot acquire actor - pool not initialized"));
		OutPoolIndex = INDEX_NONE;
		return nullptr;
	}

	if (FreeIndices.Num() == 0)
	{
		UE_LOG(LogAefPharus, Warning, TEXT("ActorPool: No free actors available (pool exhausted)"));
		OutPoolIndex = INDEX_NONE;
		return nullptr;
	}

	// Get first free index
	OutPoolIndex = FreeIndices[0];
	FreeIndices.RemoveAt(0);

	AActor* Actor = PooledActors[OutPoolIndex];
	if (!Actor || !IsValid(Actor))
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Actor at index %d is invalid"), OutPoolIndex);
		OutPoolIndex = INDEX_NONE;
		return nullptr;
	}

	// Activate actor
	ActivateActor(Actor);

	UE_LOG(LogAefPharus, Verbose, TEXT("ActorPool: Acquired actor at index %d (%d free remaining)"), 
		OutPoolIndex, FreeIndices.Num());

	return Actor;
}

bool UAefPharusActorPool::ReleaseActor(int32 PoolIndex)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Cannot release actor - pool not initialized"));
		return false;
	}

	if (PoolIndex < 0 || PoolIndex >= PooledActors.Num())
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Invalid pool index %d"), PoolIndex);
		return false;
	}

	// Check if already free
	if (FreeIndices.Contains(PoolIndex))
	{
		UE_LOG(LogAefPharus, Warning, TEXT("ActorPool: Actor at index %d is already free"), PoolIndex);
		return false;
	}

	AActor* Actor = PooledActors[PoolIndex];
	if (!Actor || !IsValid(Actor))
	{
		UE_LOG(LogAefPharus, Error, TEXT("ActorPool: Actor at index %d is invalid"), PoolIndex);
		return false;
	}

	// Deactivate actor
	DeactivateActor(Actor, PoolIndex);

	// Reset actor track info to pool defaults (TrackID=-1, InstanceName=OwningInstanceName)
	// This ensures actor doesn't retain old track data when reused
	if (Actor->GetClass()->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
	{
		IAefPharusActorInterface::Execute_SetActorTrackInfo(Actor, -1, OwningInstanceName);
	}

	// Add back to free list
	FreeIndices.Add(PoolIndex);

	UE_LOG(LogAefPharus, Verbose, TEXT("ActorPool: Released actor at index %d (%d free now)"), 
		PoolIndex, FreeIndices.Num());

	return true;
}

AActor* UAefPharusActorPool::GetActorByIndex(int32 PoolIndex) const
{
	if (!bIsInitialized)
	{
		return nullptr;
	}

	if (PoolIndex < 0 || PoolIndex >= PooledActors.Num())
	{
		return nullptr;
	}

	return PooledActors[PoolIndex];
}

//--------------------------------------------------------------------------------
// Private Helpers
//--------------------------------------------------------------------------------

AActor* UAefPharusActorPool::SpawnPooledActor(int32 Index)
{
	if (!WorldContext || !ActorClass)
	{
		return nullptr;
	}

	// Setup spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Use unique pool ID to prevent name collisions between multiple pools
	SpawnParams.Name = FName(*FString::Printf(TEXT("PharusPool_%s_%d"), *PoolUniqueID, Index));
	SpawnParams.ObjectFlags |= RF_Transient; // Don't save pooled actors

	// Calculate spawn location with index offset
	FVector SpawnLocation = PoolSpawnLocation + (PoolIndexOffset * Index);
	FRotator SpawnRotation = PoolSpawnRotation;

	AActor* Actor = WorldContext->SpawnActor(ActorClass, &SpawnLocation, &SpawnRotation, SpawnParams);
	
	if (Actor)
	{
		// Initialize as deactivated
		DeactivateActor(Actor, Index);

		// If actor implements IAefPharusActorInterface, set track info with owning instance context
		if (Actor->GetClass()->ImplementsInterface(UAefPharusActorInterface::StaticClass()))
		{
			IAefPharusActorInterface::Execute_SetActorTrackInfo(Actor, -1, OwningInstanceName);
		}

		UE_LOG(LogAefPharus, Verbose, TEXT("ActorPool: Spawned pooled actor %d: %s"), 
			Index, *Actor->GetName());
	}

	return Actor;
}

void UAefPharusActorPool::ActivateActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Make visible
	Actor->SetActorHiddenInGame(false);

	// Enable tick
	Actor->SetActorTickEnabled(true);

	// Enable collision (optional - depends on use case)
	// Actor->SetActorEnableCollision(true);

	UE_LOG(LogAefPharus, Verbose, TEXT("ActorPool: Activated actor %s"), *Actor->GetName());
}

void UAefPharusActorPool::DeactivateActor(AActor* Actor, int32 PoolIndex)
{
	if (!Actor)
	{
		return;
	}

	// Hide actor
	Actor->SetActorHiddenInGame(true);

	// Disable tick (performance optimization)
	Actor->SetActorTickEnabled(false);

	// Disable collision (optional)
	// Actor->SetActorEnableCollision(false);

	// Reset to configured pool spawn location + offset (keeps pool organized)
	FVector ResetLocation = PoolSpawnLocation + (PoolIndexOffset * PoolIndex);
	Actor->SetActorLocation(ResetLocation);
	Actor->SetActorRotation(PoolSpawnRotation);

	UE_LOG(LogAefPharus, Verbose, TEXT("ActorPool: Deactivated actor %s, reset to %s"),
		*Actor->GetName(), *ResetLocation.ToString());
}
