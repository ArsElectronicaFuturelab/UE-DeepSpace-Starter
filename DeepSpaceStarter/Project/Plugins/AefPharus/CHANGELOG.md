# Changelog

All notable changes to the AefPharus plugin are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [3.2.1] - 2026-10-20

### Fixed
- Tracker instances survive level changes without retaining the old world; active floor tracks receive actors
  and a rebuilt pool after the new level begins play.
- Destroyed pooled actors are replaced in their original deterministic slot. Restarting a pool in the same
  world no longer crashes on actor name collisions.
- Network callbacks only queue timestamped events. Bounds checks, coordinate mapping, configuration reads,
  root-origin access and actor updates now run on the game thread.
- Absolute actors follow a moving root origin each frame. Relative actors attach if the root origin actor
  registers after they spawn.
- Tracker shutdown, restart and garbage collection no longer wait up to a second for the receive thread.

---

## [3.2.0] - 2026-10-15

### Added
- Dependency on the AefUtility plugin. The configuration path is resolved by
  `UAefUtilityBPLibrary::GetConfigFilePath()` instead of being hard-coded.
- Command-line override `-aefConfig=<path>` (e.g. per nDisplay node). A missing override is logged as an error
  and the default `Config/AefConfig.ini` is used.

### Fixed
- Packaged builds now contain `Config/AefConfig.ini` as a loose, editable file (staged by AefUtility).
  Before, the file was missing in packaged builds and no tracker instances were created.

## [3.1.0] - 2026-10-15

Floor-only release for the Deep Space setup.

### Removed (breaking)
- Wall tracking: `Regions` mapping mode, `EAefPharusMappingMode`, `EAefPharusWallSide`, `FAefPharusWallRegion`,
  `UpdateWallSettings()`, `GetWallSettingsFromDisk()`, wall INI parsing and wall actor rotation.
- Config fields `MappingMode`, `WallRegions`, `WallRotation`, `bLogRegionAssignment`, `bDebugDrawWallPlanes`,
  `bDebugDrawRegionBoundaries` and `FAefPharusTrackData::AssignedWall`.
- `MappingMode` parameter of `CreateTrackerInstanceSimple()`.
- INI keys `MappingMode`, `WallRotation`, `Wall.*`, `LogRegionAssignment`, `DebugDrawWallPlanes`,
  `DebugDrawRegionBoundaries` are no longer read.

Blueprints using only floor functionality keep working; open and re-save Blueprints that use
`FAefPharusInstanceConfig` or `FAefPharusTrackData`.

### Fixed
- Actor pool naming: pooled actors are named `PharusPool_<Instance>_<Index>` instead of using a random per-process
  ID. Every nDisplay node now names the same pooled actor identically, so name-based actor sync between nodes works.
- Game (non-editor) target compiles again: added missing includes (`TSubclassOf`, `FTSTicker`, `UGameInstance`,
  `FTimerManager`).
- `GlobalRotation` and `PoolSpawnRotation` in the INI format `(Pitch=...,Yaw=...,Roll=...)` were never parsed
  (`FRotator::InitFromString` only accepts `P=/Y=/R=`). Both formats are accepted now.
- `SpawnCollisionHandling` and `LogRejectedTracks` are read by both the auto-create path and
  `GetFloorSettingsFromDisk()`.
- `CreateTrackerInstanceSimple()` uses the default multicast group `239.1.1.1` (was `239.0.0.1`).

### Added
- Startup logging: summary of all `[PharusSubsystem]` settings and one summary per parsed instance.

### Changed
- Consolidated documentation into README.md (developer reference) and this changelog;
  DOCUMENTATION.md and QUICKSTART.md were removed.
- Internal macros renamed to `AEFPHARUS_NDISPLAY_SUPPORT` / `AEFPHARUS_HAS_DISPLAYCLUSTER_COMPONENTS`.
- Removed unused module dependencies `InputCore` and `Projects`.
- nDisplay is now a required plugin dependency (was marked optional, but the module always links `DisplayCluster`).

---

## [3.0.0] - 2026-02-05

- Plugin uses the `AefPharus` naming throughout (module `AefPharus`, log category `LogAefPharus`, class prefix `Aef`,
  Blueprint category `AEF|Pharus`, config file `Config/AefConfig.ini`).
- Removed the LiveLink integration (`EnableLiveLink`, `EnableSlotMapping`, `MaxSlots`, `ReuseLowestSlot`).

## [2.x] - 2025/2026 (summary)

- GameInstance subsystem with multi-instance support and INI configuration.
- Manual start/stop (`StartPharusSystem`, `StopPharusSystem`, `AutoStartSystem`) and `SetSpawnClassOverride()`.
- Global root origin (`GlobalOrigin`, `GlobalRotation`, `AAefPharusRootOriginActor`) with full 3D rotation and
  relative spawning (`UseRelativeSpawning`).
- nDisplay actor pool and cluster-synchronized actors.
- `bIsInsideBoundary` in track data, track lost timeout, actor reuse when `AutoDestroyOnTrackLost=false`.
- TUIO top-left origin convention (`InvertY` uses `1 - Y`).
