#include "ControllerPose.h"

#include <queue>
#include <utility>

#include "TrackerDiscovery.h"
#include "Util/DriverLog.h"
#include "Util/Quaternion.h"
#include "math.h"

#define _USE_MATH_DEFINES

// Manufacturer and device type for etee trackers
const char* c_eteeTrackerManufacturer = "TG0";  // Separate from the controller manufacturer
const char* c_eteeTrackerControllerType =
    "vive_tracker";  // The etee tracker is mimicking a vive_tracker (see input json file). The only way to differentiate them is with the manufaccturer value.
                     // When using the etee tracker as handed. the controller type doesnt seem to change (i.e. "etee_tracker_handed")

// The device type of third-party trackers changes to "_handed" when you set the role to a hand in Manage Trackers. However, the suffix is lost after SVR restart, so we
// need to have both device type versions for the filter.
// - Note 1: The "_handed" suffix is independent from the tracker role. Might be a bug on SVR or VIVE's side.
// - Note 2: The device type not being "_handed" will not affect the tracker hand assignment. The DiscoverTrackedDevice() function already incorporates a filter that
// differentiates between trackers with and without Manage Tracker > Hand roles.
// - Note 3: This does not interfere with non-hand tracking (e.g. body tracking, etc)
const char* c_viveTrackerControllerType = "vive_tracker";
const char* c_viveTrackerHandedControllerType = "vive_tracker_handed";

const char* c_viveTrackerManufacturer = "HTC";
const char* c_tundraTrackerManufacturer = "Tundra Labs";

ControllerPose::ControllerPose(VRPoseConfiguration configuration)
    : m_configuration(configuration),
      m_shadowTrackerId(-1),
      m_eteeTrackerConnected(false),
      m_eteeTrackerThruRole(false),  // Disable assignment of role through SVR Manage Trackers, use auto-hand-assignment
      m_state(){};

void ControllerPose::DiscoverTrackedDevice() {
  if (m_eteeTrackerConnected) return;

  for (int32_t i = 1; i < vr::k_unMaxTrackedDeviceCount; i++) {
    vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(i);

    std::string manufacturer = vr::VRProperties()->GetStringProperty(container, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String);
    std::string deviceType = vr::VRProperties()->GetStringProperty(container, vr::ETrackedDeviceProperty::Prop_ControllerType_String);

    int32_t foundRole = vr::VRProperties()->GetInt32Property(container, vr::ETrackedDeviceProperty::Prop_ControllerRoleHint_Int32);

    if (manufacturer == c_deviceManufacturer) continue;  // make sure we're not targeting our own controller device
    if (foundRole != m_configuration.role) continue;     // make sure we're targeting the right role

    bool isRightHand = foundRole == vr::TrackedControllerRole_RightHand;

    // If it's an eteeTracker
    if (manufacturer == c_eteeTrackerManufacturer && deviceType == c_eteeTrackerControllerType) {
      DriverLog(
          "Identified a handed etee tracker! Hand: %s Setting controller to use corresponding tracker but will be overriden if tracker connects to a controller",
          isRightHand ? "right" : "left");

      float newOffsetXPos, newOffsetYPos, newOffsetZPos = 0.0f;
      float newOffsetXRot, newOffsetYRot, newOffsetZRot = 0.0f;

      newOffsetXPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
      newOffsetYPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_position" : "left_y_offset_position");
      newOffsetZPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_position" : "left_z_offset_position");

      newOffsetXRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_rotation" : "left_x_offset_rotation");
      newOffsetYRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_rotation" : "left_y_offset_rotation");
      newOffsetZRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_rotation" : "left_z_offset_rotation");

      m_configuration.offsetVector = {newOffsetXPos, newOffsetYPos, newOffsetZPos};
      m_configuration.angleOffsetQuaternion = EulerToQuaternion(DegToRad(newOffsetXRot), DegToRad(newOffsetYRot), DegToRad(newOffsetZRot));
      m_trackerIsEteeTracker = true;
      m_eteeTrackerThruRole = false;
    }

    // If it's a third-party tracker
    else if (manufacturer != c_eteeTrackerManufacturer && (deviceType == c_viveTrackerControllerType || deviceType == c_viveTrackerHandedControllerType)) {
      DriverLog(
          "Identified a %s 3rd-party tracker with handed role. Manufacturer: %s - Controller Type: %s",
          isRightHand ? "right" : "left",
          manufacturer.c_str(),
          deviceType.c_str());

      float newOffsetXPos = 0.0f, newOffsetYPos = 0.0f, newOffsetZPos = 0.0f;
      float newOffsetXRot = 0.0f, newOffsetYRot = 0.0f, newOffsetZRot = 0.0f;

      bool adaptorConnection = isRightHand ? m_adaptorConnRight : m_adaptorConnLeft;
      DriverLog("Adaptor Connection for %s hand: %s", isRightHand ? "right" : "left", adaptorConnection ? "true" : "false");

      // Identifying if it's a VIVE tracker
      if (manufacturer == c_viveTrackerManufacturer) {
        //if (adaptorConnection) {
        //  DriverLog("VIVE tracker (Smart Adaptor) offsets applied to controller rendermodel.");

        //  newOffsetXPos = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "x_offset_position");
        //  newOffsetYPos = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "y_offset_position");
        //  newOffsetZPos = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "z_offset_position");

        //  newOffsetXRot = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "x_offset_rotation");
        //  newOffsetYRot = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "y_offset_rotation");
        //  newOffsetZRot = vr::VRSettings()->GetFloat("vive_tracker_smart_adaptor_pose_settings", "z_offset_rotation");
        //} else {
          DriverLog("VIVE tracker (Basic Adaptor) offsets applied to controller rendermodel.");

          newOffsetXPos = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
          newOffsetYPos = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_y_offset_position" : "left_y_offset_position");
          newOffsetZPos = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_z_offset_position" : "left_z_offset_position");

          newOffsetXRot = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_x_offset_rotation" : "left_x_offset_rotation");
          newOffsetYRot = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_y_offset_rotation" : "left_y_offset_rotation");
          newOffsetZRot = vr::VRSettings()->GetFloat("vive_tracker_basic_adaptor_pose_settings", isRightHand ? "right_z_offset_rotation" : "left_z_offset_rotation");
        //}
      }

      // Identifying if it's a Tundra tracker
      else if (manufacturer == c_tundraTrackerManufacturer) {
        DriverLog("Adaptor Connection for %s hand: %s", isRightHand ? "right" : "left", adaptorConnection ? "true" : "false");
        //if (adaptorConnection) {
        //  DriverLog("Tundra tracker (Smart Adaptor) offsets applied to controller rendermodel.");

        //  newOffsetXPos = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "x_offset_position");
        //  newOffsetYPos = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "y_offset_position");
        //  newOffsetZPos = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "z_offset_position");

        //  newOffsetXRot = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "x_offset_rotation");
        //  newOffsetYRot = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "y_offset_rotation");
        //  newOffsetZRot = vr::VRSettings()->GetFloat("tundra_tracker_smart_adaptor_pose_settings", "z_offset_rotation");
        //} else {
          DriverLog("Tundra tracker (Basic Adaptor) offsets applied to controller rendermodel.");

          newOffsetXPos = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
          newOffsetYPos = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_y_offset_position" : "left_y_offset_position");
          newOffsetZPos = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_z_offset_position" : "left_z_offset_position");

          newOffsetXRot = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_x_offset_rotation" : "left_x_offset_rotation");
          newOffsetYRot = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_y_offset_rotation" : "left_y_offset_rotation");
          newOffsetZRot = vr::VRSettings()->GetFloat("tundra_tracker_basic_adaptor_pose_settings", isRightHand ? "right_z_offset_rotation" : "left_z_offset_rotation");
        //}
      }

      m_configuration.offsetVector = {newOffsetXPos, newOffsetYPos, newOffsetZPos};
      m_configuration.angleOffsetQuaternion = EulerToQuaternion(DegToRad(newOffsetXRot), DegToRad(newOffsetYRot), DegToRad(newOffsetZRot));
      m_trackerIsEteeTracker = false;
      m_eteeTrackerThruRole = true;
    }

    m_shadowTrackerId = i;
    DriverLog(
        "Found a tracked device to use for positioning, id: %i, manufacturer: %s, device type: %s, role: %i", i, manufacturer.c_str(), deviceType.c_str(), foundRole);
  }
};

vr::TrackedDevicePose_t ControllerPose::GetTrackerPose() {
  vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
  vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);
  return trackedDevicePoses[m_shadowTrackerId];
}

vr::DriverPose_t ControllerPose::GetStatusPose() {
  vr::DriverPose_t pose = {0};
  pose.deviceIsConnected = m_state != VRControllerState::disconnected;
  pose.poseIsValid = false;
  pose.result = vr::ETrackingResult::TrackingResult_Uninitialized;
  return pose;
}

vr::DriverPose_t ControllerPose::UpdatePose() {
  vr::DriverPose_t pose = {0};
  pose.qDriverFromHeadRotation.w = 1;
  pose.qWorldFromDriverRotation.w = 1;

  vr::TrackedDevicePose_t trackerPose = GetTrackerPose();

  if (m_shadowTrackerId < 0) {
    DiscoverTrackedDevice();

    return GetStatusPose();
  }
   
  if (m_state != VRControllerState::streaming || m_state == VRControllerState::disconnected ||
      (!m_eteeTrackerConnected && m_trackerIsEteeTracker && !m_eteeTrackerThruRole))
    return GetStatusPose(); 

  pose.deviceIsConnected = m_state != VRControllerState::disconnected;
  pose.poseIsValid = trackerPose.bPoseIsValid;
  pose.result = vr::TrackingResult_Running_OK;

  vr::HmdVector3_t trackerPosition = GetPosition(trackerPose.mDeviceToAbsoluteTracking);
  vr::HmdQuaternion_t trackerRotation = GetRotation(trackerPose.mDeviceToAbsoluteTracking);

  vr::HmdQuaternion_t trackerToWorld = {1.0, 0.0, 0.0, 0.0};
  if (m_trackerIsEteeTracker) { // for some reason the etee trackers aren't correctly aligned to steamvr coordinates - they're 90 degrees off
    trackerToWorld = EulerToQuaternion(DegToRad(90.0), DegToRad(0.0), DegToRad(0.0));
  }
   
  const vr::HmdVector3_t controllerPosition = trackerPosition + (m_configuration.offsetVector * -trackerToWorld * trackerRotation);
  pose.vecPosition[0] = controllerPosition.v[0];
  pose.vecPosition[1] = controllerPosition.v[1];
  pose.vecPosition[2] = controllerPosition.v[2];

  const vr::HmdQuaternion_t controllerRotation = trackerRotation * m_configuration.angleOffsetQuaternion;
  pose.qRotation = controllerRotation;

  const vr::HmdVector3_t& trackerVelocity = trackerPose.vVelocity;
  pose.vecVelocity[0] = trackerVelocity.v[0];
  pose.vecVelocity[1] = trackerVelocity.v[1];
  pose.vecVelocity[2] = trackerVelocity.v[2];

  return pose;
}

void ControllerPose::SetDeviceState(VRControllerState state) {
  m_state = state;
}

void ControllerPose::SetEteeTrackerIsConnected(bool eteeTrackerConnected) {
  DriverLog(
      "eteeController reported eteeTracker was %s from %s hand.",
      eteeTrackerConnected ? "connected" : "disconnected",
      m_configuration.role == vr::TrackedControllerRole_LeftHand ? "left" : "right");

  // Commented out to disable the controller rendermodel from detaching from the tracker when tracker_connection is false.
  // m_eteeTrackerConnected = eteeTrackerConnected;
}

void ControllerPose::SetAdaptorIsConnected(bool adaptorConnected, bool isRight) {
  DriverLog(
      "eteeController reported eteeAdaptor (Smart) was %s for %s hand.",
      adaptorConnected ? "connected" : "disconnected", isRight ? "right" : "left");

  if (isRight) {
    m_adaptorConnRight = adaptorConnected;
  } else {
    m_adaptorConnLeft = adaptorConnected;
  }
}

void ControllerPose::SetShadowEteeTracker(short deviceId, bool isRightHand) {
  // Commented out to allow an etee tracker to replace a 3DP tracker hand role as long as the new etee tracker physically connects to the controller. This works even if
  // the 3DPT is ON.
  /*
  if (m_eteeTrackerThruRole) {
    DriverLog("eteeTracker discovered, but not updating as eteeTracker has role set. Remove role and restart.");
    return;
  };
  */

  // Commented out to disable the controller rendermodel from detaching from the tracker when tracker_connection is false.
  /*
  if (m_eteeTrackerConnected && m_shadowTrackerId > 0) {
    DriverLog("eteeTracker is already connected!");
    return;
  }
  */

  if (m_eteeTrackerConnected == deviceId) return;

  DriverLog("Setting pose to that of connected etee tracker with id: %i and hand: %s", deviceId, isRightHand ? "right" : "left");

  float newOffsetXPos, newOffsetYPos, newOffsetZPos = 0.0f;
  newOffsetXPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
  newOffsetYPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_position" : "left_y_offset_position");
  newOffsetZPos = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_position" : "left_z_offset_position");

  float newOffsetXRot, newOffsetYRot, newOffsetZRot = 0.0f;
  newOffsetXRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_x_offset_rotation" : "left_x_offset_rotation");
  newOffsetYRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_y_offset_rotation" : "left_y_offset_rotation");
  newOffsetZRot = vr::VRSettings()->GetFloat("etee_tracker_pose_settings", isRightHand ? "right_z_offset_rotation" : "left_z_offset_rotation");

  DebugDriverLog("Offsets applied: %s", isRightHand ? "right_x_offset_position" : "left_x_offset_position");
  m_configuration.offsetVector = {newOffsetXPos, newOffsetYPos, newOffsetZPos};
  m_configuration.angleOffsetQuaternion = EulerToQuaternion(DegToRad(newOffsetXRot), DegToRad(newOffsetYRot), DegToRad(newOffsetZRot));

  m_shadowTrackerId = deviceId;

  m_trackerIsEteeTracker = true;
  m_eteeTrackerThruRole = false;
  m_eteeTrackerConnected = true;
}