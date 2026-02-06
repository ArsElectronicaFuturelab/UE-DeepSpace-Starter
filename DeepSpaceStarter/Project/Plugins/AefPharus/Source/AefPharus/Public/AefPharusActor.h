/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Base Actor Class

   Base actor class for Pharus-tracked objects.
   Inherits from AActor and implements IAefPharusActorInterface.

   Usage:
   1. Inherit from this class in Blueprint or C++
   2. Override SetActorTrackID to customize behavior
   3. Set as SpawnClass in Pharus instance configuration

   This base class provides:
   - Track ID storage
   - Default interface implementation
   - Helper functions for querying tracker instance
   - Optional visual components (add in derived classes)

   Example in INI:
   [Pharus.Floor]
   SpawnClass=/Game/MyProject/BP_PharusActor.BP_PharusActor_C
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AefPharusActorInterface.h"
#include "AefPharusActor.generated.h"

/**
 * Base Actor for Pharus Tracking
 *
 * Default implementation of actor that can be spawned by Pharus tracker.
 * Stores track ID and provides helper functions.
 *
 * Inherit from this class to:
 * - Add visual components (mesh, particles, etc.)
 * - Override SetActorTrackID to customize initialization
 * - Add custom logic based on tracking data
 *
 * The tracker instance will automatically:
 * - Spawn this actor when a new track is detected
 * - Call SetActorTrackID with the track ID
 * - Update actor transform every frame
 * - Destroy actor when track is lost (if configured)
 */
UCLASS(Blueprintable)
class AEFPHARUS_API AAefPharusActor : public AActor, public IAefPharusActorInterface
{
	GENERATED_BODY()

public:
	/**
	 * Constructor
	 */
	AAefPharusActor();

protected:
	/**
	 * Called when the game starts or when spawned
	 */
	virtual void BeginPlay() override;

public:
	/**
	 * Called every frame
	 * @param DeltaTime Time since last frame
	 */
	virtual void Tick(float DeltaTime) override;

	//--------------------------------------------------------------------------------
	// IAefPharusActorInterface Implementation
	//--------------------------------------------------------------------------------

	/**
	 * Called by tracker instance when actor is spawned
	 * @param TrackID The Pharus track ID this actor represents
	 */
	virtual void SetActorTrackID_Implementation(int32 TrackID) override;

	/**
	 * Called by tracker instance when actor is spawned (extended version with instance context)
	 * @param TrackID The Pharus track ID this actor represents
	 * @param InstanceName Name of the tracker instance that spawned this actor
	 */
	virtual void SetActorTrackInfo_Implementation(int32 TrackID, FName InstanceName) override;

	/**
	 * Called when actor is connected to a valid track (TrackID >= 0)
	 * Override in C++ or implement in Blueprint to react to track connection
	 * @param TrackID The valid Pharus track ID (>= 0)
	 * @param InstanceName Name of the tracker instance
	 */
	virtual void OnTrackConnected_Implementation(int32 TrackID, FName InstanceName) override;

	/**
	 * Called when actor's track is lost or disconnected
	 * Override in C++ or implement in Blueprint to react to track loss
	 * @param TrackID The track ID that was lost
	 * @param InstanceName Name of the tracker instance
	 * @param Reason Why the track was lost
	 */
	virtual void OnTrackLost_Implementation(int32 TrackID, FName InstanceName, const FString& Reason) override;

	//--------------------------------------------------------------------------------
	// Public API
	//--------------------------------------------------------------------------------

	/**
	 * Get the track ID this actor represents
	 * @return Track ID, or -1 if not set
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	int32 GetTrackID() const { return TrackID; }

	/**
	 * Get the tracker instance name this actor belongs to
	 * @return Instance name (e.g., "Floor", "Wall"), or None if not set
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	FName GetInstanceName() const { return InstanceName; }

	/**
	 * Check if this actor has a valid track ID
	 * @return true if track ID is valid (>= 0)
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool HasValidTrackID() const { return TrackID >= 0; }

	/**
	 * Check if this actor has valid tracker instance context
	 * @return true if instance name is set
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool HasValidInstanceName() const { return InstanceName != NAME_None; }

	/**
	 * Check if this actor's track is still active in the Pharus system
	 * Uses the stored instance name - no manual parameter needed!
	 * Useful for self-validation to detect if simulator stopped sending data
	 * @return true if track is still receiving UDP updates, false if timed out or lost
	 */
	UFUNCTION(BlueprintPure, Category = "AEF|Pharus")
	bool IsMyTrackActive() const;

protected:
	/**
	 * The Pharus track ID this actor represents
	 * Set by tracker instance via SetActorTrackID() or SetActorTrackInfo()
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus")
	int32 TrackID;

	/**
	 * The tracker instance name this actor belongs to
	 * Set by tracker instance via SetActorTrackInfo() (e.g., "Floor", "Wall")
	 * Enables actors to self-validate and know which instance they belong to
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AEF|Pharus")
	FName InstanceName;

	/**
	 * Blueprint event called when track ID is set
	 * Override this in Blueprint to react to track assignment
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AEF|Pharus", meta = (DisplayName = "On Track ID Set"))
	void OnTrackIDSet(int32 NewTrackID);

	/**
	 * Blueprint event called when track info is set (extended version)
	 * Override this in Blueprint to react to track assignment with full context
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AEF|Pharus", meta = (DisplayName = "On Track Info Set"))
	void OnTrackInfoSet(int32 NewTrackID, FName NewInstanceName);

	/**
	 * Blueprint event called when actor is connected to a valid track
	 * Override this in Blueprint to initialize visuals, start animations, etc.
	 * Only fires for valid tracks (TrackID >= 0), NOT for pool initialization
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AEF|Pharus|Events", meta = (DisplayName = "On Track Connected"))
	void BP_OnTrackConnected(int32 ConnectedTrackID, FName ConnectedInstanceName);

	/**
	 * Blueprint event called when actor's track is lost
	 * Override this in Blueprint to cleanup, stop animations, trigger effects, etc.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AEF|Pharus|Events", meta = (DisplayName = "On Track Lost"))
	void BP_OnTrackLost(int32 LostTrackID, FName LostInstanceName, const FString& LostReason);
};

