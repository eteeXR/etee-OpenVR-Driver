#include <Encode/EteeEncodingManager.h>

#include <algorithm>
#include <tuple>

#include "Encode/Thresholds.h"
#include "Util/DriverLog.h"

/// <summary>
/// Extracts subsection of bits from a byte
/// </summary>
/// <param name="byte">The input byte</param>
/// <param name="bitQuant">How many bits are needed</param>
/// <param name="startBit">Where to start taking bits from (starts from lsb)</param>
/// <returns>Value of selection as an int</returns>
static char SelectBits(const char byte, int bitQuant, int startBit) {
  return (((1 << bitQuant) - 1) & (byte >> (startBit)));
}

template <typename T>
static T GetDataUnsigned(const char* input, int bytePosition, int bitPosition, int bitLength) {
  const char byte = input[bytePosition];

  return ((uint8_t)SelectBits(byte, bitLength, bitPosition));
}

template <typename T>
static T GetDataSigned(const char* input, int bytePosition, int bitPosition, int bitLength) {
  const char byte = input[bytePosition];

  return ((int8_t)SelectBits(byte, bitLength, bitPosition));
}

static const short packetSize = 25;


VRCommInputData_t EteeEncodingManager::DecodeInputPacket(const std::string& in) {
  if (in.size() < packetSize) {
    VRCommInputData_t commData(false);
    return commData;
  }

  std::string input = in.substr(in.length() - packetSize);
  const char* c_input = input.c_str();

  // Create mew comm data objects
  VRCommInputData_t commData(true);

  // System
  commData.system.systemClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kSystemButton),
      static_cast<int>(InputSerialBitPosition::kSystemButton),
      static_cast<int>(InputSerialBitLength::kSystemButton));
  commData.system.trackerConnection = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kTrackerConnection),
      static_cast<int>(InputSerialBitPosition::kTrackerConnection),
      static_cast<int>(InputSerialBitLength::kTrackerConnection));
  commData.system.battery = (GetDataUnsigned<float>(
                                c_input,
                                static_cast<int>(InputSerialBytePosition::kBatteryLevel),
                                static_cast<int>(InputSerialBitPosition::kBatteryLevel),
                                static_cast<int>(InputSerialBitLength::kBatteryLevel))) /
                            100.0f;
  commData.system.batteryCharging = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kBatteryCharging),
      static_cast<int>(InputSerialBitPosition::kBatteryCharging),
      static_cast<int>(InputSerialBitLength::kBatteryCharging));
  commData.system.batteryChargeComplete = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kBatteryChargeComplete),
      static_cast<int>(InputSerialBitPosition::kBatteryChargeComplete),
      static_cast<int>(InputSerialBitLength::kBatteryChargeComplete));
  commData.system.systemClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kSystemButton),
      static_cast<int>(InputSerialBitPosition::kSystemButton),
      static_cast<int>(InputSerialBitLength::kSystemButton));
  commData.isRight = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kIsRightHanded),
      static_cast<int>(InputSerialBitPosition::kIsRightHanded),
      static_cast<int>(InputSerialBitLength::kIsRightHanded));

  // Thumbpad
  float thumbpadPull = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kTrackpadValue),
                           static_cast<int>(InputSerialBitPosition::kTrackpadValue),
                           static_cast<int>(InputSerialBitLength::kTrackpadValue)) /
                       126.0f;
  float thumbpadForce = GetDataSigned<float>(
                            c_input,
                            static_cast<int>(InputSerialBytePosition::kTrackpadForce),
                            static_cast<int>(InputSerialBitPosition::kTrackpadForce),
                            static_cast<int>(InputSerialBitLength::kTrackpadForce)) /
                        126.0f;
  bool thumbpadTouch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kTrackpadTouched),
      static_cast<int>(InputSerialBitPosition::kTrackpadTouched),
      static_cast<int>(InputSerialBitLength::kTrackpadTouched));
  bool thumbpadClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kTrackpadClicked),
      static_cast<int>(InputSerialBitPosition::kTrackpadClicked),
      static_cast<int>(InputSerialBitLength::kTrackpadClicked));

  // commData.thumbpad.value = thumbpadPull;
  commData.thumbpad.force = thumbpadForce;
  commData.thumbpad.touch = thumbpadTouch ? thumbpadTouch : thumbpadClick;  // hardcoded for touch = true when click = true
  commData.thumbpad.click = thumbpadClick;

  commData.thumbpad.x = ((GetDataUnsigned<float>(
                             c_input,
                             static_cast<int>(InputSerialBytePosition::kTrackpadX),
                             static_cast<int>(InputSerialBitPosition::kTrackpadX),
                             static_cast<int>(InputSerialBitLength::kTrackpadX))) -
                         126) /
                        126.0f;
  commData.thumbpad.y = ((GetDataUnsigned<float>(
                             c_input,
                             static_cast<int>(InputSerialBytePosition::kTrackpadY),
                             static_cast<int>(InputSerialBitPosition::kTrackpadY),
                             static_cast<int>(InputSerialBitLength::kTrackpadY))) -
                         126) /
                        126.0f;

  // Proximity
  commData.proximity.touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kProximityTouch),
      static_cast<int>(InputSerialBitPosition::kProximityTouch),
      static_cast<int>(InputSerialBitLength::kProximityTouch));
  commData.proximity.click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kProximityClicked),
      static_cast<int>(InputSerialBitPosition::kProximityClicked),
      static_cast<int>(InputSerialBitLength::kProximityClicked));
  commData.proximity.value = GetDataSigned<float>(
                                 c_input,
                                 static_cast<int>(InputSerialBytePosition::kProximityValue),
                                 static_cast<int>(InputSerialBitPosition::kProximityValue),
                                 static_cast<int>(InputSerialBitLength::kProximityValue)) /
                             126.0f;

  // Fingers
  // Finger 1
  float finger1Pull = GetDataSigned<float>(
                          c_input,
                          static_cast<int>(InputSerialBytePosition::kFinger1Value),
                          static_cast<int>(InputSerialBitPosition::kFinger1Value),
                          static_cast<int>(InputSerialBitLength::kFinger1Value)) /
                      126.0f;
  float finger1Force = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kFinger1Force),
                           static_cast<int>(InputSerialBitPosition::kFinger1Force),
                           static_cast<int>(InputSerialBitLength::kFinger1Force)) /
                       126.0f;
  bool finger1Touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger1Touched),
      static_cast<int>(InputSerialBitPosition::kFinger1Touched),
      static_cast<int>(InputSerialBitLength::kFinger1Touched));
  bool finger1Click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger1Clicked),
      static_cast<int>(InputSerialBitPosition::kFinger1Clicked),
      static_cast<int>(InputSerialBitLength::kFinger1Clicked));

  commData.fingers[0].pull = finger1Pull;
  commData.fingers[0].force = finger1Force;
  commData.fingers[0].touch = finger1Touch;
  commData.fingers[0].click = finger1Click;

  // Finger 2
  float finger2Pull = GetDataSigned<float>(
                          c_input,
                          static_cast<int>(InputSerialBytePosition::kFinger2Value),
                          static_cast<int>(InputSerialBitPosition::kFinger2Value),
                          static_cast<int>(InputSerialBitLength::kFinger2Value)) /
                      126.0f;
  float finger2Force = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kFinger2Force),
                           static_cast<int>(InputSerialBitPosition::kFinger2Force),
                           static_cast<int>(InputSerialBitLength::kFinger2Force)) /
                       126.0f;
  bool finger2Touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger2Touched),
      static_cast<int>(InputSerialBitPosition::kFinger2Touched),
      static_cast<int>(InputSerialBitLength::kFinger2Touched));
  bool finger2Click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger2Clicked),
      static_cast<int>(InputSerialBitPosition::kFinger2Clicked),
      static_cast<int>(InputSerialBitLength::kFinger2Clicked));

  commData.fingers[1].pull = finger2Pull;
  commData.fingers[1].force = finger2Force;
  commData.fingers[1].touch = finger2Touch;
  commData.fingers[1].click = finger2Click;

  // Finger 3
  float finger3Pull = GetDataSigned<float>(
                          c_input,
                          static_cast<int>(InputSerialBytePosition::kFinger3Value),
                          static_cast<int>(InputSerialBitPosition::kFinger3Value),
                          static_cast<int>(InputSerialBitLength::kFinger3Value)) /
                      126.0f;
  float finger3Force = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kFinger3Force),
                           static_cast<int>(InputSerialBitPosition::kFinger3Force),
                           static_cast<int>(InputSerialBitLength::kFinger3Force)) /
                       126.0f;
  bool finger3Touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger3Touched),
      static_cast<int>(InputSerialBitPosition::kFinger3Touched),
      static_cast<int>(InputSerialBitLength::kFinger3Touched));
  bool finger3Click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger3Clicked),
      static_cast<int>(InputSerialBitPosition::kFinger3Clicked),
      static_cast<int>(InputSerialBitLength::kFinger3Clicked));

  commData.fingers[2].pull = finger3Pull;
  commData.fingers[2].force = finger3Force;
  commData.fingers[2].touch = finger3Touch;
  commData.fingers[2].click = finger3Click;

  // Finger 4
  float finger4Pull = GetDataSigned<float>(
                          c_input,
                          static_cast<int>(InputSerialBytePosition::kFinger4Value),
                          static_cast<int>(InputSerialBitPosition::kFinger4Value),
                          static_cast<int>(InputSerialBitLength::kFinger4Value)) /
                      126.0f;
  float finger4Force = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kFinger4Force),
                           static_cast<int>(InputSerialBitPosition::kFinger4Force),
                           static_cast<int>(InputSerialBitLength::kFinger4Force)) /
                       126.0f;
  bool finger4Touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger4Touched),
      static_cast<int>(InputSerialBitPosition::kFinger4Touched),
      static_cast<int>(InputSerialBitLength::kFinger4Touched));
  bool finger4Click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger4Clicked),
      static_cast<int>(InputSerialBitPosition::kFinger4Clicked),
      static_cast<int>(InputSerialBitLength::kFinger4Clicked));

  commData.fingers[3].pull = finger4Pull;
  commData.fingers[3].force = finger4Force;
  commData.fingers[3].touch = finger4Touch;
  commData.fingers[3].click = finger4Click;

  // Finger 5
  float finger5Pull = GetDataSigned<float>(
                          c_input,
                          static_cast<int>(InputSerialBytePosition::kFinger5Value),
                          static_cast<int>(InputSerialBitPosition::kFinger5Value),
                          static_cast<int>(InputSerialBitLength::kFinger5Value)) /
                      126.0f;
  float finger5Force = GetDataSigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kFinger5Force),
                           static_cast<int>(InputSerialBitPosition::kFinger5Force),
                           static_cast<int>(InputSerialBitLength::kFinger5Force)) /
                       126.0f;
  bool finger5Touch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger5Touched),
      static_cast<int>(InputSerialBitPosition::kFinger5Touched),
      static_cast<int>(InputSerialBitLength::kFinger5Touched));
  bool finger5Click = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kFinger5Clicked),
      static_cast<int>(InputSerialBitPosition::kFinger5Clicked),
      static_cast<int>(InputSerialBitLength::kFinger5Clicked));

  commData.fingers[4].pull = finger5Pull;
  commData.fingers[4].force = finger5Force;
  commData.fingers[4].touch = finger5Touch;
  commData.fingers[4].click = finger5Click;

  // Array of finger results (for gesture calculations)
  float fingPulls[6] = {commData.fingers[0].pull, finger2Pull, finger3Pull, finger4Pull, finger5Pull, thumbpadPull};
  float fingForces[6] = {commData.fingers[0].force, finger2Force, finger3Force, finger4Force, finger5Force, thumbpadForce};
  bool fingTouchs[6] = {commData.fingers[0].touch, finger2Touch, finger3Touch, finger4Touch, finger5Touch, thumbpadTouch};
  bool fingClicks[6] = {commData.fingers[0].click, finger2Click, finger3Click, finger4Click, finger5Click, thumbpadClick};

  // Grip Gesture
  float gripPull = GetDataSigned<float>(
                          c_input,
                       static_cast<int>(InputSerialBytePosition::kGripGestureValue),
                       static_cast<int>(InputSerialBitPosition::kGripGestureValue),
                       static_cast<int>(InputSerialBitLength::kGripGestureValue)) /
                      126.0f;
  float gripForce = GetDataSigned<float>(
                           c_input,
                        static_cast<int>(InputSerialBytePosition::kGripGestureForce),
                        static_cast<int>(InputSerialBitPosition::kGripGestureForce),
                        static_cast<int>(InputSerialBitLength::kGripGestureForce)) /
                       126.0f;
  bool gripTouch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kGripGestureTouched),
      static_cast<int>(InputSerialBitPosition::kGripGestureTouched),
      static_cast<int>(InputSerialBitLength::kGripGestureTouched));
  
  bool gripClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kGripGestureClicked),
      static_cast<int>(InputSerialBitPosition::kGripGestureClicked),
      static_cast<int>(InputSerialBitLength::kGripGestureClicked));

  commData.gesture.gripPull = gripPull;
  commData.gesture.gripForce = gripForce;
  commData.gesture.gripTouch = gripTouch;
  commData.gesture.gripClick = gripClick;

  // Pinch Gesture A & B
  // A
  float pinchAPull = GetDataSigned<float>(
                        c_input,
                         static_cast<int>(InputSerialBytePosition::kPinchAValue),
                         static_cast<int>(InputSerialBitPosition::kPinchAValue),
                         static_cast<int>(InputSerialBitLength::kPinchAValue)) /
                    126.0f;
  bool pinchAClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kPinchAClicked),
      static_cast<int>(InputSerialBitPosition::kPinchAClicked),
      static_cast<int>(InputSerialBitLength::kPinchAClicked));

  commData.gesture.pinchAPull = pinchAPull;
  commData.gesture.pinchAClick = pinchAClick;

  // B
  float pinchBPull = GetDataSigned<float>(
                         c_input,
                         static_cast<int>(InputSerialBytePosition::kPinchBValue),
                         static_cast<int>(InputSerialBitPosition::kPinchBValue),
                         static_cast<int>(InputSerialBitLength::kPinchBValue)) /
                     126.0f;
  bool pinchBClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kPinchBClicked),
      static_cast<int>(InputSerialBitPosition::kPinchBClicked),
      static_cast<int>(InputSerialBitLength::kPinchBClicked));

  commData.gesture.pinchBPull = pinchBPull;
  commData.gesture.pinchBClick = pinchBClick;

  // Point Gesture
  bool pointBClick = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kPointBClicked),
      static_cast<int>(InputSerialBitPosition::kPointBClicked),
      static_cast<int>(InputSerialBitLength::kPointBClicked));

  commData.gesture.pointBClick = pointBClick;


  // Slider
  float sliderValue = ((GetDataUnsigned<float>(
                           c_input,
                           static_cast<int>(InputSerialBytePosition::kSliderValue),
                           static_cast<int>(InputSerialBitPosition::kSliderValue),
                           static_cast<int>(InputSerialBitLength::kSliderValue))) -
                       63) /
                      63.0f;
  bool sliderTouch = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kSliderTouch),
      static_cast<int>(InputSerialBitPosition::kSliderTouch),
      static_cast<int>(InputSerialBitLength::kSliderTouch));
  bool sliderUp = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kSliderUpTouch),
      static_cast<int>(InputSerialBitPosition::kSliderUpTouch),
      static_cast<int>(InputSerialBitLength::kSliderUpTouch));
  bool sliderDown = GetDataUnsigned<bool>(
      c_input,
      static_cast<int>(InputSerialBytePosition::kSliderDownTouch),
      static_cast<int>(InputSerialBitPosition::kSliderDownTouch),
      static_cast<int>(InputSerialBitLength::kSliderDownTouch));

  commData.slider.value = sliderValue;
  commData.slider.simulated_x = 0.0f;
  commData.slider.touch = sliderTouch;
  commData.slider.upTouch = sliderUp;
  commData.slider.downTouch = sliderDown;

  // Remap finger values to decrease sensitivity
  commData.fingers[0].pull = remapFingerValues(finger1Pull);
  commData.fingers[1].pull = remapFingerValues(finger2Pull);
  commData.fingers[2].pull = remapFingerValues(finger3Pull);
  commData.fingers[3].pull = remapFingerValues(finger4Pull);
  commData.fingers[4].pull = remapFingerValues(finger5Pull);

  if (commData.isRight == true && commData.system.trackerConnection != prevTrackerConnRight) {
    prevTrackerConnRight = commData.system.trackerConnection;
    DriverLog("Tracker connection for %s hand is: %s", commData.isRight ? "right" : "left", commData.system.trackerConnection ? "true" : "false");
  }

  else if (commData.isRight == false && commData.system.trackerConnection != prevTrackerConnLeft) {
    prevTrackerConnLeft = commData.system.trackerConnection;
    DriverLog("Tracker connection for %s hand is: %s", commData.isRight ? "right" : "left", commData.system.trackerConnection ? "true" : "false");
  }

  return commData;
};

/// <summary>
/// Attempts to find text in a string that correlates to either left or right identifies, then returns an ETrackedControllerRole based on what it's found.
/// Returns TrackedControllerRole_Invalid if it couldn't find anything.
/// </summary>
static vr::ETrackedControllerRole GetRoleFromText(const std::string& input, const std::string& right, const std::string& left) {
  return (input.find(right) != std::string::npos) ? vr::ETrackedControllerRole::TrackedControllerRole_RightHand
                                                  : ((input.find(left) != std::string::npos) ? vr::ETrackedControllerRole::TrackedControllerRole_LeftHand
                                                                                             : vr::ETrackedControllerRole::TrackedControllerRole_Invalid);
}

VRStateEvent_t EteeEncodingManager::DecodeStatePacket(const std::string& input) {
  // connected
  if (input.find("connection complete") != std::string::npos) {
    DebugDriverLog(input.c_str());

    VRStateEventData_t data;
    VRHandedControllerState_t handedControllerState(GetRoleFromText(input, "R", "L"), VRControllerState::ready);
    data.handedControllerState = handedControllerState;

    VRStateEvent_t stateEvent(VRStateEventType::controller_state, data);
    return stateEvent;
  }

  // connected (if we send BP+LR\r\n)
  if (input.find(":LR") != std::string::npos) {
    DebugDriverLog(input.c_str());

    VRStateEventData_t data;
    VRHandedControllerState_t handedControllerState(GetRoleFromText(input, "right", "left"), VRControllerState::ready);
    data.handedControllerState = handedControllerState;

    VRStateEvent_t stateEvent(VRStateEventType::controller_state, data);
    return stateEvent;
  }

  if (input.find(":VG") != std::string::npos) {
    DebugDriverLog(input.c_str());

    VRStateEventData_t data;
    VRHandedControllerState_t handedControllerState(GetRoleFromText(input, "R", "L"), VRControllerState::streaming);
    data.handedControllerState = handedControllerState;

    VRStateEvent_t stateEvent(VRStateEventType::controller_state, data);
    return stateEvent;
  }

  // disconnected
  if (input.find("disconnected") != std::string::npos) {
    DebugDriverLog(input.c_str());

    VRStateEventData_t data;
    VRHandedControllerState_t handedControllerState(GetRoleFromText(input, "R", "L"), VRControllerState::disconnected);
    data.handedControllerState = handedControllerState;

    VRStateEvent_t stateEvent(VRStateEventType::controller_state, data);
    return stateEvent;
  }

  VRStateEvent_t state(VRStateEventType::unknown, {});
  return state;
}

/// <summary>
/// Ignore the first 0.2 values of the pull range in fingers. Current changes would make it too sensitive if not
std::float_t EteeEncodingManager::remapFingerValues(const std::float_t unmappedFingValue) {
  float remappingThreshold = 0.15f;
  float remappedFingValue;
  if (unmappedFingValue < remappingThreshold) {
    remappedFingValue = 0;
  } else {
    remappedFingValue = (unmappedFingValue - remappingThreshold) / (1 - remappingThreshold);
  }
  return remappedFingValue;
}