/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2025

   AefPharus - Base Actor Implementation
  ========================================================================*/

#include "AefPharusActor.h"
#include "AefPharus.h"
#include "AefPharusSubsystem.h"
#include "Engine/World.h"

//--------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------

AAefPharusActor::AAefPharusActor()
{
	// Enable tick for this actor
	PrimaryActorTick.bCanEverTick = true;

	// Initialize track ID to invalid
	TrackID = -1;

	// Initialize instance name to none
	InstanceName = NAME_None;

	// Set default root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

//--------------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------------

void AAefPharusActor::BeginPlay()
{
	Super::BeginPlay();

	// Log spawn (useful for debugging)
	if (HasValidTrackID())
	{
		UE_LOG(LogAefPharus, Verbose, TEXT("PharusActor spawned for track %d at %s"),
			TrackID, *GetActorLocation().ToString());
	}
}

void AAefPharusActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Override in derived classes to add per-frame logic
	// Note: Transform updates are handled by the tracker instance
}

//--------------------------------------------------------------------------------
// IAefPharusActorInterface Implementation
//--------------------------------------------------------------------------------

void AAefPharusActor::SetActorTrackID_Implementation(int32 InTrackID)
{
	TrackID = InTrackID;

	UE_LOG(LogAefPharus, Verbose, TEXT("Actor %s assigned to track %d"),
		*GetName(), TrackID);

	// Notify Blueprint
	OnTrackIDSet(TrackID);
}

void AAefPharusActor::SetActorTrackInfo_Implementation(int32 InTrackID, FName InInstanceName)
{
	// Update stored values
	TrackID = InTrackID;
	InstanceName = InInstanceName;

	UE_LOG(LogAefPharus, Verbose, TEXT("Actor %s assigned to track %d in instance '%s'"),
		*GetName(), TrackID, *InstanceName.ToString());

	// Notify Blueprint (legacy events for compatibility)
	OnTrackIDSet(TrackID);
	OnTrackInfoSet(TrackID, InstanceName);

	// NOTE: OnTrackConnected and OnTrackLost are fired explicitly by AefPharusInstance
	// at the correct times to avoid duplicate events and provide accurate Reason strings
}

void AAefPharusActor::OnTrackConnected_Implementation(int32 InTrackID, FName InInstanceName)
{
	UE_LOG(LogAefPharus, Log, TEXT("Actor %s connected to track %d in instance '%s'"),
		*GetName(), InTrackID, *InInstanceName.ToString());

	// Notify Blueprint (parameter names renamed to avoid shadowing class properties)
	BP_OnTrackConnected(InTrackID, InInstanceName);
}

void AAefPharusActor::OnTrackLost_Implementation(int32 InTrackID, FName InInstanceName, const FString& Reason)
{
	UE_LOG(LogAefPharus, Log, TEXT("Actor %s lost track %d in instance '%s' (Reason: %s)"),
		*GetName(), InTrackID, *InInstanceName.ToString(), *Reason);

	// Notify Blueprint (parameter names renamed to avoid shadowing class properties)
	BP_OnTrackLost(InTrackID, InInstanceName, Reason);
}

//--------------------------------------------------------------------------------
// Track Validation
//--------------------------------------------------------------------------------

bool AAefPharusActor::IsMyTrackActive() const
{
	// Check if we have valid context
	if (!HasValidTrackID() || !HasValidInstanceName())
	{
		UE_LOG(LogAefPharus, Warning, TEXT("Actor %s: Cannot validate track - missing TrackID or InstanceName"), *GetName());
		return false;
	}

	// Get the Pharus subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	UAefPharusSubsystem* PharusSubsystem = GameInstance->GetSubsystem<UAefPharusSubsystem>();
	if (!PharusSubsystem)
	{
		return false;
	}

	// Check if our track is still active using stored instance name
	return PharusSubsystem->IsTrackActive(InstanceName, TrackID);
}
