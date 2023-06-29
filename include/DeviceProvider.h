#pragma once

#undef _WINSOCKAPI_
#define _WINSOCKAPI_

#include <openvr_driver.h>

#include <memory>
#include <thread>

#include "Communication/CommunicationManager.h"
#include "Communication/SerialCommunicationManager.h"
#include "DeviceConfiguration.h"
#include "DeviceDriver/DeviceDriver.h"
#include "DeviceDriver/EteeDriver.h"
#include "Encode/EncodingManager.h"
#include "Encode/EteeEncodingManager.h"
#include "TrackerDiscovery.h"
#include "Util/DriverLog.h"

class DeviceProvider : public vr::IServerTrackedDeviceProvider {
 public:
  DeviceProvider();

  vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
  void Cleanup() override;
  const char* const* GetInterfaceVersions() override;
  void RunFrame() override;
  bool ShouldBlockStandbyMode() override;
  void EnterStandby() override;
  void LeaveStandby() override;

 private:
  void QueryDeviceThread();

  void HandleDongleStateUpdate(VRDongleState dongleState);
  void HandleControllerStateUpdate(VRHandedControllerState_t controllerState);
  void HandleDeviceEvent(const vr::ETrackedControllerRole role, const DeviceEvent& event);

  VRDeviceConfiguration m_leftConfiguration{};
  VRDeviceConfiguration m_rightConfiguration{};
  std::unique_ptr<EteeDeviceDriver> m_leftHand;
  std::unique_ptr<EteeDeviceDriver> m_rightHand;
  VRDeviceConfiguration GetDeviceConfiguration(vr::ETrackedControllerRole role);
  std::unique_ptr<SerialCommunicationManager> m_communicationManager;
  std::unique_ptr<TrackerDiscovery> m_trackerDiscovery;

  std::atomic<bool> m_isActive;
  std::thread m_queryDeviceThread;

  void GetDriverVersion();
  std::string DriverVersionToString(int majorNumber, int minorNumber, int patchNumber, std::string buildLabel, int buildLabelNumber);
};