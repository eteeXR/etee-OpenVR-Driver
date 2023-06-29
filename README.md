# etee Drivers
This repository contains OpenVR/SteamVR driver source files and builds for TG0 etee devices.

___

## Deployable Drivers

>Device driver folders ready to be used in SteamVR

Located in '/deployable' are the following built drivers:

1) eteeTracker (Name = 'etee'): This is a resource only driver for the eteeTracker.

2) eteeController (Name = 'etee_controller'): This is a custom OpenVR driver implementation therefore it also contains a dll in the bin folder. 


### How to Use Driver

#### Method 1: Move Folder
Copy the desired driver folder to the SteamVR drivers folder usually located in "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers"

#### Method 2: Add Driver Loation To OpenVR Path
Add the path location of the desired driver folder to the "openvrpaths.vrpath" file ensuring proper JSON formatting with double dashes and correct comma placement. Use a vlidator to check the JSON format is correct as SteamVR will overwrite this if it is not causing the driver to not be activated. File usually located in "C:\Users\username\AppData\Local\openvr"

___