# About
This folder contains the source json files for etee svr devices that work with the current version of the driver. (Ready for calibration)

There should only be one source file for each device. 

These are jsons are uncalibrated.

## Documentation

Documentation for the JSON file is provided in the SVR HDK. The documentation is inncomplete but here is some additional infor.

- "model_number": The name and version of the physical hardware device (eg etee-tracker-1.0). This name should be added to "driver.vrresources" to add a reference to the devices icon paths.

- "device_class": This can be set to "hmd", "controller", "generic_tracker", "tracking_reference" or "display_redirect". This is used by SVR to determine how the device is rendered. On the driver level this would set the Prop_DeviceClass_Int32 property. 

It is also used to dermine which statusicons path is used in the driver.vrresources file of the drivers resource folder. If the "driver.vrresources" has a path specified for the type of device, it will be used rather than the "model_number" path. 

- "tracked_controller_role": This is used to specify which hand the controller is held in ("left_hand") ("right_hand") or leave blank ("") if the device doesn't have a specific hand such as a tracker. Equivalent to setting Prop_ControllerRoleHint_Int32 in a custom driver for controllers only



