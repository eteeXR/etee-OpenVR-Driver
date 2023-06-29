#include <DeviceProvider.h>

#include "Communication/SerialCommunicationManager.h"
#include "Hooks/InterfaceHookInjector.h"
#include "Util/DriverLog.h"
#include "Util/Quaternion.h"
#include "Util/Windows.h"

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "?"
#endif

VRDeviceConfiguration DeviceProvider::GetDeviceConfiguration(vr::ETrackedControllerRole role) {
  // Get driver Settings
  const bool isRightHand = role == vr::TrackedControllerRole_RightHand;

  const bool enabled = vr::VRSettings()->GetBool(c_driverSettingsSection, isRightHand ? "right_enabled" : "left_enabled");
  const bool serialPortOverride = vr::VRSettings()->GetBool(c_driverSettingsSection, "override_serial_port");
  const int port = vr::VRSettings()->GetInt32(c_driverSettingsSection, "serial_port");

  // Default controller rendermodel offsets are for etee trackers:
  float offsetXPos, offsetYPos, offsetZPos = 0.0f;
  float offsetXRot, offsetYRot, offsetZRot = 0.0f;

  offsetXPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
  offsetYPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_position" : "left_y_offset_position");
  offsetZPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_position" : "left_z_offset_position");

  offsetXRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_rotation" : "left_x_offset_rotation");
  offsetYRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_rotation" : "left_y_offset_rotation");
  offsetZRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_rotation" : "left_z_offset_rotation");

  // Haptic settings
  const bool hapticEnabled = vr::VRSettings()->GetBool("etee_haptic_settings", "enabled");

  return VRDeviceConfiguration(
      role,
      enabled,
      VRHapticConfiguration(hapticEnabled),
      VRSerialConfiguration(serialPortOverride, port),
      VRPoseConfiguration(role, {offsetXPos, offsetYPos, offsetZPos}, EulerToQuaternion(DegToRad(offsetXRot), DegToRad(offsetYRot), DegToRad(offsetZRot))));
}

DeviceProvider::DeviceProvider() : m_isActive(false){};

void DeviceProvider::GetDriverVersion() {
  const char* c_driverVersionSection = "driver_etee_version";

  // Get driver version numbers
  const int majorNum = vr::VRSettings()->GetInt32(c_driverVersionSection, "major_num");
  const int minorNum = vr::VRSettings()->GetInt32(c_driverVersionSection, "minor_num");
  const int patchNum = vr::VRSettings()->GetInt32(c_driverVersionSection, "patch_num");

  // Get the build label (i.e. dev, alpha, beta, release) and label number
  vr::CVRSettingHelper settings_helper(vr::VRSettings());
  const std::string buildLab = settings_helper.GetString(c_driverVersionSection, "build_label");
  const int buildLabNum = vr::VRSettings()->GetInt32(c_driverVersionSection, "build_label_num");

  // Combine driver version
  const std::string driverVersion = DriverVersionToString(majorNum, minorNum, patchNum, buildLab, buildLabNum);

  // Print version number
  DriverLog("etee Driver - Version: %s", driverVersion.c_str());
}

std::string DeviceProvider::DriverVersionToString(int majorNumber, int minorNumber, int patchNumber, std::string buildLabel, int buildLabelNumber) {
  std::string versionString;

  // If it's a release version, no build label number is used. For example: 1.3.2-release
  if (buildLabel == "release") {
    versionString = std::to_string(majorNumber) + "." + std::to_string(minorNumber) + "." + std::to_string(patchNumber) + "-" + buildLabel;
  }

  // If it's a pre-release label (i.e. dev, alpha, beta), the build label number is used. For example: 1.3.2-alpha.3
  else {
    versionString =
        std::to_string(majorNumber) + "." + std::to_string(minorNumber) + "." + std::to_string(patchNumber) + "-" + buildLabel + "." + std::to_string(buildLabelNumber);
  }

  return versionString;
}

vr::EVRInitError DeviceProvider::Init(vr::IVRDriverContext* pDriverContext) {
  vr::EVRInitError initError = InitServerDriverContext(pDriverContext);
  if (initError != vr::EVRInitError::VRInitError_None) return initError;

  VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
  InitDriverLog(vr::VRDriverLog());

  // This won't print if running in release mode
  DebugDriverLog("etee Driver is running in DEBUG mode.");

  // Print driver files path
  const std::string driverPath = GetDriverPath();
  DriverLog("Path to DLL: %s", driverPath.c_str());

  // Print driver version
  GetDriverVersion();

  // Print driver git commit hash
  const std::string commitHash = GIT_COMMIT_HASH;
  DriverLog("Built from: %s", commitHash.substr(0, 7).c_str());

  m_trackerDiscovery = std::make_unique<TrackerDiscovery>(pDriverContext);
  m_trackerDiscovery->StartDiscovery([&](vr::ETrackedControllerRole role, int deviceId) {
    if (m_leftHand != nullptr && m_leftHand->GetDeviceRole() == role) {
      m_leftHand->SetTrackerId(deviceId, false);
    }

    if (m_rightHand != nullptr && m_rightHand->GetDeviceRole() == role) {
      m_rightHand->SetTrackerId(deviceId, true);
    }
  });

  m_leftConfiguration = GetDeviceConfiguration(vr::TrackedControllerRole_LeftHand);
  m_rightConfiguration = GetDeviceConfiguration(vr::TrackedControllerRole_RightHand);

  // If we haven't enabled a device no need to start a communication
  if (!m_rightConfiguration.enabled || !m_leftConfiguration.enabled) {
    DriverLog("Not loading etee controllers as they were disabled in settings.");
    return vr::VRInitError_None;
  }

  m_leftHand = std::make_unique<EteeDeviceDriver>(
      m_leftConfiguration,
      std::make_unique<BoneAnimator>(GetDriverPath() + "\\resources\\anims\\etee_curl_anim.glb", GetDriverPath() + "\\resources\\anims\\etee_accessory_anim.glb"),
      [&](const DeviceEvent& deviceEvent) { HandleDeviceEvent(vr::TrackedControllerRole_LeftHand, deviceEvent); });

  m_rightHand = std::make_unique<EteeDeviceDriver>(
      m_rightConfiguration,
      std::make_unique<BoneAnimator>(GetDriverPath() + "\\resources\\anims\\etee_curl_anim.glb", GetDriverPath() + "\\resources\\anims\\etee_accessory_anim.glb"),
      [&](const DeviceEvent& deviceEvent) { HandleDeviceEvent(vr::TrackedControllerRole_RightHand, deviceEvent); });

  std::unique_ptr<EteeEncodingManager> encodingManager = std::make_unique<EteeEncodingManager>((float)vr::VRSettings()->GetInt32("encoding_legacy", "max_analog_value"));

  m_communicationManager = std::make_unique<SerialCommunicationManager>(m_rightConfiguration.serial, std::move(encodingManager));
  m_communicationManager->BeginListener(
      [&](const VRCommInputData_t& data) {
        // Update left and right hand data
        if (data.isRight) {
          m_rightHand->OnInputUpdate(data);
        } else {
          m_leftHand->OnInputUpdate(data);
        }
      },

      [&](const VRStateEvent_t& stateEvent) {
        switch (stateEvent.type) {
          case VRStateEventType::dongle_state: {
            VRDongleState_t dongleState = stateEvent.data.dongleState;
            if (!dongleState.valid) return;

            HandleDongleStateUpdate(dongleState.state);
            break;
          };

          case VRStateEventType::controller_state: {
            VRHandedControllerState_t controllerState = stateEvent.data.handedControllerState;

            if (!controllerState.valid) break;
            HandleControllerStateUpdate(controllerState);
            break;
          };
          default:
            break;
        }
      });

  m_isActive = true;
  m_queryDeviceThread = std::thread(&DeviceProvider::QueryDeviceThread, this);

  return vr::VRInitError_None;
}

bool StateIsConnected(VRControllerState state) {
  return state == VRControllerState::streaming;
}

void DeviceProvider::QueryDeviceThread() {
  while (m_isActive) {
    m_communicationManager->WriteQueryDevices();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
}

void DeviceProvider::HandleDongleStateUpdate(VRDongleState dongleState) {
  DriverLog("Dongle Connected: %s", dongleState == VRDongleState::connected ? "Yes" : "No");

  if (dongleState == VRDongleState::disconnected) {
    DriverLog("Dongle is not connected. Will turn off controllers");
    m_rightHand->OnStateUpdate(VRControllerState::disconnected);
    m_leftHand->OnStateUpdate(VRControllerState::disconnected);

    return;
  }

  if (m_leftConfiguration.enabled)
    vr::VRServerDriverHost()->TrackedDeviceAdded(m_leftHand->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_leftHand.get());
  if (m_rightConfiguration.enabled)
    vr::VRServerDriverHost()->TrackedDeviceAdded(m_rightHand->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, m_rightHand.get());

  // Whenever we have a dongle connection we want to find out what devices are connected
  m_communicationManager->WriteQueryDevices();
}

void DeviceProvider::HandleControllerStateUpdate(VRHandedControllerState_t controllerState) {
  VRControllerState newControllerState = VRControllerState::disconnected;

  if (controllerState.role == vr::ETrackedControllerRole::TrackedControllerRole_RightHand) {
    m_rightHand->OnStateUpdate(controllerState.state);

    newControllerState = m_rightHand->GetControllerState();
  } else if (controllerState.role == vr::ETrackedControllerRole::TrackedControllerRole_LeftHand) {
    m_leftHand->OnStateUpdate(controllerState.state);

    newControllerState = m_leftHand->GetControllerState();
  } else {
    DriverLog("Unable to set controller state as no role was provided.");

    return;
  }

  if (newControllerState != VRControllerState::streaming) m_communicationManager->WriteActivate();
}

void DeviceProvider::Cleanup() {
  DriverLog("Cleanup called");

  if (m_isActive.exchange(false)) {
    if (m_queryDeviceThread.joinable()) m_queryDeviceThread.join();
  }

  m_communicationManager->Disconnect();
}

const char* const* DeviceProvider::GetInterfaceVersions() {
  return vr::k_InterfaceVersions;
}

void DeviceProvider::RunFrame() {
  vr::VREvent_t pEvent{};
  while (vr::VRServerDriverHost()->PollNextEvent(&pEvent, sizeof(pEvent))) {
    if (m_leftHand != nullptr && m_leftHand->IsActive()) m_leftHand->OnVREvent(pEvent);
    if (m_rightHand != nullptr && m_rightHand->IsActive()) m_rightHand->OnVREvent(pEvent);
  }
}

void DeviceProvider::HandleDeviceEvent(const vr::ETrackedControllerRole role, const DeviceEvent& event) {
  if (role != vr::ETrackedControllerRole::TrackedControllerRole_RightHand && role != vr::ETrackedControllerRole::TrackedControllerRole_LeftHand) {
    DriverLog("Unable to handle device event as no valid role was provided.");
    return;
  }

  switch (event.type) {
    case DeviceEventType::HAPTIC_EVENT: {
      const HapticEventData& hapticEvent = event.data;
      const std::string command = role == vr::ETrackedControllerRole::TrackedControllerRole_LeftHand ? "BL+PH=" : "BR+PH=";
      m_communicationManager->WriteCommand(
          command + std::to_string(hapticEvent.onDurationUs) + "," + std::to_string(hapticEvent.offDurationUs) + "," + std::to_string(hapticEvent.pulseCount) + ",");
      break;
    }

    case DeviceEventType::STANDBY: {
      DriverLog("Switching off %s hand controller", role == vr::TrackedControllerRole_LeftHand ? "left" : "right");

      const std::string command_string = role == vr::TrackedControllerRole_LeftHand ? "BL+ES" : "BR+ES";
      m_communicationManager->WriteCommand(command_string);

      break;
    }

    default:
      break;
  }
}

bool DeviceProvider::ShouldBlockStandbyMode() {
  return false;
}

void DeviceProvider::EnterStandby() {}

void DeviceProvider::LeaveStandby() {}