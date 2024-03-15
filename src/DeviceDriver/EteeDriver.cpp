#include "DeviceDriver/EteeDriver.h"

#include <algorithm>
#include <utility>

#include "Util/DriverLog.h"
#include "Util/Math.h"

EteeDeviceDriver::EteeDeviceDriver(
    VRDeviceConfiguration configuration, std::unique_ptr<BoneAnimator>&& boneAnimator, std::function<void(const DeviceEvent& deviceEvent)> deviceEventCallback)
    : m_configuration(configuration),
      m_boneAnimator(std::move(boneAnimator)),
      m_driverId(-1),
      m_isActive(false),
      m_lastInput(false),
      m_deviceEventCallback(std::move(deviceEventCallback)),
      m_controllerPose(std::make_unique<ControllerPose>(m_configuration.pose)) {}

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

  m_driverId = unObjectId;

  m_props = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_driverId);

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

  // Inputs | These interact without the specified input profile so the paths need to be equivalent

  // System Info
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/system/click", &m_inputComponentHandles[ComponentIndex::SYSTEM_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/tracker_connection/click", &m_inputComponentHandles[ComponentIndex::TRACKERCONNECTION_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/adaptor_connection/click", &m_inputComponentHandles[ComponentIndex::ADAPTORCONNECTION_CLICK]);

  // Slider
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/slider/touch", &m_inputComponentHandles[ComponentIndex::SLIDER_TOUCH]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/slider/click", &m_inputComponentHandles[ComponentIndex::SLIDER_CLICK]);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/slider/y", &m_inputComponentHandles[ComponentIndex::SLIDER_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/slider/x", &m_inputComponentHandles[ComponentIndex::SLIDER_SIMULATED_X], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);

  // Slider Up and Down as Buttons
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/slider_up/click", &m_inputComponentHandles[ComponentIndex::SLIDER_UP_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/slider_down/click", &m_inputComponentHandles[ComponentIndex::SLIDER_DOWN_CLICK]);

  // Thumbpad / Trackpad
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/thumbpad/x", &m_inputComponentHandles[ComponentIndex::TRACKPAD_X], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/thumbpad/y", &m_inputComponentHandles[ComponentIndex::TRACKPAD_Y], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);

  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/thumbpad/touch", &m_inputComponentHandles[ComponentIndex::TRACKPAD_TOUCH]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/thumbpad/click", &m_inputComponentHandles[ComponentIndex::TRACKPAD_CLICK]);
  //vr::VRDriverInput()->CreateScalarComponent(m_props, "/input/thumbpad/pull", &m_inputComponentHandles[ComponentIndex::TRACKPAD_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/thumbpad/force", &m_inputComponentHandles[ComponentIndex::TRACKPAD_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);

  // Proximity button
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/proximity/touch", &m_inputComponentHandles[ComponentIndex::PROXIMITY_TOUCH]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/proximity/click", &m_inputComponentHandles[ComponentIndex::PROXIMITY_CLICK]);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/proximity/value", &m_inputComponentHandles[ComponentIndex::PROXIMITY_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);

  // Fingers
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/thumb/value", &m_inputComponentHandles[ComponentIndex::THUMB_PULL], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/thumb/force", &m_inputComponentHandles[ComponentIndex::THUMB_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/thumb/click", &m_inputComponentHandles[ComponentIndex::THUMB_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/thumb/touch", &m_inputComponentHandles[ComponentIndex::THUMB_TOUCH]);

  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/index/value", &m_inputComponentHandles[ComponentIndex::INDEX_PULL], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/index/force", &m_inputComponentHandles[ComponentIndex::INDEX_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/index/click", &m_inputComponentHandles[ComponentIndex::INDEX_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/index/touch", &m_inputComponentHandles[ComponentIndex::INDEX_TOUCH]);

  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/middle/value", &m_inputComponentHandles[ComponentIndex::MIDDLE_PULL], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/middle/force", &m_inputComponentHandles[ComponentIndex::MIDDLE_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/middle/click", &m_inputComponentHandles[ComponentIndex::MIDDLE_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/middle/touch", &m_inputComponentHandles[ComponentIndex::MIDDLE_TOUCH]);

  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/ring/value", &m_inputComponentHandles[ComponentIndex::RING_PULL], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/ring/force", &m_inputComponentHandles[ComponentIndex::RING_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/ring/click", &m_inputComponentHandles[ComponentIndex::RING_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/ring/touch", &m_inputComponentHandles[ComponentIndex::RING_TOUCH]);

  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/pinky/value", &m_inputComponentHandles[ComponentIndex::PINKY_PULL], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/finger/pinky/force", &m_inputComponentHandles[ComponentIndex::PINKY_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/pinky/click", &m_inputComponentHandles[ComponentIndex::PINKY_CLICK]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/finger/pinky/touch", &m_inputComponentHandles[ComponentIndex::PINKY_TOUCH]);

  // Grip Gesture
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/grip/value", &m_inputComponentHandles[ComponentIndex::GRIP_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/grip/force", &m_inputComponentHandles[ComponentIndex::GRIP_FORCE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/grip/touch", &m_inputComponentHandles[ComponentIndex::GRIP_TOUCH]);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/grip/click", &m_inputComponentHandles[ComponentIndex::GRIP_CLICK]);

  // Pinch Gestures
  vr::VRDriverInput()->CreateScalarComponent(
      m_props, "/input/pinch/value", &m_inputComponentHandles[ComponentIndex::PINCH_A_VALUE], vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/pinch/click", &m_inputComponentHandles[ComponentIndex::PINCH_A_CLICK]);
  vr::VRDriverInput()->CreateScalarComponent(
      m_props,
      "/input/pinch_thumbfinger/value",
      &m_inputComponentHandles[ComponentIndex::PINCH_B_VALUE],
      vr::VRScalarType_Absolute,
      vr::VRScalarUnits_NormalizedOneSided);
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/pinch_thumbfinger/click", &m_inputComponentHandles[ComponentIndex::PINCH_B_CLICK]);

  // Point Gesture
  vr::VRDriverInput()->CreateBooleanComponent(m_props, "/input/point_independent/click", &m_inputComponentHandles[ComponentIndex::POINT_B_CLICK]);

  // Haptics
  vr::VRDriverInput()->CreateHapticComponent(m_props, "/output/haptic", &m_inputComponentHandles[ComponentIndex::HAPTIC]);

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

  {
    vr::VRBoneTransform_t gripLimitTransforms[NUM_BONES];
    m_boneAnimator->LoadGripLimitSkeletonByHand(gripLimitTransforms, isRightHand);
    // Skeleton
    vr::EVRInputError error = vr::VRDriverInput()->CreateSkeletonComponent(
        m_props,
        isRightHand ? "/input/skeleton/right" : "/input/skeleton/left",
        isRightHand ? "/skeleton/hand/right" : "/skeleton/hand/left",
        "/pose/raw",
        vr::VRSkeletalTracking_Partial,
        gripLimitTransforms,
        NUM_BONES,
        &m_inputComponentHandles[ComponentIndex::SKELETON]);

    if (error != vr::VRInputError_None) {
      DriverLog("CreateSkeletonComponent failed.  Error: %s\n", error);
    }
  }

  StartDevice();

  m_boneAnimator->LoadDefaultSkeletonByHand(m_handTransforms, isRightHand);

  // SteamVR wants input at startup for skeletons
  vr::VRDriverInput()->UpdateSkeletonComponent(
      m_inputComponentHandles[ComponentIndex::SKELETON], vr::VRSkeletalMotionRange_WithoutController, m_handTransforms, NUM_BONES);
  vr::VRDriverInput()->UpdateSkeletonComponent(m_inputComponentHandles[ComponentIndex::SKELETON], vr::VRSkeletalMotionRange_WithController, m_handTransforms, NUM_BONES);

  m_isActive = true;

  return vr::VRInitError_None;
}

void EteeDeviceDriver::StartDevice() {
  DriverLog("Etee controller successfully initialised");

  vr::DriverPose_t pose = m_controllerPose->GetStatusPose();
  vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_driverId, pose, sizeof(vr::DriverPose_t));

  m_poseUpdateThread = std::thread(&EteeDeviceDriver::PoseUpdateThread, this);

  m_inputUpdateThread = std::thread(&EteeDeviceDriver::InputUpdateThread, this);
}

void EteeDeviceDriver::OnInputUpdate(VRCommInputData_t data) {
  // System Info

  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::SYSTEM_CLICK], data.system.systemClick, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::TRACKERCONNECTION_CLICK], data.system.trackerConnection, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::ADAPTORCONNECTION_CLICK], data.system.adaptorConnection, 0);

  // Slider
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::SLIDER_TOUCH], data.slider.touch, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::SLIDER_CLICK], data.slider.touch, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::SLIDER_VALUE], data.slider.value, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::SLIDER_SIMULATED_X], data.slider.simulated_x, 0);

  // Button A/B -- Slider Up and Down
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::SLIDER_UP_CLICK], data.slider.upTouch, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::SLIDER_DOWN_CLICK], data.slider.downTouch, 0);

  // Thumbpad
  //vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_VALUE], data.thumbpad.value, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_FORCE], data.thumbpad.force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_TOUCH], data.thumbpad.touch, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_CLICK], data.thumbpad.click, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_X], data.thumbpad.x, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::TRACKPAD_Y], data.thumbpad.y, 0);

  // Proximity Button
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PROXIMITY_TOUCH], data.proximity.touch, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PROXIMITY_CLICK], data.proximity.click, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::PROXIMITY_VALUE], data.proximity.value, 0);

  // Fingers
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::THUMB_PULL], data.fingers[0].pull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::THUMB_FORCE], data.fingers[0].force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::THUMB_CLICK], data.fingers[0].click, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::THUMB_TOUCH], data.fingers[0].touch, 0);

  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::INDEX_PULL], data.fingers[1].pull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::INDEX_FORCE], data.fingers[1].force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::INDEX_CLICK], data.fingers[1].click, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::INDEX_TOUCH], data.fingers[1].touch, 0);

  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::MIDDLE_PULL], data.fingers[2].pull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::MIDDLE_FORCE], data.fingers[2].force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::MIDDLE_CLICK], data.fingers[2].click, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::MIDDLE_TOUCH], data.fingers[2].touch, 0);

  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::RING_PULL], data.fingers[3].pull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::RING_FORCE], data.fingers[3].force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::RING_CLICK], data.fingers[3].click, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::RING_TOUCH], data.fingers[3].touch, 0);

  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::PINKY_PULL], data.fingers[4].pull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::PINKY_FORCE], data.fingers[4].force, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PINKY_CLICK], data.fingers[4].click, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PINKY_TOUCH], data.fingers[4].touch, 0);

  // Grip gesture
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::GRIP_VALUE], data.gesture.gripPull, 0);
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::GRIP_FORCE], data.gesture.gripForce, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::GRIP_TOUCH], data.gesture.gripTouch, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::GRIP_CLICK], data.gesture.gripClick, 0);

  //  Pinch Gestures
  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::PINCH_A_VALUE], data.gesture.pinchAPull, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PINCH_A_CLICK], data.gesture.pinchAClick, 0);

  vr::VRDriverInput()->UpdateScalarComponent(m_inputComponentHandles[ComponentIndex::PINCH_B_VALUE], data.gesture.pinchBPull, 0);
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::PINCH_B_CLICK], data.gesture.pinchBClick, 0);

  // Point Gesture
  vr::VRDriverInput()->UpdateBooleanComponent(m_inputComponentHandles[ComponentIndex::POINT_B_CLICK], data.gesture.pointBClick, 0);

  // Battery percentage
  vr::VRProperties()->SetFloatProperty(m_props, vr::Prop_DeviceBatteryPercentage_Float, data.system.battery);

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
  while (m_isActive) {
    {
      std::lock_guard<std::mutex> lock(m_inputMutex);
      if (m_lastInput.isValid) {
        m_boneAnimator->ComputeSkeletonTransforms(m_handTransforms, m_lastInput);
      }
    }

    vr::VRDriverInput()->UpdateSkeletonComponent(
        m_inputComponentHandles[ComponentIndex::SKELETON], vr::VRSkeletalMotionRange_WithoutController, m_handTransforms, NUM_BONES);
    vr::VRDriverInput()->UpdateSkeletonComponent(
        m_inputComponentHandles[ComponentIndex::SKELETON], vr::VRSkeletalMotionRange_WithController, m_handTransforms, NUM_BONES);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
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
  while (m_isActive) {
    vr::DriverPose_t pose = m_controllerPose->UpdatePose();
    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_driverId, pose, sizeof(vr::DriverPose_t));

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
  DebugDriverLog("Received haptic vibration: freq: %f amp: %f dur: %f", frequency, amplitude, duration);

  frequency = 170.0f;
  DebugDriverLog("Mapped haptic vibration: freq: %f amp: %f dur: %f", frequency, amplitude, duration);

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
    DebugDriverLog("New duty cycle: %f", dutyCycle);

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
    m_poseUpdateThread.join();
    m_inputUpdateThread.join();
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