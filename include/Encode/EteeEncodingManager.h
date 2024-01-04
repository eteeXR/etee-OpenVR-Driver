#pragma once

#include <Encode/EncodingManager.h>

#include <functional>

class EteeEncodingManager {
 public:
  EteeEncodingManager(float maxAnalogValue)
      : m_maxAnalogValue(maxAnalogValue) {};

  // eteeTracker and Smart 3DPT Adaptor status flags
  bool prevTrackerConnLeft;
  bool prevTrackerConnRight;

  bool prevAdaptorConnLeft;
  bool prevAdaptorConnRight;

  // decode the given string into a VRInputData_t
  VRCommInputData_t DecodeInputPacket(const std::string& input);
  VRStateEvent_t DecodeStatePacket(const std::string& in);

  // remapping pull values to decrease sensitivity
  std::float_t EteeEncodingManager::remapFingerValues(const std::float_t unmappedFingValue);

 private:
  float m_maxAnalogValue;
};