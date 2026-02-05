# UE-DeepSpace-Starter

Unreal Engine 5.7 project template for interactive installations with Pharus laser tracking and nDisplay cluster rendering.

## Overview

UE-DeepSpace-Starter is a Blueprint-based project template developed by Ars Electronica Futurelab for the [Deep Space 8K](https://ars.electronica.art/center/en/exhibitions/deepspace) installation at the Ars Electronica Center. It integrates Pharus laser tracking hardware with Unreal's nDisplay system for multi-projector setups on floor and wall surfaces.

**This is a Blueprint project** with included C++ plugins. All core functionality is accessible via Blueprints without compilation. The AefPharus plugin is included with pre-compiled binaries for Pharus laser tracking.

## Requirements

- **Unreal Engine**: 5.7
- **Platform**: Windows 64-bit / Linux

## Quick Links

Download UE DeepSpace Starter (Zips). [Deprecated] versions should still work but are not maintained anymore.

| Version | Status | Download |
|---------|--------|----------|
| UE 5.7 | **Current** | This repository |
| UE 5.3 | Experimental | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.3-DeepSpace-Starter.zip) |
| UE 5.1 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.1-DeepSpace-Starter.zip) |
| UE 5.0 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.0-DeepSpace-Starter.zip) |
| UE 4.27 | Deprecated | [Download](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-4.27-DeepSpace-Starter.zip) |

## Project Structure

```
UE-DeepSpace-Starter/
├── DeepSpaceStarter/
│   ├── Project/
│   │   ├── DeepSpaceStarter.uproject  # Unreal Engine project file
│   │   ├── Content/                   # Game assets and blueprints
│   │   │   └── DeepSpace/             # Deep Space specific content
│   │   │       ├── Library/           # Blueprint libraries
│   │   │       ├── Maps/              # Level maps
│   │   │       ├── Materials/         # Materials and shaders
│   │   │       ├── Models/            # 3D models
│   │   │       └── Switchboard/       # nDisplay configuration assets
│   │   ├── Config/                    # Project configuration
│   │   │   ├── AefConfig.ini          # Pharus tracking configuration
│   │   │   └── Default*.ini           # Engine/Input/Game settings
│   │   └── Plugins/                   # C++ plugins (optional)
│   │       └── AefPharus/             # Pharus laser tracking plugin
│   ├── Pharus/                        # Pharus simulator tools
│   │   └── DeepSpace/                 # Preconfigured simulators
│   │       ├── TrackLinkSimulator_*/  # TrackLink simulators (multicast/unicast)
│   │       └── pharus-rec-sim-*/      # Pharus recording simulators
│   ├── Deploy/                        # Deep Space deployment folder
│   │   ├── Build/                     # Packaged application (generated)
│   │   ├── Switchboard/               # nDisplay configuration files
│   │   │   └── nDisplay_Deep_Space_8K.ndisplay
│   │   └── project.json               # Deployment configuration
│   ├── project.json                   # Development configuration
│   └── project-test.json              # Test configuration
├── ReadmeMedia/                       # Documentation assets
└── LICENSE
```

## Plugins

### AefPharus

C++ plugin for Pharus laser tracking integration (included with pre-compiled binaries).

**Features:**
- Multi-instance tracking (Floor + Wall)
- 3D room mapping (Floor + 4 walls) 
- nDisplay cluster synchronization
- Thread-safe operation
- Blueprint API for runtime control

**Documentation:** See [AefPharus README](DeepSpaceStarter/Project/Plugins/AefPharus/README.md) for detailed configuration.

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
   git clone https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter.git
   cd UE-DeepSpace-Starter
   ```

   Or download as ZIP from the [Quick Links](#quick-links) section.

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
   - Choose `nDisplay_Desktop.uasset` for local testing
   - Click the Plugin-Icon in the table header to connect to listener
   - Click "Start all connected Display devices"

## Configuration

### Pharus Tracking Configuration

Configuration file: `Project/Config/AefConfig.ini`

See the [AefPharus Plugin Documentation](DeepSpaceStarter/Project/Plugins/AefPharus/README.md) for detailed configuration options.

### Pharus Simulator

Preconfigured simulators are available in `DeepSpaceStarter/Pharus/DeepSpace/`:

- `TrackLinkSimulator_MultiCast_Floor/` - TrackLink multicast simulator
- `TrackLinkSimulator_UniCast_Floor/` - TrackLink unicast simulator
- `pharus-rec-sim-v2.4.0-s0-release_MultiCast_Floor/` - Pharus recording simulator (multicast)
- `pharus-rec-sim-v2.4.0-s0-release_UniCast_Floor/` - Pharus recording simulator (unicast)

### nDisplay Configuration

Configuration files in `Content/DeepSpace/Switchboard/`:

- `nDisplay_Desktop.uasset` - Local development configuration (single screen)
- `nDisplay_DeepSpace.uasset` - Deep Space 8K production configuration

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
| Render API | DirectX 12 | |
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

For more details, see [Build Operations Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine?application_version=5.7).

### Deployment Folder Structure

The `Deploy/` folder contains everything needed for Deep Space deployment:

```
DeepSpaceStarter/Deploy/
├── Build/                              # Packaged application
│   └── Windows/
│       └── DeepSpaceStarter.exe
├── Switchboard/                        # nDisplay configuration
│   └── nDisplay_Deep_Space_8K.ndisplay
└── project.json                        # Deployment configuration
```

### project.json Configuration

Update `DeepSpaceStarter/Deploy/project.json` for production deployment:

```json
{
    "startMap": "Deep_Space_8K",
    "version": "UE5.7",
    "type": "unreal",
    "wall": {
        "executable": "\\Build\\Windows\\DeepSpaceStarter.exe",
        "nDisplay": "\\Switchboard\\nDisplay_Deep_Space_8K.ndisplay"
    },
    "floor": {
        "executable": "\\Build\\Windows\\DeepSpaceStarter.exe",
        "nDisplay": "\\Switchboard\\nDisplay_Deep_Space_8K.ndisplay"
    }
}
```

| Field | Description |
|-------|-------------|
| `startMap` | Name of the level to load at startup |
| `version` | Unreal Engine version (UE5.7) |
| `type` | Project type (`unreal`) |
| `wall.executable` | Path to packaged executable (relative to Deploy/) |
| `wall.nDisplay` | Path to nDisplay configuration for wall projection |
| `floor.executable` | Path to packaged executable (relative to Deploy/) |
| `floor.nDisplay` | Path to nDisplay configuration for floor projection |

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
- [ ] ZIP filename includes 2D or 3D mode
- [ ] ZIP filename includes unique version/date

## Important Notes

- **Fixed Framerate:** Use 30 or 60 FPS to avoid syncing issues
- **nDisplay Actor:** Always include in maps intended for Deep Space playback
- **DirectX 12:** Full support for DirectX 12 rendering in UE 5.7
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

MIT License - See [LICENSE](LICENSE) for details.

## Credits

**Developed by:** Ars Electronica Futurelab

**Website:** https://ars.electronica.art/futurelab/

## Support

For technical support or questions:

- [Ars Electronica Futurelab](https://ars.electronica.art/futurelab/)
- [Unreal Engine nDisplay Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/ndisplay-overview-for-unreal-engine?application_version=5.7)
- [Switchboard Quick Start Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/switchboard-quick-start-for-unreal-engine?application_version=5.7)
