# Unreal Engine 5 - DeepSpace-Starter

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-blue)](https://unrealengine.com)
[![Release](https://img.shields.io/github/v/release/ArsElectronicaFuturelab/UE-DeepSpace-Starter)](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-green)](LICENSE)

Unreal Engine 5.7 project template for interactive installations with Pharus laser tracking and nDisplay cluster rendering.

## Overview

UE-DeepSpace-Starter is a Blueprint-based project template developed by Ars Electronica Futurelab for the [Deep Space 8K](https://ars.electronica.art/center/en/exhibitions/deepspace) installation at the Ars Electronica Center. It integrates Pharus laser tracking hardware with Unreal's nDisplay system for multi-projector setups on floor and wall surfaces.

**This is a Blueprint project** with included C++ plugins. All core functionality is accessible via Blueprints; the editor runs without compiling anything. The AefPharus (Pharus laser tracking) and AefUtility (configuration file handling) plugins are included with pre-compiled editor binaries.

## Requirements

- **Unreal Engine**: 5.7
- **Platform**: Windows 64-bit
- **Git LFS**: required when cloning (assets and binaries are stored with [Git LFS](https://git-lfs.com))
- **Visual Studio 2022** with the "Game development with C++" workload: required for packaging only, because Unreal compiles the included C++ plugins when it packages the project

## Quick Links

Download UE DeepSpace Starter (Zips). [Deprecated] versions should still work but are not maintained anymore.

| Version | Status | Download |
|---------|--------|----------|
| UE 5.7 | **Current** | [Latest release](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/releases/latest) (ZIP incl. all LFS files) |
| UE 5.3 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.3-DeepSpace-Starter-Experimental.zip) |
| UE 5.1 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.1-DeepSpace-Starter.zip) |
| UE 5.0 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.0-DeepSpace-Starter.zip) |
| UE 4.27 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-4.27-DeepSpace-Starter.zip) |

> **Note:** For UE 5.7 use the ZIP from the release. GitHub's "Code → Download ZIP" may contain only Git LFS pointer files instead of the actual assets.

## Versioning & Releases

The project, the included plugins (AefPharus, AefUtility) and `AefConfig.ini` share **one version number** ([Semantic Versioning](https://semver.org)); the current version is shown in the release badge above.

The single source of truth is `ProjectVersion` in `DeepSpaceStarter/Project/Config/DefaultGame.ini`. These values must always match it:

| File | Key |
|------|-----|
| `DeepSpaceStarter/Project/Config/DefaultGame.ini` | `ProjectVersion` (source) |
| `DeepSpaceStarter/Project/DeepSpaceStarter.uproject` | `VersionName` |
| `DeepSpaceStarter/Project/Plugins/*/*.uplugin` | `VersionName` (also increase the integer `Version`) |
| `DeepSpaceStarter/Project/Config/AefConfig.ini` | `[General] Version` |

**Creating a release:** update all versions above (plus the plugin `CHANGELOG.md` / `README.md` files) and push. The workflow [`.github/workflows/create-release.yml`](.github/workflows/create-release.yml) runs automatically when `DefaultGame.ini` changes on `main` or `UE-5.7-DeepSpace-Starter`:

1. Reads `ProjectVersion` and checks that all versions above match; the run fails if one differs.
2. Skips the release if the tag `v<version>` already exists.
3. Creates the tag `v<version>` and a GitHub release with a ZIP that contains all Git LFS files.

The workflow can also be started manually (Actions → Create Release with LFS → Run workflow); in that case it fails if the tag already exists.

## Project Structure

```
UE-DeepSpace-Starter/
├── .github/workflows/                 # Release workflow (tag + release ZIP)
├── DeepSpaceStarter/
│   ├── Project/
│   │   ├── DeepSpaceStarter.uproject  # Unreal Engine project file
│   │   ├── Content/                   # Game assets and blueprints
│   │   │   └── DeepSpace/             # Deep Space specific content
│   │   │       ├── Examples/          # Example actors (e.g. synced person reference)
│   │   │       ├── Library/           # Blueprint libraries
│   │   │       ├── Maps/              # Level maps
│   │   │       ├── Materials/         # Materials and shaders
│   │   │       ├── Models/            # 3D models
│   │   │       └── Switchboard/       # nDisplay configuration assets
│   │   ├── Config/                    # Project configuration
│   │   │   ├── AefConfig.ini          # Pharus tracking configuration
│   │   │   ├── Default*.ini           # Engine/Input/Game settings (DefaultGame.ini holds the version)
│   │   │   └── Windows/               # Windows-specific engine settings
│   │   └── Plugins/                   # C++ plugins (pre-compiled)
│   │       ├── AefPharus/             # Pharus laser tracking plugin
│   │       ├── AefUtility/            # Loads AefConfig.ini, stages it into packaged builds
│   │       └── AefDeepSync/           # Optional, not included (see below)
│   ├── Pharus/                        # Pharus simulator tools
│   │   └── DeepSpace/                 # Preconfigured simulators
│   │       ├── TrackLinkSimulator_*/  # TrackLink simulators (multicast/unicast)
│   │       └── pharus-rec-sim-*/      # Pharus recording simulators
│   ├── Deploy/                        # Deep Space deployment folder
│   │   ├── Build/                     # Packaged application (generated)
│   │   ├── Switchboard/               # nDisplay configuration files
│   │   │   └── ND_Deep_Space_8K.ndisplay
│   │   └── project.json               # Deployment configuration
└── LICENSE
```

## Plugins

### AefPharus

C++ plugin for Pharus laser tracking integration (included with pre-compiled binaries).

**Features:**
- Floor tracking with configurable mapping (scale, rotation, global root origin)
- nDisplay cluster synchronization
- Thread-safe operation
- Blueprint API for runtime control

**Documentation:** See [AefPharus README](DeepSpaceStarter/Project/Plugins/AefPharus/README.md) for detailed configuration.

### AefUtility

Shared C++ plugin used by AefPharus (included with pre-compiled binaries).

**Features:**
- Resolves the active configuration file: `Project/Config/AefConfig.ini`, or the file given with `-dsConfig=<path>`
- Copies `AefConfig.ini` automatically into packaged builds as a loose, editable file

**Documentation:** See [AefUtility README](DeepSpaceStarter/Project/Plugins/AefUtility/README.md).

### AefDeepSync (optional)

Plugin for DeepSync wearables (heart rate, LED color, IDs) and their link to Pharus tracks. It is **not part of this repository** and is only needed for projects that use the DeepSync wearables. It is maintained in its own repository with its own version number: [UE-DeepSpace-DeepSync](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-DeepSync).

**Installation:**
1. Get the plugin from the [UE-DeepSpace-DeepSync](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-DeepSync) repository.
2. Copy its `Plugins/AefDeepSync/` folder to `DeepSpaceStarter/Project/Plugins/AefDeepSync/`. This folder is listed in `.gitignore` and is not committed to this repository.
3. Restart the editor. The plugin requires AefPharus, which is already included.
4. Configure it in the `[DeepSync]` section of `Project/Config/AefConfig.ini`. The section is already prepared and is ignored while the plugin is not installed.

**Documentation:** See the [AefDeepSync README](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-DeepSync#readme).

## Engine Plugins

The project uses the following Unreal Engine plugins:

- **nDisplay** - Multi-projector cluster rendering
- **Switchboard** - Cluster management and deployment

## Getting Started

### Prerequisites

1. **Install Unreal Engine 5.7**
   - Download from Epic Games Launcher
   - Associate .uproject files with UE 5.7

2. **Clone or download the repository**
   ```bash
   git lfs install
   git clone --branch UE-5.7-DeepSpace-Starter https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter.git
   cd UE-DeepSpace-Starter
   ```

   Or download the ZIP of the [latest release](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/releases/latest).

### Opening the Project

1. Navigate to `DeepSpaceStarter/Project/`
2. Double-click `DeepSpaceStarter.uproject`
3. Unreal Editor will open the project

### Running with Switchboard

1. **Install Switchboard Dependencies**
   - In Unreal Editor: Edit → Editor Preferences → Plugins → Switchboard
   - Click "Install Prerequisites" if prompted
   - Create desktop shortcuts for "Switchboard" and "Switchboard Listener"

2. **Start the Application**
   - Keep Unreal Editor running with the project open
   - Launch Switchboard from the desktop shortcut
   - Create a new config (Configs → New Config)
   - Run Switchboard Listener from desktop or Tools → Listener

3. **Configure nDisplay**
   - Add Device → nDisplay Cluster
   - Click "Populate"
   - Choose `ND_Deep_Space_8K.uasset`
   - Click the Plugin-Icon in the table header to connect to listener
   - Click "Start all connected Display devices"

## Configuration

### Pharus Tracking Configuration

Configuration file: `Project/Config/AefConfig.ini`

- Loaded at startup by the AefUtility plugin; restart after changes.
- Use a different file with the command-line argument `-dsConfig=<path>` (relative paths start at the project directory, e.g. in `project.json` → `args`).
- Packaged builds contain the file automatically at `Build/Windows/DeepSpaceStarter/Config/AefConfig.ini`, where it can be edited on site.

See the [AefPharus Plugin Documentation](DeepSpaceStarter/Project/Plugins/AefPharus/README.md) for detailed configuration options.

### Pharus Simulator

Preconfigured simulators are available in `DeepSpaceStarter/Pharus/DeepSpace/`:

- `TrackLinkSimulator_MultiCast_Floor/` - TrackLink multicast simulator
- `TrackLinkSimulator_UniCast_Floor/` - TrackLink unicast simulator
- `pharus-rec-sim-v2.4.0-s0-release_MultiCast_Floor/` - Pharus recording simulator (multicast)
- `pharus-rec-sim-v2.4.0-s0-release_UniCast_Floor/` - Pharus recording simulator (unicast)

### nDisplay Configuration

Configuration files in `Content/DeepSpace/Switchboard/`:

- `ND_Deep_Space_8K.uasset` - Deep Space 8K configuration (wall + floor)
- Exported config: `Deploy/Switchboard/ND_Deep_Space_8K.ndisplay` (re-export after every change to the asset)

> **Important:** Do not modify the Deep Space nDisplay configurations. Changes may cause the project to malfunction in the Deep Space environment.

## Development

### Content Structure

Main content is located in `Content/DeepSpace/`:

- **Maps/** - Level maps for various scenarios
- **Library/** - Reusable Blueprint libraries
- **Materials/** - Materials and shaders
- **Models/** - 3D meshes including projection surfaces
- **Switchboard/** - nDisplay configuration assets

### Switchboard Settings

Recommended settings for local development:

| Setting | Value | Notes |
|---------|-------|-------|
| IP Address | 127.0.0.1 | |
| Render API | DirectX 11 | |
| Render Mode | Mono | Use `Stereo` for 3D output |
| Render Sync Policy | Config | |
| Texture Streaming | Disabled | |

### Learning Resources

If you're new to nDisplay, check out these tutorials:
- [Simple nDisplay Setup in Unreal 5.6](https://www.youtube.com/watch?v=ocSTG1yqTmA)
- [Unreal Engine 5.4 | Improving nDisplay Rendering](https://www.youtube.com/watch?v=tOTzvH0p_cw)
- [Unreal Engine 4.27 In-Camera VFX: Introduction](https://www.youtube.com/watch?v=ebf1rRkYFmU&list=PLZlv_N0_O1gaXvxPtn8_THYN_Awx-VYeu)
- [Unreal Engine 4.27 In-Camera VFX: Switchboard](https://www.youtube.com/watch?v=lJcsB21vhJA)
- [Unreal Engine 4.27 In-Camera VFX: nDisplay Config](https://www.youtube.com/watch?v=5k4tB2mo_vc)

## Production

### Packaging the Project

1. Open Project Launcher (File → Project Launcher)
2. Create a custom profile with an appropriate name
3. Configure build options:
   - Build: Development or Shipping
   - Cooking: "By the book"
   - Cooked Platforms: Windows
   - Cooked Maps: Select all used maps
4. Package to `DeepSpaceStarter/Deploy/Build/`

Packaging compiles the included C++ plugins, so Visual Studio 2022 with the C++ workload has to be installed. The packaging log shows `... is being treated as a code-based project ... Creating temporary .Target.cs files.`; this is expected (see [AefUtility README → Packaging](DeepSpaceStarter/Project/Plugins/AefUtility/README.md#packaging)).

For more details, see [Build Operations Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine?application_version=5.7).

### Deployment Folder Structure

The `Deploy/` folder contains everything needed for Deep Space deployment:

```
DeepSpaceStarter/Deploy/
├── Build/                              # Packaged application
│   └── Windows/
│       ├── DeepSpaceStarter.exe
│       └── DeepSpaceStarter/Config/AefConfig.ini   # Tracking configuration (copied automatically)
├── Switchboard/                        # nDisplay configuration
│   └── ND_Deep_Space_8K.ndisplay
└── project.json                        # Deployment configuration
```

### project.json Configuration

Update `DeepSpaceStarter/Deploy/project.json` for production deployment:

```json
{
    "startMap": "Deep_Space_8K",
    "version": "UE5.7",
    "type": "unreal",
    "executable": "\\Build\\Windows\\DeepSpaceStarter.exe",
    "nDisplay": "\\Switchboard\\ND_Deep_Space_8K.ndisplay",
    "args": ""
}
```

| Field | Description |
|-------|-------------|
| `startMap` | Name of the level to load at startup |
| `version` | Unreal Engine version (UE5.7) |
| `type` | Project type (`unreal`) |
| `executable` | Path to packaged executable (relative to Deploy/) |
| `nDisplay` | Path to nDisplay configuration (wall + floor, relative to Deploy/) |
| `args` | Additional command line arguments (optional) |

### Handover / Deployment Package

For Deep Space deployment, zip the entire `Deploy/` folder with a descriptive filename:

**Naming Convention:**
```
[ProjectName]_[Date]_[2D|3D]_[Version].zip
```

**Examples:**
- `DeepSpaceStarter_2026-02-05_2D_v1.0.zip`
- `MyProject_2026-03-15_3D_v2.1.zip`
- `ArsExhibition_2026-04-20_3D_final.zip`

**Package Contents:**
```
Deploy/
├── Build/          # Packaged application (Windows/)
├── Switchboard/    # nDisplay configuration (.ndisplay)
└── project.json    # Deployment configuration
```

**Checklist before handover:**
- [ ] `project.json` updated with correct `startMap` and `version`
- [ ] nDisplay configuration exported to `Switchboard/`
- [ ] Application packaged to `Build/Windows/`
- [ ] `Build/Windows/DeepSpaceStarter/Config/AefConfig.ini` exists and contains the production values
- [ ] ZIP filename includes 2D or 3D mode
- [ ] ZIP filename includes unique version/date

## Important Notes

- **NO Fixed Framerate:** disable fixed framerate to avoid cluster desyncs
- **nDisplay Actor:** Always include in maps intended for Deep Space playback
- **DirectX 11:** Support for DirectX 11 rendering in UE 5.7 (DirectX 12 may cause applications to not launch in DeepSpace)
- **Stereoscopic 3D:** Set Render Mode to `Stereo` for active 3D output
- **Configuration Export:** When modifying DeepSpace nDisplay actors, export the `.ndisplay` file to `/Switchboard/`

### Performance Recommendations

For optimal performance in Deep Space:

| Setting | Recommended Value |
|---------|-------------------|
| Ambient Occlusion | Disabled |
| Anti-Aliasing | FXAA |
| Dynamic Global Illumination | None |
| Reflection Method | None |
| Shadow Map Method | Shadow Maps |

## License

MIT License - See [LICENSE](LICENSE) for details. The license applies to the whole repository, including the AefPharus and AefUtility plugins.

## Credits

**Developed by:** Ars Electronica Futurelab

**Website:** https://ars.electronica.art/futurelab/

## Support

For technical support or questions:

- [Ars Electronica Futurelab](https://ars.electronica.art/futurelab/)
- [Unreal Engine nDisplay Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ndisplay-overview-for-unreal-engine?application_version=5.7)
- [Switchboard Quick Start Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/switchboard-quick-start-for-unreal-engine?application_version=5.7)
