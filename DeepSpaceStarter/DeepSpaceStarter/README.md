# OVERVIEW
This Deep Space Starterkit contains Unreal Engine NDisplay configurations for the deepspace in ARS Electronica. Supported are projects from UE 4.27.3 - 5.0.3  For testing purposes, a desktop configuration is also included in the package.

The projects have all the same structure. The main folders are:
    * Cinematics -> Cinematics which are used in the CinematicMap + CameraMap
    * Core -> Blueprints as Characters, GameModeBases, ConnectorActorHelpers
    * Maps -> 4 different Maps
    * Switchboard -> there are four configuration files in the form of the type "*.uasset" and one Mesh for the floor and wall projection

# CONFIGURATION
Either you start the new project directly with one of these templates, which are based on a UE nDisplay template, or you migrate the the Switchboard folder into your project.

It is recommended to create an empty root actor in a level at position (0, 0, 0). The nDisplay files can be added as child objects from the "Plugin/Content/Switchboard" folder. The transform node must also be reset to position (0, 0, 0). After that you should check that the nDisplay Actors in the project has the property "Actor Hidden In Game" checked (Details Panel). In the Editor Preview tab, you can also adjust the "Preview Screen Percentage" to suit your needs and performance. By default this is set to "1.00".

    * If you open the nDisplay configuration file in the editor, you will see the respective settings for the desktop or deepspace. These differ by IP address(es), projection surfaces and viewport size.
    * In the panel cluster, changes can be made for the desktop area for testing purposes, if required. The "nDisplay_DeepSpace.uasset" file must not be changed.
    * The confiugration files are loaded as *.uasset. For the type *.ndisplay this file type can be exported in the editor. Both types can be used for starting the application with the Uneral Engine Switchboard Tool. 

# PRODUCTION
It is mandatory to use an *.ndisplay file for productive use in Deepspace. As mentioned above, this file has to be exported inside of the Unreal Engine Editor of the nDisplayCluster Blueprint, when changes are made on these Blueprints. 

Currently the instalaltion has to be done by a technician from ARS Electronica / Future. For the final integration and also for testing an application a "Packaged Project" (*.exe file) is needed. Starting *.exe Files in a ClusterNetwork is not possible from Switchboard. This can only be done in the DeepSpace with customized starting scripts.

# DEVELOPMENT
If there is no nDisplay experience it is recommended to have a look at the following tutorials:
    * Switchboard - https://www.youtube.com/watch?v=lJcsB21vhJA&t=11s 
    * NDisplay Config - https://www.youtube.com/watch?v=5k4tB2mo_vc&list=PLZlv_N0_O1gaXvxPtn8_THYN_Awx-VYeu&index=3

# FOLDER STRUCTURE
    \Build                      -> packaged application
    \UE_ProjectName (optional)  -> for Testing recommended
    \Switchboard                -> must be untouched
    \project.json               -> set your startmap and the path to the application exe of your packaged game (relative). 

# IMPORTANT NOTES
    * use a fixed framerate with 30 or 60 FPS (syncing issues)
    * nDisplayCluster Actor must be in the Map, which should be played in DeepSpace
    * when nDisplay_DeepSpace, nDisplay_Camera_DeepSpace, nDisplay_Remote_DeepSpace are changed -> 
        * an export of file type *.ndisplay hast to be done in the ndisplay actor blueprint (double click on ndisplay actor, Toolbar, export, save as *.ndisplay)
        * export destination \Switchboard
        * otherwise the application will not work as expected
    * The folder structure must be followed.
    * If you want to use a stereoscopic output -> select DX11

# NOTES Unreal Engine Project Settings 4 & 5
    * The Unreal Engine 5 nDisplay Template is not optimized for large screen projections. Please make sure to disable settings you don't need and.
    * Project Settings Recommandations for best FPS outcome
        * Disable Ambient Occlusion (important UE5)
        * Select Anti-Aliasing Method -> FXAA (important UE5)
        * Select Dynamic Global Illumination Method -> None
        * Reflection Method -> None
        * Shadow Map Method -> Shadow Maps
    