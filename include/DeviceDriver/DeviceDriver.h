#pragma once
#include "DeviceConfiguration.h"
#include "openvr_driver.h"

class IDeviceDriver : public vr::ITrackedDeviceServerDriver {
 public:
  virtual vr::EVRInitError Activate(uint32_t unObjectId) = 0;

  virtual void Deactivate() = 0;

  virtual void EnterStandby() = 0;

  virtual void* GetComponent(const char* pchComponentNameAndVersion) = 0;

  virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) = 0;

  virtual vr::DriverPose_t GetPose() = 0;

  virtual std::string GetSerialNumber() = 0;
  virtual bool IsActive() = 0;

  virtual const VRDeviceConfiguration& GetConfiguration() = 0;
};