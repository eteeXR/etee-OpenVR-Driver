#pragma once
#include <openvr_driver.h>

#include <memory>
#include <string>
#include <vector>

#include "DeviceConfiguration.h"
#include "Encode/EncodingManager.h"
#include "TrackerDiscovery.h"

// A data-driven, per-executable pose offset. New games are added purely via vrsettings (see
// c_appOffsetSettingsSection in ControllerPose.cpp) - no code changes required.
struct AppPoseOffsetProfile {
  std::string processName;  // lowercase executable name, e.g. "hlvr.exe"
  bool enabled = false;
  vr::HmdVector3_t offsetVector{0, 0, 0};
  vr::HmdQuaternion_t angleOffsetQuaternion{1, 0, 0, 0};
};

class ControllerPose {
 public:
  ControllerPose(VRPoseConfiguration configuration);
  vr::DriverPose_t UpdatePose();
  vr::DriverPose_t GetStatusPose();

  void SetDeviceState(VRControllerState state);
  void SetEteeTrackerIsConnected(bool eteeTrackerConnected);

  void SetShadowEteeTracker(short deviceId, bool isRightHand);
  void SetAdaptorIsConnected(bool adaptorConnected, bool isRight);

  // Arms/disarms whichever configured per-app pose offset profile (if any) matches the given process.
  void OnSceneApplicationChanged(uint32_t pid);

 private:
  void DiscoverTrackedDevice();
  void LoadAppOffsetProfiles();

  vr::TrackedDevicePose_t GetTrackerPose();
  std::atomic<int> m_shadowTrackerId;
  
  std::atomic<bool> m_trackerIsEteeTracker;
  std::atomic<bool> m_eteeTrackerConnected;
  std::atomic<bool> m_eteeTrackerThruRole;

  bool m_adaptorConnRight;
  bool m_adaptorConnLeft;

  bool m_appOffsetProfilesLoaded = false;
  std::vector<AppPoseOffsetProfile> m_appOffsetProfiles;
  bool m_activeAppOffsetEnabled = false;
  vr::HmdVector3_t m_activeAppOffsetVector{0, 0, 0};
  vr::HmdQuaternion_t m_activeAppAngleOffsetQuaternion{1, 0, 0, 0};

  VRPoseConfiguration m_configuration;
  VRControllerState m_state;
};