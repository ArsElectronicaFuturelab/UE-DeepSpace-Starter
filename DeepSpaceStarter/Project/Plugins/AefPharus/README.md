# AefPharus - Pharus Laser Tracking Plugin

**Version**: 3.0.0
**Engine**: Unreal Engine 5.7+
**Platform**: Windows 64-bit
**Developer**: Ars Electronica Futurelab
**License**: Proprietary

---

## Overview

**AefPharus** is a professional Unreal Engine plugin for integrating Pharus laser tracking hardware into interactive installations, VR/AR experiences, and live productions. It provides real-time, high-precision tracking with support for complex 3D room setups.

### Key Features

- ✅ **Multi-Instance Tracking** - Run Floor + Wall trackers simultaneously
- ✅ **3D Room Mapping** - Floor + 4 walls from 2 Pharus systems
- ✅ **WallRotation Support** - Global wall rotation for coordinate alignment
- ✅ **Root Origin Rotation** - Rotate entire tracking space via actor or config
- ✅ **GameInstance Subsystem** - Persistent, Blueprint-accessible architecture
- ✅ **Manual Start/Stop Control** - Blueprint functions to start/stop system on demand
- ✅ **Runtime SpawnClass Override** - Override DefaultSpawnClass per instance at any time
- ✅ **nDisplay Actor Pool** - Deterministic spawning for cluster synchronization
- ✅ **Flexible Mapping Modes** - Simple (Floor), Regions (Walls)
- ✅ **Thread-Safe** - Lock-free callbacks, optimized critical sections
- ✅ **Performance Optimized** - Smart pointers, pre-allocation, adaptive throttling
- ✅ **Production Ready** - Debug logging, live adjustments, comprehensive error handling

---

## Quick Start

### 1. Enable Plugin

The plugin is already integrated in your project at:
```
Project/Plugins/AefPharus/
```

### 2. Configure Tracking

Edit `Config/AefConfig.ini`:

```ini
[PharusSubsystem]
AutoCreateInstances=Floor,Wall
AutoStartSystem=true  ; false = manual start required
IsPharusDebug=false

; Global 3D origin for ALL tracking instances (Floor + Wall)
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)

; Or use a placed actor for dynamic origin control:
; UsePharusRootOriginActor=true

[Pharus.Floor]
Enable=true
BindNIC=192.168.101.55
UDPPort=44345
MappingMode=Simple
FloorZ=25.0  ; Height OFFSET relative to GlobalOrigin.Z (cm)
DefaultSpawnClass=/Game/Blueprints/BP_FloorActor.BP_FloorActor_C
```

### 3. Control System (Optional - Manual Start)

```
Get Game Instance → Get Subsystem (AefPharusSubsystem)
→ Set Spawn Class Override ("Floor", MyCustomActor)  ; Override before start
→ Start Pharus System  ; Manually start tracking
→ Stop Pharus System   ; Stop all tracking
→ Is Pharus System Running  ; Check status
```

### 4. Access in Blueprint

```
Get Game Instance → Get Subsystem (AefPharusSubsystem)
→ Get Tracker Instance ("Floor")
→ Get Active Track IDs
→ Get Track Data (TrackID) → Position, Rotation, IsInsideBoundary
```

---

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│          UAefPharusSubsystem (GameInstance)         │
│           - Multi-instance management               │
│           - Auto-creation from config               │
│           - Blueprint API                           │
└──────────────────┬──────────────────────────────────┘
                   │ Manages
                   ▼
┌─────────────────────────────────────────────────────┐
│           UAefPharusInstance (per tracker)          │
│  ┌──────────────────┐  ┌─────────────────────────┐  │
│  │ TrackLinkClient  │  │ Actor Pool (optional)   │  │
│  │ (Network Thread) │  │ (nDisplay sync)         │  │
│  └──────────────────┘  └─────────────────────────┘  │
└──────────────────┬──────────────────────────────────┘
                   │ Spawns & Updates
                   ▼
┌─────────────────────────────────────────────────────┐
│        Spawned Actors (IAefPharusActorInterface)    │
└─────────────────────────────────────────────────────┘
```

---

## Core Concepts

### Multi-Instance Support

Run multiple trackers simultaneously with independent configurations:

- **Floor Tracker** - Horizontal plane tracking (Simple mode)
- **Wall Tracker** - 4 vertical walls from single surface (Regions mode)
- **Custom Trackers** - Additional instances for specific use cases

### Mapping Modes

| Mode | Use Case | Description |
|------|----------|-------------|
| **Simple** | Floors, tables | Direct 2D→3D transformation |
| **Regions** | 4-wall CAVE | Corner-based multi-wall mapping with seamless transitions |

### Wall Regions Mode (v2.1+)

Corner-based mapping system for 4-wall CAVE setups:

```
Tracking Flow (left to right):
LEFT(0.8→1.0) → FRONT(0.5→0.8) → RIGHT(0.3→0.5) → BACK(0.0→0.3)

3D Room Layout (Top View):
                 FRONT (Y=1000, Yaw=180°)
        (0,1000)◄────────────(1500,1000)
              │                    │
   LEFT       ▼                    ▲  RIGHT
   Yaw=270°   │       ROOM         │  Yaw=90°
              │                    │
        (0,0) └────────────────────┘ (1500,0)
                 BACK (Y=0, Yaw=0°)
        (0,0)────────────────►(1500,0)
```

**Key Concept:** `WorldPosition` defines the **anchor point** (bottom-left corner) where tracking `(0,0)` maps to. `WorldRotation.Yaw` determines which direction the wall runs.

### Global Root Origin System

All tracking coordinates are calculated relative to a **single 3D origin point and rotation**:

```ini
[PharusSubsystem]
; Static origin defined in config
GlobalOrigin=(X=0.0,Y=0.0,Z=0.0)
GlobalRotation=(Pitch=0.0,Yaw=0.0,Roll=0.0)  ; NEW in v2.2.0
UsePharusRootOriginActor=false

; OR dynamic origin from placed actor (uses actor's position AND rotation)
UsePharusRootOriginActor=true
```

**Origin & Rotation Options:**

| Method | Use Case | Description |
|--------|----------|-------------|
| **Static (Config)** | Fixed installations | Define `GlobalOrigin` and `GlobalRotation` in AefConfig.ini |
| **Dynamic (Actor)** | Moving/rotating stages | Place `AAefPharusRootOriginActor` in level* |
| **Programmatic** | Runtime control | Call `SetRootOrigin(FVector)` and `SetRootOriginRotation(FRotator)` |

_*Note: Dynamic origin actor works fully with Pharus Subsystem but LiveLink only reads the initial position/rotation at startup._

**How Rotation Works:**

Tracking coordinates are first transformed in local space, then rotated by the root rotation before being positioned in world space:

```
1. Local Position = Scale × Normalized Tracking Position
2. Rotated Position = RootRotation.RotateVector(Local Position)
3. World Position = RootOrigin + Rotated Position
```

**FloorZ as Relative Offset:**

The `FloorZ` parameter is a **height offset relative to GlobalOrigin.Z**:

```
Final Z = GlobalOrigin.Z + FloorZ
```

**Why keep FloorZ?**
- Tracking sensors may be mounted at a height above the floor
- Stages with multiple levels need per-instance Z offsets
- Allows calibration without changing the global origin
- Example: `GlobalOrigin.Z=0` (stage base) + `FloorZ=25` (sensor height) = actors spawn at Z=25

### WallRotation (v2.7.0)

**New in v2.7.0:** Global wall rotation for coordinate alignment between Floor and Wall tracking.

When using `FloorRotation` to rotate floor tracking coordinates, set `WallRotation` to the same value for consistent coordinate alignment:

```ini
[Pharus.Floor]
FloorRotation=90.0  ; Rotate floor coordinates 90°

[Pharus.Wall]
WallRotation=90.0   ; Match floor rotation for consistency
```

**Why WallRotation is needed:**
- Floor and Wall tracking data come from the same Pharus coordinate system
- If Floor uses `FloorRotation=90`, Wall needs the same rotation
- Without matching rotations, wall trackers may move perpendicular to their walls

**Transformation Order (v2.7.0 fix):**
```
1. LocalPos = Normalize within region (0-1)
2. ScaledPos = LocalPos × Scale (cm)
3. RotatedPos = Apply WallRotation + Rotation2D (after scale!)
4. LocalWallPos = (RotatedPos.X, 0, RotatedPos.Y)
5. WorldPos = WorldPosition + WorldRotation.RotateVector(LocalWallPos)
```

**Dynamic Origin Behavior (v2.2.0):**

When using `UsePharusRootOriginActor=true`, all Pharus actors **dynamically follow** the actor's position AND rotation in real-time:

- ✅ Move the RootOriginActor → All Pharus actors reposition
- ✅ Rotate the RootOriginActor → All Pharus actors rotate and reposition
- ✅ Works even for **stationary trackers** (position updates happen regardless of movement speed)
- ✅ Position + Rotation are read from the actor **every frame** (when UDP packets arrive)

### Actor Lifecycle

```
Track New (UDP) → Queue on Game Thread
                → Spawn Actor or Acquire from Pool
                → IAefPharusActorInterface::OnPharusTrackAssigned()

Track Update   → Update Actor Transform
                → IAefPharusActorInterface::OnPharusTrackUpdate()

Track Lost     → IAefPharusActorInterface::OnPharusTrackLost()
                → Destroy Actor or Release to Pool
```

### nDisplay Actor Pool

For deterministic actor spawning in cluster environments:

- **Pre-spawned actors** at fixed positions
- **Index-based assignment** synchronized across cluster nodes
- **Primary node controls** which pool actor is assigned to which track
- **Secondary nodes receive** pool index + transform updates

---

## Features

### Live Configuration Adjustments

Update tracking settings at runtime without restart:

```cpp
// Blueprint
UAefPharusInstance* Instance = Subsystem->GetTrackerInstance("Floor");
Instance->UpdateFloorSettings(NewConfig);  // Updates scale, rotation, FloorZ

// Global origin (affects ALL instances)
Subsystem->SetRootOrigin(FVector(100, 200, 0));
```

**Adjustable Settings:**
- Scale factors
- FloorZ height offset
- Floor rotation
- Y-axis inversion
- Speed thresholds
- Debug visualization

**Note:** Origin is now global - use `SetRootOrigin()` or place `AAefPharusRootOriginActor`.

**Non-Adjustable (require restart):**
- Network settings (BindNIC, Port)
- Mapping mode

### Debug Logging

Control logging verbosity:

```cpp
// Enable detailed debug logs
Subsystem->SetIsPharusDebug(true);

// Production mode (minimal logging)
Subsystem->SetIsPharusDebug(false);
```

**Log Categories:**
- `Log` - Important events (instance creation, track spawn/lost)
- `Verbose` - Detail information (actor pool operations, transforms)
- `VeryVerbose` - UDP packet details
- `Warning` - Non-critical issues
- `Error` - Critical failures

---

## Documentation

### 📘 User Guide
- **[DOCUMENTATION.md](DOCUMENTATION.md)** - Complete technical documentation
- **[CONFIGURATION.md](Documentation/CONFIGURATION.md)** - Configuration reference

### 🔧 Technical
- **[nDisplay_Actor_Synchronization_Report.md](Documentation/nDisplay_Actor_Synchronization_Report.md)** - Actor pool implementation
- **[ERRORS.md](Documentation/ERRORS.md)** - Error codes and troubleshooting

---

## System Requirements

### Hardware
- **CPU**: 4+ cores (multi-threading support)
- **RAM**: 8GB minimum, 16GB recommended
- **Network**: Gigabit NIC(s) for Pharus connection
- **Pharus**: Compatible tracking hardware

### Software
- **Unreal Engine**: 5.7 or later
- **Platform**: Windows 11 64-bit
- **Visual Studio**: 2022 (for C++ development)

### Network
- **Protocol**: UDP Multicast or Unicast
- **Bandwidth**: ~1-5 Mbps per tracker
- **Latency**: <10ms recommended

---

## Performance

### Expected Metrics

| Metric | Target | Typical |
|--------|--------|---------|
| Update Rate | 60-120 FPS | 60 FPS |
| Latency | <16ms | ~8ms |
| Frame Impact | <1ms/100 tracks | ~0.5ms |
| Memory/Instance | <10MB | ~6MB |
| Network Thread CPU | <10% | ~5% |

### Optimization Tips

1. **Pre-allocate Actor Pool** for predictable performance
2. **Disable Verbose Logging** in production

---

## Support

**Created by:** Ars Electronica Futurelab - Johannes Lugstein | Friedrich Bachinger | Otto Naderer  
**Website**: https://ars.electronica.art/futurelab/  
**Copyright**: © 2020-2025 Ars Electronica Futurelab

For technical support, see [DOCUMENTATION.md](DOCUMENTATION.md) or contact the development team.

---

## Changelog

### v3.0.0 (2026-02-05) - BREAKING CHANGES
- ⚠️ **Plugin Renamed**: `MoxPharus` → `AefPharus` (module, classes, config)
- ⚠️ **LiveLink Removed**: Complete removal of LiveLink integration
- 📚 See [CHANGELOG.md](CHANGELOG.md) for full migration guide

### v2.7.1 (2026-02-04)
- 🔧 **FIXED:** Floor-Wall Alignment with FloorRotation=90°

### v2.7.0 (2026-02-04)
- ✨ **NEW:** WallRotation Configuration

### v2.6.0 (2026-02-03)
- 🔧 **FIXED:** Normalize Coordinates Bug

*See [CHANGELOG.md](CHANGELOG.md) for complete version history.*

---

**Last Updated**: 2026-02-05
**Plugin Version**: 3.0.0
**Minimum UE Version**: 5.7
