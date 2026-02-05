# Changelog

All notable changes to the AefPharus plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [3.0.0] - 2026-02-05

### ⚠️ BREAKING CHANGES

This is a major release with breaking changes. See the migration notes below.

### Changed
- **Plugin Renamed**: `AefPharus` → `AefPharus`
  - Module name: `AefPharus` → `AefPharus`
  - Log category: `LogAefPharus` → `LogAefPharus`
  - All class prefixes: `Mox` → `Aef`, `AefXR` → `AefXR`
  - Blueprint category: `AefXR|Pharus` → `AefXR|Pharus`
  - Config file: `AefConfig.ini` → `AefConfig.ini`
  
### Removed
- **LiveLink Integration**: Completely removed from the plugin
  - Deleted: `bEnableLiveLink`, `bEnableSlotMapping`, `MaxSlots`, `bReuseLowestSlot` config options
  - Deleted: `AAefPharusTestActor` (was only for LiveLink testing)
  - Removed: LiveLink dependencies from `.uplugin` and `.Build.cs`
  - The plugin now focuses exclusively on direct actor spawning

### Migration Guide v2.x → v3.0

1. **Rename config file**: `AefConfig.ini` → `AefConfig.ini`
2. **Update Blueprint references**:
   - `Get Subsystem (UAefPharusSubsystem)` → `Get Subsystem (UAefPharusSubsystem)`
   - `AAefPharusRootOriginActor` → `AAefPharusRootOriginActor`
3. **Remove LiveLink configuration**: Delete any `EnableLiveLink`, `EnableSlotMapping`, `MaxSlots`, `ReuseLowestSlot` lines
4. **Update C++ includes**: `#include "AefPharus.h"` → `#include "AefPharus.h"`
5. **Update log filtering**: `LogAefPharus` → `LogAefPharus`

---


## [2.7.1] - 2026-02-04

### Fixed
- **Floor-Wall Alignment with FloorRotation=90°**: Corrected wall perimeter order and WorldPositions
  - **Before**: Wall TUIO X=0 mapped to LEFT-FRONT corner (X=0, Y=0)
  - **After**: Wall TUIO X=0 maps to RIGHT-FRONT corner (X=0, Y=0) - matches Floor's Y=0 front area
  - Wall perimeter order changed: `RIGHT → BACK → LEFT → FRONT` (was: FRONT → RIGHT → BACK → LEFT)
  - All Wall.WorldPosition.X values shifted by -1350 to align with Floor's negative X direction

### Changed
- **Wall Layout Coordinate System**: Updated to match FloorRotation=90° transformation
  - RootOriginActor now defines the RIGHT-FRONT corner (0,0,0)
  - Floor extends in -X (left) and +Y (back) directions from RootOriginActor
  - Wall anchors updated accordingly:
    - Right: (0, 0, 0) - at RootOriginActor
    - Back: (0, 1220, 0)
    - Left: (-1350, 1220, 0)
    - Front: (-1350, 0, 0)

### Documentation
- Updated wall layout ASCII diagrams in AefConfig.ini and AefPreset_X-Reality-Lab.ini
- Clarified that RootOriginActor is the single source of truth for (0,0,0) origin

---

## [2.7.0] - 2026-02-04

### Added
- **WallRotation Configuration**: New global wall rotation setting for wall tracking
  - `WallRotation` in `[Pharus.Wall]` section works analogously to `FloorRotation`
  - Applied to all wall regions before individual `Rotation2D`
  - **Use case**: When `FloorRotation=90`, set `WallRotation=90` for consistent coordinate alignment
  - Loaded from `AefConfig.ini` and applied via `TrackToWorld()` and `TrackToLocal()`

### Fixed
- **Wall Transformation Order**: Corrected the order of Scale and Rotation operations
  - **Before (incorrect)**: Rotation applied to normalized 0-1 coordinates, then Scale
  - **After (correct)**: Scale applied first (to cm), then Rotation (consistent with Floor)
  - This fixes the issue where `WallRotation` had no visible effect
  - Both `TrackToWorld()` and `TrackToLocal()` in `FPharusWallRegion` now match Floor behavior

### Changed
- **Wall Layout Origin**: Moved origin to LEFT-FRONT corner (0,0,0)
  - **Before**: Origin at FRONT-RIGHT corner
  - **After**: Origin at LEFT-FRONT corner (consistent with TUIO top-left convention)
  - Tracking flow: FRONT → RIGHT → BACK → LEFT (counter-clockwise from origin)

### Configuration
```ini
[Pharus.Wall]
; WallRotation: Global 2D rotation for all wall regions (degrees)
; Use this to align wall tracking with floor tracking when FloorRotation is used
; Example: If FloorRotation=90, set WallRotation=90 for consistent coordinate alignment
WallRotation=90.0
```

### Wall Layout (X-Reality Lab)
```
              ★ FRONT WALL (Y=0)
         (0,0)────────────────►(1350,0)
           │                      │
  LEFT     │       FLOOR          │  RIGHT
  WALL     │   13.5m × 12.2m      │  WALL
  (X=0)    │                      │  (X=1350)
           │                      │
       (0,1220)◄────────────────(1350,1220)
                BACK WALL (Y=1220)

★ = Origin at LEFT-FRONT corner (0,0,0)
```

### Technical Details
- `FPharusInstanceConfig::WallRotation` - New float field for global wall rotation
- `FPharusWallRegion::TrackToWorld()` - Now accepts `GlobalWallRotation` parameter
- `FPharusWallRegion::TrackToLocal()` - Now accepts `GlobalWallRotation` parameter
- `UAefPharusSubsystem::LoadConfigurationFromIni()` - Loads `WallRotation` from INI
- `FAefPharusLiveLinkEditorSource::LoadConfigFromIni()` - Loads `WallRotation` for LiveLink

### Migration Notes
- If you have existing wall configurations, add `WallRotation` matching your `FloorRotation`
- Review wall anchor points if they were configured for the old origin position
- The transformation order change may affect existing configurations - test thoroughly

---

## [2.6.0] - 2026-02-03

### Fixed
- **Normalize Coordinates Bug**: Fixed coordinate transformation for Floor tracking
  - **Problem**: InvertY was using `-Y` for normalized coordinates, which is incorrect for 0-1 range
  - **Solution**: Now correctly uses `1.0 - Y` for normalized coordinates (TUIO origin top-left)
  - **Result**: Coordinates are now correctly inverted within the 0-1 range

### Changed
- **Reference Actor Origin Position**: Updated origin convention to match TUIO standard
  - **Before**: Origin was at bottom-left (entrance area, like mathematical coordinate system)
  - **After**: Origin at top-left (like screen coordinates where (0,0) is top-left)
  - This follows the **TUIO standard** for normalized coordinates
  
- **Coordinate System Documentation**: Clarified that normalized coordinates (0-1) follow screen/TUIO convention:
  - (0,0) = Top-left corner (Front-left edge of tracking surface)
  - (1,0) = Top-right corner
  - (0,1) = Bottom-left corner (Entrance/Back area)
  - (1,1) = Bottom-right corner

### Technical Details
- `TrackToWorldFloor()`: Fixed InvertY calculation - uses `1.0 - Y` for normalized, `-Y` for absolute
- `TrackToLocalFloor()`: Added explicit TUIO origin top-left documentation
- `FPharusWallRegion::TrackToWorld()`: Consistent InvertY behavior with Floor
- Comments in code now clearly document TUIO origin convention

### Visualization
```
    TUIO Coordinate System (Normalized 0-1):
    
         X=0                    X=1
        ┌─────────────────────────┐
   Y=0  │ (0,0)           (1,0)   │  ← ORIGIN TOP-LEFT (Front wall edge)
        │                         │
        │    TRACKING SURFACE     │
        │                         │
   Y=1  │ (0,1)           (1,1)   │  ← Back (Entrance)
        └─────────────────────────┘
```

### Migration Notes
- If you were using `InvertY=true` with normalized coordinates, the behavior is now correct
- The visual result may differ from before - test your configuration
- For existing installations: Verify actor positions match expected physical locations

---

## [2.5.0] - 2026-01-21

### Fixed
- **Wall Actor Rotation**: Fixed incorrect rotation axis for Wall (Regions mode) actors
  - **Problem**: Wall actors were rotating around World Z (Yaw), same as Floor actors
  - **Solution**: Wall actors now rotate around the Wall Normal (perpendicular to wall surface)
  - **Result**: Actors on walls now correctly face the movement direction within the wall plane, like a swipe gesture direction on a touch wall
  - When `ApplyOrientationFromMovement=true`, actors rotate planar to the wall surface
  - When `ApplyOrientationFromMovement=false`, actors use the wall's base orientation

### Added
- **`GetWallActorRotation()`**: New helper function for Wall-specific rotation calculation
  - Projects 2D tracking orientation onto the wall plane
  - Uses `FRotationMatrix::MakeFromXZ()` to align actor Forward with movement direction and Up with wall normal

### Technical Details
- Floor mode: Uses `GetRotationFromDirection()` → Yaw rotation around World Z
- Wall mode: Uses `GetWallActorRotation()` → Rotation around Wall Normal
- Both `SpawnActorForTrack()` and `UpdateActorForTrack()` now correctly differentiate between mapping modes

---

## [2.4.0] - 2026-01-20

### Added
- **UseRelativeSpawning Mode**: New performance optimization for nDisplay setups with moving camera/stage
  - When `UseRelativeSpawning=true` AND `UsePharusRootOriginActor=true`:
    - Actors are attached as children to `AAefPharusRootOriginActor`
    - Actors use LOCAL coordinates instead of world coordinates
    - When RootOriginActor moves/rotates, actors move automatically (no per-frame recalculation needed)
  - **Benefits**:
    - Eliminates per-frame transform recalculations when origin moves
    - Reduced latency for nDisplay cluster setups
    - Actors automatically inherit RootOriginActor movement
  - **Configuration** in `[PharusSubsystem]` only:
    - `UseRelativeSpawning=true` (requires `UsePharusRootOriginActor=true`)
    - Applies globally to ALL tracking instances (Floor + Wall)

- **New API Methods**:
  - `UAefPharusSubsystem::GetRootOriginActor()` - Get the registered RootOriginActor
  - `UAefPharusSubsystem::IsRelativeSpawningActive()` - Check if relative spawning is active

- **New Coordinate Functions**:
  - `UAefPharusInstance::TrackToLocal()` - Calculate LOCAL position without RootOrigin/RootRotation
  - `UAefPharusInstance::TrackToLocalFloor()` - Floor-specific local coordinate calculation
  - `UAefPharusInstance::TrackToLocalRegions()` - Wall-specific local coordinate calculation
  - `FPharusWallRegion::TrackToLocal()` - Wall region local transformation

### Changed
- **SpawnActorForTrack()**: Now supports relative spawning mode with actor attachment
- **UpdateActorForTrack()**: Uses local coordinates when relative spawning is active

### Deprecated
- **`FPharusInstanceConfig::bUseRelativeSpawning`**: Per-instance setting is now ignored. Use global `[PharusSubsystem].UseRelativeSpawning` instead.

### Technical Details
- Actors in relative mode are attached via `AttachToActor(RootActor, FAttachmentTransformRules::KeepRelativeTransform)`
- Position updated via `SetActorRelativeLocation()` instead of `SetActorLocation()`
- Rotation updated via `SetActorRelativeRotation()` when `ApplyOrientationFromMovement=true`
- Fallback to absolute mode if RootOriginActor is not valid

---


## [2.3.2] - 2026-01-14

### Fixed
- **Critical Crash Fix: Dynamic Spawn Mode Actor Reuse**
  - Fixed fatal crash "Cannot generate unique name for 'PharusTrack_...'" when `UseActorPool=false`
  - **Root Cause**: When tracks exit and re-enter bounds with `bAutoDestroyOnTrackLost=false`, the system tried to spawn a new actor with the same name as an existing actor
  - **Solution**: Implemented actor reuse strategy that detects and reuses existing actors instead of spawning duplicates

### Added
- **`FindExistingActorByName()`**: New helper function to locate existing Pharus actors by expected name pattern
  - Enables proper actor reuse when tracks re-enter valid bounds
  
### Changed  
- **`SpawnActorForTrack()`**: Now checks for existing actors before spawning (Dynamic Spawn Mode only)
  - If actor exists: Reuse it (unhide, enable collision/tick)
  - If no actor: Spawn new one as before
- **`DestroyActorForTrack()`**: When `bAutoDestroyOnTrackLost=false`, actors are now properly hidden
  - `SetActorHiddenInGame(true)`
  - `SetActorEnableCollision(false)`
  - `SetActorTickEnabled(false)`
  - Previously actors were left visible but removed from tracking map

### Technical Details
- New include: `EngineUtils.h` for `TActorIterator`
- Actor name pattern: `PharusTrack_{InstanceName}_{TrackID}`
- Logging: "Reusing existing actor for track X" when actor reuse occurs

---

## [2.3.1] - 2026-01-14

### Fixed
- **RootOriginActor Relative Orientation**: Actors are now correctly oriented relative to the RootOriginActor
  - **Behavior**: When RootOriginActor moves/rotates (like a camera), all tracked actors move and rotate with it
  - **Position**: Full 3D rotation (Pitch, Yaw, Roll) is applied - objects stay on the correct plane relative to origin
  - **Actor Rotation**: Full 3D rotation is applied - actors stay aligned relative to the origin, not world space
  - This enables proper camera-relative tracking where objects appear fixed relative to the moving origin

### Technical Details
- `AefPharusInstance::TrackToWorldFloor()` - Full 3D rotation for positions
- `AefPharusInstance::SpawnActorForTrack()` - Full 3D rotation for actor orientation
- `AefPharusInstance::UpdateActorForTrack()` - Full 3D rotation for actor updates
- `FPharusWallRegion::TrackToWorld()` - Full 3D rotation for wall positions
- `FAefPharusLiveLinkEditorSource::TrackToWorldFloor()` - Full 3D rotation for LiveLink

---

## [2.3.0] - 2026-01-13

### Added
- **AefXR Custom Config Editor**: Major UX overhaul of the configuration panel
  - Renamed tab title from "AefConfig Editor" to "AefXR - Custom Config" for consistency with other AefXR tools
  - Added new "Actor Pool Configuration" category for nDisplay pool settings
  - Added "Movement & Transform" category for behavior settings
  - Improved two-column layout for preset selection (Current Preset vs. Change Preset To)
  - Streamlined "Tracking Instances" section with side-by-side Floor/Wall configuration
  - Added "LiveLink Integration" category to Advanced & Debug section

### Changed
- **Preset Management UI**: Renamed controls for clarity
  - "Template" → "Change Preset To"
  - "Preset" → "Current Preset"
  - "Select Template..." → "Select Preset..."
  - "Apply Template" → "Apply"
- **Config File Naming**: Standardized directory and file naming
  - Directory: `Project/Config/AefPresets/` → `Plugins/MoxEditor/Config/Presets/`
  - File prefix: `AefConfig-*.ini` → `AefPreset_*.ini`
  - Main config: `AefConfig.ini` → `AefConfig.ini`
- **Advanced & Debug**: Reorganized into clear sub-categories
  - LiveLink Integration (Floor + Wall)
  - Actor Pool Configuration (consolidated pool settings)
  - Movement & Transform (UseLocalSpace, ApplyOrientationFromMovement, LiveAdjustments)
  - Debug & Logging

### Removed
- **Non-functional Settings**: Removed broken/unimplemented performance settings
  - `MinSpeedThreshold` - Was intended to filter slow-moving tracks but broke when RootOriginActor moved
  - `MaxUpdatesPerSecond` - Was loaded from config but never actually implemented in code
  - `MaxExpectedTracks` - Reserved for pre-allocation but never used
- **UI Clutter**: Removed less-critical settings from Quick Setup
  - UDP Port, Spawn Class (moved to text-mode editing only with helpful note)
  - Entire "Pharus Tracking" category (advanced network/coordinate settings)
  - "Enable OptiTrack" checkbox and OptiTrack category
  - Performance optimization category (after removing broken settings)

### Fixed
- **Config Consistency**: All files now use consistent `AefConfig` and `AefPreset` naming
- **Panel Headers**: Added consistent headers to all editor panels
  - Pharus Config Panel: "Custom Config Editor"
  - Pharus Launcher Panel: "Pharus Tracker Simulators"

### Documentation
- **Removed All Legacy Setting References**: Cleaned up all mentions of removed settings from documentation
  - Removed from: `DOCUMENTATION.md`, `Documentation/CONFIGURATION.md`, `Documentation/README.md`
  - Total cleanup: Removed all code examples, troubleshooting tips, and configuration guides related to deprecated settings
- Updated all documentation to reflect new naming conventions
- Created workflow for removing legacy performance settings
- Added comprehensive code usage analysis reports

---

## [2.2.1] - 2026-01-12

### Added
- **`bIsInsideBoundary` Track Status**: New field in `FPharusTrackData` indicating whether a track is inside valid tracking bounds
  - `true` = Actor is spawned and visible
  - `false` = Track is known but actor is hidden/not spawned (outside boundary)
- **Extended `GetTrackData()`**: New output parameter `bOutIsInsideBoundary` to query boundary status
  - Signature: `GetTrackData(int32 TrackID, FVector& OutPosition, FRotator& OutRotation, bool& bOutIsInsideBoundary)`
  - Returns whether the track's actor is currently visible or hidden due to boundary constraints

### Changed
- `FPharusTrackData` struct now includes `bIsInsideBoundary` field for Blueprint access
- Track boundary status is now properly tracked and updated in real-time as tracks move in/out of valid bounds

---

## [2.2.0] - 2026-01-12

### Added
- **Root Origin Rotation Support**: `AAefPharusRootOriginActor` rotation is now applied to all tracking transformations
- **`GetRootOriginRotation()`**: New Blueprint-accessible method on Subsystem to get current origin rotation
- **`SetRootOriginRotation()`**: Programmatic control of static origin rotation
- **`GlobalRotation` config**: New INI parameter `GlobalRotation=(Pitch=...,Yaw=...,Roll=...)` for static rotation configuration
- **Dynamic Actor Transforms**: Pharus actors now dynamically update position AND rotation when `AAefPharusRootOriginActor` moves

### Fixed
- **Rotation Bug**: Fixed issue where Pharus actors always remained horizontally aligned regardless of root origin actor rotation

---

## [2.1.0] - 2025-12-10

### Added
- **Corner-Based Wall Mapping**: `WorldPosition` now defines the anchor point (bottom-left corner) where tracking `(0,0)` maps to
- **Origin Parameter**: New `Origin` field in `FPharusWallRegion` for coordinate system offset
- **Yaw Rotation Reference**: Clear documentation for wall direction mapping:
  - `Yaw=0°`: Wall runs along +X axis
  - `Yaw=90°`: Wall runs along +Y axis
  - `Yaw=180°`: Wall runs along -X axis
  - `Yaw=270°`: Wall runs along -Y axis
- **Seamless Wall Transitions**: Walls now connect properly at corners for continuous tracking flow

### Changed
- **Wall Transformation Logic**: Completely rewritten `FPharusWallRegion::TrackToWorld()` function
  - Uses `WorldRotation.RotateVector()` instead of `FTransform.TransformPosition()`
  - Proper separation of anchor point and local position rotation
- **InvertY Behavior**: Now correctly flips Y-axis using `1.0 - Y` instead of `-Y`
- **Config Format**: Wall regions now use corner-based `WorldPosition` instead of center-based

### Fixed
- **Wall Mapping Offset Bug**: Fixed issue where tracking `(0,0)` appeared in the middle of walls instead of the corner
- **Wall Direction**: Fixed walls running in wrong direction (right-to-left instead of left-to-right)
- **Region Transitions**: Fixed seamless transitions between adjacent wall regions

### Documentation
- Updated `DOCUMENTATION.md` with complete wall mapping diagrams and transformation steps
- Updated `README.md` with Wall Regions Mode section and visual layout
- Updated `QUICKSTART.md` with v2.1 corner-based configuration examples
- Added Yaw rotation reference tables to all documentation

---

## [2.0.0] - 2025-11-25

### Added
- **SetSpawnClassOverride()**: Override `DefaultSpawnClass` on Subsystem before `StartPharusSystem()`
- **SetSpawnClass() / GetSpawnClass()**: Change SpawnClass at runtime on Instance level
  - Allows setting custom actor classes per instance from Blueprint/C++
  - New tracks use the new class, existing tracks keep their current actors

### Changed
- Improved Blueprint API for runtime configuration

---

## [1.0.0] - 2025-10-31

### Added
- **Manual Start/Stop Control**: Blueprint functions for system control
  - `StartPharusSystem()` - Start tracking manually
  - `StopPharusSystem()` - Stop all tracking
  - `IsPharusSystemRunning()` - Check system status
- **AutoStartSystem Config**: Option for manual control (`true` = auto-start, `false` = manual)
- **GameInstance Subsystem Architecture**: Persistent tracking across level transitions
- **Multi-Instance Support**: Run Floor + Wall trackers simultaneously
- **LiveLink Integration**: Connect tracks to Animation Blueprints & Sequencer
  - Slot mapping for fixed subject names
  - `EnableLiveLink` flag controls actor spawning
- **nDisplay Actor Pool**: Deterministic spawning for cluster synchronization
- **Flexible Mapping Modes**: Simple, Regions
- **Global Root Origin System**: Single 3D origin for all tracking instances
- **Live Configuration Adjustments**: Update settings at runtime
- **Debug Logging System**: Configurable verbosity levels

### Fixed
- **EnableLiveLink Flag**: Now correctly controls actor spawning
  - `true` = Actors + LiveLink subjects
  - `false` = LiveLink-only mode (no actors spawn)

---

## [0.1.0] - 2025-09-15

### Added
- Initial plugin release
- Basic Pharus UDP tracking support
- Simple floor mapping mode
- Actor spawning system
- Thread-safe network callbacks

---

## Migration Guide

### Migrating from v2.0.x to v2.1.x

**Wall Configuration Changes:**

The wall mapping system has been completely redesigned. You must update your `AefConfig.ini`:

#### Before (v2.0.x - Center-Based):
```ini
; WorldPosition was the CENTER of the wall
Wall.Left.WorldPosition=(X=0.0,Y=500.0,Z=300.0)
Wall.Left.WorldRotation=(Pitch=0.0,Yaw=270.0,Roll=0.0)
Wall.Left.Origin=(X=-500.0,Y=-300.0)  ; Offset to shift (0,0) to corner
```

#### After (v2.1.x - Corner-Based):
```ini
; WorldPosition is now the ANCHOR POINT (bottom-left corner)
Wall.Left.WorldPosition=(X=0.0,Y=1000.0,Z=0.0)
Wall.Left.WorldRotation=(Pitch=0.0,Yaw=270.0,Roll=0.0)
Wall.Left.Origin=(X=0.0,Y=0.0)  ; No offset needed
```

**Key Changes:**
1. `WorldPosition` = Anchor point where tracking `(0,0)` maps to
2. `WorldPosition.Z` should be `0` (floor level), not wall center height
3. `Origin` should be `(0,0)` for corner-based mapping
4. Yaw determines wall direction from anchor point

**Yaw Reference:**
| Wall | Yaw | Direction | Anchor Position |
|------|-----|-----------|-----------------|
| Left | 270° | X→-Y | (0, 1000, 0) |
| Front | 180° | X→-X | (1500, 1000, 0) |
| Right | 90° | X→+Y | (1500, 0, 0) |
| Back | 0° | X→+X | (0, 0, 0) |

---

**Copyright © 2020-2025 Ars Electronica Futurelab**
