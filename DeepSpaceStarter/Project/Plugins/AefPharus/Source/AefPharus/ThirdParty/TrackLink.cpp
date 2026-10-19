/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2013-2025

   IMPROVEMENTS IN THIS VERSION:
   - Smart pointers (std::unique_ptr) for automatic cleanup
   - Thread-safe callback dispatch (copy data before releasing lock)
   - Updated module references for AefPharus
  ========================================================================*/

#include "TrackLink.h"
#include "UDPManager.h"

#include "AefPharus.h" // Module logging
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace pharus;

ITrackReceiver::ITrackReceiver()
{}

ITrackReceiver::~ITrackReceiver()
{}

TrackLinkClient::TrackLinkClient(bool _multicast, unsigned short _port, const char* _multicastGroup)
: udpman(nullptr)
, threadExit(false)
, multicast(_multicast)
, localIP(nullptr)
, port(_port)
, multicastGroup(_multicastGroup)
{
   recvThread = std::thread(&TrackLinkClient::receiveData, this);
}

TrackLinkClient::TrackLinkClient(bool _multicast, const char* _localIP, unsigned short _port, const char* _multicastGroup)
	: udpman(nullptr)
	, threadExit(false)
	, multicast(_multicast)
	, localIP(_localIP)
	, port(_port)
	, multicastGroup(_multicastGroup)
{
	recvThread = std::thread(&TrackLinkClient::receiveData, this);
}

TrackLinkClient::~TrackLinkClient()
{
    threadExit = true;
    recvThread.join();
}

void TrackLinkClient::registerTrackReceiver(ITrackReceiver* newReceiver)
{
    if (!newReceiver)
        return;

    for (ITrackReceiver* receiver : trackReceivers)
    {
        if (receiver == newReceiver)  // already added
            return;
    }

    // notify new receiver about current tracks
    std::lock_guard<std::mutex> lock(recvMutex);  // RAII lock
    for (auto iter : trackMap)
    {
        if (iter.second.state != TS_OFF)
            newReceiver->onTrackNew(iter.second);
    }
    trackReceivers.push_back(newReceiver);
}

void TrackLinkClient::unregisterTrackReceiver(ITrackReceiver* oldReceiver)
{
    for (auto receiver = trackReceivers.begin(); receiver != trackReceivers.end(); ++receiver)
    {
        if (*receiver == oldReceiver)
        {
            std::lock_guard<std::mutex> lock(recvMutex);  // RAII lock
            trackReceivers.erase(receiver);
            return;
        }
    }
}

const TrackMap& TrackLinkClient::getTrackMap() const
{
    return trackMap;
}

void TrackLinkClient::receiveData()
{
    char recvBuf[20480];
    int recvSize = 1;

    // set up udp connection
    bool connGood = false;
    while (!connGood && !threadExit)
    {
        udpman = std::make_unique<UDPManager>();  // FIXED: Use smart pointer
        if (!udpman->Create())
        {
			UE_LOG(LogAefPharus, Log, TEXT("TrackLinkClient: Unable to create socket, retrying..."));
			std::this_thread::sleep_for(std::chrono::seconds(1));
            udpman.reset();  // Explicit reset (automatic on reassignment anyway)
            continue;
        }

        bool bindOK = false;
		if (multicast)
		{
			bindOK = udpman->BindMcast(multicastGroup, localIP, port);
			UE_LOG(LogAefPharus, Log, TEXT("TrackLinkClient: Attempting to bind multicast %s on NIC: %s"), multicastGroup ? *FString(multicastGroup) : TEXT("239.1.1.1"), localIP ? *FString(localIP) : TEXT("INADDR_ANY"));
		}
        else
		{
            bindOK = udpman->Bind(port, localIP);
			UE_LOG(LogAefPharus, Log, TEXT("TrackLinkClient: Attempting to bind unicast on NIC: %s"), localIP ? *FString(localIP) : TEXT("INADDR_ANY"));
		}

        if (!bindOK)
        {
			UE_LOG(LogAefPharus, Warning, TEXT("TrackLinkClient: Unable to bind socket to port %d, retrying..."), port);
			std::this_thread::sleep_for(std::chrono::seconds(1));
            udpman->Close();
            udpman.reset();
            continue;
        }

        UE_LOG(LogAefPharus, Log, TEXT("TrackLinkClient: Successfully bound to port %d"), port);
        udpman->SetTimeoutReceive(1);
        connGood = true;
    }

    while (!threadExit)
    {
        recvSize = udpman->Receive(recvBuf, 20480);

        if (recvSize > 0)
        {
            while (!threadExit && recvSize > 0 && recvBuf[recvSize - 1] != 't')
            {
                int framerecv = udpman->Receive(recvBuf + recvSize, 20480 - recvSize);
                if (framerecv <= 0)
                {
                    UE_LOG(LogAefPharus, Warning, TEXT("TrackLinkClient: Incomplete packet, dropping (ret=%d)"), framerecv);
                    recvSize = 0;
                    break;
                }
                if (recvSize + framerecv > 20480)
                {
                    UE_LOG(LogAefPharus, Warning, TEXT("TrackLinkClient: Packet exceeds buffer, dropping"));
                    recvSize = 0;
                    break;
                }
                recvSize += framerecv;
            }

            int curPos = 0;
            while (curPos < recvSize)
            {
                if (recvBuf[curPos++] != 'T')
                {
					UE_LOG(LogAefPharus, Warning, TEXT("TrackLinkClient: Unexpected header byte, skipping packet"));
                    curPos = recvSize;
                    continue;
                }

                // get the track's id
                unsigned int tid;
                memcpy(&tid, recvBuf + curPos, 4); curPos += 4;

                // CRITICAL FIX: Copy track data and receiver list under lock, then dispatch callbacks without lock
                TrackRecord trackCopy;
                std::vector<ITrackReceiver*> receiversCopy;
                bool unknownTrack = false;
                bool shouldNotify = false;

                {
                    std::lock_guard<std::mutex> lock(recvMutex);  // RAII lock - scope limited

                    // is this track known? if so, update, else add:
                    unknownTrack = (trackMap.find(tid) == trackMap.end());

                    if (unknownTrack)
                    {
                        trackMap.insert(std::pair<unsigned int, TrackRecord>(tid, TrackRecord()));
                    }

                    auto trackIter = trackMap.find(tid);
                    TrackRecord& track = trackIter->second;

                    track.trackID = tid; //necessary for new tracks
                    memcpy(&(track.state),         recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.currentPos.x),  recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.currentPos.y),  recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.expectPos.x),   recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.expectPos.y),   recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.orientation.x), recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.orientation.y), recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.speed),         recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.relPos.x),      recvBuf + curPos, 4); curPos += 4;
                    memcpy(&(track.relPos.y),      recvBuf + curPos, 4); curPos += 4;

                    track.echoes.clear();
                    while (recvBuf[curPos] == 'E')  // peek if echo(es) available
                    {
                        ++curPos;   // yep, then skip 'E'
						PharusVector2f e;
                        memcpy(&(e.x), recvBuf + curPos, 4); curPos += 4;
                        memcpy(&(e.y), recvBuf + curPos, 4); curPos += 4;
                        track.echoes.push_back(e);
                        ++curPos;   // 'e'
                    }

                    if (recvBuf[curPos++] != 't')
                    {
						UE_LOG(LogAefPharus, Warning, TEXT("TrackLinkClient: Unexpected tailing byte, skipping packet"));
                        curPos = recvSize;
                        continue;  // Lock will be released here
                    }

                    // Copy data for callback dispatch outside the lock
                    trackCopy = track;
                    receiversCopy = trackReceivers;
                    shouldNotify = true;

                    // If track is being removed, erase from map now (under lock)
                    if (!unknownTrack && track.state == TS_OFF)
                    {
                        trackMap.erase(trackIter);
                    }
                } // LOCK RELEASED HERE - critical section ends

                // Now dispatch callbacks WITHOUT holding the lock
                if (shouldNotify)
                {
                    for (auto receiver : receiversCopy)
                    {
                        // track is unknown yet AND is not about to die
                        if (unknownTrack && trackCopy.state != TS_OFF)
                        {
                            receiver->onTrackNew(trackCopy);
                        }
                        // standard track update
                        else if (!unknownTrack && trackCopy.state != TS_OFF)
                        {
                            receiver->onTrackUpdate(trackCopy);
                        }
                        // track is known and this is his funeral
                        else if (!unknownTrack && trackCopy.state == TS_OFF)
                        {
                            receiver->onTrackLost(trackCopy);
                        }
                    }
                }
            }
        }
    }

    // Smart pointer will automatically clean up udpman
    if (udpman)
    {
        udpman->Close();
        udpman.reset();
    }
}
