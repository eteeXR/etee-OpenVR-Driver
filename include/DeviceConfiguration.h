#pragma once

#include "Communication/CommunicationManager.h"
#include "openvr_driver.h"

static const char* c_driverSettingsSection = "driver_etee";
static const char* c_deviceManufacturer = "etee";  // Separate from the tracker manufacturer

struct VRPoseConfiguration {
  VRPoseConfiguration() : offsetVector({0, 0, 0}), angleOffsetQuaternion({0, 0, 0, 0}){};
  VRPoseConfiguration(vr::ETrackedControllerRole role, vr::HmdVector3_t offsetVector, vr::HmdQuaternion_t angleOffsetQuaternion)
      : role(role), offsetVector(offsetVector), angleOffsetQuaternion(angleOffsetQuaternion){};

  vr::ETrackedControllerRole role;
  vr::HmdVector3_t offsetVector;
  vr::HmdQuaternion_t angleOffsetQuaternion;
};

struct VRSerialConfiguration {
  VRSerialConfiguration() : port(-1), overridePort(false){};
  VRSerialConfiguration(bool overridePort, int port) : port(port), overridePort(overridePort){};
  int port;
  bool overridePort;
};

struct VRHapticConfiguration {
  VRHapticConfiguration(){};
  VRHapticConfiguration(bool enabled) : enabled(enabled){};

  bool enabled = false;
};

struct VRDeviceConfiguration {
  VRDeviceConfiguration(){};
  VRDeviceConfiguration(vr::ETrackedControllerRole role, bool enabled, VRHapticConfiguration haptic, VRSerialConfiguration serial, VRPoseConfiguration poseConfiguration)
      : role(role), enabled(enabled), haptic(haptic), serial(serial), pose(poseConfiguration){};

  bool enabled;
  vr::ETrackedControllerRole role;

  VRHapticConfiguration haptic;
  VRSerialConfiguration serial;
  VRPoseConfiguration pose;
};
