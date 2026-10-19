# AefPharus – Pharus Floor Tracking for Unreal Engine

| | |
|---|---|
| Version | 3.2.0 |
| Engine | Unreal Engine 5.7 |
| Platform | Windows 64-bit |
| Dependencies | nDisplay, AefUtility |
| Developer | Ars Electronica Futurelab |
| License | MIT – see [LICENSE](../../../../LICENSE) |

AefPharus receives Pharus laser-tracking data (TrackLink protocol over UDP), maps every track onto the floor
and spawns one actor per tracked person. It runs as a `UGameInstanceSubsystem`, is configured through
`Config/AefConfig.ini` and works in nDisplay clusters. The path of the configuration file is resolved by the
[AefUtility](../AefUtility/README.md) plugin.

---

## How it works

```
Pharus / TrackLink simulator ──UDP──► UAefPharusInstance (network thread)
                                          │  relPos (0-1) → validate → normalize
                                          ▼
                                   pending queue ──► game thread tick
                                                        │  spawn / update / destroy actor
                                                        ▼
                                                 Actor implementing IAefPharusActorInterface
```

- **`UAefPharusSubsystem`** – entry point. Reads `AefConfig.ini`, creates/removes tracker instances, owns the global root origin.
- **`UAefPharusInstance`** – one per tracking surface (UDP port). Handles the TrackLink client, coordinate mapping and actor lifecycle.
- **`IAefPharusActorInterface`** – implemented by every spawned actor; receives track ID, instance name and connect/lost events.
- **`AAefPharusActor` / `AAefPharusClusterActor`** – ready-made base classes (the cluster variant adds nDisplay transform sync).
- **`AAefPharusRootOriginActor`** – optional level actor that defines origin and rotation of the tracking space.
- **`UAefPharusActorPool`** – optional pre-spawned actor pool (deterministic names for nDisplay).

### Coordinate mapping

For each track the plugin uses the TrackLink `relPos` (normalized 0-1, TUIO origin top-left) and computes:

1. Normalize (`UseNormalizedCoordinates=false` divides by `TrackingSurfaceDimensions`)
2. Reject tracks outside 0-1 (actor is hidden; `bIsInsideBoundary=false`)
3. `InvertY` → `1 - Y`
4. Multiply by `Scale` (cm)
5. Rotate by `FloorRotation` (degrees), set Z = `FloorZ`
6. Apply root rotation and root origin (`GlobalRotation`/`GlobalOrigin` or the `AAefPharusRootOriginActor`)

With `UseRelativeSpawning=true` step 6 is skipped and actors are attached to the root origin actor instead.

---

## Setup

1. The plugin lives in `Project/Plugins/AefPharus/` and is enabled by default. It requires
   `Project/Plugins/AefUtility/`, which is enabled automatically as a dependency.
2. Configure `Project/Config/AefConfig.ini` (see reference below). In packaged builds the file is copied
   automatically to `<Build>/DeepSpaceStarter/Config/AefConfig.ini`, where it stays editable.
3. Provide a spawn class that implements `IAefPharusActorInterface`, e.g. derive from
   `AAefPharusClusterActor` or use the example `BP_PharusClusterFloorActor`.
4. Start tracking:
   - `AutoStartSystem=true` – instances are created automatically after `AutoCreateDelay`, or
   - `AutoStartSystem=false` – call `StartPharusSystem()` yourself (the project's `BP_Pharus_Manager` does this).
5. For testing without hardware run a simulator from `Pharus/DeepSpace/`
   (e.g. `START_PHARUS_FLOOR_UNICAST.bat`). Its surface size must match `TrackingSurfaceDimensions`.

### Minimal C++ usage

```cpp
#include "AefPharusSubsystem.h"

UAefPharusSubsystem* Pharus = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();
Pharus->SetSpawnClassOverride("Floor", AMyTrackedActor::StaticClass()); // optional, before start
Pharus->StartPharusSystem();

if (UAefPharusInstance* Floor = Pharus->GetTrackerInstance("Floor"))
{
	Floor->OnTrackSpawned.AddDynamic(this, &AMyManager::HandleTrackSpawned);
	Floor->OnTrackLost.AddDynamic(this, &AMyManager::HandleTrackLost);
}
```

The module name for `Build.cs` is `AefPharus`.

---

## Configuration reference (`Config/AefConfig.ini`)

AefPharus reads the file returned by `UAefUtilityBPLibrary::GetConfigFilePath()`:

| Start | File used |
|---|---|
| no argument | `<Project>/Config/AefConfig.ini` |
| `-aefConfig=<path>` | the given file (relative paths start at the project directory); if it does not exist, an error is logged (`LogAefUtility`) and the default file is used |

The configuration is read once at startup; restart after changes. `AefConfig.ini` may contain sections of other
plugins (e.g. `[DeepSync]`); AefPharus only reads the sections below.

### `[PharusSubsystem]`

| Key | Default | Description |
|---|---|---|
| `DefaultInstance` | – | Name of the primary instance (informational). |
| `AutoCreateInstances` | – | Comma-separated instance names. Each name `X` is read from `[Pharus.X]`. |
| `AutoStartSystem` | `true` | Create instances automatically on startup. `false` = call `StartPharusSystem()`. |
| `AutoCreateDelay` | `0.5` | Seconds before auto-creation (clamped 0.1-10). Gives nDisplay/network time to come up. |
| `GlobalOrigin` | `(X=0,Y=0,Z=0)` | Static origin of the tracking space (cm). |
| `GlobalRotation` | `(Pitch=0,Yaw=0,Roll=0)` | Static rotation of the tracking space. `(P=0,Y=0,R=0)` is accepted too. |
| `UsePharusRootOriginActor` | `false` | Use a placed `AAefPharusRootOriginActor` (position + rotation, updated every frame). |
| `UseRelativeSpawning` | `false` | Attach actors to the root origin actor and use local coordinates. Requires `UsePharusRootOriginActor=true`. |

### `[Pharus.<Instance>]` (e.g. `[Pharus.Floor]`)

| Key | Default | Description |
|---|---|---|
| `Enable` | `true` | Disabled instances are not created. |
| `BindNIC` | `127.0.0.1` | Local interface to bind (`0.0.0.0` = all). |
| `UDPPort` | `44345` | TrackLink UDP port. |
| `IsMulticast` / `MulticastGroup` | `true` / `239.1.1.1` | Multicast reception. |
| `DefaultSpawnClass` | – | Class path of the actor to spawn, e.g. `/AefPharus/Examples/PharusActors/BP_PharusClusterFloorActor.BP_PharusClusterFloorActor_C`. Used by auto-creation; overridden by `SetSpawnClassOverride()`. |
| `TrackingSurfaceDimensions` | `(X=10,Y=15)` | Tracking surface in meters (used when coordinates are absolute). |
| `UseNormalizedCoordinates` | `false` | `true` = input is 0-1 (TrackLink `relPos`, recommended). |
| `Scale` | `(X=100,Y=100)` | World size of the 0-1 range in cm. Deep Space 8K floor: `(X=1500.0,Y=830.0)`. |
| `FloorZ` | `0` | Height offset (cm) relative to the origin. |
| `FloorRotation` | `0` | 2D rotation of the tracking coordinates (degrees). |
| `InvertY` | `false` | Flip Y (`1 - Y`). |
| `AutoDestroyOnTrackLost` | `true` | `false` = hide and reuse actors instead of destroying them. |
| `SpawnCollisionHandling` | `AlwaysSpawn` | Collision handling for dynamic spawning. |
| `UseActorPool` | `true` | Pre-spawn actors (recommended for nDisplay). |
| `ActorPoolSize` | `50` | Number of pooled actors (1-200). |
| `PoolSpawnLocation` / `PoolSpawnRotation` / `PoolIndexOffset` | `(0,0,0)` / `(0,0,0)` / `(0,10,0)` | Resting transform of inactive pool actors. |
| `UseLocalSpace` | `false` | Set relative instead of world transforms (absolute mode only). |
| `ApplyOrientationFromMovement` | `true` | Rotate actors (yaw) along their movement direction. |
| `LiveAdjustments` | `true` | Allow `UpdateConfig`/`UpdateFloorSettings`/`RestartWithNewNetwork` at runtime. |
| `TrackLostTimeout` | `3.0` | Seconds without UDP update before a track counts as lost (0 = off). |
| `LogTrackerSpawned` / `LogTrackerUpdated` / `LogTrackerRemoved` | `true` / `false` / `true` | Per-track logging (`Updated` is very verbose). |
| `LogRejectedTracks` | `true` | Log tracks outside the 0-1 bounds. |

Notes:
- `DebugVisualization`, `DebugDrawBounds`, `DebugDrawOrigin` and `LogNetworkStats` are parsed but currently have no effect.
- Keys that are no longer read since 3.1.0: `MappingMode`, `WallRotation`, `Wall.*`, `LogRegionAssignment`, `DebugDrawWallPlanes`, `DebugDrawRegionBoundaries`, `EnableLiveLink`, `EnableSlotMapping`, `MaxSlots`, `ReuseLowestSlot`.

---

## Blueprint / C++ API

All functions are in the Blueprint category `AEF|Pharus`.

### `UAefPharusSubsystem`

| Function | Purpose |
|---|---|
| `StartPharusSystem()` / `StopPharusSystem()` / `IsPharusSystemRunning()` | Create all configured instances / remove all instances. |
| `SetSpawnClassOverride(Instance, Class)` | Override `DefaultSpawnClass`; call before `StartPharusSystem()`. |
| `CreateTrackerInstance(Config, SpawnClass)` | Create an instance from a full `FAefPharusInstanceConfig`. Returns `FAefPharusCreateInstanceResult` (success flag, `EAefPharusCreateInstanceError`, message). |
| `CreateTrackerInstanceSimple(Name, BindNIC, UDPPort, SpawnClass)` | Create an instance with defaults. |
| `GetFloorSettingsFromDisk(Instance, OutConfig)` | Read `[Pharus.<Instance>]` from disk into a config struct. |
| `GetTrackerInstance` / `RemoveTrackerInstance` / `GetAllInstanceNames` / `GetInstanceCount` / `HasInstance` | Instance management. |
| `IsTrackActive(Instance, TrackID)` | Check whether a track still receives data. |
| `SetRootOrigin` / `GetRootOrigin` / `SetRootOriginRotation` / `GetRootOriginRotation` / `HasValidRootOrigin` | Static root origin. |
| `IsUsingRootOriginActor` / `GetRootOriginActor` / `IsRelativeSpawningActive` | Dynamic root origin state. |
| `GetIsPharusDebug` / `SetIsPharusDebug` | Verbose logging of instance creation. |

### `UAefPharusInstance`

| Function / Event | Purpose |
|---|---|
| `GetTrackData(TrackID, OutPosition, OutRotation, bOutIsInsideBoundary)` | Current transform of a track. |
| `GetActiveTrackIDs` / `GetActiveTrackCount` / `GetSpawnedActor` / `IsTrackActive` | Track queries. |
| `GetConfig` / `UpdateConfig` / `UpdateFloorSettings` | Read and change mapping settings at runtime (`LiveAdjustments=true`). |
| `RestartWithNewNetwork(BindNIC, UDPPort)` | Reconnect with new network settings (destroys all actors). |
| `SetSpawnClass` / `GetSpawnClass` | Change the class for new tracks. |
| `OnTrackSpawned(TrackID, Actor)` / `OnTrackUpdated(TrackID, FAefPharusTrackData)` / `OnTrackLost(TrackID)` | Multicast delegates. |
| `DebugInjectTrack(X, Y, TrackID)` | Inject a test track at a normalized position. |
| `UpdateFloorSettingsSimple(...)` | Deprecated – use `UpdateFloorSettings`. |

`FAefPharusTrackData` contains `TrackID`, `WorldPosition`, `Velocity`, `Speed`, `Orientation`, `RawPosition` (0-1),
`LastUpdateTime` and `bIsInsideBoundary`.

### `IAefPharusActorInterface` (implemented by spawned actors)

| Event | Called when |
|---|---|
| `SetActorTrackInfo(TrackID, InstanceName)` | An actor is assigned to a track (or reset to `-1` when returned to the pool). |
| `OnTrackConnected(TrackID, InstanceName)` | A track becomes active for this actor. |
| `OnTrackLost(TrackID, InstanceName, Reason)` | The track is lost: `LeftBounds` (track lost or left the tracking area) or `Timeout` (no UDP updates for `TrackLostTimeout`). |
| `SetActorTrackID(TrackID)` | Legacy, kept for compatibility. |

`AAefPharusActor` implements the interface and exposes `GetTrackID`, `GetInstanceName`, `HasValidTrackID`,
`IsMyTrackActive` and the Blueprint events `OnTrackInfoSet`, `BP_OnTrackConnected`, `BP_OnTrackLost`.
`AAefPharusClusterActor` additionally carries an nDisplay sync component so transforms replicate to all cluster nodes.

### `AAefPharusRootOriginActor`

Place exactly one in the level and set `UsePharusRootOriginActor=true`. It registers itself with the subsystem
(`GetOriginLocation`, `GetOriginRotation`, `IsRegisteredAsOrigin`) and is cluster-synchronized. Moving or rotating it
moves the whole tracking space.

---

## nDisplay

- Every node receives the tracking data, but only the primary node activates actors; actors exist on all nodes and are
  synchronized via the nDisplay sync components of `AAefPharusClusterActor` / `AAefPharusRootOriginActor`.
- With `UseActorPool=true` pooled actors are named `PharusPool_<Instance>_<Index>` (e.g. `PharusPool_Floor_0`),
  identical on every node, so name-based actor sync works.
- `AutoCreateDelay` gives the cluster time to initialize before sockets are opened.

---

## Troubleshooting

| Symptom | Check |
|---|---|
| No actors spawn | Log `LogAefPharus`: "No DefaultSpawnClass found" or "Failed to load SpawnClass" → fix `DefaultSpawnClass`. With `AutoStartSystem=false` make sure `StartPharusSystem()` is called. |
| Wrong or no config loaded | Log `LogAefUtility` shows the file in use ("Using default config" / "Using config override"). "Configuration file not found" from `LogAefPharus` → the file is missing at that path; in a packaged build check `<Build>/DeepSpaceStarter/Config/AefConfig.ini`. |
| Tracks are rejected | `LogRejectedTracks=true` shows positions outside 0-1 → check `UseNormalizedCoordinates` / `TrackingSurfaceDimensions`. |
| Actors at wrong position/orientation | `Scale`, `FloorRotation`, `InvertY`, root origin (`GlobalOrigin` or the placed root origin actor). |
| No data received | `BindNIC`, `UDPPort`, `IsMulticast`/`MulticastGroup`, firewall; the simulator must send to the same port. |
| Startup summary | On load the subsystem logs all `[PharusSubsystem]` values and a summary per instance. |

Enable verbose output with `log LogAefPharus Verbose` in the console.

---

See [CHANGELOG.md](CHANGELOG.md) for version history.

© 2020-2026 Ars Electronica Futurelab
