# AefPharus Plugin - Complete Technical Documentation

**Version**: 3.0.0
**Last Updated**: 2026-02-05
**Author**: Ars Electronica Futurelab

> ⚠️ **v3.0.0 Breaking Change**: LiveLink integration has been completely removed from this plugin.
> Any LiveLink references in this documentation are outdated and will be removed in a future update.

---

## Table of Contents

### I. System Architecture
- [1.1 Overview](#11-overview)
- [1.2 Component Hierarchy](#12-component-hierarchy)
- [1.3 Data Flow](#13-data-flow)
- [1.4 Thread Model](#14-thread-model)
- [1.5 Coordinate System (TUIO Standard)](#15-coordinate-system-tuio-standard)

### II. Core Components
- [2.1 UAefPharusSubsystem](#21-uAefPharussubsystem)
- [2.2 UAefPharusInstance](#22-uAefPharusinstance)
- [2.3 TrackLink Network Layer](#23-tracklink-network-layer)
- [2.4 Actor Pool System](#24-actor-pool-system)

### III. Configuration System
- [3.1 INI Configuration](#31-ini-configuration)
- [3.2 Instance Configuration](#32-instance-configuration)
- [3.3 Network Settings](#33-network-settings)
- [3.4 Live Adjustments](#34-live-adjustments)
- [3.5 Global Root Origin System](#35-global-root-origin-system)

### IV. Mapping Modes
- [4.1 Simple Mode (Floor Tracking)](#41-simple-mode-floor-tracking)
- [4.2 Regions Mode (Wall Tracking)](#42-regions-mode-wall-tracking)

### V. Actor Lifecycle
- [5.1 Spawning Logic](#51-spawning-logic)
- [5.2 Update Mechanism](#52-update-mechanism)
- [5.3 Destruction Process](#53-destruction-process)
- [5.4 Actor Interface](#54-actor-interface)

### VI. nDisplay Integration
- [6.1 Actor Pool Architecture](#61-actor-pool-architecture)
- [6.2 Cluster Synchronization](#62-cluster-synchronization)
- [6.3 Primary vs Secondary Nodes](#63-primary-vs-secondary-nodes)
- [6.4 Pool Configuration](#64-pool-configuration)

- - [7.2 Slot Mapping](#72-slot-mapping)
- [7.3 Subject Naming](#73-subject-naming)
- [7.4 Use Cases](#74-use-cases)

### VIII. Blueprint API
- [8.1 Subsystem Nodes](#81-subsystem-nodes)
- [8.2 Instance Nodes](#82-instance-nodes)
- [8.3 Track Data Access](#83-track-data-access)
- [8.4 Events & Delegates](#84-events--delegates)

### IX. Advanced Features
- [9.1 Debug Logging System](#91-debug-logging-system)
- [9.2 Performance Monitoring](#92-performance-monitoring)
- [9.3 Error Handling](#93-error-handling)
- [9.4 Timeout Management](#94-timeout-management)

### X. Best Practices
- [10.1 Performance Optimization](#101-performance-optimization)
- [10.2 Production Deployment](#102-production-deployment)
- [10.3 Troubleshooting Guide](#103-troubleshooting-guide)

---

## I. System Architecture

### 1.1 Overview

AefPharus is built on a **GameInstance Subsystem** architecture that provides persistent, thread-safe tracking across level transitions. The system supports multiple simultaneous tracker instances, each with independent configuration and lifecycle.

**Design Principles:**
- **Thread Safety**: Network callbacks run on separate thread, game thread synchronization via pending queues
- **Memory Safety**: Smart pointers (TUniquePtr, UPROPERTY) prevent memory leaks
- **Performance**: Pre-allocated collections, lock-free callback dispatch, adaptive update throttling
- **Extensibility**: Interface-based actor communication, Blueprint-friendly API

### 1.2 Component Hierarchy

```
UGameInstance
    └─> UAefPharusSubsystem (Auto-created on game start)
            ├─> UAefPharusInstance "Floor" (Created from config or runtime)
            │       ├─> TrackLinkClient (Network thread)
            │       ├─> UAefPharusActorPool (Optional, nDisplay)
            │       ├─>             │       └─> TMap<TrackID, AActor*> SpawnedActors
            │
            └─> UAefPharusInstance "Wall" (Additional instances...)
                    └─> ... (same structure)
```

**Key Components:**

| Component | Lifetime | Purpose |
|-----------|----------|---------|
| `UAefPharusSubsystem` | Game instance | Manages all tracker instances |
| `UAefPharusInstance` | Manual or config-driven | Single tracker logic |
| `TrackLinkClient` | Instance lifetime | UDP network communication |
| `UAefPharusActorPool` | Instance lifetime (if enabled) | Pre-spawned actor pool |
| `### 1.3 Data Flow

```
                    NETWORK THREAD              GAME THREAD

Pharus Hardware
    │ UDP Multicast/Unicast
    ▼
TrackLinkClient::receiveData()
    │ Parse packet
    ▼
ITrackReceiver::onTrackNew()──────────────┐
ITrackReceiver::onTrackUpdate()           │
ITrackReceiver::onTrackLost()             │
    │                                     │
    │ (Thread-safe queue)                │
    ▼                                     │
PendingSpawns/Updates/Removals            │
    │                                     │
    │                                     │
    └──────────────────────────────────> FTSTicker::Tick()
                                                │
                                                ▼
                                        ProcessPendingOperations()
                                                │
                                ┌───────────────┼───────────────┐
                                │               │               │
                                ▼               ▼               ▼
                        SpawnActorForTrack  UpdateActorForTrack  DestroyActorForTrack
                                │               │               │
                                │               │               │
                                ├───────────────┼───────────────┤
                                │                               │
                                ▼                               ▼
                        Actor Pool (if enabled)         Direct Spawn/Destroy
                                │                               │
                                ▼                               ▼
                        Acquire/Release Actor           SpawnActor/Destroy
                                │                               │
                                └───────────────┬───────────────┘
                                                │
                                                ▼
                                        IAefPharusActorInterface
                                        - OnPharusTrackAssigned()
                                        - OnPharusTrackUpdate()
                                        - OnPharusTrackLost()
                                                │
                                                ▼
                                        Actor Transform Updated
                                        LiveLink Subject Published (if enabled)
```

### 1.4 Thread Model

**Network Thread** (TrackLinkClient):
- UDP packet reception
- Track record parsing
- Callback dispatch (non-blocking)
- Runs continuously until shutdown

**Game Thread** (ProcessPendingOperations):
- Actor spawning/destruction
- Transform updates
- LiveLink publishing
- Tick-based processing (every frame)

**Thread Synchronization:**
- `PendingOperationsMutex` protects pending operation queues
- `ActorSpawnMutex` protects actor pool operations
- Lock-free callback dispatch (data copied before lock release)
- No blocking operations on network thread

---

### 1.5 Coordinate System (TUIO Standard)

AefPharus uses the **TUIO standard** for normalized coordinates (0-1 range). This is consistent with touch-screen and laser tracking conventions.

#### Origin Position

The **RootOriginActor** (or `GlobalOrigin` from config) defines the reference point:

```
                   FRONT WALL
                   ┌─────────┐
        ┌──────────┼─────────┼──────────┐
        │          │         │          │
  LEFT  │  ★ ─ ─ ─│─ ─ ─ ─ ─│─ ─ ─ ─ ► │  RIGHT
  WALL  │  Origin  │         │  X-axis  │  WALL
        │  (0,0)   │         │          │
        │  │                            │
        │  ▼ Y-axis                     │
        │                               │
        │         FLOOR                 │
        │                               │
        └───────────────────────────────┘
                   BACK WALL (Entrance)
```

- **★ Origin (0,0)** = Top-left corner (edge of Floor and Front Wall, front-left)
- **X-axis** = Points right (0 → 1)
- **Y-axis** = Points down/back toward entrance (0 → 1)

#### Normalized Coordinate Grid (0-1)

```
    TUIO Coordinate System (Normalized 0-1):
    
         X=0                    X=1
        ┌─────────────────────────┐
   Y=0  │ (0,0)           (1,0)   │  ← ORIGIN TOP-LEFT (Front-left corner)
        │                         │
        │    TRACKING SURFACE     │
        │                         │
   Y=1  │ (0,1)           (1,1)   │  ← Back (Entrance area)
        └─────────────────────────┘
```

| Position | Normalized Coords | Physical Location |
|----------|-------------------|-------------------|
| (0.0, 0.0) | Origin | Front-left corner |
| (1.0, 0.0) | Top-right | Front-right corner |
| (0.5, 0.5) | Center | Center of tracking area |
| (0.0, 1.0) | Bottom-left | Back-left (entrance) |
| (1.0, 1.0) | Bottom-right | Back-right (entrance) |

#### InvertY Behavior

The `bInvertY` setting correctly handles the coordinate flip:

**For Normalized Coordinates (0-1, TUIO style):**
```cpp
if (bInvertY)
{
    AdjustedPos.Y = 1.0f - AdjustedPos.Y;  // Flip within 0-1 range
}
```

**For Absolute Coordinates (meters):**
```cpp
if (bInvertY)
{
    AdjustedPos.Y = -AdjustedPos.Y;  // Mirror around Y=0
}
```

#### Why TUIO Origin Top-Left?

1. **Industry Standard**: TUIO is the de-facto standard for touch/tracking surfaces
2. **Screen Consistency**: Same as display coordinates (0,0 top-left)
3. **Hardware Alignment**: Most Pharus systems orient origin at front-left
4. **Wall Mapping**: Consistent with wall regions where (0,0) maps to wall anchor

---

## II. Core Components

### 2.1 UAefPharusSubsystem

**Class**: `UAefPharusSubsystem : public UGameInstanceSubsystem`
**Header**: `AefPharusSubsystem.h`

GameInstance-level subsystem that manages all tracker instances.

#### Lifecycle

```cpp
// Auto-created when GameInstance starts
UGameInstance::Initialize()
    └─> UAefPharusSubsystem::Initialize()
            ├─> ReadConfigFromIni()
            ├─> CreateInstancesFromConfig()  // Auto-create configured instances
            └─> Ready for runtime API calls

// Auto-destroyed when GameInstance ends
UGameInstance::Shutdown()
    └─> UAefPharusSubsystem::Deinitialize()
            └─> Shutdown all instances
```

#### Public API

**Instance Management:**

```cpp
// Create new tracker instance
FPharusCreateInstanceResult CreateTrackerInstance(
    const FPharusInstanceConfig& Config,
    TSubclassOf<AActor> SpawnClass = nullptr
);

// Simplified creation (convenience)
FPharusCreateInstanceResult CreateTrackerInstanceSimple(
    FName InstanceName,
    FString BindNIC,
    int32 UDPPort,
    EPharusMappingMode MappingMode,
    TSubclassOf<AActor> SpawnClass = nullptr
);

// Get existing instance
UFUNCTION(BlueprintCallable)
UAefPharusInstance* GetTrackerInstance(FName InstanceName);

// Remove instance
UFUNCTION(BlueprintCallable)
bool RemoveTrackerInstance(FName InstanceName);

// Query instances
UFUNCTION(BlueprintPure)
TArray<FName> GetAllInstanceNames() const;

UFUNCTION(BlueprintPure)
int32 GetInstanceCount() const;

UFUNCTION(BlueprintPure)
bool HasInstance(FName InstanceName) const;
```

**SpawnClass Override (Runtime):**

```cpp
/**
 * Set a custom SpawnClass override for a specific instance before calling StartPharusSystem()
 * This allows overriding the DefaultSpawnClass from AefConfig.ini at runtime.
 * Must be called BEFORE StartPharusSystem() to take effect.
 * @param InstanceName Name of the instance (e.g., "Floor", "Wall")
 * @param SpawnClass Actor class to spawn (must implement IAefPharusActorInterface)
 */
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
void SetSpawnClassOverride(FName InstanceName, TSubclassOf<AActor> SpawnClass);
```

**Usage Example:**

```cpp
// C++
UAefPharusSubsystem* Pharus = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();
Pharus->SetSpawnClassOverride("Floor", AMyCustomFloorActor::StaticClass());
Pharus->SetSpawnClassOverride("Wall", AMyCustomWallActor::StaticClass());
Pharus->StartPharusSystem();  // Uses overridden classes instead of INI defaults
```

```
// Blueprint
Get Game Instance → Get Subsystem (AefPharusSubsystem)
→ Set Spawn Class Override ("Floor", BP_MyCustomFloorActor)
→ Set Spawn Class Override ("Wall", BP_MyCustomWallActor)
→ Start Pharus System
```

**Important Notes:**
- Override must be set **BEFORE** `StartPharusSystem()` is called
- If called after system start, a warning is logged and override has no effect
- Passing `nullptr` as SpawnClass clears the override (falls back to INI default)
- SpawnClass must implement `IAefPharusActorInterface`

**Debug Control:**

```cpp
UFUNCTION(BlueprintCallable)
void SetIsPharusDebug(bool bNewValue);

UFUNCTION(BlueprintPure)
bool GetIsPharusDebug() const;
```

**Root Origin API (v2.2.0):**

```cpp
// Get current root origin (position)
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
FVector GetRootOrigin() const;

// Set root origin (position) - only used when UsePharusRootOriginActor=false
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Origin")
void SetRootOrigin(FVector Origin);

// Get current root rotation
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
FRotator GetRootOriginRotation() const;

// Set root rotation - only used when UsePharusRootOriginActor=false
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Origin")
void SetRootOriginRotation(FRotator Rotation);

// Check if a valid root origin is configured
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus|Origin")
bool HasValidRootOrigin() const;
```

**Dynamic Origin Behavior:**

When `UsePharusRootOriginActor=true`:
- Position AND rotation are read from the placed `AAefPharusRootOriginActor` every frame
- All Pharus actors dynamically follow the actor's transform
- Works even for **stationary trackers** (no speed threshold filtering)
- Register actor: `RegisterRootOriginActor(AAefPharusRootOriginActor*)` (called automatically by actor)
- Unregister actor: `UnregisterRootOriginActor(AAefPharusRootOriginActor*)` (called automatically on destroy)

#### Error Handling

**Result Struct:**

```cpp
USTRUCT(BlueprintType)
struct FPharusCreateInstanceResult
{
    UPROPERTY(BlueprintReadOnly)
    bool bSuccess;

    UPROPERTY(BlueprintReadOnly)
    EPharusCreateInstanceError ErrorCode;

    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;
};
```

**Error Codes:**

| Error Code | Description | Resolution |
|------------|-------------|------------|
| `None` | Success | - |
| `InstanceDisabled` | `bEnable = false` in config | Enable instance |
| `InstanceAlreadyExists` | Name conflict | Use unique name or remove existing |
| `SpawnClassMissingInterface` | Actor doesn't implement interface | Implement `IAefPharusActorInterface` |
| `InstanceCreationFailed` | NewObject failed | Check memory/logs |
| `NoValidWorld` | No World context | Call during valid gameplay |
| `InitializationFailed` | Network/setup failed | Check config/network |

---

### 2.2 UAefPharusInstance

**Class**: `UAefPharusInstance : public UObject, public pharus::ITrackReceiver`
**Header**: `AefPharusInstance.h`

Represents a single Pharus tracking system instance.

#### Configuration

**FPharusInstanceConfig Structure:**

```cpp
USTRUCT(BlueprintType)
struct AefPharus_API FPharusInstanceConfig
{
    // ============================================================================
    // Identity
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Identity")
    FName InstanceName = "Floor";

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Identity")
    bool bEnable = true;

    // ============================================================================
    // Network Settings
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Network")
    FString BindNIC = "";  // Empty = INADDR_ANY (0.0.0.0)

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Network")
    int32 UDPPort = 44345;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Network")
    bool bIsMulticast = true;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Network")
    FString MulticastGroup = "239.1.1.1";

    // ============================================================================
    // Mapping Mode
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping")
    EPharusMappingMode MappingMode = EPharusMappingMode::Simple;

    // ============================================================================
    // Simple Mode (Floor) Settings
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Simple")
    FVector2D SimpleOrigin = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Simple")
    FVector2D SimpleScale = FVector2D(100.0f, 100.0f);  // m → cm

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Simple")
    float FloorZ = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Simple")
    float FloorRotation = 0.0f;  // Degrees, rotates 2D plane around Z

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Simple")
    bool bInvertY = false;  // Flip Y axis (fixes left/right swap)

    // ============================================================================
    // Regions Mode (Wall) Settings
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Mapping|Regions")
    TArray<FPharusWallRegion> WallRegions;

    // ============================================================================
    // Tracking Surface Configuration
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Tracking")
    FVector2D TrackingSurfaceDimensions = FVector2D(1.0f, 1.0f);

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Tracking")
    bool bUseNormalizedCoordinates = true;  // true = 0-1 range, false = meters

    // ============================================================================
    // Actor Spawning
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Spawning")
    ESpawnActorCollisionHandlingMethod SpawnCollisionHandling =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Spawning")
    bool bAutoDestroyOnTrackLost = true;

    // ============================================================================
    // Actor Pool (nDisplay Cluster Support)
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|ActorPool")
    bool bUseActorPool = false;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|ActorPool")
    int32 ActorPoolSize = 50;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|ActorPool")
    FVector PoolSpawnLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|ActorPool")
    FRotator PoolSpawnRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|ActorPool")
    int32 PoolIndexOffset = 0;  // For multiple pools

    // ============================================================================
    // LiveLink Integration
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|LiveLink")
    bool     UPROPERTY(BlueprintReadWrite, Category = "Pharus|LiveLink")
    bool     UPROPERTY(BlueprintReadWrite, Category = "Pharus|LiveLink")
    int32     UPROPERTY(BlueprintReadWrite, Category = "Pharus|LiveLink")
    bool     // ============================================================================
    // Performance & Filtering
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Performance")
    float TrackTimeout = 2.0f;  // Seconds without update before track lost

    // ============================================================================
    // Debug & Visualization
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Debug")
    bool bDebugVisualization = false;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Debug")
    bool bLiveAdjustments = false;  // Allow runtime config updates

    // ============================================================================
    // Logging
    // ============================================================================

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Logging")
    bool bLogTrackerSpawned = false;

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Logging")
    bool bLogTrackerUpdated = false;  // VERY verbose!

    UPROPERTY(BlueprintReadWrite, Category = "Pharus|Logging")
    bool bLogTrackerRemoved = false;
};
```

#### Public API

**Track Data Access:**

```cpp
// Get world position, rotation, and boundary status for track
// bOutIsInsideBoundary: true = actor visible, false = track outside bounds (hidden)
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
bool GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation, bool& bOutIsInsideBoundary);

// Get all active track IDs
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
TArray<int32> GetActiveTrackIDs() const;

// Get track count
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
int32 GetActiveTrackCount() const;

// Get spawned actor for track
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
AActor* GetSpawnedActor(int32 TrackID);

// Check if track is active (receiving UDP updates)
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
bool IsTrackActive(int32 TrackID) const;
```

**Runtime Configuration:**

```cpp
// Update configuration at runtime (if bLiveAdjustments enabled)
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
bool UpdateConfig(const FPharusInstanceConfig& NewConfig);

// Update Floor settings
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Floor")
bool UpdateFloorSettings(const FPharusInstanceConfig& NewConfig);

// Update Wall settings
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Wall")
bool UpdateWallSettings(const FPharusInstanceConfig& NewConfig);

// Restart with new network settings (WARNING: destroys all actors!)
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
bool RestartWithNewNetwork(const FString& NewBindNIC, int32 NewUDPPort);

// Set SpawnClass at runtime (new tracks will use this class)
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus")
void SetSpawnClass(TSubclassOf<AActor> NewSpawnClass);

// Get current SpawnClass
UFUNCTION(BlueprintPure, Category = "AefXR|Pharus")
TSubclassOf<AActor> GetSpawnClass() const;
```

**Events:**

```cpp
// Called when a new track is spawned
UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
FPharusTrackSpawnedDelegate OnTrackSpawned;  // (int32 TrackID, FPharusTrackData TrackData)

// Called when a track is updated
UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
FPharusTrackUpdatedDelegate OnTrackUpdated;  // (int32 TrackID, FPharusTrackData TrackData)

// Called when a track is lost
UPROPERTY(BlueprintAssignable, Category = "AefXR|Pharus")
FPharusTrackLostDelegate OnTrackLost;  // (int32 TrackID)
```

**Debug:**

```cpp
// Inject test track (for testing without hardware)
UFUNCTION(BlueprintCallable, Category = "AefXR|Pharus|Debug")
void DebugInjectTrack(float NormalizedX, float NormalizedY, int32 TrackID = -1);
```

---

### 2.3 TrackLink Network Layer

**Class**: `pharus::TrackLinkClient`
**Header**: `TrackLink.h`

Low-level UDP network communication with Pharus hardware.

#### Architecture

```
TrackLinkClient
    ├─> UDPManager (Socket management)
    │       ├─> Multicast Join (if enabled)
    │       └─> Unicast Bind (if not multicast)
    │
    ├─> std::thread recvThread (Network receive loop)
    │       └─> Runs continuously until shutdown
    │
    └─> std::vector<ITrackReceiver*> trackReceivers
            └─> Callbacks dispatched on network thread
```

#### Packet Format

**Track Packet Structure:**

```
'T' (Header, 1 byte)
TrackID (4 bytes, uint32)
State (4 bytes, TrackState enum)
CurrentPos.X (4 bytes, float)
CurrentPos.Y (4 bytes, float)
ExpectPos.X (4 bytes, float)
ExpectPos.Y (4 bytes, float)
Orientation.X (4 bytes, float)
Orientation.Y (4 bytes, float)
Speed (4 bytes, float)
RelPos.X (4 bytes, float)
RelPos.Y (4 bytes, float)
[Optional: 'E' + Echo.X + Echo.Y + 'e' (per echo)]
't' (Footer, 1 byte)
```

**Track States:**

| State | Value | Description |
|-------|-------|-------------|
| `TS_NEW` | 0 | First notification of track |
| `TS_CONT` | 1 | Position update |
| `TS_OFF` | 2 | Track disappeared |

#### Network Thread Safety

**Critical Path:**

```cpp
// Network thread
void TrackLinkClient::receiveData()
{
    while (!threadExit)
    {
        // Receive UDP packet
        recvSize = udpman->Receive(recvBuf, bufferSize);

        // Parse track data
        TrackRecord track;
        memcpy(&track, recvBuf, sizeof(TrackRecord));

        // CRITICAL: Copy data and receiver list UNDER lock
        {
            std::lock_guard<std::mutex> lock(recvMutex);
            trackCopy = track;
            receiversCopy = trackReceivers;
        } // Lock released

        // Dispatch callbacks WITHOUT lock (prevents blocking)
        for (auto receiver : receiversCopy)
        {
            receiver->onTrackNew(trackCopy);  // Game thread queues this
        }
    }
}
```

**Why Lock-Free Dispatch?**

❌ **Bad** (Old implementation):
```cpp
{
    std::lock_guard<std::mutex> lock(recvMutex);
    for (auto receiver : trackReceivers)
    {
        receiver->onTrackNew(track);  // Blocks if game thread is slow!
    }
}
```

✅ **Good** (Current implementation):
- Copy data and receiver list under lock
- Release lock immediately
- Dispatch callbacks without lock
- **Result**: Network thread never blocks on game thread operations

---

### 2.4 Actor Pool System

**Class**: `UAefPharusActorPool`
**Header**: `AefPharusActorPool.h`

Pre-spawned actor pool for deterministic nDisplay cluster synchronization.

#### Why Actor Pool?

**Problem without Pool:**
```
Primary Node: Track 5 appears → Spawn BP_Actor_C_123
Secondary Node: Track 5 appears → Spawn BP_Actor_C_456  // Different instance!
Result: Desynchronized actors, replication issues
```

**Solution with Pool:**
```
All Nodes: Pre-spawn 50 actors at startup → [Actor_0, Actor_1, ..., Actor_49]
Primary Node: Track 5 appears → Assign to pool index 7 → Replicate index
Secondary Nodes: Receive index 7 → Activate same pool actor
Result: All nodes use Actor_7, fully synchronized!
```

#### Architecture

```cpp
class AefPharus_API UAefPharusActorPool : public UObject
{
private:
    // Pre-spawned actors (UPROPERTY for GC safety)
    UPROPERTY()
    TArray<AActor*> PooledActors;

    // Free indices (actors not currently assigned)
    TArray<int32> FreeIndices;

    // Track ID → Pool Index mapping
    TMap<int32, int32> ActiveAssignments;

    // Configuration
    TSubclassOf<AActor> ActorClass;
    int32 PoolSize;
    FVector SpawnLocation;
    FRotator SpawnRotation;
};
```

#### Lifecycle

**Initialization:**

```cpp
bool Initialize(
    UWorld* InWorld,
    TSubclassOf<AActor> InActorClass,
    int32 InPoolSize,
    FName InInstanceName,
    FVector InSpawnLocation,
    FRotator InSpawnRotation,
    int32 InIndexOffset
)
{
    // Spawn all actors at fixed location
    for (int32 i = 0; i < InPoolSize; ++i)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*FString::Printf(TEXT("PharusPoolActor_%s_%d"),
            *InInstanceName.ToString(), InIndexOffset + i));
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AActor* Actor = InWorld->SpawnActor<AActor>(
            InActorClass,
            InSpawnLocation,
            InSpawnRotation,
            SpawnParams
        );

        if (Actor)
        {
            // Hide and disable collision initially
            Actor->SetActorHiddenInGame(true);
            Actor->SetActorEnableCollision(false);

            PooledActors.Add(Actor);
            FreeIndices.Add(i);
        }
    }

    return PooledActors.Num() > 0;
}
```

**Acquire Actor:**

```cpp
AActor* AcquireActor(int32& OutPoolIndex)
{
    if (FreeIndices.Num() == 0)
    {
        UE_LOG(LogAefPharus, Warning, TEXT("Actor pool exhausted!"));
        return nullptr;
    }

    // Get first free index
    OutPoolIndex = FreeIndices[0];
    FreeIndices.RemoveAt(0);

    AActor* Actor = PooledActors[OutPoolIndex];

    // Activate actor
    Actor->SetActorHiddenInGame(false);
    Actor->SetActorEnableCollision(true);

    return Actor;
}
```

**Release Actor:**

```cpp
void ReleaseActor(int32 PoolIndex)
{
    if (!PooledActors.IsValidIndex(PoolIndex))
        return;

    AActor* Actor = PooledActors[PoolIndex];

    // Reset to pool spawn location
    Actor->SetActorLocation(PoolSpawnLocation);
    Actor->SetActorRotation(PoolSpawnRotation);

    // Deactivate
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);

    // Return to free list
    FreeIndices.Add(PoolIndex);
}
```

#### nDisplay Synchronization

**Primary Node Logic:**

```cpp
void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const TrackRecord& Track)
{
    if (ActorPool)
    {
        int32 PoolIndex;
        AActor* Actor = ActorPool->AcquireActor(PoolIndex);

        if (Actor)
        {
            // Store mapping
            TrackToPoolIndex.Add(TrackID, PoolIndex);
            SpawnedActors.Add(TrackID, Actor);

            // DisplayCluster replicates pool index + transform
            if (IDisplayClusterSceneComponentSync* SyncComponent =
                Actor->FindComponentByClass<UDisplayClusterSceneComponentSync>())
            {
                SyncComponent->SetPoolIndex(PoolIndex);  // Replicated
                SyncComponent->SetTrackID(TrackID);      // Replicated
            }

            // Update transform (replicated via DisplayCluster)
            Actor->SetActorTransform(CalculateTransform(Track));
        }
    }
}
```

**Secondary Node Logic:**

```cpp
// Receives pool index + transform from primary via DisplayCluster
// Uses same pool actor index → deterministic synchronization
void OnReplicationReceived(int32 PoolIndex, int32 TrackID, FTransform Transform)
{
    AActor* Actor = ActorPool->PooledActors[PoolIndex];
    Actor->SetActorTransform(Transform);

    // Both primary and secondary now have same actor at same transform!
}
```

---

## III. Configuration System

### 3.1 INI Configuration

**File**: `Config/AefConfig.ini` (or custom path via `SetConfigFilePath()`)

#### Subsystem Section

```ini
[PharusSubsystem]
# Instances to auto-create on subsystem initialization
AutoCreateInstances=Floor,Wall

# Enable detailed debug logging
IsPharusDebug=false

# Auto-create delay (seconds, for nDisplay cluster coordination)
AutoCreateDelay=0.0

# Global 3D origin for ALL tracking instances (NEW in v2.2.0)
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)

# Global rotation for ALL tracking instances (NEW in v2.2.0)
# Coordinates are rotated around GlobalOrigin before being placed in world space
GlobalRotation=(Pitch=0.0,Yaw=0.0,Roll=0.0)

# Use placed actor for dynamic origin/rotation (position AND rotation are read from actor)
UsePharusRootOriginActor=false
```

#### Instance Section Template

```ini
[Pharus.{InstanceName}]

# ============================================================================
# Basic Settings
# ============================================================================

Enable=true
BindNIC=192.168.101.55          # Empty = INADDR_ANY (0.0.0.0)
UDPPort=44345
IsMulticast=true
MulticastGroup=239.1.1.1
MappingMode=Simple              # Simple | Regions

# ============================================================================
# Simple Mode (Floor) Settings
# ============================================================================

OriginX=-910.0
OriginY=-7400.0
ScaleX=100.0
ScaleY=100.0
FloorZ=0.0
FloorRotation=0.0
InvertY=false

# ============================================================================
# Tracking Surface Configuration
# ============================================================================

TrackingSurfaceWidth=1.0
TrackingSurfaceHeight=1.0
UseNormalizedCoordinates=true   # true = 0-1 range, false = meters

# ============================================================================
# Actor Spawning
# ============================================================================

DefaultSpawnClass=/Game/Blueprints/BP_PharusActor.BP_PharusActor_C
SpawnCollisionHandling=AlwaysSpawn
AutoDestroyOnTrackLost=true

# ============================================================================
# Actor Pool (nDisplay Cluster Support)
# ============================================================================

UseActorPool=false
ActorPoolSize=50
PoolSpawnLocationX=0.0
PoolSpawnLocationY=0.0
PoolSpawnLocationZ=-1000.0
PoolSpawnRotationPitch=0.0
PoolSpawnRotationYaw=0.0
PoolSpawnRotationRoll=0.0
PoolIndexOffset=0

# ============================================================================
# ============================================================================

EnableLiveLink=false
EnableSlotMapping=false
ReuseLowestSlot=true

# ============================================================================
# Performance & Filtering
# ============================================================================

TrackTimeout=2.0               # Seconds without update before lost

# ============================================================================
# Debug & Visualization
# ============================================================================

DebugVisualization=false
LiveAdjustments=false          # Allow runtime config updates

# ============================================================================
# Logging (use with IsPharusDebug=true for details)
# ============================================================================

LogTrackerSpawned=false
LogTrackerUpdated=false        # WARNING: VERY verbose!
LogTrackerRemoved=false
```

#### Wall Regions Configuration

```ini
[Pharus.Wall]
MappingMode=Regions
WallCount=4

# Front Wall (Region 0.0-0.25 of tracking surface)
Wall.Front.TrackingMinX=0.0
Wall.Front.TrackingMaxX=0.25
Wall.Front.TrackingMinY=0.0
Wall.Front.TrackingMaxY=1.0
Wall.Front.WorldPositionX=1000.0
Wall.Front.WorldPositionY=0.0
Wall.Front.WorldPositionZ=200.0
Wall.Front.WorldRotationYaw=180.0
Wall.Front.WorldSizeX=2000.0
Wall.Front.WorldSizeZ=400.0
Wall.Front.ScaleX=100.0
Wall.Front.ScaleZ=100.0
Wall.Front.InvertY=false

# Left Wall (Region 0.25-0.50)
Wall.Left.TrackingMinX=0.25
Wall.Left.TrackingMaxX=0.50
# ... (same structure as Front)

# Back Wall (Region 0.50-0.75)
Wall.Back.TrackingMinX=0.50
Wall.Back.TrackingMaxX=0.75
# ...

# Right Wall (Region 0.75-1.0)
Wall.Right.TrackingMinX=0.75
Wall.Right.TrackingMaxX=1.0
# ...
```

### 3.2 Instance Configuration

**Loading Priority:**

1. **Auto-Creation**: `AutoCreateInstances` in `[PharusSubsystem]`
2. **Runtime API**: `CreateTrackerInstance()` calls
3. **Blueprint**: Config structs constructed in Blueprint

**Validation:**

- `BindNIC` validated for IP format (or empty for INADDR_ANY)
- `UDPPort` must be 1-65535
- `MappingMode` must match configured regions/settings
- `DefaultSpawnClass` checked for `IAefPharusActorInterface` implementation

### 3.3 Network Settings

#### Multicast vs Unicast

**Multicast** (1 sender → N receivers):

```ini
IsMulticast=true
MulticastGroup=239.1.1.1
BindNIC=                        # Empty = join on all interfaces
UDPPort=44345
```

**Advantages:**
- One Pharus can send to multiple Unreal instances
- Efficient for installations with multiple machines
- Standard Pharus setup

**Disadvantages:**
- Requires multicast-capable network switches
- Router configuration needed for cross-subnet

**Unicast** (1 sender → 1 receiver):

```ini
IsMulticast=false
BindNIC=192.168.101.55          # Must specify interface
UDPPort=44345
```

**Advantages:**
- Works on all networks
- No router configuration needed
- Better control over traffic

**Disadvantages:**
- One Pharus per Unreal instance
- Multiple network interfaces needed for multiple Pharus

#### Network Interface Binding

**INADDR_ANY** (Bind to all interfaces):
```ini
BindNIC=                        # Empty string
```

TrackLink binds to `0.0.0.0`, receives on all network interfaces.

**Specific Interface:**
```ini
BindNIC=192.168.101.55          # Specific IP
```

TrackLink binds only to this interface.

**Best Practice:**
- **Single NIC**: Use INADDR_ANY
- **Multiple NICs**: Specify interface to avoid cross-talk
- **nDisplay Cluster**: Use specific IPs for each node

### 3.4 Live Adjustments

#### Enabling Live Adjustments

```ini
[Pharus.Floor]
LiveAdjustments=true
```

Allows runtime configuration updates via Blueprint/C++.

#### Adjustable Settings

**Floor Settings:**

```cpp
FPharusInstanceConfig NewConfig = Instance->GetConfig();
NewConfig.SimpleOrigin = FVector2D(-910.0f, -7400.0f);
NewConfig.SimpleScale = FVector2D(100.0f, 100.0f);
NewConfig.FloorZ = 0.0f;
NewConfig.FloorRotation = 90.0f;  // Rotate 90°
NewConfig.bInvertY = true;         // Flip Y axis

Instance->UpdateFloorSettings(NewConfig);
```

**Wall Settings:**

```cpp
// Update wall region transforms
Instance->UpdateWallSettings(NewConfig);
```

**Debug Settings:**

```cpp
NewConfig.bDebugVisualization = true;  // Show debug visuals

Instance->UpdateConfig(NewConfig);
```

#### Non-Adjustable Settings (Require Restart)

```cpp
// These require RestartWithNewNetwork()
Config.BindNIC = "192.168.101.56";
Config.UDPPort = 44346;
Config.bIsMulticast = false;
Config.MulticastGroup = "239.1.1.2";

// WARNING: Shuts down current connection, destroys all actors!
Instance->RestartWithNewNetwork(Config.BindNIC, Config.UDPPort);
```

---

## IV. Mapping Modes

### 4.1 Simple Mode (Floor Tracking)

**Use Case:** Horizontal floors, tables, flat surfaces

**Coordinate Transformation:**

```cpp
FVector TrackToWorldSimple(const FVector2D& TrackPos) const
{
    // 1. Normalize if needed
    FVector2D NormalizedPos = TrackPos;
    if (!bUseNormalizedCoordinates)
    {
        // Convert meters to 0-1 range
        NormalizedPos.X /= TrackingSurfaceDimensions.X;
        NormalizedPos.Y /= TrackingSurfaceDimensions.Y;
    }

    // 2. Apply Y-axis inversion (fixes left/right swap)
    if (bInvertY)
    {
        NormalizedPos.Y = -NormalizedPos.Y;
    }

    // 3. Apply 2D rotation (if specified)
    if (!FMath::IsNearlyZero(FloorRotation))
    {
        float RotationRad = FMath::DegreesToRadians(FloorRotation);
        float CosRot = FMath::Cos(RotationRad);
        float SinRot = FMath::Sin(RotationRad);

        FVector2D RotatedPos;
        RotatedPos.X = NormalizedPos.X * CosRot - NormalizedPos.Y * SinRot;
        RotatedPos.Y = NormalizedPos.X * SinRot + NormalizedPos.Y * CosRot;
        NormalizedPos = RotatedPos;
    }

    // 4. Apply scale and origin
    FVector2D ScaledPos = NormalizedPos * SimpleScale + SimpleOrigin;

    // 5. Create 3D position (X, Y from track, Z from config)
    return FVector(ScaledPos.X, ScaledPos.Y, FloorZ);
}
```

**Configuration Example:**

```ini
[Pharus.Floor]
MappingMode=Simple

# Room: 10m × 8m, centered at (0, 0)
# Pharus surface: 1m × 1m (normalized 0-1)

OriginX=-500.0          # -5m in cm
OriginY=-400.0          # -4m in cm
ScaleX=1000.0           # 10m in cm
ScaleY=800.0            # 8m in cm
FloorZ=0.0              # Ground level
FloorRotation=0.0       # No rotation
InvertY=false           # No Y flip

UseNormalizedCoordinates=true
TrackingSurfaceWidth=1.0
TrackingSurfaceHeight=1.0
```

**Result:**

```
Pharus (0.0, 0.0) → UE (-500, -400, 0)    // Bottom-left corner
Pharus (0.5, 0.5) → UE (0, 0, 0)          // Center
Pharus (1.0, 1.0) → UE (500, 400, 0)      // Top-right corner
```

### 4.2 Regions Mode (Wall Tracking)

**Use Case:** 4-wall CAVE system from single planar Pharus surface

**Concept:**

The tracking surface is divided into regions, each mapping to a different wall. Tracking flows 
left-to-right through the regions, creating seamless transitions between walls.

```
Physical Pharus Surface (Linear strip, normalized 0-1):

  ◄─────────────────── Tracking Flow (left to right) ───────────────────►

┌──────────┬───────────────┬──────────┬───────────────┐
│   LEFT   │     FRONT     │  RIGHT   │     BACK      │
│ 0.8-1.0  │   0.5-0.8     │ 0.3-0.5  │   0.0-0.3     │
│  (10m)   │    (15m)      │  (10m)   │    (15m)      │
└──────────┴───────────────┴──────────┴───────────────┘
     ↓             ↓             ↓            ↓
   Y=1000→0    X=1500→0      Y=0→1000     X=0→1500
```

**3D Room Layout (Top View):**

```
                     FRONT WALL (Y=1000)
                     Yaw=180° (runs X: 1500→0)
        (0,1000)◄──────────────────(1500,1000)
              │                         │
              ▼                         ▲
   LEFT       │                         │  RIGHT
   Yaw=270°   │        ROOM             │  Yaw=90°
   (Y:1000→0) │                         │  (Y:0→1000)
              │                         │
              │                         │
        (0,0) │                         │ (1500,0)
              └─────────────────────────┘
                     BACK WALL (Y=0)
                     Yaw=0° (runs X: 0→1500)
        (0,0)──────────────────►(1500,0)
```

**Seamless Wall Transitions:**
- **Left → Front**: Left ends at `(0, 0)`, Front starts at `(1500, 1000)` *(tracking jump)*
- **Front → Right**: Front ends at `(0, 1000)`, Right starts at `(1500, 0)` *(tracking jump)*
- **Right → Back**: Right ends at `(1500, 1000)`, Back starts at `(0, 0)` *(tracking jump)*
- **Back → Left**: Back ends at `(1500, 0)`, Left starts at `(0, 1000)` *(tracking jump)*

**Transformation Steps:**

```cpp
FVector FPharusWallRegion::TrackToWorld(const FVector2D& TrackPos, const FVector& RootOrigin) const
{
    // Step 1: Normalize position within region bounds (0-1)
    FVector2D RegionSize = TrackingBounds.GetSize();
    FVector2D LocalPos = (TrackPos - TrackingBounds.Min) / RegionSize;

    // Step 2: Apply InvertY if enabled
    if (bInvertY)
    {
        LocalPos.Y = 1.0f - LocalPos.Y;  // Flip Y: 0→1, 1→0
    }

    // Step 3: Apply 2D rotation if specified
    if (!FMath::IsNearlyZero(Rotation2D))
    {
        // Rotate 2D point around origin
        float RotationRad = FMath::DegreesToRadians(Rotation2D);
        FVector2D RotatedPos;
        RotatedPos.X = LocalPos.X * FMath::Cos(RotationRad) - LocalPos.Y * FMath::Sin(RotationRad);
        RotatedPos.Y = LocalPos.X * FMath::Sin(RotationRad) + LocalPos.Y * FMath::Cos(RotationRad);
        LocalPos = RotatedPos;
    }

    // Step 4: Apply scale (converts 0-1 to actual wall dimensions in cm)
    FVector2D ScaledPos = LocalPos * Scale;

    // Step 5: Apply Origin offset (shifts coordinate system)
    FVector2D OffsetPos = ScaledPos + Origin;

    // Step 6: Build 3D position on wall plane
    // Wall local space: X=horizontal along wall, Y=0 (wall surface), Z=vertical
    FVector LocalWallPos(OffsetPos.X, 0.0f, OffsetPos.Y);

    // Step 7: Calculate wall anchor position in world space
    FVector WallAnchor = WorldPosition + RootOrigin;

    // Step 8: Apply wall rotation to local position, then add anchor
    FVector RotatedLocalPos = WorldRotation.RotateVector(LocalWallPos);
    FVector WorldPos = WallAnchor + RotatedLocalPos;

    return WorldPos;
}
```

**Configuration Example (15m×10m Room, 6m High Walls):**

```ini
[Pharus.Wall]
MappingMode=Regions
WallCount=4
TrackingSurfaceDimensions=(X=50.0,Y=6.0)  ; 50m wide × 6m high tracking surface
UseNormalizedCoordinates=false             ; Input is in meters

;==============================================================================
; LEFT WALL (Tracking: 0.8-1.0, runs Y: 1000→0)
;==============================================================================
Wall.Left.TrackingMinX=0.8
Wall.Left.TrackingMaxX=1.0
Wall.Left.TrackingMinY=0.0
Wall.Left.TrackingMaxY=1.0

; WorldPosition = Bottom-left corner (anchor point where (0,0) maps to)
Wall.Left.WorldPosition=(X=0.0,Y=1000.0,Z=0.0)

; Yaw=270° rotates local X-axis to world -Y direction
Wall.Left.WorldRotation=(Pitch=0.0,Yaw=270.0,Roll=0.0)

Wall.Left.Scale=(X=1000.0,Y=600.0)    ; 10m wide × 6m high
Wall.Left.Origin=(X=0.0,Y=0.0)        ; No offset (corner-based)
Wall.Left.InvertY=false

;==============================================================================
; FRONT WALL (Tracking: 0.5-0.8, runs X: 1500→0)
;==============================================================================
Wall.Front.TrackingMinX=0.5
Wall.Front.TrackingMaxX=0.8
Wall.Front.TrackingMinY=0.0
Wall.Front.TrackingMaxY=1.0

Wall.Front.WorldPosition=(X=1500.0,Y=1000.0,Z=0.0)
Wall.Front.WorldRotation=(Pitch=0.0,Yaw=180.0,Roll=0.0)  ; X→-X

Wall.Front.Scale=(X=1500.0,Y=600.0)   ; 15m wide × 6m high
Wall.Front.Origin=(X=0.0,Y=0.0)

;==============================================================================
; RIGHT WALL (Tracking: 0.3-0.5, runs Y: 0→1000)
;==============================================================================
Wall.Right.TrackingMinX=0.3
Wall.Right.TrackingMaxX=0.5
Wall.Right.TrackingMinY=0.0
Wall.Right.TrackingMaxY=1.0

Wall.Right.WorldPosition=(X=1500.0,Y=0.0,Z=0.0)
Wall.Right.WorldRotation=(Pitch=0.0,Yaw=90.0,Roll=0.0)   ; X→+Y

Wall.Right.Scale=(X=1000.0,Y=600.0)   ; 10m wide × 6m high
Wall.Right.Origin=(X=0.0,Y=0.0)

;==============================================================================
; BACK WALL (Tracking: 0.0-0.3, runs X: 0→1500)
;==============================================================================
Wall.Back.TrackingMinX=0.0
Wall.Back.TrackingMaxX=0.3
Wall.Back.TrackingMinY=0.0
Wall.Back.TrackingMaxY=1.0

Wall.Back.WorldPosition=(X=0.0,Y=0.0,Z=0.0)
Wall.Back.WorldRotation=(Pitch=0.0,Yaw=0.0,Roll=0.0)     ; X→+X

Wall.Back.Scale=(X=1500.0,Y=600.0)    ; 15m wide × 6m high
Wall.Back.Origin=(X=0.0,Y=0.0)
```

**Key Configuration Parameters:**

| Parameter | Description |
|-----------|-------------|
| `WorldPosition` | **Anchor point** (bottom-left corner) where tracking `(0,0)` maps to |
| `WorldRotation` | Wall orientation - Yaw determines which direction local X-axis points |
| `Scale` | Wall dimensions in cm: `X=width`, `Y=height` |
| `Origin` | Offset from anchor point (usually `(0,0)` for corner-based mapping) |
| `InvertY` | Flip vertical axis if needed |

**Yaw Rotation Reference:**

| Yaw | Local X → World | Use For |
|-----|-----------------|---------|
| 0°  | +X | Back wall (runs left to right) |
| 90° | +Y | Right wall (runs front to back) |
| 180° | -X | Front wall (runs right to left) |
| 270° | -Y | Left wall (runs back to front) |

**Result Example:**

```
Tracking (0.9, 0.5)  → Left Wall  → World (0, 500, 300)    ; Middle of left wall
Tracking (0.65, 0.5) → Front Wall → World (750, 1000, 300) ; Middle of front wall
Tracking (0.4, 0.5)  → Right Wall → World (1500, 500, 300) ; Middle of right wall
Tracking (0.15, 0.5) → Back Wall  → World (750, 0, 300)    ; Middle of back wall
```

---

## V. Actor Lifecycle

### 5.1 Spawning Logic

#### Standard Spawning (Without Actor Pool)

```cpp
void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
    // 1. Calculate world transform
    FVector WorldPos = TrackToWorld(FVector2D(Track.currentPos.x, Track.currentPos.y), Track);
    FRotator WorldRot = GetRotationFromDirection(FVector2D(Track.orientation.x, Track.orientation.y));
    FTransform WorldTransform(WorldRot, WorldPos);

    // 2. Setup spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = Config.SpawnCollisionHandling;
    SpawnParams.Name = FName(*FString::Printf(TEXT("PharusTrack_%d"), TrackID));

    // 3. Spawn actor
    AActor* SpawnedActor = WorldContext->SpawnActor<AActor>(
        SpawnClass,
        WorldTransform,
        SpawnParams
    );

    if (!SpawnedActor)
    {
        UE_LOG(LogAefPharus, Warning, TEXT("[%s] Failed to spawn actor for track %d"),
            *Config.InstanceName.ToString(), TrackID);
        return;
    }

    // 4. Store in map
    SpawnedActors.Add(TrackID, SpawnedActor);

    // 5. Notify actor via interface
    if (SpawnedActor->Implements<UAefPharusActorInterface>())
    {
        IAefPharusActorInterface::Execute_OnPharusTrackAssigned(
            SpawnedActor,
            TrackID,
            Config.InstanceName
        );
    }

    // 6. Fire Blueprint event
    FPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, FVector2D(Track.currentPos.x, Track.currentPos.y));
    OnTrackSpawned.Broadcast(TrackID, TrackData);

    // 7. Publish to LiveLink (if enabled)
    if (    {
            }
}
```

#### Actor Pool Spawning (nDisplay Clusters)

```cpp
void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
    if (!ActorPool)
    {
        // Fall back to standard spawning
        return;
    }

    // 1. Acquire actor from pool
    int32 PoolIndex;
    AActor* PooledActor = ActorPool->AcquireActor(PoolIndex);

    if (!PooledActor)
    {
        UE_LOG(LogAefPharus, Warning, TEXT("[%s] Actor pool exhausted for track %d"),
            *Config.InstanceName.ToString(), TrackID);
        return;
    }

    // 2. Store mapping
    TrackToPoolIndex.Add(TrackID, PoolIndex);
    SpawnedActors.Add(TrackID, PooledActor);

    // 3. Calculate transform
    FVector WorldPos = TrackToWorld(FVector2D(Track.currentPos.x, Track.currentPos.y), Track);
    FRotator WorldRot = GetRotationFromDirection(FVector2D(Track.orientation.x, Track.orientation.y));
    FTransform WorldTransform(WorldRot, WorldPos);

    // 4. Update actor
    PooledActor->SetActorTransform(WorldTransform);

    // 5. Notify via interface (with pool index)
    if (PooledActor->Implements<UAefPharusActorInterface>())
    {
        IAefPharusActorInterface::Execute_OnPharusTrackAssignedWithPoolIndex(
            PooledActor,
            TrackID,
            Config.InstanceName,
            PoolIndex  // Important for cluster sync!
        );
    }

    // 6. Setup DisplayCluster sync (if available)
    if (UDisplayClusterSceneComponentSync* SyncComp =
        PooledActor->FindComponentByClass<UDisplayClusterSceneComponentSync>())
    {
        SyncComp->SetSyncEnabled(true);
        // Pool index is replicated via DisplayCluster
    }

    // 7. Fire events and LiveLink (same as standard)
    OnTrackSpawned.Broadcast(TrackID, ConvertTrackData(Track, WorldPos, ...));
    if (}
```

### 5.2 Update Mechanism

```cpp
void UAefPharusInstance::UpdateActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
    // 1. Get actor
    AActor** ActorPtr = SpawnedActors.Find(TrackID);
    if (!ActorPtr || !*ActorPtr)
        return;

    AActor* Actor = *ActorPtr;

    // 2. Calculate new transform
    FVector WorldPos = TrackToWorld(FVector2D(Track.currentPos.x, Track.currentPos.y), Track);
    FRotator WorldRot = GetRotationFromDirection(FVector2D(Track.orientation.x, Track.orientation.y));
    FTransform WorldTransform(WorldRot, WorldPos);

    // 3. Update actor transform
    Actor->SetActorTransform(WorldTransform);

    // 4. Notify via interface
    if (Actor->Implements<UAefPharusActorInterface>())
    {
        FPharusTrackData TrackData = ConvertTrackData(Track, WorldPos, ...);
        IAefPharusActorInterface::Execute_OnPharusTrackUpdate(Actor, TrackData);
    }

    // 5. Fire Blueprint event
    OnTrackUpdated.Broadcast(TrackID, ConvertTrackData(Track, WorldPos, ...));

    // 6. Update LiveLink
    if (    {
            }
}
```

**Update Filtering:**

```cpp
// In onTrackUpdate() (Network Thread)
void UAefPharusInstance::onTrackUpdate(const pharus::TrackRecord& Track)
{
    // ALWAYS update LastUpdateTime (prevent timeout)
    {
        FScopeLock Lock(&PendingOperationsMutex);
        FPharusTrackData* ExistingData = TrackDataCache.Find(Track.trackID);
        if (ExistingData)
        {
            ExistingData->LastUpdateTime = FPlatformTime::Seconds();
        }
    }

    // Queue for game thread processing
    {
        FScopeLock Lock(&PendingOperationsMutex);
        PendingUpdates.AddUnique(Track.trackID);
        TrackDataCache.FindOrAdd(Track.trackID) = ConvertTrackData(Track, ...);
    }
}
```

### 5.3 Destruction Process

#### Destruction Trigger Events

1. **Track Lost (UDP)**: `onTrackLost()` callback
2. **Track Timeout**: No UDP updates for `TrackTimeout` seconds
3. **Manual Removal**: `RemoveTrackerInstance()` or `Shutdown()`

#### Standard Destruction (Without Actor Pool)

```cpp
void UAefPharusInstance::DestroyActorForTrack(int32 TrackID, const FString& Reason)
{
    // 1. Get actor
    AActor** ActorPtr = SpawnedActors.Find(TrackID);
    if (!ActorPtr || !*ActorPtr)
        return;

    AActor* Actor = *ActorPtr;

    // 2. Notify via interface
    if (Actor->Implements<UAefPharusActorInterface>())
    {
        IAefPharusActorInterface::Execute_OnPharusTrackLost(Actor, TrackID, Reason);
    }

    // 3. Fire Blueprint event
    OnTrackLost.Broadcast(TrackID);

    // 4. Remove from LiveLink
    if (    {
            }

    // 5. Destroy or hide (v2.3.2: proper hiding for reuse)
    if (Config.bAutoDestroyOnTrackLost)
    {
        Actor->Destroy();
    }
    else
    {
        // Actor persists for reuse when track re-enters bounds
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
        Actor->SetActorTickEnabled(false);
    }

    // 6. Remove from map
    SpawnedActors.Remove(TrackID);
}
```

#### Dynamic Spawn Mode: Actor Reuse (v2.3.2+)

When `UseActorPool=false` and `bAutoDestroyOnTrackLost=false`, actors persist in the level but hidden. When the same track re-enters valid bounds, the system **reuses the existing actor** instead of spawning a new one:

```cpp
void UAefPharusInstance::SpawnActorForTrack(int32 TrackID, const pharus::TrackRecord& Track)
{
    // ... (pool mode code)

    // Dynamic Spawn Mode: Check for existing actor first
    SpawnedActor = FindExistingActorByName(TrackID);

    if (SpawnedActor)
    {
        // Actor was found - reuse it
        SpawnedActor->SetActorHiddenInGame(false);
        SpawnedActor->SetActorEnableCollision(true);
        SpawnedActor->SetActorTickEnabled(true);
    }
    else
    {
        // No existing actor - spawn new one
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = FName(*FString::Printf(TEXT("PharusTrack_%s_%d"), 
            *Config.InstanceName.ToString(), TrackID));
        SpawnedActor = WorldContext->SpawnActor(SpawnClass, &Location, &Rotation, SpawnParams);
    }
    // ...
}

AActor* UAefPharusInstance::FindExistingActorByName(int32 TrackID) const
{
    FString ExpectedName = FString::Printf(TEXT("PharusTrack_%s_%d"), 
        *Config.InstanceName.ToString(), TrackID);
    FName ActorName(*ExpectedName);

    for (TActorIterator<AActor> It(WorldContext); It; ++It)
    {
        if (Actor->GetFName() == ActorName && Actor->IsA(SpawnClass))
        {
            return Actor;
        }
    }
    return nullptr;
}
```

**Why This Matters:**

Without this fix, when `UseActorPool=false` and `bAutoDestroyOnTrackLost=false`:
- Track exits bounds → Actor hidden but not destroyed, removed from tracking map
- Track re-enters bounds → System tries to spawn new actor with same name
- **CRASH**: `Cannot generate unique name for 'PharusTrack_Floor_9'`

#### Actor Pool Release (nDisplay Clusters)

```cpp
void UAefPharusInstance::DestroyActorForTrack(int32 TrackID, const FString& Reason)
{
    if (!ActorPool)
    {
        // Fall back to standard destruction
        return;
    }

    // 1. Get pool index
    int32* PoolIndexPtr = TrackToPoolIndex.Find(TrackID);
    if (!PoolIndexPtr)
        return;

    int32 PoolIndex = *PoolIndexPtr;

    // 2. Get actor
    AActor* Actor = SpawnedActors.FindRef(TrackID);
    if (!Actor)
        return;

    // 3. Notify via interface
    if (Actor->Implements<UAefPharusActorInterface>())
    {
        IAefPharusActorInterface::Execute_OnPharusTrackLost(Actor, TrackID, Reason);
    }

    // 4. Fire events
    OnTrackLost.Broadcast(TrackID);
    if (    // 5. Release to pool (actor returns to pool location, hidden)
    ActorPool->ReleaseActor(PoolIndex);

    // 6. Remove mappings
    TrackToPoolIndex.Remove(TrackID);
    SpawnedActors.Remove(TrackID);
}
```

#### Track Timeout System

```cpp
bool UAefPharusInstance::ProcessPendingOperations(float DeltaTime)
{
    // ... (process spawns/updates)

    // Check for timed-out tracks
    if (Config.TrackTimeout > 0.0f)
    {
        double CurrentTime = FPlatformTime::Seconds();
        TArray<int32> TimedOutTracks;

        for (const auto& Pair : TrackDataCache)
        {
            int32 TrackID = Pair.Key;
            const FPharusTrackData& TrackData = Pair.Value;

            double TimeSinceUpdate = CurrentTime - TrackData.LastUpdateTime;
            if (TimeSinceUpdate > Config.TrackTimeout)
            {
                UE_LOG(LogAefPharus, Warning, TEXT("[%s] Track %d timed out (%.1fs)"),
                    *Config.InstanceName.ToString(), TrackID, TimeSinceUpdate);
                TimedOutTracks.Add(TrackID);
            }
        }

        // Destroy timed-out tracks
        for (int32 TrackID : TimedOutTracks)
        {
            DestroyActorForTrack(TrackID, TEXT("Timeout"));
            TrackDataCache.Remove(TrackID);
        }
    }

    return true;  // Keep ticking
}
```

### 5.4 Actor Interface

**Interface**: `IAefPharusActorInterface`
**Header**: `AefPharusActorInterface.h`

Actors that implement this interface can receive Pharus tracking events.

```cpp
UINTERFACE(Blueprintable)
class AefPharus_API UAefPharusActorInterface : public UInterface
{
    GENERATED_BODY()
};

class AefPharus_API IAefPharusActorInterface
{
    GENERATED_BODY()

public:
    // Called when actor is assigned to track
    UFUNCTION(BlueprintNativeEvent, Category = "AefXR|Pharus")
    void OnPharusTrackAssigned(int32 TrackID, FName InstanceName);

    // Called when actor is assigned with pool index (nDisplay)
    UFUNCTION(BlueprintNativeEvent, Category = "AefXR|Pharus")
    void OnPharusTrackAssignedWithPoolIndex(int32 TrackID, FName InstanceName, int32 PoolIndex);

    // Called every frame with track update
    UFUNCTION(BlueprintNativeEvent, Category = "AefXR|Pharus")
    void OnPharusTrackUpdate(const FPharusTrackData& TrackData);

    // Called when track is lost
    UFUNCTION(BlueprintNativeEvent, Category = "AefXR|Pharus")
    void OnPharusTrackLost(int32 TrackID, const FString& Reason);

    // Query if track is still active (useful for self-validation)
    UFUNCTION(BlueprintNativeEvent, Category = "AefXR|Pharus")
    bool ValidateTrackStillActive();
};
```

**Blueprint Implementation:**

```
Event OnPharusTrackAssigned (TrackID, InstanceName)
    ↓
    Print "Assigned to Track {TrackID} in {InstanceName}"
    Set Actor Color to Random

Event OnPharusTrackUpdate (TrackData)
    ↓
    Update Speed Effect (TrackData.Speed)
    Spawn Trail Particles

Event OnPharusTrackLost (TrackID, Reason)
    ↓
    Play Despawn Effect
    Print "Track {TrackID} lost: {Reason}"
```

**C++ Implementation:**

```cpp
class MYGAME_API AMy

PharusActor : public AActor, public IAefPharusActorInterface
{
    GENERATED_BODY()

protected:
    // Interface implementation
    virtual void OnPharusTrackAssigned_Implementation(int32 TrackID, FName InstanceName) override
    {
        CurrentTrackID = TrackID;
        CurrentInstanceName = InstanceName;

        // Setup actor visuals
        SetActorHiddenInGame(false);
        SetActorEnableCollision(true);
    }

    virtual void OnPharusTrackUpdate_Implementation(const FPharusTrackData& TrackData) override
    {
        // Already handled by SetActorTransform in Instance
        // Optional: Custom behavior based on speed, orientation, etc.
        if (TrackData.Speed > 50.0f)  // Fast movement
        {
            PlaySpeedEffect();
        }
    }

    virtual void OnPharusTrackLost_Implementation(int32 TrackID, const FString& Reason) override
    {
        // Cleanup or special effects
        PlayDespawnEffect();

        // Actor will be destroyed/released by instance
    }

    virtual bool ValidateTrackStillActive_Implementation() override
    {
        // Check if our track ID is still in the instance
        if (UAefPharusSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>())
        {
            if (UAefPharusInstance* Instance = Subsystem->GetTrackerInstance(CurrentInstanceName))
            {
                return Instance->IsTrackActive(CurrentTrackID);
            }
        }
        return false;
    }

private:
    int32 CurrentTrackID;
    FName CurrentInstanceName;
};
```

---

## VI. nDisplay Integration

### 6.1 Actor Pool Architecture

**Purpose**: Deterministic actor spawning for nDisplay cluster synchronization.

**Problem Statement:**

In nDisplay clusters, dynamic actor spawning (`SpawnActor()`) creates different instances on each node:

```
Primary Node:   SpawnActor() → BP_Actor_C_123
Secondary Node: SpawnActor() → BP_Actor_C_456  // Different instance!

Result: Replication fails, actors desynchronized
```

**Solution: Pre-Spawned Actor Pool**

```
All Nodes: Pre-spawn pool at startup → [Actor_0, Actor_1, ..., Actor_N]
Primary: Track 5 appears → Assign pool index 7 → Replicate index
Secondary: Receive index 7 → Use Actor_7 from pool

Result: All nodes use same pool actor → Perfect synchronization!
```

### 6.2 Cluster Synchronization

**DisplayCluster Sync Component:**

```cpp
UCLASS(ClassGroup = (DisplayCluster), meta = (BlueprintSpawnableComponent))
class DISPLAYCLUSTER_API UDisplayClusterSceneComponentSync : public USceneComponent
{
    GENERATED_BODY()

public:
    // Set sync enabled/disabled
    UFUNCTION(BlueprintCallable, Category = "DisplayCluster")
    void SetSyncEnabled(bool bEnable);

    // Pool index (replicated to all nodes)
    UPROPERTY(Replicated)
    int32 PoolIndex;

    // Track ID (replicated)
    UPROPERTY(Replicated)
    int32 TrackID;
};
```

**Actor Setup (Blueprint or C++):**

```cpp
// BP_PharusClusterActor (inherits from AAefPharusClusterActor)
UCLASS()
class AAefPharusClusterActor : public AActor, public IAefPharusActorInterface
{
    GENERATED_BODY()

public:
    AAefPharusClusterActor()
    {
        // Create sync component (auto-detected by DisplayCluster)
        SyncComponent = CreateDefaultSubobject<UDisplayClusterSceneComponentSync>(TEXT("SyncComponent"));
        RootComponent = SyncComponent;

        // DisplayCluster will replicate transform + custom properties
        bReplicates = true;  // Not used in standalone, only in nDisplay
    }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UDisplayClusterSceneComponentSync* SyncComponent;
};
```

### 6.3 Primary vs Secondary Nodes

**Primary Node (Cluster Master):**

- Runs full Pharus tracking logic
- Receives UDP packets from Pharus hardware
- Makes actor pool assignment decisions
- Replicates pool index + transform via DisplayCluster

**Secondary Nodes (Cluster Clients):**

- Do NOT receive UDP packets
- Do NOT run tracking logic
- Receive replicated pool index + transform
- Apply transform to same pool actor

**Configuration:**

```ini
# Primary Node Config
[PharusSubsystem]
AutoCreateInstances=Floor,Wall
IsPharusDebug=false

[Pharus.Floor]
Enable=true                     # Primary runs tracking
BindNIC=192.168.101.55
UDPPort=44345
UseActorPool=true               # MUST use pool for cluster sync
ActorPoolSize=50

# Secondary Node Config
[PharusSubsystem]
AutoCreateInstances=Floor,Wall  # Create same instances

[Pharus.Floor]
Enable=false                    # Secondary does NOT run tracking!
UseActorPool=true               # But MUST create same pool
ActorPoolSize=50                # Same size as primary
PoolIndexOffset=0               # Same offset as primary
```

**Code Execution Flow:**

```
PRIMARY NODE:
1. Pharus UDP packet arrives
2. onTrackNew(Track)
3. ActorPool->AcquireActor(PoolIndex)
4. Actor_7 activated
5. SetActorTransform(WorldPos)
6. DisplayCluster replicates: PoolIndex=7, Transform=WorldPos
        ↓ Network ↓
SECONDARY NODES:
7. Receive: PoolIndex=7, Transform=WorldPos
8. Actor_7 = PooledActors[7]  // Same actor!
9. Actor_7->SetActorTransform(WorldPos)  // Same transform!
10. Perfect synchronization!
```

### 6.4 Pool Configuration

**INI Configuration:**

```ini
[Pharus.Floor]
UseActorPool=true
ActorPoolSize=50                # Must be ≥ max concurrent tracks
PoolSpawnLocationX=0.0          # Pool hidden location
PoolSpawnLocationY=0.0
PoolSpawnLocationZ=-1000.0      # Below floor
PoolSpawnRotationPitch=0.0
PoolSpawnRotationYaw=0.0
PoolSpawnRotationRoll=0.0
PoolIndexOffset=0               # For multiple pools

DefaultSpawnClass=/Game/Blueprints/BP_PharusClusterActor.BP_PharusClusterActor_C
```

**Multiple Pool Support:**

```ini
# Floor Pool (50 actors, index 0-49)
[Pharus.Floor]
UseActorPool=true
ActorPoolSize=50
PoolIndexOffset=0

# Wall Pool (100 actors, index 50-149)
[Pharus.Wall]
UseActorPool=true
ActorPoolSize=100
PoolIndexOffset=50              # Avoids index collision
```

**Pool Exhaustion Handling:**

```cpp
// If pool is exhausted (all actors assigned)
if (FreeIndices.Num() == 0)
{
    UE_LOG(LogAefPharus, Warning, TEXT("[%s] Actor pool exhausted for track %d (consider increasing ActorPoolSize)"),
        *Config.InstanceName.ToString(), TrackID);

    // Track is still tracked, but no actor spawned
    // Solutions:
    // 1. Increase ActorPoolSize
    // 2. Reduce TrackTimeout to release actors faster
    return;
}
```

---

### 7.1 LiveLink Architecture

**LiveLink** is Unreal Engine's framework for streaming live data into the engine. AefPharus publishes Pharus tracks as LiveLink Subjects.

**Architecture:**

```
UAefPharusInstance
    └─>             ├─> ILiveLinkClient::PushSubjectStaticData()
            ├─> ILiveLinkClient::PushSubjectFrameData()
            └─> Subjects: { "Pharus_Floor_Track_1", "Pharus_Floor_Track_2", ... }

LiveLink Client
    ├─> Animation Blueprint (Transform role)
    ├─> Sequencer (Take Recorder)
    └─> Niagara Systems
```

**Enabling LiveLink:**

```ini
[Pharus.Floor]
EnableLiveLink=true
```

### 7.2 Slot Mapping

**Purpose**: Limit LiveLink subjects to specific slots for performance and organization.

**Configuration:**

```ini
[Pharus.Floor]
EnableLiveLink=true
EnableSlotMapping=true          # Enable slot system
ReuseLowestSlot=true            # Reuse lowest available slot
```

**How It Works:**

```
Without Slot Mapping:
    Track 1 → Subject "Pharus_Floor_Track_1"
    Track 2 → Subject "Pharus_Floor_Track_2"
    Track 50 → Subject "Pharus_Floor_Track_50"
    Result: 50 LiveLink subjects (performance impact!)

With Slot Mapping (    Track 1 → Slot 0 → Subject "Pharus_Floor_Slot_0"
    Track 2 → Slot 1 → Subject "Pharus_Floor_Slot_1"
    Track 11 → Slot 2 → Subject "Pharus_Floor_Slot_2" (reused)
    Result: Only 10 LiveLink subjects (optimized!)
```

**Slot Assignment Algorithm:**

```cpp
int32 AssignSlot(int32 TrackID)
{
    // Option 1: ReuseLowestSlot = true (FIFO)
    if (    {
        // Find lowest free slot
        for (int32 Slot = 0; Slot <         {
            if (!SlotToTrack.Contains(Slot))
            {
                SlotToTrack.Add(Slot, TrackID);
                TrackToSlot.Add(TrackID, Slot);
                return Slot;
            }
        }

        // All slots full, replace oldest
        int32 OldestSlot = FindOldestSlot();
        int32 OldTrackID = SlotToTrack[OldestSlot];

        // Reassign
        SlotToTrack[OldestSlot] = TrackID;
        TrackToSlot.Remove(OldTrackID);
        TrackToSlot.Add(TrackID, OldestSlot);

        return OldestSlot;
    }

    // Option 2: ReuseLowestSlot = false (Hash-based)
    else
    {
        int32 Slot = TrackID %         if (SlotToTrack.Contains(Slot))
        {
            // Slot collision, evict old track
            int32 OldTrackID = SlotToTrack[Slot];
            TrackToSlot.Remove(OldTrackID);
        }

        SlotToTrack.Add(Slot, TrackID);
        TrackToSlot.Add(TrackID, Slot);

        return Slot;
    }
}
```

### 7.3 Subject Naming

**Subject Name Format:**

```
Without Slot Mapping:
    Pharus_{InstanceName}_Track_{TrackID}
    Examples:
        Pharus_Floor_Track_1
        Pharus_Floor_Track_42
        Pharus_Wall_Track_15

With Slot Mapping:
    Pharus_{InstanceName}_Slot_{SlotID}
    Examples:
        Pharus_Floor_Slot_0
        Pharus_Floor_Slot_1
        Pharus_Floor_Slot_9
```

**Accessing in Blueprint:**

```
LiveLink Controller
    → Subject Name: "Pharus_Floor_Slot_0"
    → Role: Transform
    → Auto-update Component: ✓
```

### 7.4 Use Cases

#### Use Case 1: Animation Blueprint Retargeting

**Setup:**

1. Enable LiveLink on instance:
   ```ini
   [Pharus.Floor]
   EnableLiveLink=true
   EnableSlotMapping=true
      ```

2. In Animation Blueprint:
   ```
   LiveLink Pose Node
       → Subject Name: "Pharus_Floor_Slot_0"
       → Retarget To: Character Skeleton
       → Output: Retargeted Pose
   ```

3. Result: Character follows Pharus track position

#### Use Case 2: Sequencer Recording

**Setup:**

1. Window → LiveLink → Add Source → AefPharus LiveLink
2. Window → Take Recorder
3. Add Source → LiveLink → Subject: "Pharus_Floor_Slot_0"
4. Record → Creates animation asset with tracked movement

**Use Case:**

- Capture live performances
- Create reference animations
- Prototype character movement

#### Use Case 3: Niagara Particle Tracking

**Setup:**

1. Niagara System → Add User Parameter:
   ```
   Name: TrackPosition
   Type: Vector
   ```

2. In Level Blueprint:
   ```cpp
   Event Tick
       → Get LiveLink Subject Frame Data ("Pharus_Floor_Slot_0")
       → Extract Transform → Location
       → Set Niagara Vector Parameter ("TrackPosition")
   ```

3. Niagara Emitter:
   ```
   Spawn Location → User.TrackPosition
   ```

**Result**: Particles spawn at tracked positions

#### Use Case 4: DMX Light Control

**Setup:**

1. Enable LiveLink
2. Create DMX Blueprint Actor
3. In Blueprint:
   ```cpp
   Event Tick
       → Get LiveLink Transform ("Pharus_Floor_Slot_0")
       → Map Position to DMX Channels
       → Send DMX (Pan/Tilt for moving lights)
   ```

**Result**: Moving lights follow tracked positions

---

## VIII. Blueprint API

### 8.1 Subsystem Nodes

**Get Subsystem:**

```
Get Game Instance → Get Subsystem (AefPharusSubsystem) → UAefPharusSubsystem
```

**Create Instance:**

```
Create Tracker Instance Simple
    → Instance Name: "Floor"
    → Bind NIC: "192.168.101.55"
    → UDP Port: 44345
    → Mapping Mode: Simple
    → Spawn Class: BP_PharusActor
    → Return: FPharusCreateInstanceResult
```

**Query Instances:**

```
Get All Instance Names → TArray<FName>
Get Instance Count → int32
Has Instance ("Floor") → bool
```

**Debug Control:**

```
Set Is Pharus Debug (true/false)
Get Is Pharus Debug → bool
```

### 8.2 Instance Nodes

**Get Instance:**

```
Get Tracker Instance ("Floor") → UAefPharusInstance
```

**Track Queries:**

```
Get Active Track IDs → TArray<int32>
Get Active Track Count → int32
Get Track Data (TrackID) → Position, Rotation, bool (success)
Get Spawned Actor (TrackID) → AActor
Is Track Active (TrackID) → bool
```

**Runtime Config:**

```
Update Floor Settings (NewConfig) → bool
Update Wall Settings (NewConfig) → bool
Restart With New Network (BindNIC, Port) → bool
```

**Configuration Access:**

```
Get Config → FPharusInstanceConfig
```

### 8.3 Track Data Access

**Track Data Structure:**

```cpp
USTRUCT(BlueprintType)
struct FPharusTrackData
{
    // Track identification
    UPROPERTY(BlueprintReadOnly)
    int32 TrackID;

    // World position and orientation
    UPROPERTY(BlueprintReadOnly)
    FVector WorldPosition;

    UPROPERTY(BlueprintReadOnly)
    FVector2D Orientation;  // Normalized direction vector

    // Tracking coordinates
    UPROPERTY(BlueprintReadOnly)
    FVector2D RawPosition;  // Original 2D position (0-1 or meters)

    // Movement data
    UPROPERTY(BlueprintReadOnly)
    float Speed;  // cm/s

    // Wall assignment (Regions mode only)
    UPROPERTY(BlueprintReadOnly)
    EPharusWallSide WallSide;

    // Timestamps
    UPROPERTY(BlueprintReadOnly)
    double SpawnTime;

    UPROPERTY(BlueprintReadOnly)
    double LastUpdateTime;

    // Boundary status (NEW in v2.2.1)
    // true = Actor is spawned and visible (inside valid tracking bounds)
    // false = Track is known but actor is hidden (outside boundary)
    UPROPERTY(BlueprintReadOnly)
    bool bIsInsideBoundary;
};
```

**Usage Example:**

```
Event Tick
    → Get Tracker Instance ("Floor")
    → Get Active Track IDs
    → For Each (TrackID)
        → Get Track Data (TrackID)
        → Print ("Track {TrackID} at {Position}, Speed: {Speed} cm/s")
```

### 8.4 Events & Delegates

**Subsystem Events:**

Currently no subsystem-level events (planned for v2.1).

**Instance Events:**

```cpp
// Bind in Blueprint
Event Begin Play
    → Get Tracker Instance ("Floor")
    → Bind Event to OnTrackSpawned
        → Event: My On Track Spawned (TrackID, TrackData)

// Event implementation
My On Track Spawned (TrackID, TrackData)
    → Print ("New track: {TrackID} at {TrackData.WorldPosition}")
    → Spawn Visual Effect at Position
```

**Available Delegates:**

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FPharusTrackSpawnedDelegate,
    int32, TrackID,
    FPharusTrackData, TrackData
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FPharusTrackUpdatedDelegate,
    int32, TrackID,
    FPharusTrackData, TrackData
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FPharusTrackLostDelegate,
    int32, TrackID
);
```

**Event Firing Frequency:**

| Event | Frequency | Use Case |
|-------|-----------|----------|
| `OnTrackSpawned` | Once per track lifetime | Initialization, spawn effects |
| `OnTrackUpdated` | 60-120 FPS | Movement tracking (expensive!) |
| `OnTrackLost` | Once per track lifetime | Cleanup, despawn effects |

**Performance Note:**

`OnTrackUpdated` fires VERY frequently (60-120 FPS per track). For 50 tracks, that's 3,000-6,000 events/second!

**Recommended:**
- Use `OnTrackUpdated` only for critical logic
- For visual updates, use `Tick` + `GetTrackData()` instead

---

## IX. Advanced Features

### 9.1 Debug Logging System

**Architecture:**

```
IsPharusDebug Flag (Subsystem-level)
    ├─> false → Production Mode (minimal logs)
    │      ├─> Errors (critical failures)
    │      ├─> Warnings (non-critical issues)
    │      └─> Important events (instance creation, track spawn/lost)
    │
    └─> true → Debug Mode (detailed logs)
           ├─> All production logs
           ├─> Config parsing details
           ├─> Transform step-by-step
           ├─> Network binding info
           └─> Success confirmations
```

**Enabling Debug Mode:**

```cpp
// Blueprint
UAefPharusSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();
Subsystem->SetIsPharusDebug(true);

// INI
[PharusSubsystem]
IsPharusDebug=true
```

**Log Categories:**

| Level | When to Use | Example |
|-------|-------------|---------|
| `Error` | Critical failures | "Failed to bind socket to port 44345" |
| `Warning` | Non-critical issues | "Actor pool exhausted" |
| `Log` | Important events | "Instance created: Floor:44345" |
| `Verbose` | Detail information | "Actor pool: Acquired index 7" |
| `VeryVerbose` | UDP packet details | "Received 48 bytes from 192.168.101.55" |

**Production Logging (IsPharusDebug=false):**

```
LogAefPharus: Instance 'Floor' created: 192.168.101.55:44345, Mode=Simple, Pool=Yes
LogAefPharus: Auto-create complete: 2/2 instances created successfully
LogAefPharus: Actor pool exhausted for track 51 (consider increasing ActorPoolSize)
LogAefPharus: Failed to restart instance 'Floor' with 192.168.101.56:44346
```

**Debug Logging (IsPharusDebug=true):**

```
LogAefPharus: ========================================
LogAefPharus: CreateTrackerInstance CALLED
LogAefPharus:   InstanceName: Floor
LogAefPharus:   bEnable: true
LogAefPharus:   BindNIC: '192.168.101.55'
LogAefPharus:   UDPPort: 44345
LogAefPharus:   bIsMulticast: true
LogAefPharus:   MulticastGroup: 239.1.1.1
LogAefPharus:   MappingMode: Simple
LogAefPharus:   SpawnClass: BP_PharusActor_C
LogAefPharus: ========================================
LogAefPharus: Instance 'Floor' created: 192.168.101.55:44345, Mode=Simple, Pool=Yes
LogAefPharus: ========================================
LogAefPharus: CreateTrackerInstance RESULT
LogAefPharus:   bSuccess: true
LogAefPharus:   ErrorCode: None
LogAefPharus:   ErrorMessage:
LogAefPharus: ========================================
```

**Per-Instance Logging Flags:**

```ini
[Pharus.Floor]
LogTrackerSpawned=true          # Log when tracks spawn
LogTrackerUpdated=false         # Log every update (VERY verbose!)
LogTrackerRemoved=true          # Log when tracks are lost
```

**Example Output:**

```
# LogTrackerSpawned=true
LogAefPharus: [Floor] Track 1 spawned at X=100 Y=200 Z=0 (Speed: 25.50 cm/s)

# LogTrackerUpdated=true (WARNING: 60-120 FPS per track!)
LogAefPharus: VeryVerbose: [Floor] Track 1 updated at X=102 Y=201 Z=0

# LogTrackerRemoved=true
LogAefPharus: [Floor] Track 1 lost
```

### 9.2 Performance Monitoring

**Built-in Metrics:**

```cpp
// Blueprint: Get performance stats
UAefPharusInstance* Instance = Subsystem->GetTrackerInstance("Floor");
int32 ActiveTracks = Instance->GetActiveTrackCount();
TArray<int32> TrackIDs = Instance->GetActiveTrackIDs();

UE_LOG(LogTemp, Log, TEXT("Floor: %d active tracks"), ActiveTracks);
```

**Unreal Engine Profiling:**

```
Console Commands:
    stat fps           - Show FPS
    stat game          - Show frame time breakdown
    stat memory        - Show memory usage
    stat net           - Show network stats

    t.MaxFPS 0         - Uncap FPS for testing

Insights:
    UnrealEditor.exe -trace=cpu,memory,frame
    → Analyze in Unreal Insights
```

**Key Metrics to Monitor:**

| Metric | Command | Target | Warning |
|--------|---------|--------|---------|
| Frame Time | `stat game` | <16ms (60 FPS) | >16ms |
| Network Thread CPU | Task Manager | <10% | >20% |
| Memory/Instance | `stat memory` | <10MB | >20MB |
| UDP Packet Rate | Log output | 60-120 FPS | <30 or >200 |

**Performance Bottlenecks:**

1. **Too Many Tracks**: Limit with `2. **Verbose Logging**: Disable `LogTrackerUpdated` in production
3. **Actor Spawning**: Use Actor Pool for predictable performance
4. **Blueprints**: Minimize logic in `OnTrackUpdated` event

### 9.3 Error Handling

**Error Categories:**

| Category | Severity | Action |
|----------|----------|--------|
| Configuration | Error | Fix config, restart instance |
| Network | Error/Warning | Check network, firewall |
| Actor Spawning | Warning | Check SpawnClass, increase pool size |
| Track Timeout | Warning | Informational, track removed |

**Common Errors:**

**1. Network Binding Failure:**

```
Error: UDPManager::Bind: can't bind to port 44345! Error: 10048
```

**Cause**: Port already in use
**Solution**:
- Check other processes: `netstat -ano | findstr :44345`
- Change port in config
- Kill process using port

**2. Spawn Class Missing Interface:**

```
Warning: SpawnClass 'BP_MyActor_C' does not implement IAefPharusActorInterface
```

**Cause**: Actor Blueprint doesn't implement interface
**Solution**:
- Class Settings → Interfaces → Add `AefPharusActorInterface`
- Implement interface events

**3. Actor Pool Exhausted:**

```
Warning: [Floor] Actor pool exhausted for track 51 (consider increasing ActorPoolSize)
```

**Cause**: More concurrent tracks than pool size
**Solution**:
- Increase `ActorPoolSize` in config
- Reduce `TrackTimeout` to release actors faster

**4. Track Timeout:**

```
Warning: [Floor] Track 5 timed out (no UDP updates for 2.1s) - treating as lost
```

**Cause**: No UDP packets for `TrackTimeout` seconds
**Solution**:
- Check network connection
- Verify Pharus hardware is transmitting
- Increase `TrackTimeout` if hardware is slow

**5. Multicast Join Failed:**

```
Error: UDPManager::BindMcast: setsockopt failed! Error: 10049
```

**Cause**: Invalid network interface or multicast group
**Solution**:
- Verify `BindNIC` is correct interface IP
- Check `MulticastGroup` is valid multicast address (239.x.x.x)
- Try `BindNIC=""` (INADDR_ANY)

### 9.4 Timeout Management

**Timeout System:**

```cpp
// Config
TrackTimeout = 2.0f;  // Seconds without update before track lost

// Check on every tick
bool UAefPharusInstance::ProcessPendingOperations(float DeltaTime)
{
    if (Config.TrackTimeout > 0.0f)
    {
        double CurrentTime = FPlatformTime::Seconds();

        for (const auto& Pair : TrackDataCache)
        {
            int32 TrackID = Pair.Key;
            const FPharusTrackData& TrackData = Pair.Value;

            double TimeSinceUpdate = CurrentTime - TrackData.LastUpdateTime;
            if (TimeSinceUpdate > Config.TrackTimeout)
            {
                // Track timed out → treat as lost
                DestroyActorForTrack(TrackID, TEXT("Timeout"));
                TrackDataCache.Remove(TrackID);
            }
        }
    }

    return true;
}
```

**Why Timeout?**

- **Network Issues**: UDP packets dropped, no delivery guarantee
- **Hardware Failure**: Pharus stops transmitting
- **Occlusion**: Track leaves tracking area but no "lost" packet sent

**Timeout Configuration:**

```ini
[Pharus.Floor]
TrackTimeout=2.0  # 2 seconds without update → track lost

# Disable timeout (not recommended)
TrackTimeout=0.0  # Tracks never timeout
```

**Best Practices:**

- **Indoor/Stable**: `TrackTimeout=2.0` (default)
- **Outdoor/Unreliable**: `TrackTimeout=5.0` (higher tolerance)
- **Critical/Real-time**: `TrackTimeout=1.0` (fast detection)
- **Testing**: `TrackTimeout=0.0` (disable timeout)

**Preventing False Timeouts:**

1. **Stable Network**: Use wired Gigabit connections
2. **QoS**: Prioritize UDP traffic on network
3. **Monitor Packet Rate**: Ensure 60-120 FPS from Pharus

---

## X. Best Practices

### 10.1 Performance Optimization

**1. Use Actor Pool for nDisplay:**

```ini
[Pharus.Floor]
UseActorPool=true
ActorPoolSize=50  # Match or exceed max concurrent tracks
```

**Benefits:**
- Predictable performance (no runtime spawning)
- nDisplay cluster synchronization
- Memory pre-allocated

**2. Limit LiveLink Subjects:**

```ini
[Pharus.Floor]
EnableLiveLink=true
EnableSlotMapping=true
```

**3. Disable Verbose Logging:**

```ini
[PharusSubsystem]
IsPharusDebug=false

[Pharus.Floor]
LogTrackerUpdated=false  # VERY high frequency!
```

**4. Minimize Blueprint Logic in Events:**

```
❌ BAD:
Event OnTrackUpdated
    → Complex calculations
    → Expensive function calls
    → Particle systems spawn

✅ GOOD:
Event OnTrackUpdated
    → Set flag "NeedsUpdate"

Event Tick (Rate limited to 30 FPS)
    → If NeedsUpdate
        → Do expensive logic
        → Clear flag
```

**7. Pre-allocate Collections:**

```cpp
// In config: pre-allocate data structures for expected track count
ActorPoolSize=50  // Matches expected max concurrent tracks
```

**7. Profile Regularly:**

```
stat fps
stat game
stat memory
UnrealInsights -trace=cpu,memory
```

### 10.2 Production Deployment

**1. Configuration Checklist:**

```ini
[PharusSubsystem]
IsPharusDebug=false  # Production mode
AutoCreateInstances=Floor,Wall

[Pharus.Floor]
Enable=true
BindNIC=192.168.101.55  # Specific interface
UDPPort=44345
LogTrackerSpawned=false  # Minimal logging
LogTrackerUpdated=false
LogTrackerRemoved=false
UseActorPool=true  # Predictable performance
ActorPoolSize=50  # Sufficient for installation
```

**2. Network Setup:**

- **Wired Connections**: Always use wired Gigabit
- **Dedicated NICs**: Separate NIC per Pharus
- **QoS**: Prioritize UDP port 44345/44346
- **Firewall**: Allow UDP inbound
- **Multicast**: Enable IGMP snooping on switches

**3. Hardware Recommendations:**

- **CPU**: 4+ cores, high single-thread performance
- **RAM**: 16GB minimum
- **Network**: Gigabit NIC(s)
- **OS**: Windows 10/11 Pro 64-bit
- **UE Version**: 5.3+ (plugin requirement)

**4. Testing Protocol:**

1. **Single Track**: Verify basic tracking works
2. **Multiple Tracks**: Test with expected load
3. **Timeout**: Disconnect Pharus, verify timeout works
4. **Cluster Sync** (if nDisplay): Verify all nodes synchronized
5. **Performance**: Profile at expected load
6. **Stability**: 24-hour stress test

**5. Monitoring:**

```cpp
// Log key metrics on interval
void AMyGameMode::Tick(float DeltaTime)
{
    static float TimeSinceLog = 0.0f;
    TimeSinceLog += DeltaTime;

    if (TimeSinceLog > 60.0f)  // Every minute
    {
        UAefPharusSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();

        for (FName InstanceName : Subsystem->GetAllInstanceNames())
        {
            UAefPharusInstance* Instance = Subsystem->GetTrackerInstance(InstanceName);
            int32 TrackCount = Instance->GetActiveTrackCount();

            UE_LOG(LogTemp, Log, TEXT("[%s] Active Tracks: %d"), *InstanceName.ToString(), TrackCount);
        }

        TimeSinceLog = 0.0f;
    }
}
```

**6. Backup & Recovery:**

- **Config Backup**: Version control for `.ini` files
- **Blueprint Backup**: Git or Perforce
- **Fallback Mode**: Disable Pharus if hardware fails
- **Remote Monitoring**: Log aggregation (Graylog, ELK)

### 10.3 Troubleshooting Guide

**Problem: No tracking data received**

```
Symptoms: Subsystem initializes, but no actors spawn
Checks:
    1. Pharus hardware powered and transmitting
    2. Network cable connected
    3. Correct NIC IP in BindNIC
    4. Firewall allows UDP port
    5. Enable debug logging:
       [PharusSubsystem]
       IsPharusDebug=true
    6. Check logs for "Successfully bound to port"
```

**Problem: Tracks spawn at wrong location**

```
Symptoms: Actors appear but positions incorrect
Checks:
    1. Verify SimpleOrigin and SimpleScale
    2. Check FloorZ height
    3. Test FloorRotation (try 0°, 90°, 180°, 270°)
    4. Toggle bInvertY
    5. Use DebugInjectTrack to test known positions:
       Instance->DebugInjectTrack(0.5f, 0.5f, 999);
    6. Enable debug visualization:
       DebugVisualization=true
```

**Problem: Wall tracks not appearing**

```
Symptoms: Floor works, wall doesn't
Checks:
    1. Verify WallRegions configured correctly
    2. Check TrackingBounds cover full 0-1 range
    3. Verify WorldPosition and WorldRotation
    4. Enable region logging (temporarily):
       IsPharusDebug=true
    5. Check logs for "Track position outside all wall regions"
    6. Test with DebugInjectTrack in each region:
       Instance->DebugInjectTrack(0.125f, 0.5f, 100);  // Front
       Instance->DebugInjectTrack(0.375f, 0.5f, 200);  // Left
```

**Problem: Performance issues / stuttering**

```
Symptoms: Frame rate drops, hitching
Checks:
    1. Profile with: stat game, stat memory
    2. Check active track count: Instance->GetActiveTrackCount()
    3. Disable LogTrackerUpdated=false
    4. Use Actor Pool instead of dynamic spawning
    5. Profile Blueprints: stat blueprintvm
    6. Check network thread CPU (should be <10%)
```

**Problem: Actor pool exhausted**

```
Symptoms: Warning "Actor pool exhausted for track X"
Checks:
    1. Increase ActorPoolSize to match max concurrent tracks
    2. Reduce TrackTimeout to release actors faster
    3. Monitor peak track count:
       int32 PeakCount = Instance->GetActiveTrackCount();
    5. Consider dynamic spawning as fallback
```

**Problem: nDisplay cluster desynchronized**

```
Symptoms: Different actors on primary vs secondary nodes
Checks:
    1. Verify UseActorPool=true on ALL nodes
    2. Check ActorPoolSize same on all nodes
    3. Verify PoolIndexOffset same on all nodes
    4. Ensure Enable=false on secondary nodes
    5. Check DisplayCluster sync component on actors
    6. Verify network replication working:
       ShowFlag.VisualizeDisplayCluster 1
```

**Problem: LiveLink subjects not appearing**

```
Symptoms: No subjects in LiveLink window
Checks:
    1. Verify EnableLiveLink=true in config
    2. Check LiveLink window: Window → LiveLink
    3. Look for subjects: "Pharus_{InstanceName}_Slot_X"
    4. Verify LiveLink module enabled in .uproject
    5. Check logs for LiveLink initialization errors
```

**Problem: Track timeout false positives**

```
Symptoms: Tracks lost prematurely despite hardware working
Checks:
    1. Check network stability: ping Pharus IP
    2. Increase TrackTimeout (try 5.0 seconds)
    3. Check UDP packet rate in logs
    4. Monitor network drops: netstat -e
```

**Problem: High memory usage**

```
Symptoms: Memory grows over time
Checks:
    1. Profile with: stat memory
    2. Check for leaked actors: World Outliner
    3. Verify AutoDestroyOnTrackLost=true
    4. Monitor with Unreal Insights: -trace=memory
    5. Check TrackDataCache size:
       TrackDataCache.Num()
    6. Ensure proper Shutdown() on instance removal
```

---

## Appendix A: Configuration Examples

### Example 1: Simple Floor Tracking

```ini
[PharusSubsystem]
AutoCreateInstances=Floor
IsPharusDebug=false

[Pharus.Floor]
Enable=true
BindNIC=192.168.101.55
UDPPort=44345
IsMulticast=true
MulticastGroup=239.1.1.1
MappingMode=Simple

# Room: 10m × 8m, centered at origin
OriginX=-500.0
OriginY=-400.0
ScaleX=1000.0
ScaleY=800.0
FloorZ=0.0
FloorRotation=0.0
InvertY=false

UseNormalizedCoordinates=true
TrackingSurfaceWidth=1.0
TrackingSurfaceHeight=1.0

DefaultSpawnClass=/Game/Blueprints/BP_FloorActor.BP_FloorActor_C
SpawnCollisionHandling=AlwaysSpawn
AutoDestroyOnTrackLost=true

TrackTimeout=2.0

DebugVisualization=false
LiveAdjustments=false
LogTrackerSpawned=false
LogTrackerUpdated=false
LogTrackerRemoved=false
```

### Example 2: 4-Wall CAVE System

```ini
[PharusSubsystem]
AutoCreateInstances=Wall
IsPharusDebug=false

[Pharus.Wall]
Enable=true
BindNIC=192.168.101.56
UDPPort=44346
IsMulticast=true
MulticastGroup=239.1.1.2
MappingMode=Regions

# Front Wall (0-25%)
Wall.Front.TrackingMinX=0.0
Wall.Front.TrackingMaxX=0.25
Wall.Front.TrackingMinY=0.0
Wall.Front.TrackingMaxY=1.0
Wall.Front.WorldPositionX=500.0
Wall.Front.WorldPositionY=0.0
Wall.Front.WorldPositionZ=200.0
Wall.Front.WorldRotationYaw=180.0
Wall.Front.WorldSizeX=2000.0
Wall.Front.WorldSizeZ=400.0
Wall.Front.ScaleX=100.0
Wall.Front.ScaleZ=100.0
Wall.Front.InvertY=false

# Left Wall (25-50%)
Wall.Left.TrackingMinX=0.25
Wall.Left.TrackingMaxX=0.50
Wall.Left.TrackingMinY=0.0
Wall.Left.TrackingMaxY=1.0
Wall.Left.WorldPositionX=0.0
Wall.Left.WorldPositionY=-500.0
Wall.Left.WorldPositionZ=200.0
Wall.Left.WorldRotationYaw=90.0
Wall.Left.WorldSizeX=1000.0
Wall.Left.WorldSizeZ=400.0
Wall.Left.ScaleX=100.0
Wall.Left.ScaleZ=100.0
Wall.Left.InvertY=false

# Back Wall (50-75%)
Wall.Back.TrackingMinX=0.50
Wall.Back.TrackingMaxX=0.75
Wall.Back.TrackingMinY=0.0
Wall.Back.TrackingMaxY=1.0
Wall.Back.WorldPositionX=-500.0
Wall.Back.WorldPositionY=0.0
Wall.Back.WorldPositionZ=200.0
Wall.Back.WorldRotationYaw=0.0
Wall.Back.WorldSizeX=2000.0
Wall.Back.WorldSizeZ=400.0
Wall.Back.ScaleX=100.0
Wall.Back.ScaleZ=100.0
Wall.Back.InvertY=false

# Right Wall (75-100%)
Wall.Right.TrackingMinX=0.75
Wall.Right.TrackingMaxX=1.0
Wall.Right.TrackingMinY=0.0
Wall.Right.TrackingMaxY=1.0
Wall.Right.WorldPositionX=0.0
Wall.Right.WorldPositionY=500.0
Wall.Right.WorldPositionZ=200.0
Wall.Right.WorldRotationYaw=270.0
Wall.Right.WorldSizeX=1000.0
Wall.Right.WorldSizeZ=400.0
Wall.Right.ScaleX=100.0
Wall.Right.ScaleZ=100.0
Wall.Right.InvertY=false

DefaultSpawnClass=/Game/Blueprints/BP_WallActor.BP_WallActor_C
TrackTimeout=2.0
```

### Example 3: nDisplay Cluster with Actor Pool

```ini
# Primary Node Config
[PharusSubsystem]
AutoCreateInstances=Floor
IsPharusDebug=false

[Pharus.Floor]
Enable=true  # Primary runs tracking
BindNIC=192.168.101.55
UDPPort=44345
MappingMode=Simple

UseActorPool=true
ActorPoolSize=50
PoolSpawnLocationX=0.0
PoolSpawnLocationY=0.0
PoolSpawnLocationZ=-1000.0
PoolIndexOffset=0

DefaultSpawnClass=/Game/Blueprints/BP_ClusterActor.BP_ClusterActor_C
```

```ini
# Secondary Node Config
[PharusSubsystem]
AutoCreateInstances=Floor
IsPharusDebug=false

[Pharus.Floor]
Enable=false  # Secondary does NOT run tracking
BindNIC=  # Not used
UDPPort=44345  # Not used
MappingMode=Simple

UseActorPool=true  # MUST create same pool
ActorPoolSize=50  # Same size as primary
PoolSpawnLocationX=0.0
PoolSpawnLocationY=0.0
PoolSpawnLocationZ=-1000.0
PoolIndexOffset=0

DefaultSpawnClass=/Game/Blueprints/BP_ClusterActor.BP_ClusterActor_C
```

### Example 4: LiveLink with Slot Mapping

```ini
[Pharus.Floor]
Enable=true
BindNIC=192.168.101.55
UDPPort=44345
MappingMode=Simple

EnableLiveLink=true
EnableSlotMapping=true
ReuseLowestSlot=true

TrackTimeout=2.0
```

---

## Appendix B: API Quick Reference

### Subsystem API

```cpp
// Instance Management
FPharusCreateInstanceResult CreateTrackerInstance(const FPharusInstanceConfig& Config, TSubclassOf<AActor> SpawnClass);
UAefPharusInstance* GetTrackerInstance(FName InstanceName);
bool RemoveTrackerInstance(FName InstanceName);
TArray<FName> GetAllInstanceNames() const;
int32 GetInstanceCount() const;
bool HasInstance(FName InstanceName) const;

// Debug
void SetIsPharusDebug(bool bNewValue);
bool GetIsPharusDebug() const;
```

### Instance API

```cpp
// Track Data
bool GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation);
TArray<int32> GetActiveTrackIDs() const;
int32 GetActiveTrackCount() const;
AActor* GetSpawnedActor(int32 TrackID);
bool IsTrackActive(int32 TrackID) const;

// Configuration
const FPharusInstanceConfig& GetConfig() const;
bool UpdateConfig(const FPharusInstanceConfig& NewConfig);
bool UpdateFloorSettings(const FPharusInstanceConfig& NewConfig);
bool UpdateWallSettings(const FPharusInstanceConfig& NewConfig);
bool RestartWithNewNetwork(const FString& NewBindNIC, int32 NewUDPPort);

// Debug
void DebugInjectTrack(float NormalizedX, float NormalizedY, int32 TrackID = -1);

// Events
FPharusTrackSpawnedDelegate OnTrackSpawned;
FPharusTrackUpdatedDelegate OnTrackUpdated;
FPharusTrackLostDelegate OnTrackLost;
```

### Actor Interface API

```cpp
// Called when actor assigned to track
void OnPharusTrackAssigned(int32 TrackID, FName InstanceName);
void OnPharusTrackAssignedWithPoolIndex(int32 TrackID, FName InstanceName, int32 PoolIndex);

// Called every update
void OnPharusTrackUpdate(const FPharusTrackData& TrackData);

// Called when track lost
void OnPharusTrackLost(int32 TrackID, const FString& Reason);

// Validate track still active
bool ValidateTrackStillActive();
```

---

## Appendix C: Glossary

| Term | Definition |
|------|------------|
| **Instance** | Single Pharus tracker configuration (e.g., "Floor", "Wall") |
| **Track** | Individual tracked point with unique ID |
| **Subject** | LiveLink representation of a track |
| **Pool** | Pre-spawned actor collection for cluster sync |
| **Slot** | Limited LiveLink subject assignment |
| **Mapping Mode** | Coordinate transformation method (Simple, Regions) |
| **Region** | 2D area mapped to 3D wall plane |
| **Multicast** | UDP 1-to-many network mode |
| **Unicast** | UDP 1-to-1 network mode |
| **nDisplay** | Unreal's multi-projector cluster rendering system |
| **INADDR_ANY** | Bind to all network interfaces (0.0.0.0) |
| **TrackTimeout** | Seconds without UDP update before track considered lost |
| **GlobalOrigin** | Single 3D origin point for all tracking calculations |
| **RootOriginActor** | Optional actor that defines dynamic origin in level |
| **FloorZ** | Height offset relative to GlobalOrigin.Z |

---

## 3.5 Global Root Origin System

### Overview

All Pharus tracking coordinates are calculated relative to a **single 3D origin point** called the **GlobalOrigin**. This centralized origin system replaces the previous per-instance 2D origin configuration.

### Configuration Options

#### Option 1: Static Origin (Config File)

Define the origin in `AefConfig.ini`:

```ini
[PharusSubsystem]
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)
UsePharusRootOriginActor=false
```

#### Option 2: Dynamic Origin (Actor)

Place an `AAefPharusRootOriginActor` in your level for dynamic origin control:

```ini
[PharusSubsystem]
UsePharusRootOriginActor=true
```

The actor's world position becomes the tracking origin. This is useful for:
- Moving stages or platforms
- Runtime calibration
- nDisplay cluster synchronization (actor uses DisplayClusterSceneComponentSyncThis)

> **⚠️ IMPORTANT LIMITATION:**
> Dynamic origin (`UsePharusRootOriginActor=true`) is fully supported by the **Pharus Subsystem** (Blueprints/C++ logic).
> However, **LiveLink** reads the origin configuration **ONLY ONCE** at initialization.
> If you use LiveLink for animation, it will capture the origin at startup (static or actor position at that moment) and **will NOT update dynamically** if the actor moves.
> If dynamic runtime updates are required for LiveLink, a custom LiveLink source recalibration would be needed (currently not supported).

#### Option 3: Programmatic Origin (Blueprint/C++)

Set the origin at runtime via API:

```cpp
// C++
UAefPharusSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();
Subsystem->SetRootOrigin(FVector(1000.0f, 2000.0f, 50.0f));

// Blueprint
Get Game Instance → Get Subsystem (AefPharusSubsystem) → Set Root Origin (Vector)
```

### FloorZ: Height Offset

The `FloorZ` parameter is a **per-instance height offset** relative to `GlobalOrigin.Z`:

```
Final World Z = GlobalOrigin.Z + FloorZ
```

**Why is FloorZ still needed?**

| Use Case | GlobalOrigin.Z | FloorZ | Final Z |
|----------|---------------|--------|---------|
| Stage at ground level | 0 | 0 | 0 |
| Tracking sensors 25cm above floor | 0 | 25 | 25 |
| Elevated platform 100cm high | 100 | 0 | 100 |
| Platform + sensor offset | 100 | 25 | 125 |

**Configuration Example:**

```ini
[PharusSubsystem]
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)  ; Stage base level

[Pharus.Floor]
FloorZ=25.0  ; Actors spawn 25cm above origin (sensor height)
```

### AAefPharusRootOriginActor

A special actor that defines the tracking origin dynamically:

**Features:**
- Uses `UDisplayClusterSceneComponentSyncThis` for nDisplay cluster synchronization
- Automatically registers/unregisters with the subsystem on BeginPlay/EndPlay
- Singleton pattern: only one instance allowed per level
- Editor visualization with Billboard and Arrow components

**Usage:**
1. Place `AAefPharusRootOriginActor` in your level
2. Position it at the desired origin point
3. Set `UsePharusRootOriginActor=true` in config
4. The actor's world location becomes the global origin

**Blueprint Access:**

```cpp
// Check if using actor origin
bool bUsingActor = Subsystem->IsUsingRootOriginActor();

// Get current origin (returns actor position if using actor, else static origin)
FVector Origin = Subsystem->GetRootOrigin();

// Check if valid origin is available
bool bHasValidOrigin = Subsystem->HasValidRootOrigin();
```

### Coordinate Transformation

The tracking pipeline applies the global origin as follows:

**Floor Tracking (TrackToWorldFloor):**
```
WorldPos.X = RootOrigin.X + (NormalizedX * Scale.X)
WorldPos.Y = RootOrigin.Y + (NormalizedY * Scale.Y)
WorldPos.Z = RootOrigin.Z + FloorZ
```

**Wall Tracking (TrackToWorld):**
```
WallWorldPosition = RootOrigin + WallRegion.WorldPosition
// Transform applied relative to wall's local coordinate system
```

### Migration from 2D Origin

The previous `Origin=(X=...,Y=...)` per-instance configuration has been replaced:

| Old Configuration | New Configuration |
|-------------------|-------------------|
| `[Pharus.Floor]` `Origin=(X=100,Y=200)` | `[PharusSubsystem]` `GlobalOrigin=(X=100,Y=200,Z=0)` |
| `FloorZ=25` | `FloorZ=25` (still relative to origin Z) |
| Wall.Front.Origin | Removed (use GlobalOrigin) |

**Note:** Per-instance Origin settings in config files are now **ignored**. Use GlobalOrigin for all positioning.

---

**End of Documentation**

For additional support, see [README.md](README.md) or contact Ars Electronica Futurelab.

**Last Updated**: 2025-12-09
**Plugin Version**: 2.0.0
**Document Version**: 1.1
