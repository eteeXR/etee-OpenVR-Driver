#pragma once
#include <openvr_driver.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "Communication/CommunicationManager.h"
#include "ControllerPose.h"
#include "DeviceConfiguration.h"
#include "DeviceDriver/DeviceDriver.h"
#include "Encode/EteeEncodingManager.h"
#include "Util/Bones.h"

enum class DeviceEventType { HAPTIC_EVENT, STANDBY };

struct HapticEventData {
  int onDurationUs;
  int offDurationUs;
  int pulseCount;
};

union DeviceEventData {
  HapticEventData hapticEvent;
};

struct DeviceEvent {
  DeviceEventType type;
  HapticEventData data;
};

enum ComponentIndex : int {
  // System
  SYSTEM_CLICK,
  TRACKERCONNECTION_CLICK,
  ADAPTORCONNECTION_CLICK,

  // Trackpad
  TRACKPAD_VALUE,
  TRACKPAD_FORCE,
  TRACKPAD_TOUCH,
  TRACKPAD_CLICK,
  TRACKPAD_X,
  TRACKPAD_Y,

  // Slider
  SLIDER_TOUCH,
  SLIDER_VALUE,
  SLIDER_SIMULATED_X,
  SLIDER_CLICK,

  SLIDER_UP_CLICK,
  SLIDER_DOWN_CLICK,

  PROXIMITY_TOUCH,
  PROXIMITY_CLICK,
  PROXIMITY_VALUE,

  // Grip Gesture
  GRIP_VALUE,
  GRIP_FORCE,
  GRIP_TOUCH,
  GRIP_CLICK,

  // Pinch and Point Gesture
  PINCH_A_VALUE,
  PINCH_A_CLICK,
  PINCH_B_VALUE,
  PINCH_B_CLICK,
  POINT_B_CLICK,

  // Fingers
  THUMB_PULL,
  THUMB_FORCE,
  THUMB_CLICK,
  THUMB_TOUCH,

  INDEX_PULL,
  INDEX_FORCE,
  INDEX_CLICK,
  INDEX_TOUCH,

  MIDDLE_PULL,
  MIDDLE_FORCE,
  MIDDLE_CLICK,
  MIDDLE_TOUCH,

  RING_PULL,
  RING_FORCE,
  RING_CLICK,
  RING_TOUCH,

  PINKY_PULL,
  PINKY_FORCE,
  PINKY_CLICK,
  PINKY_TOUCH,

  SKELETON,
  HAPTIC,

  COMPONENT_INDEX_MAX,
};

class EteeDeviceDriver : public IDeviceDriver {
 public:
  EteeDeviceDriver(
      VRDeviceConfiguration configuration, std::unique_ptr<BoneAnimator>&& boneAnimator, std::function<void(const DeviceEvent& deviceEvent)> deviceEventCallback);

  vr::EVRInitError Activate(uint32_t unObjectId);
  void Deactivate();

  void EnterStandby();
  void* GetComponent(const char* pchComponentNameAndVersion);
  void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize);
  vr::DriverPose_t GetPose();

  std::string GetSerialNumber();
  bool IsActive();
  const VRDeviceConfiguration& GetConfiguration();

  void OnInputUpdate(VRCommInputData_t data);
  void OnStateUpdate(VRControllerState data);

  void OnVREvent(const vr::VREvent_t& vrEvent);

  static HapticEventData OnHapticEvent(const vr::VREvent_HapticVibration_t& hapticEvent);

  vr::ETrackedControllerRole GetDeviceRole() const;

  void SetTrackerId(short deviceId, bool isRightHand);

  VRControllerState GetControllerState();

 private:
  void StartDevice();
  bool IsRightHand() const;
  void PoseUpdateThread();
  void InputUpdateThread();

  std::atomic<bool> m_isActive;
  std::atomic<uint32_t> m_driverId;

  vr::VRInputComponentHandle_t m_inputComponentHandles[ComponentIndex::COMPONENT_INDEX_MAX]{};

  vr::VRBoneTransform_t m_handTransforms[NUM_BONES];

  VRDeviceConfiguration m_configuration;

  std::unique_ptr<ControllerPose> m_controllerPose;
  std::shared_ptr<BoneAnimator> m_boneAnimator;
  vr::PropertyContainerHandle_t m_props;

  std::atomic<VRControllerState> m_deviceState;

  std::mutex m_inputMutex;
  VRCommInputData_t m_lastInput;

  std::function<void(const DeviceEvent& deviceEvent)> m_deviceEventCallback;

  std::thread m_inputUpdateThread;
  std::thread m_poseUpdateThread;

  short m_updateCount;
  std::chrono::milliseconds m_startUpdateCountTime;
};
