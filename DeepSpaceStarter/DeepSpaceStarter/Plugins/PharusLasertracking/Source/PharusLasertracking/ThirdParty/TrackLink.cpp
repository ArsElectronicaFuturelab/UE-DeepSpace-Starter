/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2013-2020
  ========================================================================*/


#include "TrackLink.h"
#include "UDPManager.h"

#include "PharusLasertracking.h" // Unreal Module + AELOG definition
#include "PharusTrackerManager.h" // members of the Manager Actor

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>


using namespace pharus;
///using namespace std; // ID HACK this is intentionally disabled

ITrackReceiver::ITrackReceiver() // ID FIX without a proforma definition this class would not link
{}

ITrackReceiver::~ITrackReceiver() // ID FIX without a proforma definition this class would not link
{}

TrackLinkClient::TrackLinkClient(bool _multicast, unsigned short _port)
: udpman(nullptr)
, threadExit(false)
, multicast(_multicast)
, port(_port)
{
   ///recvThread = thread(&TrackLinkClient::receiveData, this); 
   recvThread = std::thread(&TrackLinkClient::receiveData, this); // ID HACK
}

TrackLinkClient::TrackLinkClient(bool _multicast, const char* _localIP, unsigned short _port) // ID NOTE: added version where we can bind to a specific NIC
	: udpman(nullptr)
	, threadExit(false)
	, multicast(_multicast)
	, localIP(_localIP)
	, port(_port)
{
	///recvThread = thread(&TrackLinkClient::receiveData, this); 
	recvThread = std::thread(&TrackLinkClient::receiveData, this); // ID HACK
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
    recvMutex.lock();
    for (auto iter : trackMap)
    {
        if (iter.second.state != TS_OFF)
            newReceiver->onTrackNew(iter.second);
    }
    trackReceivers.push_back(newReceiver);
    recvMutex.unlock();
}

void TrackLinkClient::unregisterTrackReceiver(ITrackReceiver* oldReceiver)
{
    for (auto receiver = trackReceivers.begin(); receiver != trackReceivers.end(); ++receiver)
    {
        if (*receiver == oldReceiver)
        {
            recvMutex.lock();
            trackReceivers.erase(receiver);
            recvMutex.unlock();
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
        udpman = new UDPManager();
        if (!udpman->Create())
        {
            ///cout << "TrackLinkClient: Unable to create socket, retrying..." << endl; // ID NOTE: implement AELOG
			UE_LOG(AELog, Log, TEXT("PharusTracker :: TrackLinkClient :: --- Unable to create socket, retrying... ---"));
			///this_thread::sleep_for(chrono::seconds(1));
			std::this_thread::sleep_for(std::chrono::seconds(1)); // ID HACK
            delete udpman;
            udpman = nullptr;
            continue;
        }
        
        bool bindOK = false;
		if (multicast)
		{ 
            ///bindOK = udpman->BindMcast("239.1.1.1", port); // ID NOTE: dis is da needed Multicast IP // DEPRECATED
			bindOK = udpman->BindMcast("239.1.1.1", localIP, port); // ID NOTE: tryout to bind socket for receiving Multicast on a particular *specified* NIC
			UE_LOG(AELog, Log, TEXT("PharusTracker :: TrackLinkClient :: Attempting to bind socket on NIC: %s"), *FString(localIP)); // ID DEBUG ONLY
			///bindOK = udpman->BindMcast("239.1.1.1", "192.168.19.190", port); // ID NOTE: tryout to bind socket for receiving Multicast on a *specified* NIC on the DeepSpace wall-machine
		}
        else
		{
            ///bindOK = udpman->Bind(port); // DEPRECATED
            bindOK = udpman->Bind(port, localIP);
			UE_LOG(AELog, Log, TEXT("PharusTracker :: TrackLinkClient :: Attempting to bind socket on NIC: %s"), *FString(localIP)); // ID DEBUG ONLY
            ///bindOK = udpman->Bind(port, "192.168.19.190"); // ID NOTE: tryout to bind socket on a *specified* NIC on the DeepSpace wall-machine
		}

        if (!bindOK)
        {
            ///cout << "TrackLinkClient: Unable to bind socket to port " << port << ", retrying..." << endl; // ID NOTE: implement AELOG
			UE_LOG(AELog, Log, TEXT("PharusTracker :: TrackLinkClient :: --- Unable to bind socket to port! ---"));
			///this_thread::sleep_for(chrono::seconds(1));
			std::this_thread::sleep_for(std::chrono::seconds(1)); // ID HACK
            udpman->Close();
            delete udpman;
            udpman = nullptr;
            continue;
        }
        ///cout << "TrackLinkClient: Successfully set up link at port " << port << endl; // ID NOTE: implement AELOG
		UE_LOG(AELog, Log, TEXT("PharusTracker :: TrackLinkClient :: --- Successfully set up link at port! ---"));
        udpman->SetTimeoutReceive(1);
        connGood = true;
    }

    while (!threadExit)
    {
        recvSize = udpman->Receive(recvBuf, 20480); // ID NOTE: why the hardcoded size?

        if (recvSize > 0)
        {
            //while ((recvSize % 46) != 0)    // apparently not done yet, go on receiving data
            while (recvBuf[recvSize - 1] != 't')    // packet doesn't end with tailing byte, go on receiving
            {
                int framerecv = udpman->Receive(recvBuf + recvSize, 20480 - recvSize);
                recvSize += framerecv;
            }

            int curPos = 0;
            while (curPos < recvSize)
            {
                if (recvBuf[curPos++] != 'T')
                {
					UE_LOG(AELog, Warning, TEXT("PharusTracker :: TrackLinkClient :: --- Unexpected header byte, skipping packet! ---"));
                    curPos = recvSize;
                    continue;
                }

                // get the track's id
                unsigned int tid;
                memcpy(&tid, recvBuf + curPos, 4); curPos += 4;
                recvMutex.lock();
                // is this track known? if so, update, else add:
                bool unknownTrack = (trackMap.find(tid) == trackMap.end());

                if (unknownTrack)
                {
                    ///trackMap.insert(pair<unsigned int, TrackRecord>(tid, TrackRecord()));
                    trackMap.insert(std::pair<unsigned int, TrackRecord>(tid, TrackRecord())); // ID HACK
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
                    ///cout << "TrackLinkClient: Unexpected tailing byte, skipping packet." << endl; // ID NOTE: implement AELOG
					UE_LOG(AELog, Warning, TEXT("PharusTracker :: TrackLinkClient :: --- Unexpected tailing byte, skipping packet! ---"));
                    curPos = recvSize;
                    recvMutex.unlock();
                    continue;
                }

                //notify callbacks
                for (auto receiver : trackReceivers)
                {
                    // track is unknown yet AND is not about to die
                    if (unknownTrack && track.state != TS_OFF)
                    {
                        receiver->onTrackNew(track);
                        //cout << "SIZE: " << trackMap.size() << endl;
                    }
                    // standard track update
                    else if (!unknownTrack && track.state != TS_OFF)
                    {
                        receiver->onTrackUpdate(track);
                    }
                    // track is known and this is his funeral
                    else if (!unknownTrack && track.state == TS_OFF)
                    {
                        receiver->onTrackLost(track);
                        trackMap.erase(trackIter);
                        //cout << "SIZE: " << trackMap.size() << endl;
                    }
                }
                recvMutex.unlock();
            }
        }
    }
    if (udpman)
    {
        udpman->Close();
        delete udpman;
        udpman = nullptr;
    }
        
}
