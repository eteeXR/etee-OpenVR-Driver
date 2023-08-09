# Etee Device OpenXR Driver Specification

This document aims to provide a clear description of the ideal SteamVR driver experience that users should have with etee devices (including the tracker and controller) and the associated OpenVR driver functionality that is required to achieve this experience. We want the experience of using etee devices with SteamVR to be completely seamless and intuitive, allowing customers to use etee as they would any other native SteamVR device.

<br>

---
## Objectives
1. Professional and seamless experience: User should only have to download the etee program from Steam and the device should “just work” when turned on. The user should not need to dive into driver settings and files to get it to work.

2. Intuitive usage: The display of controllers and trackers in the SteamVR UI should be clear and match those of the system standard and the mental model of SteamVR. Users should be able to easily use and change bindings for different inputs of the controller and tracker in games.

3. Maintainable and stable: The drivers and associated software should be reliable, stable and easy to push updates to for the end customer. The driver should be well documented allowing new developers to contribute and join the project easily.

<br>

---
## OpenVR device driver overview

**etee Tracker:** Same behaviour as standard SteamVR tracker such as the HTC tracker. The etee Tracker has one input button (the Power Button) and is a tracker so reports pose.

- Uses a resource only driver.
- One input button.
- Device type is "generic tracker" or equivalent.
- None handed. Device isn't porgrammed as beinging strictly left or right handed.
- The tracker mode can be changed in the manage vive trackers panel to different body parts such as "right shoulder" or "held in left hand".

**etee Controller:** A custom virtual driver that receives input data through a serial connection with the eteeController Dongle. The device has a range of inputs including ones directly recevied from the controller, such as trackpad position" and ones calculated by the driver based upon the raw input data such as skeleton pose.

- Uses a custom OpenXR driver along with the static resources since skeleton pose is not supported by resource only.
- All inputs from the etee controller + derived inputs.
- Reported tracked pose is derived from another steamvr connected tracker.
- Is handed, Each device is specifically right or left handed.

<br>

---
## Controller Tracker Relationship
To use the etee trackers to track the etee controller the following steps will be taken.

1. The trackers are paired to the headset or external SVR dongles and both controllers paired to a single eteeController Dongle (Which happens automatically). 

2. The trackers act as generic trackers bound to the headset or USB key (as standard). To use them with the controllers, they shall be put into "held in hand mode" in "manage vive trackers" and physically connected to the controllers (Via the USB C port). When the controller detects a tracker is connected, it sends this data (Along with the input API data) to the custom driver. The driver then knows a tracker is connected so it checks through the list of SVR connected devices until it finds one which is set to "held in hand mode" of the corresponding hand for that device. If one is found then it will use the tracking data of that tracker to update it's own pose.

3. As soon as the devices are physically disconnected the trackers will stop being used to update the controllers position. The user can then easily set them to a different mode eg right shoulder for other gaming experiences. 

This aims to be a SteamVR style way of implementing the driver while keeping everything standard and reliable. It allows the trackers to be easily used for any use case and only one setting needs to be changed to allow them to be used to track the controllers.


<br>

---
## Device behaviour in SVR GUI
In the case where all drivers for devices are installed and working how should what should the SVR GUI display.

<br>

|Tracker Device State|SVR GUI|
|---|---|
|Device off, not paired|Tracker icon not shown(standard)|
|Device off, paired|Grey tracker icon shown (standard)|
|Device on, pairing mode|Device can be paired through SVR "add controller" (standard)|
|Device on, paired, tracking active|Tracker icon solid highlight (standard)|
|Device on, paired, not tracking|Tracker icon flashing highlight (standard)|

Note: The trackers SVR GUI behaviour is unaffected by the it's physical connection to an etee controller or the controller driver. 

**ToDo:** verify this is the correct standard behaviour for an SVR device. Are they greyed out if bound but not turned on?

<br>

|Controller Device State|SVR GUI|
|---|---|
|eteeController dongle not connected|Controller icon greyed out|
|eteeController dongle connected, left/right/both controllers connected to dongle, no tracker physically connected or no suitable tracker available in SVR|Controller icon flashing highlight|
|eteeController dongle connected, left/right/both controllers connected to dongle, tracker physically connected and suitable tracker available in SVR|Controller icon solid highlight|

Note: Charging warning symbol is an additional icon added on top of the current device icon.


**ToDo:** Verify that if a controller is paired but turned off (Shows as grey in UI) that other controllers can be connected and will be used instead. If it's the case that the etee controller icons always show in the SVR UI window but are deactivated when the dongle is removed, we want to make sure that users can connect another set of controllers without having to remove the driver.

**ToDo** Find out all states of SVR icons. Are there any additional ones other than, Grey, Flashing highlight, solid highlight

<br>

---
## SVR Device Rendermodels

The Rendermodels shall reflect that of the physical controllers. The controller and tracker are independent devices that can be attached or separated. As such the virtual version should mimick this to help the user easily understand how to use both devices. 

- The etee Tracker rendermodel should just be the tracker (with the correct tracking origin point).

- The etee Controller rendermodel should just be the controller. When the tracker is physically connected to the controller. No render model changes shall happen. Since the controller driver will use the tracker to update it's own position the two rendermodels shall allign as they are in the real world. In this case care should be taken to ensure the two rendermodels look good when alligned next to each other avoiding clipping and artefacts with perfectly alligned mesh faces.

<br>

---
## Device origin points

To simplify transform setting and allow users to easily use the etee tracker with other objects to be tracked, the origin point shall be placed at the centre of top surface of the USBC port on the tracker and the corresponding point on the controller. This means that by setting the transform of the controller to match that of the tracker exaclty, they shall allign perfectly as in the real world.

This will need to be applied to the rendermodels, driver and json files (developed in a separated repo).

<br>


---
## Hand Skeleton Behaviour
- Finger joint poses shall be set based upon input from the etee controller sensors using custom skeletal animations provided in glb format. 
- The wrist pose shall be dermined by the initial pose of the wrist in the custom skeletal animation file. The wrist shall not be animated.


---
## Edge case behaviours
How should the devices and drivers behave in unusual configurations.

*Two etee dongles CON connected to PC:* One dongle is ignored as only one pair of etee controllers can be used at a time. 

<br>

---
## Dev Tools

The etee controller custom driver requires a simple method of displaying data about the drivers state in a window when running in dev mode. This could be achieved through using a SteamVR overlay window or launching a terminal window when running in debug mode.

<br>

---

## etee Device Reference

*etee Tracker:* TG0's etee tracker device

*etee Controller:* TG0's etee controller device

*eteeController dongle:* TG0's dongle with controller communication firmware installed

*eteeSteamVR dongle:* TG0's dongle with SVR watchman firmware installed

<br>

---
## Driver Release Versioning

[Semantic versioning](https://semver.org/) shall be used to name the releases of the driver. This follows the naming scheme Major.Minor.Patch (See link for more info). During pre-release stages the "-alpha" is appended. Meaning the first pre-release version would be named "1.0.0-alpha" which is incremented until the first public release which shall be named "1.0.0". Please see link for more info.

<br>

## Development Guidlines
For new code additions to the code base please follow the [Google C++ style guide](https://google.github.io/styleguide/cppguide.html) where possible.


---
## Driver Steam VR Deployment
The driver shall be deployed through steam and be easily updateable by the user. All setup should be automatic allowing the user to simply download the driver and begin using the etee system immediately.


<br>

---
## Document Terminology
SVR = SteamVR