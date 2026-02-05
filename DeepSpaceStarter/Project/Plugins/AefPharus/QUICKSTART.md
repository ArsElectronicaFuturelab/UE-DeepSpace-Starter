# Quick Start Guide - AefXR Pharus Tracking

Get up and running with Pharus tracking in **5 minutes**!

---

## Prerequisites

- ✅ Unreal Engine 5.3 or later
- ✅ Pharus laser tracking hardware (or simulator for testing)
- ✅ Network connectivity to Pharus system

> **📐 Coordinate System (v2.7.1):** AefPharus uses TUIO standard coordinates where **(0,0) is top-left** (front-left corner of tracking surface). Y-axis points toward the entrance/back. With `FloorRotation=90°`, the wall perimeter order is `RIGHT → BACK → LEFT → FRONT` to ensure Floor and Wall trackers align correctly. See [DOCUMENTATION.md](DOCUMENTATION.md) Section 1.5 for details.

---

## Step 1: Install Plugin (1 minute)

### Option A: Existing Project
1. Copy `AefPharus` folder to your project's `Plugins/` directory
2. Restart Unreal Editor
3. Enable plugin: Edit → Plugins → Search "AefPharus" → Enable → Restart

### Option B: New Project
1. Create new UE5 project (any template)
2. Copy `AefPharus` to `YourProject/Plugins/`
3. Open project in Unreal Editor
4. Plugin will be enabled automatically

**Verify Installation**:
- Check Output Log for: `LogAefPharus: Plugin initialized`

---

## Step 2: Choose Configuration (1 minute)

Select the configuration template that matches your setup:

| Your Setup | Template to Use | Next Step |
|------------|-----------------|-----------|
| **Just testing / No hardware** | `AefPreset_Testing-Localhost.ini` | [Testing Setup](#testing-setup) |
| **Floor tracking only** | `AefPreset_FloorOnly.ini` | [Floor Setup](#floor-setup) |
| **Complete 3D room** | `AefPreset_Complete3DRoom.ini` | [3D Room Setup](#3d-room-setup) |

All templates are in: `Plugins/MoxEditor/Config/Presets/`

---

## Testing Setup

**For development without hardware** (5 minutes total)

### 1. Copy Configuration
```bash
# Windows
copy "Plugins\AefPharus\Config\Examples\AefPreset_Testing-Localhost.ini" "Config\AefConfig.ini"

# Mac/Linux
cp Plugins/AefPharus/Config/Examples/AefPreset_Testing-Localhost.ini Config/AefConfig.ini
```

### 2. Test in Editor
1. Open your project in Unreal Editor
2. Press **Play** (Alt+P)
3. Open Output Log (Window → Developer Tools → Output Log)
4. Look for:
   ```
   LogAefPharus: Floor tracker initialized (127.0.0.1:44345)
   LogAefPharus: Wall tracker initialized (127.0.0.1:44346)
   ```

### 3. Inject Test Tracks
Open **Output Log** and run these commands (~ key to open console):

```
AefPharus.InjectTrack Floor 0.5 0.5
AefPharus.InjectTrack Wall 0.125 0.5
```

**Expected Result**: You should see spheres/cubes spawn in the level!

### 4. Next Steps
- ✅ Test successful? Continue to [Using in Blueprints](#using-in-blueprints)
- ❌ Issues? See [Troubleshooting](#troubleshooting)

---

## Floor Setup

**For single floor tracking** (5 minutes total)

### 1. Copy Configuration
```bash
# Windows
copy "Plugins\AefPharus\Config\Examples\AefPreset_FloorOnly.ini" "Config\AefConfig.ini"

# Mac/Linux
cp Plugins/AefPharus/Config/Examples/AefPreset_FloorOnly.ini Config/AefConfig.ini
```

### 2. Edit Configuration
Open `Config/AefConfig.ini` and update these lines:

```ini
[PharusSubsystem]
# GLOBAL ORIGIN for ALL tracking (Floor + Wall)
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)  # ← Adjust to align with your world center

[Pharus.Floor]
# YOUR NETWORK INTERFACE IP (find with 'ipconfig' or 'ifconfig')
BindNIC=192.168.101.55  # ← Change this to your PC's IP

# YOUR BLUEPRINT ACTOR (or keep default for testing)
SpawnClass=/Engine/BasicShapes/Sphere.Sphere  # ← Change this later

# FloorZ = height offset relative to GlobalOrigin.Z (cm)
# Use this if your tracking sensors are mounted above the floor
FloorZ=0.0
```

### 3. Create Blueprint Actor (Optional)
1. Content Browser → Right-click → Blueprint Class → Actor
2. Name it `BP_PharusFloorCharacter`
3. Add mesh/collision as needed
4. Update SpawnClass in config to point to this Blueprint

### 4. Test with Hardware
1. Start Pharus system
2. Press **Play** in Editor
3. Move in front of Pharus sensor
4. Verify actors spawn at your location

### 5. Next Steps
- ✅ Tracking working? Continue to [Using in Blueprints](#using-in-blueprints)
- ❌ Issues? See [Troubleshooting](#troubleshooting)

---

## 3D Room Setup

**For floor + 4 walls** (10 minutes total)

### 1. Copy Configuration
```bash
# Windows
copy "Plugins\AefPharus\Config\Examples\AefPreset_Complete3DRoom.ini" "Config\AefConfig.ini"

# Mac/Linux
cp Plugins/AefPharus/Config/Examples/AefPreset_Complete3DRoom.ini Config/AefConfig.ini
```

### 2. Edit Global Origin & Rotation
Open `Config/AefConfig.ini`, find `[PharusSubsystem]` section:

```ini
[PharusSubsystem]
# Global 3D origin for ALL tracking (replaces per-instance Origin)
GlobalOrigin=(X=-910.0,Y=-7400.0,Z=0.0)  # Center of your tracking space

# Global rotation for ALL tracking (NEW in v2.2.0)
# Use this to rotate the entire tracking coordinate system
GlobalRotation=(Pitch=0.0,Yaw=0.0,Roll=0.0)  # Rotate around origin

# OR use a placed actor for dynamic origin AND rotation control:
; UsePharusRootOriginActor=true
# Note: Dynamic origin actor works fully with Pharus Subsystem/Blueprint system.
```

> 💡 **Tip (v2.2.0):** When using `UsePharusRootOriginActor=true`, all Pharus actors follow the RootOriginActor's position AND rotation in real-time. This works even for **stationary trackers** - move or rotate the RootOriginActor and all tracked actors update immediately.

### 3. Edit Floor Configuration
Find `[Pharus.Floor]` section:

```ini
# Update these for Floor tracking
BindNIC=192.168.101.55      # Your floor Pharus network interface
UDPPort=44345               # Your floor Pharus UDP port
FloorZ=0.0                  # Height offset from GlobalOrigin.Z (cm)
SpawnClass=/Game/Pharus/Floor/BP_PharusFloorCharacter.BP_PharusFloorCharacter_C
```

### 3. Edit Wall Configuration
Find `[Pharus.Wall]` section:

```ini
# Update these for Wall tracking
BindNIC=192.168.101.56      # Your wall Pharus network interface (different from floor!)
UDPPort=44346               # Different port from floor!
SpawnClass=/Game/Pharus/Wall/BP_PharusWallCharacter.BP_PharusWallCharacter_C
```

### 4. Configure Wall Regions (v2.1+ Corner-Based Mapping)

Wall mapping uses **corner-based anchoring** - `WorldPosition` defines where tracking `(0,0)` maps to.

```ini
;==============================================================================
; WALL CONFIGURATION - 15m×10m Room, 6m High Walls
;==============================================================================
; Tracking flows left-to-right: LEFT → FRONT → RIGHT → BACK

; LEFT WALL (Tracking: 0.8-1.0, runs Y: 1000→0)
Wall.Left.WorldPosition=(X=0.0,Y=1000.0,Z=0.0)      ; Anchor = top-left corner
Wall.Left.WorldRotation=(Pitch=0.0,Yaw=270.0,Roll=0.0)  ; X→-Y
Wall.Left.Scale=(X=1000.0,Y=600.0)                  ; 10m wide × 6m high

; FRONT WALL (Tracking: 0.5-0.8, runs X: 1500→0)
Wall.Front.WorldPosition=(X=1500.0,Y=1000.0,Z=0.0)  ; Anchor = top-right corner
Wall.Front.WorldRotation=(Pitch=0.0,Yaw=180.0,Roll=0.0)  ; X→-X
Wall.Front.Scale=(X=1500.0,Y=600.0)                 ; 15m wide × 6m high

; RIGHT WALL (Tracking: 0.3-0.5, runs Y: 0→1000)
Wall.Right.WorldPosition=(X=1500.0,Y=0.0,Z=0.0)     ; Anchor = bottom-right corner
Wall.Right.WorldRotation=(Pitch=0.0,Yaw=90.0,Roll=0.0)   ; X→+Y
Wall.Right.Scale=(X=1000.0,Y=600.0)                 ; 10m wide × 6m high

; BACK WALL (Tracking: 0.0-0.3, runs X: 0→1500)
Wall.Back.WorldPosition=(X=0.0,Y=0.0,Z=0.0)         ; Anchor = bottom-left corner
Wall.Back.WorldRotation=(Pitch=0.0,Yaw=0.0,Roll=0.0)     ; X→+X
Wall.Back.Scale=(X=1500.0,Y=600.0)                  ; 15m wide × 6m high
```

**Yaw Rotation Reference:**

| Wall | Yaw | Direction | Anchor Point |
|------|-----|-----------|--------------|
| Left | 270° | X→-Y | (0, 1000, 0) |
| Front | 180° | X→-X | (1500, 1000, 0) |
| Right | 90° | X→+Y | (1500, 0, 0) |
| Back | 0° | X→+X | (0, 0, 0) |

### 5. Test Both Systems
1. Start both Pharus systems
2. Press **Play** in Editor
3. Test floor tracking (walk on floor)
4. Test wall tracking (move hand on walls)
5. Verify actors spawn on correct surfaces

### 6. Next Steps
- ✅ Both systems working? Continue to [Using in Blueprints](#using-in-blueprints)
- ❌ Issues? See [Troubleshooting](#troubleshooting)

---

## Using in Blueprints

### Manual Start/Stop Control (Optional)

If you set `AutoStartSystem=false` in config, you need to manually start the system:

**Blueprint**:
```
Event BeginPlay
 └─> Get Game Instance
      └─> Get Subsystem (UAefPharusSubsystem)
           └─> Start Pharus System
                └─> Branch (Success?)
                     ├─> True: System started
                     └─> False: Already running or error
```

### Override SpawnClass at Runtime (NEW)

Override the `DefaultSpawnClass` from AefConfig.ini **before** starting the system:

**Blueprint**:
```
Event BeginPlay
 └─> Get Game Instance
      └─> Get Subsystem (UAefPharusSubsystem)
           └─> Set Spawn Class Override
                ├─> Instance Name: "Floor"
                └─> Spawn Class: BP_MyCustomFloorActor
           └─> Set Spawn Class Override
                ├─> Instance Name: "Wall"
                └─> Spawn Class: BP_MyCustomWallActor
           └─> Start Pharus System
```

**Important**: `SetSpawnClassOverride()` must be called **BEFORE** `StartPharusSystem()` to take effect.

### Change SpawnClass on Running Instance (NEW)

Change the SpawnClass for an already running tracker instance:

**Blueprint**:
```
Get Game Instance
 └─> Get Subsystem (UAefPharusSubsystem)
      └─> Get Tracker Instance ("Floor")
           └─> Set Spawn Class (BP_MyNewFloorActor)
```

**Note**: Only **new tracks** will use the new SpawnClass. Existing tracks keep their current actors.

**Stop System**:
```
Event (e.g., End Play)
 └─> Get Game Instance
      └─> Get Subsystem (UAefPharusSubsystem)
           └─> Stop Pharus System
```

**Check Status**:
```
Event Tick
 └─> Get Game Instance
      └─> Get Subsystem (UAefPharusSubsystem)
           └─> Is Pharus System Running
                └─> Branch
```

### Access Floor Tracker

**Blueprint**:
```
Event BeginPlay
 └─> Get Game Instance
      └─> Get Subsystem (UAefPharusSubsystem)
           └─> Get Tracker Instance (Name: "Floor")
                └─> (Store as variable)
```

**Blueprint Visual**:
![Blueprint Example](https://via.placeholder.com/600x200/4CAF50/FFFFFF?text=Get+Floor+Tracker)

### Bind to Events

**Blueprint**:
```
Event BeginPlay
 └─> Get Floor Tracker (from above)
      └─> Bind Event to OnTrackSpawned
           └─> Custom Event "Handle Track Spawned"
```

**Custom Event Signature**:
- **Handle Track Spawned**
  - Input: `Track ID` (Integer)
  - Input: `Spawned Actor` (Actor)

### Example: Count Active Tracks

**Blueprint**:
```
Event Tick
 └─> Get Floor Tracker
      └─> Get Active Track Count
           └─> Print String
```

### Example: Highlight Tracked Object

**Blueprint**:
```
Custom Event "Handle Track Spawned"
 ├─> Input: Track ID (Integer)
 ├─> Input: Spawned Actor (Actor)
 └─> Cast to BP_PharusFloorCharacter
      └─> Set Actor Hidden in Game (False)
           └─> Add Component
                └─> Static Mesh Component (Highlight glow)
```

---

## Using in C++

### Access Subsystem

```cpp
// In BeginPlay or Initialize
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    UAefPharusSubsystem* PharusSubsystem =
        GetGameInstance()->GetSubsystem<UAefPharusSubsystem>();

    if (PharusSubsystem)
    {
        // Optional: Override SpawnClass before starting (if AutoStartSystem=false)
        // Must be called BEFORE StartPharusSystem()!
        PharusSubsystem->SetSpawnClassOverride("Floor", AMyCustomFloorActor::StaticClass());
        PharusSubsystem->SetSpawnClassOverride("Wall", AMyCustomWallActor::StaticClass());
        PharusSubsystem->StartPharusSystem();

        // Get Floor tracker
        FloorTracker = PharusSubsystem->GetTrackerInstance("Floor");

        if (FloorTracker && FloorTracker->IsRunning())
        {
            // Bind events
            FloorTracker->OnTrackSpawned.AddDynamic(
                this, &AMyActor::HandleTrackSpawned);

            FloorTracker->OnTrackUpdated.AddDynamic(
                this, &AMyActor::HandleTrackUpdated);

            FloorTracker->OnTrackLost.AddDynamic(
                this, &AMyActor::HandleTrackLost);

            UE_LOG(LogTemp, Log, TEXT("Pharus Floor tracker initialized"));
        }
    }
}
```

### Event Handlers

```cpp
// Header file
UFUNCTION()
void HandleTrackSpawned(int32 TrackID, AActor* SpawnedActor);

UFUNCTION()
void HandleTrackUpdated(int32 TrackID, const FPharusTrackData& TrackData);

UFUNCTION()
void HandleTrackLost(int32 TrackID);

// Implementation
void AMyActor::HandleTrackSpawned(int32 TrackID, AActor* SpawnedActor)
{
    UE_LOG(LogTemp, Log, TEXT("Track %d spawned at %s"),
        TrackID, *SpawnedActor->GetActorLocation().ToString());

    // Store reference
    TrackedActors.Add(TrackID, SpawnedActor);
}

void AMyActor::HandleTrackUpdated(int32 TrackID, const FPharusTrackData& TrackData)
{
    // Track moved - update your logic here
    if (AActor* TrackedActor = TrackedActors.FindRef(TrackID))
    {
        // Actor is at TrackData.WorldPosition
        // Moving at TrackData.Velocity
    }
}

void AMyActor::HandleTrackLost(int32 TrackID)
{
    UE_LOG(LogTemp, Log, TEXT("Track %d lost"), TrackID);
    TrackedActors.Remove(TrackID);
}
```

### Get Track Count

```cpp
int32 ActiveTracks = FloorTracker->GetActiveTrackCount();
UE_LOG(LogTemp, Log, TEXT("Active tracks: %d"), ActiveTracks);
```

### Get Spawned Actor

```cpp
AActor* SpawnedActor = FloorTracker->GetSpawnedActor(TrackID);
if (SpawnedActor)
{
    FVector Location = SpawnedActor->GetActorLocation();
}
```

---

## Troubleshooting

### Plugin Not Loading

**Problem**: Plugin doesn't appear in Plugins window

**Solution**:
1. Verify `AefPharus` folder is in `YourProject/Plugins/`
2. Check `AefPharus.uplugin` file exists
3. Right-click `.uproject` → Generate Visual Studio files
4. Rebuild project

---

### Configuration Not Found

**Problem**: Log shows "No configuration found for instance 'Floor'"

**Solution**:
1. Verify `AefConfig.ini` is in `YourProject/Config/` (NOT `Saved/Config/`)
2. Check file contains `[PharusSubsystem]` and `[Pharus.Floor]` sections
3. Section names are case-sensitive: `[Pharus.Floor]` not `[pharus.floor]`
4. Enable verbose logging to see config loading:
   ```
   Log LogAefPharus VeryVerbose
   ```

---

### Network Connection Fails

**Problem**: Log shows "Failed to bind to 192.168.101.55:44345"

**Solution**:
1. **Check IP exists on your system**:
   ```bash
   # Windows
   ipconfig

   # Mac/Linux
   ifconfig
   ```
   Find your network interface IP and update `BindNIC` in config

2. **Check port is available**:
   ```bash
   # Windows
   netstat -an | findstr 44345

   # Mac/Linux
   netstat -an | grep 44345
   ```
   If port is in use, try different port (44346, 44347, etc.)

3. **Check firewall**:
   ```bash
   # Windows - allow UDP port
   netsh advfirewall firewall add rule name="Pharus Floor" dir=in action=allow protocol=UDP localport=44345
   ```

4. **Try localhost for testing**:
   ```ini
   BindNIC=127.0.0.1
   IsMulticast=false
   ```

---

### No Actors Spawning

**Problem**: Network connected but no actors appear

**Solution**:
ini
   # Must be true to spawn actors
   
   ```

2. **Check SpawnClass is valid**:
   ```ini
   # For testing, use engine defaults
   SpawnClass=/Engine/BasicShapes/Sphere.Sphere
   ```

3. **Check tracking data is arriving**:
   ```
   Log LogAefPharus Verbose
   ```
   Look for: "Received track update for ID X"

4. **Enable debug visualization**:
   ```ini
   DebugVisualization=true
   DebugDrawBounds=true
   DebugDrawOrigin=true
   ```


---

### Actors Spawn at Wrong Location

**Problem**: Actors spawn but coordinates are wrong

**Solution**:
1. **Adjust origin offsets**:
   ```ini
   # These offset the tracking origin to align with world origin
   OriginX=0.0   # Try different values
   OriginY=0.0   # Try different values
   ```

2. **Check scale factors**:
   ```ini
   # Standard: 100.0 (converts meters → centimeters)
   ScaleX=100.0
   ScaleY=100.0
   ```

3. **Enable debug drawing**:
   ```
   AefPharus.DebugDraw 1
   ```
   This shows tracking bounds and coordinate system

4. **Test with known coordinates**:
   ```
   # Spawn test track at center of tracking area
   AefPharus.InjectTrack Floor 0.5 0.5
   ```

---

### Wall Tracking Issues

**Problem**: Wall tracks go to wrong wall or don't spawn

**Solution**:
1. **Enable region logging**:
   ```ini
   LogRegionAssignment=true
   ```
   Check which wall is being assigned

2. **Visualize regions**:
   ```ini
   DebugDrawRegionBoundaries=true
   DebugDrawWallPlanes=true
   ```

3. **Verify region bounds**:
   ```ini
   # Must sum to 0.0-1.0 range with no gaps/overlaps
   Wall.Front.TrackingMinX=0.0
   Wall.Front.TrackingMaxX=0.25
   Wall.Left.TrackingMinX=0.25
   Wall.Left.TrackingMaxX=0.5
   # ... etc
   ```

4. **Test each wall individually**:
   ```
   # Front wall (region 0.0-0.25)
   AefPharus.InjectTrack Wall 0.125 0.5

   # Left wall (region 0.25-0.5)
   AefPharus.InjectTrack Wall 0.375 0.5

   # Back wall (region 0.5-0.75)
   AefPharus.InjectTrack Wall 0.625 0.5

   # Right wall (region 0.75-1.0)
   AefPharus.InjectTrack Wall 0.875 0.5
   ```

---

## Console Commands

Open console with **`~`** key:

```
# List all tracker instances
AefPharus.ListInstances

# Show detailed status
AefPharus.ShowStatus Floor
AefPharus.ShowStatus Wall

# Enable debug visualization
AefPharus.DebugDraw 1

# Show performance stats
AefPharus.Stats

# Inject test track
AefPharus.InjectTrack Floor 0.5 0.5
AefPharus.InjectTrack Wall 0.125 0.5

# Restart instance
AefPharus.RestartInstance Floor
```

---

## Next Steps

### Learn More
- **[Complete README](README.md)**: Full documentation
- **[API Reference](README.md#api-reference)**: All classes and functions
- **[Configuration Guide](Config/Examples/README.md)**: All config parameters

### Migration from Legacy
- **[Migration Guide](MIGRATION_GUIDE.md)**: Step-by-step migration from PharusLasertracking plugin

### Advanced Topics
- **Performance Optimization**: [README.md#performance-guidelines](README.md#performance-guidelines)
- **Mapping Modes**: [README.md#mapping-modes](README.md#mapping-modes)
- **Multi-User Sync**: DeepSync integration

---

## Support

**Found a bug?**
- GitHub Issues: https://github.com/your-org/AefPharus/issues

**Need help?**
- Documentation: https://docs.your-company.com/AefPharus
- Email: pharus-support@your-company.com

---

## What You've Learned

✅ How to install AefPharus plugin
✅ How to configure floor and wall tracking
✅ How to access trackers in Blueprints and C++
✅ How to handle tracking events
✅ How to troubleshoot common issues

**Ready for production?** See [README.md](README.md) for advanced configuration and best practices.

---

**Quick Start Complete!** 🎉

You're now tracking with Pharus in Unreal Engine 5.
