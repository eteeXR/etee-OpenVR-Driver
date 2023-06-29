#pragma once
#include <array>
#include <string>

#include "openvr_driver.h"

enum class VRControllerState : int { disconnected, ready, streaming };

struct VRHandedControllerState_t {
  VRHandedControllerState_t() : valid(false), role(vr::TrackedControllerRole_Invalid), state(VRControllerState::disconnected){};
  VRHandedControllerState_t(vr::ETrackedControllerRole role, VRControllerState state) : valid(true), role(role), state(state){};

  vr::ETrackedControllerRole role;
  VRControllerState state;
  bool valid;
};

enum class VRDongleState { connected, disconnected };

struct VRDongleState_t {
  VRDongleState_t() : valid(false){};
  VRDongleState_t(VRDongleState state) : state(state), valid(true){};

  VRDongleState state;
  bool valid;
};

typedef union VRStateEventData_t {
  VRStateEventData_t(){};
  VRHandedControllerState_t handedControllerState;
  VRDongleState_t dongleState;
} VRStateEventData_t;

enum class VRStateEventType : int {
  dongle_state,
  controller_state,
  unknown,
};

struct VRStateEvent_t {
  VRStateEvent_t(VRStateEventType type, VRStateEventData_t data) : type(type), data(data){};
  VRStateEventType type;
  VRStateEventData_t data;
};

struct SystemData_t {
  SystemData_t() {}
  SystemData_t(bool systemClick, bool trackerConnection, float battery, bool batteryCharging, bool batteryChargeComplete)
      : systemClick(systemClick),
        trackerConnection(trackerConnection),
        battery(battery),
        batteryCharging(batteryCharging),
        batteryChargeComplete(batteryChargeComplete){};
  bool systemClick;
  bool trackerConnection;
  float battery;
  bool batteryCharging;
  bool batteryChargeComplete;
};

struct SliderData_t {
  SliderData_t(){};
  SliderData_t(bool touch, float value, bool up, bool down) : touch(touch), value(value), upTouch(upTouch), downTouch(downTouch){};
  bool touch;
  float value;
  float simulated_x;
  bool upTouch;
  bool downTouch;
};

struct ThumbpadData_t {
  ThumbpadData_t(){};
  ThumbpadData_t(float value, float force, bool touch, bool click, float x, float y) : value(value), force(force), touch(touch), click(click), x(x), y(y){};
  float value;
  float force;
  bool touch;
  bool click;
  float x;
  float y;
};

struct ProximityData_t {
  ProximityData_t(){};
  ProximityData_t(bool touch, bool click, float value) : touch(touch), click(click), value(value){};
  bool touch;
  bool click;
  float value;
};

struct GestureData_t {
  GestureData_t(){};
  GestureData_t(float gripPull, float gripForce, bool gripTouch, bool gripClick, float pinchValue, bool pinchClick, float pointValue, bool pointClick)
      : gripPull(gripPull),
        gripForce(gripForce),
        gripTouch(gripTouch),
        gripClick(gripClick),
        pinchAPull(pinchAPull),
        pinchAClick(pinchAClick),
        pinchBPull(pinchBPull),
        pinchBClick(pinchAClick),
        pointBClick(pointBClick){};
  float gripPull;
  float gripForce;
  bool gripTouch;
  bool gripClick;
  float pinchAPull;
  bool pinchAClick;
  float pinchBPull;
  bool pinchBClick;
  bool pointBClick;
};

struct FingerData_t {
  FingerData_t(){};
  FingerData_t(float pull, float force, bool click, bool touch) : pull(pull), force(force), click(click), touch(touch){};
  float pull;
  float force;
  bool click;
  bool touch;
};

/*
The ranges the this data format should set to.
Fingers: 0-1 incl.
Trackpad x,y: -1 - 1 incl.
Trackpad pressure: 0-1 incl.
Slider: 0-1 incl.
Tracker button: 0-1 incl.
Battery: 0-100 incl.
*/

/// <summary>
/// Strucutre to store all decoded comm data received from serial
/// </summary>
struct VRCommInputData_t {
  VRCommInputData_t(bool isValid) : isValid(isValid){};

  VRCommInputData_t(
      bool isRight,
      SystemData_t system,
      SliderData_t slider,
      ThumbpadData_t thumbpad,
      ProximityData_t proximity,
      GestureData_t gesture,
      std::array<FingerData_t, 5> fingers,
      bool isValid)
      : isRight(isRight), system(system), slider(slider), thumbpad(thumbpad), proximity(proximity), gesture(gesture), fingers(fingers), isValid(isValid){};

  bool isRight;
  SystemData_t system;
  SliderData_t slider;
  ThumbpadData_t thumbpad;
  ProximityData_t proximity;
  GestureData_t gesture;
  std::array<FingerData_t, 5> fingers;
  bool isValid;
};

// Decoding data structure
enum class InputSerialBytePosition : int {
  kSystemButton = 0,  // Byte 0
  kTrackpadClicked = 0,
  kTrackpadTouched = 0,
  kFinger1Clicked = 0,
  kFinger2Clicked = 0,
  kFinger3Clicked = 0,
  kFinger4Clicked = 0,
  kFinger5Clicked = 0,
  kFinger1Touched = 1,  // Byte 1
  kFinger1Value = 1,
  kFinger2Touched = 2,  // Byte 2
  kFinger2Value = 2,
  kFinger3Touched = 3,  // Byte 3
  kFinger3Value = 3,
  kFinger4Touched = 4,  // Byte 4
  kFinger4Value = 4,
  kFinger5Touched = 5,  // Byte 5
  kFinger5Value = 5,
  kTrackpadX = 6,       // Byte 6
  kTrackpadY = 7,       // Byte 7
  kProximityTouch = 8,  // Byte 8
  kProximityValue = 8,
  kSliderTouch = 9,  // Byte 9
  kSliderValue = 9,
  kGripGestureTouched = 10,  // Byte 10
  kGripGestureValue = 10,
  kGripGestureClicked = 11,  // Byte 11
  kProximityClicked = 11,
  kTrackerConnection = 11,
  kIsRightHanded = 11,
  kBatteryCharging = 11,
  kSliderUpTouch = 11,
  kSliderDownTouch = 11,
  kBatteryChargeComplete = 12,  // Byte 12
  kBatteryLevel = 12,
  kPointAClicked = 13,  // Byte 13
  kTrackpadValue = 13,
  kPointBClicked = 14,  // Byte 14
  kGripGestureForce = 14,
  kPinchAClicked = 15,  // Byte 15
  kPinchAValue = 15,
  kPinchBClicked = 16,  // Byte 16
  kPinchBValue = 16,
  kTrackpadForce = 17,  // Byte 17
  kFinger1Force = 18,   // Byte 18
  kFinger2Force = 19,   // Byte 19
  kFinger3Force = 20,   // Byte 20
  kFinger4Force = 21,   // Byte 21
  kFinger5Force = 22    // Byte 22
};

enum class InputSerialBitPosition : int {
  kSystemButton = 0,  // Byte 0
  kTrackpadClicked = 1,
  kTrackpadTouched = 2,
  kFinger1Clicked = 3,
  kFinger2Clicked = 4,
  kFinger3Clicked = 5,
  kFinger4Clicked = 6,
  kFinger5Clicked = 7,
  kFinger1Touched = 0,  // Byte 1
  kFinger1Value = 1,
  kFinger2Touched = 0,  // Byte 2
  kFinger2Value = 1,
  kFinger3Touched = 0,  // Byte 3
  kFinger3Value = 1,
  kFinger4Touched = 0,  // Byte 4
  kFinger4Value = 1,
  kFinger5Touched = 0,  // Byte 5
  kFinger5Value = 1,
  kTrackpadX = 0,       // Byte 6
  kTrackpadY = 0,       // Byte 7
  kProximityTouch = 0,  // Byte 8
  kProximityValue = 1,
  kSliderTouch = 0,  // Byte 9
  kSliderValue = 1,
  kGripGestureTouched = 0,  // Byte 10
  kGripGestureValue = 1,
  kGripGestureClicked = 0,  // Byte 11
  kProximityClicked = 1,
  kTrackerConnection = 2,
  kIsRightHanded = 3,
  kBatteryCharging = 4,
  kSliderUpTouch = 5,
  kSliderDownTouch = 6,
  kBatteryChargeComplete = 0,  // Byte 12
  kBatteryLevel = 1,
  kPointAClicked = 0,  // Byte 13
  kTrackpadValue = 1,
  kPointBClicked = 0,  // Byte 14
  kGripGestureForce = 1,
  kPinchAClicked = 0,  // Byte 15
  kPinchAValue = 1,
  kPinchBClicked = 0,  // Byte 16
  kPinchBValue = 1,
  kTrackpadForce = 1,  // Byte 17
  kFinger1Force = 1,   // Byte 18
  kFinger2Force = 1,   // Byte 19
  kFinger3Force = 1,   // Byte 20
  kFinger4Force = 1,   // Byte 21
  kFinger5Force = 1    // Byte 22
};

enum class InputSerialBitLength : int {
  kSystemButton = 1,  // Byte 0
  kTrackpadClicked = 1,
  kTrackpadTouched = 1,
  kFinger1Clicked = 1,
  kFinger2Clicked = 1,
  kFinger3Clicked = 1,
  kFinger4Clicked = 1,
  kFinger5Clicked = 1,
  kFinger1Touched = 1,  // Byte 1
  kFinger1Value = 7,
  kFinger2Touched = 1,  // Byte 2
  kFinger2Value = 7,
  kFinger3Touched = 1,  // Byte 3
  kFinger3Value = 7,
  kFinger4Touched = 1,  // Byte 4
  kFinger4Value = 7,
  kFinger5Touched = 1,  // Byte 5
  kFinger5Value = 7,
  kTrackpadX = 8,       // Byte 6
  kTrackpadY = 8,       // Byte 7
  kProximityTouch = 1,  // Byte 8
  kProximityValue = 7,
  kSliderTouch = 1,  // Byte 9
  kSliderValue = 7,
  kGripGestureTouched = 1,  // Byte 10
  kGripGestureValue = 7,
  kGripGestureClicked = 1,  // Byte 11
  kProximityClicked = 1,
  kTrackerConnection = 1,
  kIsRightHanded = 1,
  kBatteryCharging = 1,
  kSliderUpTouch = 1,
  kSliderDownTouch = 1,
  kBatteryChargeComplete = 1,  // Byte 12
  kBatteryLevel = 7,
  kPointAClicked = 1,  // Byte 13
  kTrackpadValue = 7,
  kPointBClicked = 1,  // Byte 14
  kGripGestureForce = 7,
  kPinchAClicked = 1,  // Byte 15
  kPinchAValue = 7,
  kPinchBClicked = 1,  // Byte 16
  kPinchBValue = 7,
  kTrackpadForce = 7,  // Byte 17
  kFinger1Force = 7,   // Byte 18
  kFinger2Force = 7,   // Byte 19
  kFinger3Force = 7,   // Byte 20
  kFinger4Force = 7,   // Byte 21
  kFinger5Force = 7    // Byte 22
};