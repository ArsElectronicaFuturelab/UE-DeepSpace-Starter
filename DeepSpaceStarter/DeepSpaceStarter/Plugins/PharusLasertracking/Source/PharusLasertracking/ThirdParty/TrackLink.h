/*========================================================================
   Copyright (c) Ars Electronica Futurelab, 2013-2020
  ========================================================================*/

#pragma once
///#ifndef  ___TRACKLINK__H__
///#define  ___TRACKLINK__H__

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>

class UDPManager; // ID NOTE: Otto's 2020 fix

namespace pharus
{
//! Helper structure to store two-dimensional values
struct PharusVector2f
{
    float x, y;
};

//!Denotes the track's state
enum TrackState
{
    //! The track has been made public for the first iteration
    TS_NEW,
    //! The track is already known - this is a position update
    TS_CONT,
    //! The track has disappeared - this is the last notification of it
    TS_OFF
};
//! Structure that holds all information of a track.
/** Distances, positions are in meters, velocities in meters per second */
struct TrackRecord
{
    //! The track's unique ID
    unsigned int trackID;
    //! The track's current position
	PharusVector2f currentPos;
    //! The position the track is expected in the next frame
	PharusVector2f expectPos;
    //! The track's current position in relative coordinates (TUIO style)
	PharusVector2f relPos;
    //! The track's current heading (normalized directional vector). Valid if speed is above 0.25.
	PharusVector2f orientation;
    //! The track's current speed
    float speed;
    //! Yields in what state the track currently is
    TrackState state;
    //! Vector of CONFIRMED echoes that 'belong' to this track in relative coordinates (TUIO style)
    std::vector<PharusVector2f> echoes;
};
//! Track map with track ID as key value
typedef std::map<unsigned int, TrackRecord> TrackMap;

//! Base class for everything that wants to receive track updates from TransmissionClient
/** A class that should be notified of track updates has to derive from ITrackReceiver.
  * It features callback methods for each a new track, a position update for an existing
  * track and one for a leaving track.
  * \note See TransmissionClient to obtain a list of all current tracks. */
class ITrackReceiver
{
public:
	//! The Constructor
	ITrackReceiver();
	//! The Destructor
	virtual ~ITrackReceiver(); // ID FIX class had virtual functions but not a virtual destructor
    //! A new or unknown track has appeared
    /** This is the first notification of a track that has entered the
      * Observation Space. Tracks that are already known to Pharus but not
      * yet to your client are treated as "new" tracks as well. */
    virtual void onTrackNew(const TrackRecord&) = 0;
    //! A known track received an update
    /** Once a track is considered 'known' it will receive continuous updates
      * over this callback once a frame. */
    virtual void onTrackUpdate(const TrackRecord&) = 0;
    //! A known track has disappeared
    /** When a previously known track disappears (eg. left the area) the client
      * is notified one last time via this callback. It's safe to delete your
      * internal representation of it as once this function is called the track
      * will not appear anymore. */
    virtual void onTrackLost(const TrackRecord&) = 0;
};

//! Handles network connection and updates registered track receivers
/** This class deals with the actual network packets. It keeps a list of registered
  * ITrackReceiver derivates that are continuously updated with track data.
  * Of course it is possible to register as many receivers as desired. */
class TrackLinkClient
{
public:
    //! The Constructor
    /** Implicitly tries to connect to Pharus over the default
      * multicast address 239.1.1.1 on port 44345
      * Set multicast to false to use TrackLink in a unicast setup*/
    TrackLinkClient(bool _multicast = true, unsigned short _port = 44345);
	TrackLinkClient(bool _multicast = true, const char* _localIP = "192.168.19.190", unsigned short _port = 44345); // ID NOTE: added version where we can bind to a specific NIC
    //! The Destructor
    ~TrackLinkClient();
    //! Add a track receiver to be provided with tracking data
    /** Adds an ITrackReceiver derivate to the list. All tracks already known to the client
      * are immediately introduced via onTrackNew() to establish consistency.*/
    void registerTrackReceiver(ITrackReceiver*);
    //! Removes a track receiver from the list
    /** If a track receiver no longer wishes to be notified of track updates it should be
      * removed from the list.
      * \note Never leave deleted track receivers registered with TrackLinkClient as
      * this will cause a segfault. */
    void unregisterTrackReceiver(ITrackReceiver*);
    //! Obtain all current tracks at once
    /** It is recommended to use ITrackReceiver's callbacks to obtain data. As such, contrary
      * to all other methods, this one is not thread-safe, so race conditions have to be expected.
      * It's just left here in case someone desperately looks for an iterateable for the tracking data.
      * \return A const reference to TrackLinkClient's internal keep-safe of tracks. */
    const TrackMap& getTrackMap() const;

private:
    std::vector<ITrackReceiver*> trackReceivers;
    TrackMap trackMap;
    class UDPManager* udpman;
    std::thread recvThread;
    std::atomic<bool> threadExit;
    std::mutex recvMutex;
    void receiveData();
    bool multicast;
	const char* localIP; // ID NOTE: added so we can bind to a specific NIC
    unsigned short port;
};

} // #end namespace pharus
///#endif // ___TRACKLINK__H__
