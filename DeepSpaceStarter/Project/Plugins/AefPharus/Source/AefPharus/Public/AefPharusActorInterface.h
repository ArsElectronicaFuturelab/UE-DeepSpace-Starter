/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Actor Interface

   Interface for actors that receive Pharus tracking data.
   Implement this interface in your custom actors to receive track IDs
   and react to tracking events.

   Usage in Blueprint:
   1. Create a Blueprint Actor
   2. Add Interface: Class Settings → Interfaces → AefPharusActorInterface
   3. Implement "Set Actor Track ID" event
   4. Set as SpawnClass in configuration

   Usage in C++:
   class AMyActor : public AActor, public IAefPharusActorInterface
   {
       virtual void SetActorTrackID_Implementation(int32 TrackID) override;
   };
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AefPharusActorInterface.generated.h"

/**
 * UInterface wrapper (required by Unreal)
 * Do not modify this class.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UAefPharusActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Pharus Actor Interface
 *
 * Implement this interface to receive tracking data from Pharus system.
 * The tracker instance will call SetActorTrackID() when spawning your actor.
 *
 * This allows your actor to:
 * - Know which Pharus track it represents
 * - Query additional track data from the tracker instance
 * - Implement custom behavior based on track ID or tracking state
 */
class AEFPHARUS_API IAefPharusActorInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when actor is spawned for a new track
	 *
	 * @param TrackID The Pharus track ID this actor represents
	 *
	 * Use this to:
	 * - Store the track ID for later queries
	 * - Initialize actor state based on track data
	 * - Query additional information from tracker instance
	 *
	 * Example in Blueprint:
	 *   Event Set Actor Track ID
	 *     → Store Track ID in variable
	 *     → Initialize visual effects
	 *
	 * Example in C++:
	 *   void AMyActor::SetActorTrackID_Implementation(int32 TrackID)
	 *   {
	 *       MyTrackID = TrackID;
	 *       UE_LOG(LogTemp, Log, TEXT("Spawned for track %d"), TrackID);
	 *   }
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AefXR|Pharus")
	void SetActorTrackID(int32 TrackID);

	/**
	 * Called when actor is spawned for a new track (extended version with instance context)
	 *
	 * @param TrackID The Pharus track ID this actor represents
	 * @param InstanceName Name of the tracker instance that spawned this actor (e.g., "Floor", "Wall")
	 *
	 * This extended version provides full context:
	 * - TrackID: Which track this actor represents
	 * - InstanceName: Which tracker instance spawned this actor
	 *
	 * Benefits:
	 * - Actor can self-validate without manual InstanceName parameter
	 * - Enables automatic track validation and timeout detection
	 * - Supports multiple tracker instances properly
	 * - Pool actors know which pool they belong to
	 *
	 * Example in Blueprint:
	 *   Event Set Actor Track Info
	 *     → Store Track ID and Instance Name
	 *     → Initialize with full context
	 *
	 * Example in C++:
	 *   void AMyActor::SetActorTrackInfo_Implementation(int32 TrackID, FName InstanceName)
	 *   {
	 *       MyTrackID = TrackID;
	 *       MyInstanceName = InstanceName;
	 *       UE_LOG(LogTemp, Log, TEXT("Spawned for track %d in instance %s"), TrackID, *InstanceName.ToString());
	 *   }
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AefXR|Pharus")
	void SetActorTrackInfo(int32 TrackID, FName InstanceName);

	/**
	 * Called when actor is connected to a valid track (TrackID >= 0)
	 *
	 * This event fires when:
	 * - Actor is acquired from pool and assigned to a real track
	 * - Actor is spawned for a new track in dynamic mode
	 *
	 * Use this to initialize actor visuals, start animations, etc.
	 * Does NOT fire when actor is in pool with TrackID=-1
	 *
	 * @param TrackID The valid Pharus track ID (>= 0)
	 * @param InstanceName Name of the tracker instance (e.g., "Floor", "Wall")
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AefXR|Pharus")
	void OnTrackConnected(int32 TrackID, FName InstanceName);

	/**
	 * Called when actor's track is lost or disconnected
	 *
	 * This event fires when:
	 * - Track times out (no UDP packets received)
	 * - Simulator crashes or closes
	 * - Actor is released back to pool
	 *
	 * Use this to cleanup, stop animations, trigger visual effects, etc.
	 *
	 * @param TrackID The track ID that was lost
	 * @param InstanceName Name of the tracker instance
	 * @param Reason Why the track was lost ("Timeout", "Released", "Destroyed")
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AefXR|Pharus")
	void OnTrackLost(int32 TrackID, FName InstanceName, const FString& Reason);
};
