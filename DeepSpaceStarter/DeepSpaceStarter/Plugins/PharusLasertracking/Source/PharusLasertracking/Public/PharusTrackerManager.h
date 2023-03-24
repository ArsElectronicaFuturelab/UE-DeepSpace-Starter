/*========================================================================
	Copyright (c) Ars Electronica Futurelab, 2020
  ========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PharusLasertracking.h" 
#include "TrackLink.h"
#include "PharusTrackerManager.generated.h"

class APharusTrackerManager;

// this is a working Pharus ITrackReceiver child, with Unreal Logging added
class TrackReceiverUConsole : public pharus::ITrackReceiver
{
public:
	TrackReceiverUConsole();
	TrackReceiverUConsole(APharusTrackerManager* ThisUConsolesCreator);
	~TrackReceiverUConsole();

	void onTrackNew(const pharus::TrackRecord& track) override;
	void onTrackUpdate(const pharus::TrackRecord& track) override;
	void onTrackLost(const pharus::TrackRecord& track) override;
	
	APharusTrackerManager* manager;
};

UCLASS(Blueprintable, BlueprintType)
	class PHARUSLASERTRACKING_API APharusTrackerManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APharusTrackerManager();

	UPROPERTY(EditAnywhere, Category = "Pharus Tracker")
		uint8 debugpublicint;

	// PHARUS related settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		bool PharusTracker_IsMulticast = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		int PharusTracker_UDPPort = 44345; // TODO Otto's UDP ports: 3333 44345 64147

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		///FString PharusTracker_BindNIC = "192.168.19.190"; // IP of the *particular* network card where we want to receive the Multicast (DeepSpace specific)
		FString PharusTracker_BindNIC = "127.0.0.1"; // IP of the *particular* network card where we want to receive the Multicast (localhost)

	char* c_BindNIC;
	unsigned short c_udpport;

	// Interaction Actors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
	TMap<int, AActor*> TargetActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
	TMap<int, FTransform> PharusTransforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
	TSubclassOf<class AActor> SpawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		float xsize = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		float ysize = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		bool useLocalSpace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pharus Tracker")
		bool logMovements = true;

private:

	FString PharusTrackerBuildNumber = "0.9";

	//PHARUS TrackLink related
	pharus::TrackLinkClient* p_PHTrackLinkClient = nullptr;
	TrackReceiverUConsole* p_PHTrackReceiverConsole = nullptr;
	
	TMap<int, pharus::TrackRecord> trackDict;



	TArray<int> idsToSpawn;
	TArray<int> idsToChange;
	TArray<int> idsToRemove;
	FCriticalSection criticalSection;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; // Called when the game stops

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//PHARUS callbacks propagated to this class's member functions
	void _onTrackNew(const pharus::TrackRecord& _track);
	void _onTrackUpdate(const pharus::TrackRecord& _track);
	void _onTrackLost(const pharus::TrackRecord& _track);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pharus Tracker")
	void trackUpdated(int id, FVector location, FRotator rotation);

	void SpawnTrackerRepresentant(int trackID);
	void RemoveTrackerRepresentant(int trackID);
	void UpdateTrackerRepresentant(int trackID, FVector2D orientation, FVector2D currentPos);

	UFUNCTION(BlueprintCallable, Category = "Pharus Tracker")
	void _GetTrackRecordForId(int id, bool& success, FVector2D& currentPos, FVector2D& expectPos, FVector2D& relPos, FVector2D& orientation, float& speed);

	UFUNCTION(BlueprintCallable, Category = "Pharus Tracker")
	void _GetTrackRecordForIndex(int index, bool& success, FVector2D& currentPos, FVector2D& expectPos, FVector2D& relPos, FVector2D& orientation, float& speed);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pharus Tracker")
		void SpawnSpawnable(int trackId);

	void SpawnSpawnable_Implementation(int trackId);
};

