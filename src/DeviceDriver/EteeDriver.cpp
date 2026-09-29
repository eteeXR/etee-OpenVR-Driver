#include "DeviceDriver/EteeDriver.h"

#include <algorithm>
#include <utility>

#include "Util/DriverLog.h"
#include "Util/Math.h"

namespace {
constexpr uint64_t InputComponentBit(ComponentIndex index) {
  return 1ULL << static_cast<uint64_t>(index);
}

constexpr uint64_t kCriticalCreateMask =
    InputComponentBit(ComponentIndex::SYSTEM_CLICK) |
    InputComponentBit(ComponentIndex::TRACKPAD_TOUCH) |
    InputComponentBit(ComponentIndex::TRACKPAD_X) |
    InputComponentBit(ComponentIndex::TRACKPAD_Y) |
    InputComponentBit(ComponentIndex::GRIP_VALUE) |
    InputComponentBit(ComponentIndex::SKELETON) |
    InputComponentBit(ComponentIndex::HAPTIC);
}

EteeDeviceDriver::EteeDeviceDriver(
    VRDeviceConfiguration configuration, std::unique_ptr<BoneAnimator>&& boneAnimator, std::function<void(const DeviceEvent& deviceEvent)> deviceEventCallback)
    : m_configuration(configuration),
      m_boneAnimator(std::move(boneAnimator)),
      m_driverId(-1),
      m_isActive(false),
      m_lastInput(false),
      m_deviceEventCallback(std::move(deviceEventCallback)),
      m_controllerPose(std::make_unique<ControllerPose>(m_configuration.pose)) {
  m_batteryRingBuffer.Init(128);
}

bool EteeDeviceDriver::IsRightHand() const {
  return m_configuration.role == vr::TrackedControllerRole_RightHand;
}

std::string EteeDeviceDriver::GetSerialNumber() {
  return IsRightHand() ? "ETEE-7NQD0123R" : "ETEE-7NQD0123L";
};

bool EteeDeviceDriver::IsActive() {
  return m_isActive;
};

const VRDeviceConfiguration& EteeDeviceDriver::GetConfiguration() {
  return m_configuration;
};

vr::EVRInitError EteeDeviceDriver::Activate(uint32_t unObjectId) {
  DriverLog("Starting activation of id: %i", unObjectId);
  const bool isRightHand = IsRightHand();

  m_invalidHandleWarningMask.store(0);
  m_createdComponentMask.store(0);
  m_failedCreateComponentMask.store(0);
  m_loggedFirstInputThreadCycle.store(false);
  m_loggedFirstPoseThreadCycle.store(false);
  m_loggedFirstOnInputUpdate.store(false);
  for (auto& handle : m_inputComponentHandles) {
    handle = vr::k_ulInvalidInputComponentHandle;
  }

  m_driverId = unObjectId;

  m_props = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_driverId);
  if (m_props == vr::k_ulInvalidPropertyContainer) {
    DriverLog("Activation failed: invalid property container for device id=%u", unObjectId);
    m_driverId = vr::k_unTrackedDeviceIndexInvalid;
    return vr::VRInitError_Driver_Failed;
  }

  vr::VRProperties()->SetStringProperty(m_props, vr::Prop_SerialNumber_String, GetSerialNumber().c_str());
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_WillDriftInYaw_Bool, false);
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_DeviceIsWireless_Bool, true);
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_DeviceIsCharging_Bool, false);

  vr::HmdMatrix34_t l_matrix = {-1.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, -1.f, 0.f, 0.f};
  vr::VRProperties()->SetProperty(m_props, vr::Prop_StatusDisplayTransform_Matrix34, &l_matrix, sizeof(vr::HmdMatrix34_t), vr::k_unHmdMatrix34PropertyTag);

  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_Firmware_UpdateAvailable_Bool, false);
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_DeviceProvidesBatteryStatus_Bool, true);
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_DeviceCanPowerOff_Bool, true);

  vr::VRProperties()->SetInt32Property(m_props, vr::Prop_DeviceClass_Int32, (int32_t)vr::TrackedDeviceClass_Controller);
  vr::VRProperties()->SetBoolProperty(m_props, vr::Prop_Identifiable_Bool, true);
  vr::VRProperties()->SetInt32Property(m_props, vr::Prop_Axis0Type_Int32, vr::k_eControllerAxis_TrackPad);
  vr::VRProperties()->SetInt32Property(m_props, vr::Prop_Axis1Type_Int32, vr::k_eControllerAxis_Trigger);
  vr::VRProperties()->SetInt32Property(
      m_props, vr::Prop_ControllerRoleHint_Int32, IsRightHand() ? vr::TrackedControllerRole_RightHand : vr::TrackedControllerRole_LeftHand);
  vr::VRProperties()->SetInt32Property(m_props, vr::Prop_ControllerHandSelectionPriority_Int32, (int32_t)7002);
  vr::VRProperties()->SetStringProperty(m_props, vr::Prop_ModelNumber_String, IsRightHand() ? "etee right" : "etee left");
  vr::VRProperties()->SetStringProperty(
      m_props, vr::Prop_RenderModelName_String, IsRightHand() ? "{etee}/rendermodels/etee_controller_right" : "{etee}/rendermodels/etee_controller_left");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_ManufacturerName_String,
      "etee");  // anything other than TG0 (what the manufacturer of the tracker is, as we use this to get the pose)
  vr::VRProperties()->SetStringProperty(m_props, vr::Prop_ResourceRoot_String, "etee_controller");
  vr::VRProperties()->SetStringProperty(m_props, vr::Prop_InputProfilePath_String, "{etee}/input/etee_controller_profile.json");
  vr::VRProperties()->SetInt32Property(m_props, vr::Prop_Axis2Type_Int32, vr::k_eControllerAxis_Trigger);
  vr::VRProperties()->SetStringProperty(m_props, vr::Prop_ControllerType_String, "etee_controller");

    bool activationFailed = false;

  // Inputs | These interact without the specified input profile so the paths need to be equivalent

  // System Info
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::SYSTEM_CLICK, "/input/system/click", true);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::TRACKERCONNECTION_CLICK, "/input/tracker_connection/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::ADAPTORCONNECTION_CLICK, "/input/adaptor_connection/click", false);

  // Slider
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::SLIDER_TOUCH, "/input/slider/touch", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::SLIDER_CLICK, "/input/slider/click", false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::SLIDER_VALUE,
      "/input/slider/y",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedTwoSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::SLIDER_SIMULATED_X,
      "/input/slider/x",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedTwoSided,
      false);

  // Slider Up and Down as Buttons
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::SLIDER_UP_CLICK, "/input/slider_up/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::SLIDER_DOWN_CLICK, "/input/slider_down/click", false);

  // Thumbpad / Trackpad
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::TRACKPAD_X,
      "/input/thumbpad/x",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedTwoSided,
      true);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::TRACKPAD_Y,
      "/input/thumbpad/y",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedTwoSided,
      true);

    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::TRACKPAD_TOUCH, "/input/thumbpad/touch", true);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::TRACKPAD_CLICK, "/input/thumbpad/click", false);
  //vr::VRDriverInput()->CreateScalarComponent(m_props, "/input/thumbpad/pull", &m_inputComponentHandles[ComponentIndex::TRACKPAD_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::TRACKPAD_FORCE,
      "/input/thumbpad/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);

  // Proximity button
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PROXIMITY_TOUCH, "/input/proximity/touch", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PROXIMITY_CLICK, "/input/proximity/click", false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::PROXIMITY_VALUE,
      "/input/proximity/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);

  // Fingers
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::THUMB_PULL,
      "/input/finger/thumb/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::THUMB_FORCE,
      "/input/finger/thumb/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::THUMB_CLICK, "/input/finger/thumb/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::THUMB_TOUCH, "/input/finger/thumb/touch", false);

    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::INDEX_PULL,
      "/input/finger/index/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::INDEX_FORCE,
      "/input/finger/index/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::INDEX_CLICK, "/input/finger/index/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::INDEX_TOUCH, "/input/finger/index/touch", false);

    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::MIDDLE_PULL,
      "/input/finger/middle/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::MIDDLE_FORCE,
      "/input/finger/middle/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::MIDDLE_CLICK, "/input/finger/middle/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::MIDDLE_TOUCH, "/input/finger/middle/touch", false);

    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::RING_PULL,
      "/input/finger/ring/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::RING_FORCE,
      "/input/finger/ring/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::RING_CLICK, "/input/finger/ring/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::RING_TOUCH, "/input/finger/ring/touch", false);

    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::PINKY_PULL,
      "/input/finger/pinky/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::PINKY_FORCE,
      "/input/finger/pinky/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PINKY_CLICK, "/input/finger/pinky/click", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PINKY_TOUCH, "/input/finger/pinky/touch", false);

  // Grip Gesture
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::GRIP_VALUE,
      "/input/grip/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      true);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::GRIP_FORCE,
      "/input/grip/force",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::GRIP_TOUCH, "/input/grip/touch", false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::GRIP_CLICK, "/input/grip/click", false);

  // Pinch Gestures
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::PINCH_A_VALUE,
      "/input/pinch/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PINCH_A_CLICK, "/input/pinch/click", false);
    activationFailed |= !TryCreateScalarComponent(
      ComponentIndex::PINCH_B_VALUE,
      "/input/pinch_thumbfinger/value",
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided,
      false);
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::PINCH_B_CLICK, "/input/pinch_thumbfinger/click", false);

  // Point Gesture
    activationFailed |= !TryCreateBooleanComponent(ComponentIndex::POINT_B_CLICK, "/input/point_independent/click", false);

  // Haptics
    activationFailed |= !TryCreateHapticComponent(ComponentIndex::HAPTIC, "/output/haptic", true);

  // Icons
  vr::VRProperties()->SetStringProperty(
      m_props, vr::Prop_NamedIconPathDeviceOff_String, IsRightHand() ? "{etee}/icons/right_controller_status_off.png" : "{etee}/icons/left_controller_status_off.png");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceSearching_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_searching.gif" : "{etee}/icons/left_controller_status_searching.gif");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceSearchingAlert_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_searching_alert.gif" : "{etee}/icons/left_controller_status_searching_alert.gif");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceReady_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_ready.png" : "{etee}/icons/left_controller_status_ready.png");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceReadyAlert_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_ready_alert.png" : "{etee}/icons/left_controller_status_ready_alert.png");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceNotReady_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_error.png" : "{etee}/icons/left_controller_status_error.png");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceStandby_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_off.png" : "{etee}/icons/left_controller_status_off.png");
  vr::VRProperties()->SetStringProperty(
      m_props,
      vr::Prop_NamedIconPathDeviceAlertLow_String,
      IsRightHand() ? "{etee}/icons/right_controller_status_ready_low.png" : "{etee}/icons/left_controller_status_ready_low.png");

  activationFailed |= !TryCreateSkeletonComponent(isRightHand, true);

  if (activationFailed || !HasAllCriticalComponents()) {
    const uint64_t createdMask = m_createdComponentMask.load();
    const uint64_t failedMask = m_failedCreateComponentMask.load();
    DriverLog(
        "Activation failed due to missing critical components. createdMask=0x%llx failedMask=0x%llx criticalMask=0x%llx",
        static_cast<unsigned long long>(createdMask),
        static_cast<unsigned long long>(failedMask),
        static_cast<unsigned long long>(kCriticalCreateMask));
    m_props = vr::k_ulInvalidPropertyContainer;
    m_driverId = vr::k_unTrackedDeviceIndexInvalid;
    m_isActive = false;
    return vr::VRInitError_Driver_Failed;
  }

  m_isActive = true;

  StartDevice();

  m_boneAnimator->LoadDefaultSkeletonByHand(m_handTransforms, isRightHand);

  // SteamVR wants input at startup for skeletons
  UpdateSkeletonComponentSafe(vr::VRSkeletalMotionRange_WithoutController);
  UpdateSkeletonComponentSafe(vr::VRSkeletalMotionRange_WithController);

  return vr::VRInitError_None;
}

void EteeDeviceDriver::StartDevice() {
  DriverLog("Etee controller successfully initialised");
  DriverLog("Breadcrumb: StartDevice entered for %s hand", IsRightHand() ? "right" : "left");

  vr::DriverPose_t pose = m_controllerPose->GetStatusPose();
  vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_driverId, pose, sizeof(vr::DriverPose_t));
  DriverLog("Breadcrumb: StartDevice status pose pushed");

  m_poseUpdateThread = std::thread(&EteeDeviceDriver::PoseUpdateThread, this);
  DriverLog("Breadcrumb: pose update thread launched");

  m_inputUpdateThread = std::thread(&EteeDeviceDriver::InputUpdateThread, this);
  DriverLog("Breadcrumb: input update thread launched");
}

void EteeDeviceDriver::OnInputUpdate(VRCommInputData_t data) {
  if (!m_isActive || m_props == vr::k_ulInvalidPropertyContainer) {
    return;
  }

  if (!m_loggedFirstOnInputUpdate.exchange(true)) {
    DriverLog("Breadcrumb: first OnInputUpdate received (valid=%d)", data.isValid ? 1 : 0);
  }

  // System Info

  UpdateBooleanComponentSafe(ComponentIndex::SYSTEM_CLICK, data.system.systemClick);
  UpdateBooleanComponentSafe(ComponentIndex::TRACKERCONNECTION_CLICK, data.system.trackerConnection);
  UpdateBooleanComponentSafe(ComponentIndex::ADAPTORCONNECTION_CLICK, data.system.adaptorConnection);

  // Slider
  UpdateBooleanComponentSafe(ComponentIndex::SLIDER_TOUCH, data.slider.touch);
  UpdateBooleanComponentSafe(ComponentIndex::SLIDER_CLICK, data.slider.touch);
  UpdateScalarComponentSafe(ComponentIndex::SLIDER_VALUE, data.slider.value);
  UpdateScalarComponentSafe(ComponentIndex::SLIDER_SIMULATED_X, data.slider.simulated_x);

  // Button A/B -- Slider Up and Down
  UpdateBooleanComponentSafe(ComponentIndex::SLIDER_UP_CLICK, data.slider.upTouch);
  UpdateBooleanComponentSafe(ComponentIndex::SLIDER_DOWN_CLICK, data.slider.downTouch);

  // Thumbpad
  //vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_VALUE], data.thumbpad.value, 0);
  UpdateScalarComponentSafe(ComponentIndex::TRACKPAD_FORCE, data.thumbpad.force);
  UpdateBooleanComponentSafe(ComponentIndex::TRACKPAD_TOUCH, data.thumbpad.touch);
  UpdateBooleanComponentSafe(ComponentIndex::TRACKPAD_CLICK, data.thumbpad.click);
  UpdateScalarComponentSafe(ComponentIndex::TRACKPAD_X, data.thumbpad.x);
  UpdateScalarComponentSafe(ComponentIndex::TRACKPAD_Y, data.thumbpad.y);

  // Proximity Button
  UpdateBooleanComponentSafe(ComponentIndex::PROXIMITY_TOUCH, data.proximity.touch);
  UpdateBooleanComponentSafe(ComponentIndex::PROXIMITY_CLICK, data.proximity.click);
  UpdateScalarComponentSafe(ComponentIndex::PROXIMITY_VALUE, data.proximity.value);

  // Fingers
  UpdateScalarComponentSafe(ComponentIndex::THUMB_PULL, data.fingers[0].pull);
  UpdateScalarComponentSafe(ComponentIndex::THUMB_FORCE, data.fingers[0].force);
  UpdateBooleanComponentSafe(ComponentIndex::THUMB_CLICK, data.fingers[0].click);
  UpdateBooleanComponentSafe(ComponentIndex::THUMB_TOUCH, data.fingers[0].touch);

  UpdateScalarComponentSafe(ComponentIndex::INDEX_PULL, data.fingers[1].pull);
  UpdateScalarComponentSafe(ComponentIndex::INDEX_FORCE, data.fingers[1].force);
  UpdateBooleanComponentSafe(ComponentIndex::INDEX_CLICK, data.fingers[1].click);
  UpdateBooleanComponentSafe(ComponentIndex::INDEX_TOUCH, data.fingers[1].touch);

  UpdateScalarComponentSafe(ComponentIndex::MIDDLE_PULL, data.fingers[2].pull);
  UpdateScalarComponentSafe(ComponentIndex::MIDDLE_FORCE, data.fingers[2].force);
  UpdateBooleanComponentSafe(ComponentIndex::MIDDLE_CLICK, data.fingers[2].click);
  UpdateBooleanComponentSafe(ComponentIndex::MIDDLE_TOUCH, data.fingers[2].touch);

  UpdateScalarComponentSafe(ComponentIndex::RING_PULL, data.fingers[3].pull);
  UpdateScalarComponentSafe(ComponentIndex::RING_FORCE, data.fingers[3].force);
  UpdateBooleanComponentSafe(ComponentIndex::RING_CLICK, data.fingers[3].click);
  UpdateBooleanComponentSafe(ComponentIndex::RING_TOUCH, data.fingers[3].touch);

  UpdateScalarComponentSafe(ComponentIndex::PINKY_PULL, data.fingers[4].pull);
  UpdateScalarComponentSafe(ComponentIndex::PINKY_FORCE, data.fingers[4].force);
  UpdateBooleanComponentSafe(ComponentIndex::PINKY_CLICK, data.fingers[4].click);
  UpdateBooleanComponentSafe(ComponentIndex::PINKY_TOUCH, data.fingers[4].touch);

  // Grip gesture
  UpdateScalarComponentSafe(ComponentIndex::GRIP_VALUE, data.gesture.gripPull);
  UpdateScalarComponentSafe(ComponentIndex::GRIP_FORCE, data.gesture.gripForce);
  UpdateBooleanComponentSafe(ComponentIndex::GRIP_TOUCH, data.gesture.gripTouch);
  UpdateBooleanComponentSafe(ComponentIndex::GRIP_CLICK, data.gesture.gripClick);

  //  Pinch Gestures
  UpdateScalarComponentSafe(ComponentIndex::PINCH_A_VALUE, data.gesture.pinchAPull);
  UpdateBooleanComponentSafe(ComponentIndex::PINCH_A_CLICK, data.gesture.pinchAClick);

  UpdateScalarComponentSafe(ComponentIndex::PINCH_B_VALUE, data.gesture.pinchBPull);
  UpdateBooleanComponentSafe(ComponentIndex::PINCH_B_CLICK, data.gesture.pinchBClick);

  // Point Gesture
  UpdateBooleanComponentSafe(ComponentIndex::POINT_B_CLICK, data.gesture.pointBClick);

  // Battery percentage
  if (m_batteryRingBuffer.IsValid()) {
    // Push the current battery value to the ring buffer so that we can eliminate random "battery low" warnings
    m_batteryRingBuffer.Push((uint8_t)(data.system.battery * 100.0f));
    vr::VRProperties()->SetFloatProperty(m_props, vr::Prop_DeviceBatteryPercentage_Float, m_batteryRingBuffer.MostCommonElement() / 100.0f);
  } else {
    // Fallback to raw battery value
    vr::VRProperties()->SetFloatProperty(m_props, vr::Prop_DeviceBatteryPercentage_Float, data.system.battery);
  }

  if (m_lastInput.system.trackerConnection != data.system.trackerConnection) {
    m_controllerPose->SetEteeTrackerIsConnected(data.system.trackerConnection);
  }

  if (m_lastInput.system.adaptorConnection != data.system.adaptorConnection) {
    m_controllerPose->SetAdaptorIsConnected(data.system.adaptorConnection, data.isRight);
  }

  {
    std::lock_guard<std::mutex> lock(m_inputMutex);

    m_lastInput = data;
  }

#ifdef _DEBUG
  {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    if (now > m_startUpdateCountTime + std::chrono::milliseconds(1000)) {
      std::chrono::milliseconds milliSinceLastUpdate = now - m_startUpdateCountTime;
      // DebugDriverLog("%s Hand updated %i times last second", IsRightHand() ? "Right" : "Left", m_updateCount);

      m_updateCount = 0;
      m_startUpdateCountTime = now;
    } else
      m_updateCount++;
  }
#endif
}

void EteeDeviceDriver::InputUpdateThread() {
  DriverLog("Breadcrumb: InputUpdateThread entered");
  while (m_isActive) {
    {
      std::lock_guard<std::mutex> lock(m_inputMutex);
      if (m_lastInput.isValid) {
        m_boneAnimator->ComputeSkeletonTransforms(m_handTransforms, m_lastInput);
      }
    }

    UpdateSkeletonComponentSafe(vr::VRSkeletalMotionRange_WithoutController);
    UpdateSkeletonComponentSafe(vr::VRSkeletalMotionRange_WithController);

    if (!m_loggedFirstInputThreadCycle.exchange(true)) {
      DriverLog("Breadcrumb: InputUpdateThread first skeleton update cycle complete");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  DriverLog("Breadcrumb: InputUpdateThread exiting");
}

void EteeDeviceDriver::OnStateUpdate(VRControllerState data) {
  if (data == m_deviceState || (data == VRControllerState::ready && m_deviceState == VRControllerState::streaming)) return;

  DriverLog(
      "%s hand updated. Connected: %s Streaming: %s",
      IsRightHand() ? "Right" : "Left",
      data != VRControllerState::disconnected ? "Yes" : "No",
      data == VRControllerState::streaming ? "Yes" : "No");

  m_deviceState = data;

  m_controllerPose->SetDeviceState(m_deviceState);

  // If we're active let's also update the status pose. We might get events before we've activated
  if (!IsActive()) return;

  // then update the pose to reflect status
  vr::DriverPose_t pose = m_controllerPose->GetStatusPose();
  vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_driverId, pose, sizeof(vr::DriverPose_t));
}

// I've never seen this function called, but will add an empty pose for good measure.
vr::DriverPose_t EteeDeviceDriver::GetPose() {
  return m_controllerPose->UpdatePose();
}

void EteeDeviceDriver::PoseUpdateThread() {
  DriverLog("Breadcrumb: PoseUpdateThread entered");
  while (m_isActive) {
    vr::DriverPose_t pose = m_controllerPose->UpdatePose();
    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_driverId, pose, sizeof(vr::DriverPose_t));

    if (!m_loggedFirstPoseThreadCycle.exchange(true)) {
      DriverLog("Breadcrumb: PoseUpdateThread first pose update pushed");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  DriverLog("Closing pose thread");
}

void EteeDeviceDriver::OnVREvent(const vr::VREvent_t& vrEvent) {
  switch (vrEvent.eventType) {
    case vr::VREvent_Input_HapticVibration: {
      if (vrEvent.data.hapticVibration.componentHandle == m_inputComponentHandles[ComponentIndex::HAPTIC] && m_configuration.haptic.enabled) {
        const HapticEventData data = OnHapticEvent(vrEvent.data.hapticVibration);
        m_deviceEventCallback({DeviceEventType::HAPTIC_EVENT, data});

        break;
      }
    }
  }
}

HapticEventData EteeDeviceDriver::OnHapticEvent(const vr::VREvent_HapticVibration_t& hapticEvent) {
  // No haptics if amplitude or frequency is 0.
  if (hapticEvent.fAmplitude <= 0.0f || hapticEvent.fFrequency <= 0.0f) {
    return {0, 0, 0};
  }

  const float duration = std::clamp(hapticEvent.fDurationSeconds, 0.f, 10.f);
  float frequency = std::clamp(hapticEvent.fFrequency, 1000000.f / 65535.f, 1000000.f / 300.f);
  float amplitude = std::clamp(hapticEvent.fAmplitude, 0.f, 1.f);
  //DebugDriverLog("Received haptic vibration: freq: %f amp: %f dur: %f", frequency, amplitude, duration);

  frequency = 170.0f;
  //DebugDriverLog("Mapped haptic vibration: freq: %f amp: %f dur: %f", frequency, amplitude, duration);

  if (duration < 0.f) {
    const int onDurationUs = (int)(amplitude * 4000.f);
    return {
        onDurationUs,
        onDurationUs,
        1,
    }; 
  }

  float dutyCycle = 0.1f;

  const float periodSeconds = 1 / frequency;
  float maxPulseDurationSeconds = std::min(0.003999f, dutyCycle * periodSeconds);
  float onDurationSeconds = Lerp(0.000080f, maxPulseDurationSeconds, amplitude);

  const int pulseCount = (int)std::clamp(duration * frequency, 1.f, 65535.f);
  int onDurationUs = (int)std::clamp(1000000.f * onDurationSeconds, 0.f, 65535.f);
  int offDurationUs = (int)std::clamp(1000000.f * (periodSeconds - onDurationSeconds), 0.f, 65535.f);

  // If vibration is too weak, map to minimum value
  // Note: min onDurationUs = 323; max onDurationUs = 588
  const int minimumOnDurationUs = 323;
  const float minimumPercentageFactor = minimumOnDurationUs/5882.f;

  if (onDurationUs < minimumOnDurationUs) {
    dutyCycle = minimumPercentageFactor / amplitude;
    //DebugDriverLog("New duty cycle: %f", dutyCycle);

    maxPulseDurationSeconds = std::min(0.003999f, dutyCycle * periodSeconds);
    onDurationSeconds = Lerp(0.000080f, maxPulseDurationSeconds, amplitude);

    onDurationUs = (int)std::clamp(1000000.f * onDurationSeconds, 0.f, 65535.f);
    offDurationUs = (int)std::clamp(1000000.f * (periodSeconds - onDurationSeconds), 0.f, 65535.f);
  }

  return {
      onDurationUs,
      offDurationUs,
      pulseCount,
  };
}

vr::ETrackedControllerRole EteeDeviceDriver::GetDeviceRole() const {
  return m_configuration.role;
}

void EteeDeviceDriver::SetTrackerId(short deviceId, bool isRightHand) {
  m_controllerPose->SetShadowEteeTracker(deviceId, isRightHand);
}

VRControllerState EteeDeviceDriver::GetControllerState() {
  return m_deviceState;
}

void EteeDeviceDriver::Deactivate() {
  if (m_isActive.exchange(false)) {
    if (m_poseUpdateThread.joinable()) {
      m_poseUpdateThread.join();
    }
    if (m_inputUpdateThread.joinable()) {
      m_inputUpdateThread.join();
    }
  }
}

void* EteeDeviceDriver::GetComponent(const char* pchComponentNameAndVersion) {
  return nullptr;
}

void EteeDeviceDriver::EnterStandby() {
  m_deviceEventCallback({DeviceEventType::STANDBY, {}});

  m_controllerPose->SetDeviceState(VRControllerState::disconnected);
}

void EteeDeviceDriver::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) {
  if (unResponseBufferSize >= 1) {
    pchResponseBuffer[0] = 0;
  }
}

bool EteeDeviceDriver::TryCreateBooleanComponent(ComponentIndex index, const char* path, bool critical) {
  const vr::EVRInputError error = vr::VRDriverInput()->CreateBooleanComponent(m_props, path, &m_inputComponentHandles[index]);
  if (error == vr::VRInputError_None && m_inputComponentHandles[index] != vr::k_ulInvalidInputComponentHandle) {
    m_createdComponentMask.fetch_or(InputComponentBit(index));
    return true;
  }

  m_inputComponentHandles[index] = vr::k_ulInvalidInputComponentHandle;
  m_failedCreateComponentMask.fetch_or(InputComponentBit(index));
  DriverLog(
      "CreateBooleanComponent failed index=%d path=%s error=%d critical=%d",
      static_cast<int>(index),
      path,
      static_cast<int>(error),
      critical ? 1 : 0);
  return !critical;
}

bool EteeDeviceDriver::TryCreateScalarComponent(
    ComponentIndex index, const char* path, vr::EVRScalarType scalarType, vr::EVRScalarUnits scalarUnits, bool critical) {
  const vr::EVRInputError error =
      vr::VRDriverInput()->CreateScalarComponent(m_props, path, &m_inputComponentHandles[index], scalarType, scalarUnits);
  if (error == vr::VRInputError_None && m_inputComponentHandles[index] != vr::k_ulInvalidInputComponentHandle) {
    m_createdComponentMask.fetch_or(InputComponentBit(index));
    return true;
  }

  m_inputComponentHandles[index] = vr::k_ulInvalidInputComponentHandle;
  m_failedCreateComponentMask.fetch_or(InputComponentBit(index));
  DriverLog(
      "CreateScalarComponent failed index=%d path=%s error=%d critical=%d",
      static_cast<int>(index),
      path,
      static_cast<int>(error),
      critical ? 1 : 0);
  return !critical;
}

bool EteeDeviceDriver::TryCreateHapticComponent(ComponentIndex index, const char* path, bool critical) {
  const vr::EVRInputError error = vr::VRDriverInput()->CreateHapticComponent(m_props, path, &m_inputComponentHandles[index]);
  if (error == vr::VRInputError_None && m_inputComponentHandles[index] != vr::k_ulInvalidInputComponentHandle) {
    m_createdComponentMask.fetch_or(InputComponentBit(index));
    return true;
  }

  m_inputComponentHandles[index] = vr::k_ulInvalidInputComponentHandle;
  m_failedCreateComponentMask.fetch_or(InputComponentBit(index));
  DriverLog(
      "CreateHapticComponent failed index=%d path=%s error=%d critical=%d",
      static_cast<int>(index),
      path,
      static_cast<int>(error),
      critical ? 1 : 0);
  return !critical;
}

bool EteeDeviceDriver::TryCreateSkeletonComponent(bool isRightHand, bool critical) {
  vr::VRBoneTransform_t gripLimitTransforms[NUM_BONES];
  m_boneAnimator->LoadGripLimitSkeletonByHand(gripLimitTransforms, isRightHand);

  const char* inputPath = isRightHand ? "/input/skeleton/right" : "/input/skeleton/left";
  const char* skeletonPath = isRightHand ? "/skeleton/hand/right" : "/skeleton/hand/left";

  const vr::EVRInputError error = vr::VRDriverInput()->CreateSkeletonComponent(
      m_props,
      inputPath,
      skeletonPath,
      "/pose/raw",
      vr::VRSkeletalTracking_Partial,
      gripLimitTransforms,
      NUM_BONES,
      &m_inputComponentHandles[ComponentIndex::SKELETON]);

  if (error == vr::VRInputError_None && m_inputComponentHandles[ComponentIndex::SKELETON] != vr::k_ulInvalidInputComponentHandle) {
    m_createdComponentMask.fetch_or(InputComponentBit(ComponentIndex::SKELETON));
    return true;
  }

  m_inputComponentHandles[ComponentIndex::SKELETON] = vr::k_ulInvalidInputComponentHandle;
  m_failedCreateComponentMask.fetch_or(InputComponentBit(ComponentIndex::SKELETON));
  DriverLog(
      "CreateSkeletonComponent failed index=%d inputPath=%s skeletonPath=%s error=%d critical=%d",
      static_cast<int>(ComponentIndex::SKELETON),
      inputPath,
      skeletonPath,
      static_cast<int>(error),
      critical ? 1 : 0);
  return !critical;
}

bool EteeDeviceDriver::HasAllCriticalComponents() const {
  return (m_createdComponentMask.load() & kCriticalCreateMask) == kCriticalCreateMask;
}

bool EteeDeviceDriver::WasComponentCreated(ComponentIndex index) const {
  return (m_createdComponentMask.load() & InputComponentBit(index)) != 0;
}

void EteeDeviceDriver::UpdateBooleanComponentSafe(ComponentIndex index, bool value) {
  if (!IsInputComponentHandleValid(index)) {
    if (ShouldLogInvalidHandle(index)) {
      DriverLog("Skipping UpdateBooleanComponent for invalid handle index=%d", static_cast<int>(index));
    }
    return;
  }

  const vr::EVRInputError error = vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[index], value, 0);
  if (error != vr::VRInputError_None && ShouldLogInvalidHandle(index)) {
    DriverLog("UpdateBooleanComponent error for index=%d error=%d", static_cast<int>(index), static_cast<int>(error));
  }
}

void EteeDeviceDriver::UpdateScalarComponentSafe(ComponentIndex index, float value) {
  if (!IsInputComponentHandleValid(index)) {
    if (ShouldLogInvalidHandle(index)) {
      DriverLog("Skipping UpdateScalarComponent for invalid handle index=%d", static_cast<int>(index));
    }
    return;
  }

  const vr::EVRInputError error = vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[index], value, 0);
  if (error != vr::VRInputError_None && ShouldLogInvalidHandle(index)) {
    DriverLog("UpdateScalarComponent error for index=%d error=%d", static_cast<int>(index), static_cast<int>(error));
  }
}

void EteeDeviceDriver::UpdateSkeletonComponentSafe(vr::EVRSkeletalMotionRange range) {
  const ComponentIndex skeletonIndex = ComponentIndex::SKELETON;
  if (!IsInputComponentHandleValid(skeletonIndex)) {
    if (ShouldLogInvalidHandle(skeletonIndex)) {
      DriverLog("Skipping UpdateSkeletonComponent for invalid skeleton handle");
    }
    return;
  }

  const vr::EVRInputError error =
      vr::VRDriverInput()->UpdateSkeletonComponent(m_inputComponentHandles[skeletonIndex], range, m_handTransforms, NUM_BONES);
  if (error != vr::VRInputError_None && ShouldLogInvalidHandle(skeletonIndex)) {
    DriverLog("UpdateSkeletonComponent error for range=%d error=%d", static_cast<int>(range), static_cast<int>(error));
  }
}

bool EteeDeviceDriver::IsInputComponentHandleValid(ComponentIndex index) const {
  return WasComponentCreated(index) && m_inputComponentHandles[index] != vr::k_ulInvalidInputComponentHandle;
}

bool EteeDeviceDriver::ShouldLogInvalidHandle(ComponentIndex index) {
  const uint64_t bit = InputComponentBit(index);
  const uint64_t oldMask = m_invalidHandleWarningMask.fetch_or(bit);
  return (oldMask & bit) == 0;
}