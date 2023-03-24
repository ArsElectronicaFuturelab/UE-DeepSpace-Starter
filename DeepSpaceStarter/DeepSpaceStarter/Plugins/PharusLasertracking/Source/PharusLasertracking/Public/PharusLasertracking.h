/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2020
  ========================================================================*/

// ID NOTES:
// * InetAddr.h - added #if PLATFORM_WINDOWS // EPIC UBER METHOD to #include <winsock2.h>
// * InetAddr.h - added #define _WINSOCK_DEPRECATED_NO_WARNINGS UGLY HACK
// * UDPManager.cpp - added #if PLATFORM_WINDOWS // EPIC UBER METHOD to #include <Ws2tcpip.h>
// * MAYBE ISSUE: ws2_32.lib is linked from Build.cs, not sure whether this was proper
// * UDPManager.cpp - disabled TraceLog and all ensuing LOG calls // TODO add Unreal Logging
// * MAYBE ISSUE: UDPManager.cpp - replaced all strcpy calls with strcpy_s [with sizof(third argument)], not sure whether this was proper because double calling functions that produce the argument
// * added NO_TRACELOG pre-processor definition to Build.cs, which skips compiling of all old logging bits in 'UDPManager.cpp'
// * TrackLink.h - changed destructor of ITrackReceiver class to virtual
// * TrackLink.cpp - added proforma constructor/destructor definitions to ITrackReceiver class
// * TrackLink.cpp - disabled 'namespace std'
// * MAYBE ISSUE: TrackLink.cpp - added 'std::' before all presumed-std calls, not sure whether this was proper
// * TrackLink.cpp - disabled all 'cout' // TODO add Unreal Logging

// * TODO make sure thread in TrackLink.cpp is killed properly
// * TODO check if SO_REUSEADDR and SO_REUSEPORT in UDPManager.cpp play nice with this project

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FPharusLasertrackingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

///private:
	///FString PharusLasertrackingBuildNumber = "0.300"; // ID NOTE: there's not much point to version tracking in this place

};

DECLARE_LOG_CATEGORY_EXTERN(AELog, Log, All);
