#pragma once
#include <openvr_driver.h>

#include <memory>

#include "DeviceConfiguration.h"
#include "Encode/EncodingManager.h"
#include "TrackerDiscovery.h"

class ControllerPose {
 public:
  ControllerPose(VRPoseConfiguration configuration);
  vr::DriverPose_t UpdatePose();
  vr::DriverPose_t GetStatusPose();

  void SetDeviceState(VRControllerState state);
  void SetEteeTrackerIsConnected(bool eteeTrackerConnected);

  void SetShadowEteeTracker(short deviceId, bool isRightHand);

 private:
  void DiscoverTrackedDevice();

  vr::TrackedDevicePose_t GetTrackerPose();
  std::atomic<int> m_shadowTrackerId;
  
  std::atomic<bool> m_trackerIsEteeTracker;
  std::atomic<bool> m_eteeTrackerConnected;
  std::atomic<bool> m_eteeTrackerThruRole;

  VRPoseConfiguration m_configuration;
  VRControllerState m_state;
};