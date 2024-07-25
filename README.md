# Unreal Engine - DeepSpace Starterkit
## Table of Contents
- [Unreal Engine - DeepSpace Starterkit](#unreal-engine---deepspace-starterkit)
  - [Table of Contents](#table-of-contents)
  - [Quick links](#quick-links)
  - [Overview](#overview)
    - [Default DeepSpace Views](#default-deepspace-views)
      - [Character Connected to the nDisplay Actor](#character-connected-to-the-ndisplay-actor)
      - [Camera Connected to the nDisplay Actor](#camera-connected-to-the-ndisplay-actor)
      - [Cinematic Actor Camera Connected to the nDisplay Actor](#cinematic-actor-camera-connected-to-the-ndisplay-actor)
    - [Development with UE pharus and pharus Simulator](#development-with-ue-pharus-and-pharus-simulator)
  - [Folder Structure](#folder-structure)
  - [Configuration](#configuration)
  - [Quickstart Project with Switchboard](#quickstart-project-with-switchboard)
  - [Switchboard Settings](#switchboard-settings)
  - [Development](#development)
  - [Production](#production)
    - [Package project](#package-project)
    - [Update project.json and project-test.json](#update-projectjson-and-project-testjson)
    - [Zip project](#zip-project)
  - [Notes](#notes)
    - [Important Notes](#important-notes)
    - [Notes on Unreal Engine Project Settings 4 \& 5](#notes-on-unreal-engine-project-settings-4--5)

## Quick links 
Download UE DeepSpace Starter (Zips). [Deprecated] version should still work but will are not maintained anymore.
* [[Deprecated] UE 4.27 DeepSpace Starter](https://codeload.github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/zip/refs/heads/UE-4.27-DeepSpace-Starter)
* [[Deprecated] UE 5.0 DeepSpace Starter](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.0-DeepSpace-Starter.zip)
* [[Deprecated] UE 5.1 DeepSpace Starter](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.1-DeepSpace-Starter.zip)
<!--- PHARUS is included in the UE-5.3 starter kit
Download UE Pharus DeepSpace Plugin
* [UE 4.27 Pharus Lasertracking](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-PharusLasertracking/archive/refs/heads/UE-4.27-Pharus-Lasertracking-Plugin.zip)
* [UE 5.0 Pharus Lasertracking](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-PharusLasertracking/archive/refs/heads/UE-5.0-Pharus-Lasertracking-Plugin.zip)
* [UE 5.1 Pharus Lasertracking](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-PharusLasertracking/archive/refs/heads/UE-5.1-Pharus-Lasertracking-Plugin.zip)
--->

## Overview
The UE-DeepSpace Starterkit is an Unreal Engine template designed specifically for the [Ars Electronica](https://ars.electronica.art/news/en/) museum's [DeepSpace](https://ars.electronica.art/center/en/exhibitions/deepspace), which features two large projection screens. The template supports [Unreal Engine](https://www.unrealengine.com/) versions 4.27.3 to 5.3.2 and includes [nDisplay](https://docs.unrealengine.com/4.26/en-US/WorkingWithMedia/nDisplay/Overview/) configuration files for both DeepSpace and standard PC/laptop (Devlopment & Testing) setups.

The project templates share the same structure, with main folders located in "Content/DeepSpace":

* Cinematics: Example cinematics for the CinematicMap
* Core: Blueprints for characters, GameModeBases, and ConnectorActorHelpers
* Maps: Four maps for various scenarios
* Settings: Configuration files for DeepSpace devices
* Switchboard: nDisplay configuration files in "*.uasset" format and a mesh for floor and wall projection

### Prerequisites
* Install *VaRest* from the Epic Games Marketplace. *VaRest* is used by pharus tracking.

### Default DeepSpace Views
#### Character Connected to the nDisplay Actor
* The character/pawn view is controlled using a gamepad for movement and view
![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-CharacterMap.png)

#### Camera Connected to the nDisplay Actor
* A standard camera is connected, allowing movement and rotation control via its transformation
  ![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-CinemaMap.png)

#### Cinematic Actor Camera Connected to the nDisplay Actor
DeepSpace View as a [In-Camera VFX](https://docs.unrealengine.com/5.1/en-US/in-camera-vfx-overview-in-unreal-engine)
* Requires more performance when used for the entire screen
* Utilize camera settings like FOV, aperture, etc.
![Switchboard View Camera Map](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-CameraMap.png)


### Development with UE pharus and pharus Simulator
2 Views (normal wall + floor view)
![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-Pharus-StartMap-1.png)

2 Views (wall = front view, floor = top down view)
![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-Pharus-StartMap-2.png)

2 Views (wall = front view, floor = top down view)
![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Switchboard-View-Pharus-StartMap-3.png)

## Folder Structure
    \Build                      -> packaged application (empty if not packaged)
    \DeepSpaceStarter           -> Starter Kit
    \Switchboard                -> must be untouched
    \project.json               -> set your startmap and the path to the application exe of your packaged game (relative).  

## Configuration
To start a new project, either use one of the included templates based on a [UE nDisplay template](https://docs.unrealengine.com/5.1/en-US/) or migrate the Switchboard folder into an existing project. Follow the Unreal Engine guide on [Add nDisplay to an Existing Project](https://docs.unrealengine.com/4.26/en-US/WorkingWithMedia/nDisplay/AddToExisting/).

When adding nDisplay to an existing project, create an empty root actor at position (0, 0, 0) in a level. Add the nDisplay files as child objects from the "/Content/DeepSpace/Switchboard" folder and reset the transform node to position (0, 0, 0). Ensure the nDisplay Actors have the "Actor Hidden In Game" property checked (Details Panel). Adjust the "Preview Screen Percentage" in the Editor Preview tab to suit your needs and performance, with the default value set to 0.25.

* The nDisplay configuration files in the editor display settings for either desktop or DeepSpace, differing by IP address(es), projection surfaces, and viewport size. ![nDisplay DeepSpace configuration](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/nDisplay_DeepSpace.png)
* Make changes to the panel cluster for the desktop files for testing purposes if required. Do not modify the DeepSpace nDisplay configurations, as this may cause the project to malfunction in DeepSpace.
* Configuration files are loaded as ".uasset" in Unreal Engine. If you want to customize by extending a configuration file, ensure you export the configuration file to "/DeepSpaceStarter/Switchboard/". ![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/nDisplay_export.png)
* For further information on configuration possibilities, refer to the [nDisplay 3D Config Editor](https://docs.unrealengine.com/5.1/en-US/ndisplay-3d-config-editor-in-unreal-engine) documentation.

## Quickstart Project with Switchboard
* Download [UE 5.1 DeepSpace Starter](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/archive/refs/heads/UE-5.0-DeepSpace-Starter.zip)
* Open "/DeepSpaceStarter/DeepSpaceStarter.uproject"
* Rebuild all Maps in "DeepSpaceStarter/Content/DeepSpace/Maps"
* Editor Preferences
  * Reinstall Dependencies if necessary and create shortcuts on the desktop for "Switchboard" and "Switchboard Listener"
  * Switchboard/Virtual Environment Path is the location of Switchboard
  * Follow those steps if you run into any problems [Prerequisites](https://docs.unrealengine.com/5.0/en-US/switchboard-quick-start-for-unreal-engine)
* Run UE Project (DeepSpaceStarter) in background
* Start Switchboard (Desktop Shortcut or "\Engine\Plugins\VirtualProduction\Switchboard\Source\Switchboard\Switchboard.bat")
* Switchboard
  * Configs/New config and click ok (should be auto filled out)
  * Run Switchboard listener from the desktop or "Tools/Listener"
  * Choose the "CinemaMap" in the "Level" select option
  * Add Device/nDisplay Cluster
    * Click "Populate"
    * Choose "nDisplay_Desktop.uasset"
  * Click the Plugin-Icon in the table header (connects to listener)
  * nDisplay Monitor gets updated with the current nDisplay configuration
  * Click "Start all connected Display devices"

## Switchboard Settings
Additional information can be found at [Switchboard Settings Reference](https://docs.unrealengine.com/5.0/en-US/switchboard-settings-reference-for-unreal-engine/)
* Switchboard
  * IP Address: 127.0.0.1
* Multi User Server
  * Auto Launch: [ ]
  * Clean History: [ ]
  * Auto Build: [ ]
  * Auto Endpoint: [ ]
  * Unreal Multi-user Server Auto-join: []
* nDisplay Settings
  * Texture Streaming: [ ]
  * Render API: dx 11
  * Render Mode: Mono
  * Render Sync policy: Config
  * Minimize Before Launch: [ ]
  * Unicast Endpoint: 127.0.0.1:0
  * node_wall
    * IP address: 127.0.0.1
    * Unicast Endpoint: 127.0.0.1:0


## Development
If there is no nDisplay experience it is recommended to have a look at the following tutorials:
* Overview [Unreal Engine 4.27 In-Camera VFX Tutorials | 1: Introduction](https://www.youtube.com/watch?v=ebf1rRkYFmU&list=PLZlv_N0_O1gaXvxPtn8_THYN_Awx-VYeu)
* Switchboard [Unreal Engine 4.27 In-Camera VFX Tutorials | 2: Switchboard](https://www.youtube.com/watch?v=lJcsB21vhJA&t=11s) 
* NDisplay Config [Unreal Engine 4.27 In-Camera VFX Tutorials | 3: nDisplay Config](https://www.youtube.com/watch?v=5k4tB2mo_vc&list=PLZlv_N0_O1gaXvxPtn8_THYN_Awx-VYeu&index=3)

## Production
For productive use in DeepSpace, an *.ndisplay configuration file is required ("DeepSpaceStarter/Switchboard/*.ndisplay). Export this file from the Unreal Engine Editor's nDisplayCluster Blueprint when changes are made to the Blueprints.

### Package project
Once the project configuration and setup are complete, it can be uploaded by technicians from the Ars Electronica museum staff or authorized individuals. Follow these recommended steps:
* Open the Project Launcher ![nDisplay configuration export](https://github.com/ArsElectronicaFuturelab/UE-DeepSpace-Starter/blob/main/Files/Build-Step-Project-Launcher.png)
* Create custom profile
* Choose a appropriate  name for the new profile (each project has a profile)
* The project should be automatically set
* Build options can be set to "development" or "shipping"
* Cooking should be set to "By the book"
  * Cooked Platforms: Windows
  * Cooked Cultures: en (or project related cultures)
  * Cooked Maps: choose all used maps in the final project
* Package and store locally
  * Choose Folder "DeepSpaceStarter/Build"
* No archive, deployment, or launch is required
* find additional information on [Build Operations: Cook, Package, Deploy and Run](https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Deployment/BuildOperations/)

### Update project.json and project-test.json
Update the project.json in the root folder based on the nDisplay_Configuration file used in the project:
* change the "startMap" content, which should be loaded at application start
* Keep the version unchanged if you used the DeepSpace-Starter template
  * Available Options:
    * UE4.27
    * UE5.00
    * UE5.01
    * UE5.03
* Choose the the nDisplay configuration file from the folder "/DeepSpaceStarter/Switchboard" and replace  the content in" wall/nDisplay" and "floor/nDisplay"

### Zip project
The Zip Root folder is located in "/DeepSpaceStarter/". In the DeepSpace environment, only the following files and folders are required:
  * /Build
  * /Switchboard
  * /project.json

## Notes 
### Important Notes
  * Use a fixed framerate of 30 or 60 FPS to avoid syncing issues
  * Include the nDisplayCluster Actor in the map intended for DeepSpace playback
  * when "nDisplay_DeepSpace", "nDisplay_Camera_DeepSpace", "nDisplay_Remote_DeepSpace", "nDisplay_DeepSpace_2_Views" are changed -> 
    * Export the *.ndisplay file type from the nDisplay actor blueprint (double-click the nDisplay actor, toolbar, export, save as *.ndisplay)
    * Set export destination to "\DeepSpaceStarter\Switchboard"
    * Failure to follow these steps may result in unexpected application behavior
  * The folder structure must be followed
  * Currently Unreal Engine with nDisplay configuration and stereoscopic output requires DX11

### Notes on Unreal Engine Project Settings 4 & 5
* Unreal Engine 5's nDisplay Template is not optimized for large screen projections. Disable unnecessary settings for better performance.
* Recommended project settings for optimal FPS:
  * Disable Ambient Occlusion (especially important for UE5)
  * Set Anti-Aliasing Method to FXAA (especially important for UE5)
  * Set Dynamic Global Illumination Method to None
  * Set Reflection Method to None
  * Set Shadow Map Method to Shadow Maps
    
