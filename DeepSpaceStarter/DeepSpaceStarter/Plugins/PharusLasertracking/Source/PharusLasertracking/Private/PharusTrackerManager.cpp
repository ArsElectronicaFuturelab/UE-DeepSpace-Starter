/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2020
  ========================================================================*/

#include "PharusTrackerManager.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "PharusActorInterface.h"

#include "Runtime/Engine/Classes/Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

TrackReceiverUConsole::TrackReceiverUConsole() {}
TrackReceiverUConsole::TrackReceiverUConsole(APharusTrackerManager* ThisUConsolesCreator)
{
	manager = ThisUConsolesCreator;
}	
TrackReceiverUConsole::~TrackReceiverUConsole() {}

void TrackReceiverUConsole::onTrackNew(const pharus::TrackRecord& track)
{
	///std::cout << "New track ID " << track.trackID << " at " << track.currentPos.x << ' ' << track.currentPos.y << std::endl;
	#ifdef VERBOSE_AELOG // set in Build.cs
	std::stringstream ss;
	ss << "New Track ID ----> " << std::hex << track.trackID << " at ----> " << std::hex << track.currentPos.x << " :: " << std::hex << track.currentPos.y << std::endl;
	std::cout << ss.str() << std::endl;
	std::string MSGString = ss.str();
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackReceiverConsole :: %s"), *FString(MSGString.c_str()));
	#endif


	manager->_onTrackNew(track);
}

void TrackReceiverUConsole::onTrackUpdate(const pharus::TrackRecord& track)
{
	///std::cout << "Update track ID" << track.trackID << " at " << track.currentPos.x << ' ' << track.currentPos.y << std::endl;
	#ifdef VERBOSE_AELOG // set in Build.cs
	std::stringstream ss;
	ss << "Update Track ID ----> " << std::hex << track.trackID << " at ----> " << std::hex << track.currentPos.x << " :: " << std::hex << track.currentPos.y << std::endl;
	std::cout << ss.str() << std::endl;
	std::string MSGString = ss.str();
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackReceiverConsole :: %s"), *FString(MSGString.c_str()));
	#endif

	manager->_onTrackUpdate(track);


	for (const auto& echo : track.echoes)
	{
		std::cout << "\t Echo at " << echo.x << ' ' << echo.y << std::endl;
		#ifdef VERBOSE_AELOG // set in Build.cs
		std::stringstream ss;
		ss << "\t Echo at ----> " << std::hex << echo.x << " :: " << std::hex << echo.y << std::endl;
		std::cout << ss.str() << std::endl;
		std::string MSGString2 = ss.str();
		UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackReceiverConsole :: %s"), *FString(MSGString2.c_str()));
		#endif
	}
}

void TrackReceiverUConsole::onTrackLost(const pharus::TrackRecord& track)
{
	///std::cout << "Track ID " << track.trackID << " has left the building!" << std::endl;
	#ifdef VERBOSE_AELOG // set in Build.cs
	std::stringstream ss;
	ss << "Track ID ----> " << std::hex << track.trackID << " has left the building!" << std::endl;
	std::cout << ss.str() << std::endl;
	std::string MSGString = ss.str();
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackReceiverConsole :: %s"), *FString(MSGString.c_str()));
	#endif

	manager->_onTrackLost(track);
}


// Sets default values
APharusTrackerManager::APharusTrackerManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// NOT IN GAME THREAD!
void APharusTrackerManager::_onTrackNew(const pharus::TrackRecord& _track)
{
	// Just saving the ids of the tracks that have to be spawned -> spawning here is not possible because this is called from the DLL and not in Game Thread
	criticalSection.Lock();
	idsToSpawn.Add(_track.trackID);
	trackDict.Add(_track.trackID, _track);
	criticalSection.Unlock();
}

// NOT IN GAME THREAD!
void APharusTrackerManager::_onTrackUpdate(const pharus::TrackRecord& _track)
{
	// Just saving the ids that have been updated -> updating here is not always possible because this is called from the DLL and not in Game Thread and actors that need to be updated may or may not be completely spawned or destroyed here (no way of checking)
	criticalSection.Lock();
	idsToChange.AddUnique(_track.trackID);
	trackDict.Add(_track.trackID, _track);
	criticalSection.Unlock();
}

// NOT IN GAME THREAD!
void APharusTrackerManager::_onTrackLost(const pharus::TrackRecord& _track)
{
	// Just saving the ids that need to be removed -> removing them here would possibly cause a crash because the Game Thread still needs the actor
	criticalSection.Lock();
	idsToRemove.AddUnique(_track.trackID);
	criticalSection.Unlock();
}

void APharusTrackerManager::_GetTrackRecordForId(int id, bool& success, FVector2D& currentPos, FVector2D& expectPos, FVector2D& relPos, FVector2D& orientation, float& speed)
{
	TArray<int> keys;

	criticalSection.Lock();
	trackDict.GetKeys(keys); // Making sure only this thread (Game Thread) can access this variable now
	criticalSection.Unlock();

	if (keys.Contains(id)) {

		criticalSection.Lock();
		pharus::TrackRecord* track = trackDict.Find(id); 
		criticalSection.Unlock();

		success = true;
		currentPos = FVector2D(track->currentPos.y, track->currentPos.x);
		expectPos = FVector2D(track->expectPos.y, track->expectPos.x);
		relPos = FVector2D(track->relPos.y, track->relPos.x);
		orientation = FVector2D(track->orientation.y, track->orientation.x);
		speed = track->speed;
	}
	else {
		success = false;
	}
}

// Is called from BP -> is in Game Thread (Because Blueprint almost always happens in Game Thread)
void APharusTrackerManager::_GetTrackRecordForIndex(int index, bool& success, FVector2D& currentPos, FVector2D& expectPos, FVector2D& relPos, FVector2D& orientation, float& speed) {
	TArray<int> keys;

	criticalSection.Lock();
	trackDict.GetKeys(keys);
	criticalSection.Unlock();

	if (keys.Num() > index) {
		return _GetTrackRecordForId(keys[index], success, currentPos, expectPos, relPos, orientation, speed);
	}
}

// Called when the game starts or when spawned
void APharusTrackerManager::BeginPlay()
{
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: BeginPlay BUILD %s"), *PharusTrackerBuildNumber); 

	Super::BeginPlay();

	idsToSpawn.Empty();
	idsToChange.Empty();
	idsToRemove.Empty();
	trackDict.Empty();

	c_BindNIC = (char*) calloc(PharusTracker_BindNIC.Len(), sizeof(char));
	strcpy(c_BindNIC,TCHAR_TO_ANSI(*PharusTracker_BindNIC));
	c_udpport = static_cast<unsigned short>(PharusTracker_UDPPort);
	p_PHTrackLinkClient = new pharus::TrackLinkClient(PharusTracker_IsMulticast, c_BindNIC, c_udpport); // TrackLink Client's port number is set *here* !
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackLink Client :: NIC to Bind: %s"), *FString(c_BindNIC));
	
	p_PHTrackReceiverConsole = new TrackReceiverUConsole(this);
	p_PHTrackLinkClient->registerTrackReceiver(p_PHTrackReceiverConsole);
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackLink Infrastructure :: --- Receiver Running! ---")); 
}

// Called every frame
void APharusTrackerManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Removing of tracked entities is done here because removing Actors from the world has to be done in Game Thread
	if (idsToRemove.Num() > 0) {

		for (int i = idsToRemove.Num() - 1; i >= 0; i--) {
			criticalSection.Lock();
			
			pharus::TrackRecord* _track = trackDict.Find(idsToRemove[i]);
			
			TArray<int> keys;
			trackDict.GetKeys(keys);

			if (_track) {

				trackDict.Remove(_track->trackID);
				idsToChange.Remove(_track->trackID);

				PharusTransforms.Remove(_track->trackID);

				criticalSection.Unlock();

				if (SpawnClass) {
					RemoveTrackerRepresentant(_track->trackID);
				}
			}
		}

		criticalSection.Lock();
		idsToRemove.Empty();
		criticalSection.Unlock();
	}

	
	// Spawning of tracked entities is done here because it has to be done in Game Thread
	if (idsToSpawn.Num() > 0) {

		for (int i = idsToSpawn.Num() - 1; i >= 0; i--) {
			criticalSection.Lock();
			pharus::TrackRecord* _track = trackDict.Find(idsToSpawn[i]);
			criticalSection.Unlock();

			if(_track){ 

				if (useLocalSpace) {
					trackUpdated(_track->trackID, GetActorLocation() + UKismetMathLibrary::Quat_RotateVector(GetActorRotation().Quaternion(), FVector(_track->currentPos.x * xsize, _track->currentPos.y * ysize, 0)), UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.x, -_track->currentPos.y, 0), FVector::UpVector));
					PharusTransforms.Add(_track->trackID, FTransform(UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.x, -_track->currentPos.y, 0), FVector::UpVector), GetActorLocation() + UKismetMathLibrary::Quat_RotateVector(GetActorRotation().Quaternion(), FVector(_track->currentPos.x * xsize, _track->currentPos.y * ysize, 0)), FVector::OneVector));
				}
				else {
					trackUpdated(_track->trackID, FVector(_track->currentPos.x * xsize, _track->currentPos.y * ysize, 0), UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.x, -_track->currentPos.y, 0), FVector::UpVector));
					PharusTransforms.Add(_track->trackID, FTransform(UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.x, -_track->currentPos.y, 0), FVector::UpVector), GetActorLocation() + FVector(_track->currentPos.x * xsize, _track->currentPos.y * ysize, 0), FVector::OneVector));
				}


				if (SpawnClass) {
					SpawnTrackerRepresentant(_track->trackID);


				}

			}
		}

		criticalSection.Lock();
		idsToSpawn.Empty();
		criticalSection.Unlock();
		
	}

	// Changing the data of spawned Actors is done here because it has to be done in Game Thread
	if (idsToChange.Num() > 0) {
		
		for (int i = idsToChange.Num() - 1; i >= 0; i--) {
			criticalSection.Lock();
			pharus::TrackRecord* _track = trackDict.Find(idsToChange[i]);
			criticalSection.Unlock();

			if (_track) {

				if (useLocalSpace) {

					FVector loc = GetActorLocation() + UKismetMathLibrary::Quat_RotateVector(GetActorRotation().Quaternion(), FVector(_track->currentPos.y * xsize, _track->currentPos.x * ysize, 0));
					FRotator rot = UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.y, -_track->currentPos.x, 0), FVector::UpVector);
					FVector scale = FVector::OneVector;

					trackUpdated(_track->trackID, loc, rot);
					PharusTransforms.Add(_track->trackID, FTransform(rot, loc, scale));
				}
				else {


					trackUpdated(_track->trackID, FVector(_track->currentPos.y * xsize, _track->currentPos.x * ysize, 0), UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.y, -_track->currentPos.x, 0), FVector::UpVector));
					PharusTransforms.Add(_track->trackID, FTransform(UKismetMathLibrary::MakeRotFromYZ(FVector(_track->orientation.y, -_track->currentPos.x, 0), FVector::UpVector), GetActorLocation() + FVector(_track->currentPos.y * xsize, _track->currentPos.x * ysize, 0), FVector::OneVector));
				}

				if (SpawnClass) {
					UpdateTrackerRepresentant(_track->trackID, FVector2D(_track->orientation.y, _track->orientation.x), FVector2D(_track->currentPos.y, _track->currentPos.x));
				}

			}
		}
		criticalSection.Lock();
		idsToChange.Empty();
		criticalSection.Unlock();
	}
}

void APharusTrackerManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	p_PHTrackLinkClient->unregisterTrackReceiver(p_PHTrackReceiverConsole);

	delete p_PHTrackReceiverConsole; 
	delete p_PHTrackLinkClient;
	
	UE_LOG(AELog, Warning, TEXT("APharusTrackerManager :: TrackLink Infrastructure :: --- Shutdown Complete! ---"));

	free(c_BindNIC);

	Super::EndPlay(EndPlayReason);
}

void APharusTrackerManager::SpawnTrackerRepresentant(int trackID) {

	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UWorld* world = GetWorld();
	if (world) {

		AActor* spawnedActor = world->SpawnActor<AActor>(SpawnClass, GetActorLocation(), GetActorRotation(), spawnParams);
		TargetActors.Add(trackID, spawnedActor);	

#
	//	spawnedActor->GetClass()->ImplementsInterface(UPharusActorInterface::StaticClass())
		//IPharusActorInterface* s = Cast<IPharusActorInterface>(spawnedActor->GetClass());

		if (spawnedActor->GetClass()->ImplementsInterface(UPharusActorInterface::StaticClass()))
		{
			IPharusActorInterface::Execute_setActorTrackID(spawnedActor, trackID);
		}


		if (logMovements) {
			UE_LOG(AELog, Log, TEXT("Tracker %d spawned"), trackID);
		}
	}
}

void APharusTrackerManager::RemoveTrackerRepresentant(int trackID) {
	auto actor = TargetActors.Find(trackID);

	if (actor && (*actor)->GetRootComponent())
		(*actor)->Destroy();

	if (logMovements) {
		UE_LOG(AELog, Log, TEXT("Tracker %d removed"), trackID);
	}

	TargetActors.Remove(trackID);
}

void APharusTrackerManager::UpdateTrackerRepresentant(int trackID, FVector2D orientation, FVector2D currentPos) {
	auto actor = TargetActors.Find(trackID);

	if (actor && (*actor)->GetRootComponent()) {
		if (useLocalSpace) {
			FVector loc = GetActorLocation() + UKismetMathLibrary::Quat_RotateVector(GetActorRotation().Quaternion(), FVector(currentPos.X * xsize, currentPos.Y * ysize, 0));
			FRotator rot = UKismetMathLibrary::MakeRotFromYZ(FVector(orientation.X, -orientation.Y, 0), FVector::UpVector);
			(*actor)->SetActorLocationAndRotation(loc, rot);
		}
		else { 
			(*actor)->SetActorLocationAndRotation(FVector(currentPos.X * xsize, currentPos.Y * ysize, 0), UKismetMathLibrary::MakeRotFromYZ(FVector(orientation.X, -orientation.Y, 0), FVector::UpVector)); 
		}		

		if (logMovements) {
			UE_LOG(AELog, Log, TEXT("Tracker %d updated"), trackID);
		}
	}
}