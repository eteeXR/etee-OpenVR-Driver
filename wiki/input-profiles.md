# Input Profiles

The input profiles are accessed by the driver and used to tell SteamVr what inputs are available by that device. 

Each device should have at least one input profile but can have more than one for different modes.

For example the HTC tracker driver has a main input profile that is referenced in the json file. This input profile has sub input profiles that become active when the manage vive trackers setting is changed. A similar method can be used for the etee tracker.

The etee controller has only one input profile that is reference in the custom driver

## Documentation

[Input profile documentation](https://github.com/ValveSoftware/openvr/wiki/Input-Profiles)

The official documentaitons seams to be incomplete and does not included all the parameters that can be used. 

- "priority", This number is used by SVR to priorities which device is used as your main controllers. (Unsure whether the priority is accending or decending)