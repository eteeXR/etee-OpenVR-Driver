#pragma once
#include <functional>
#include <memory>

#include "Encode/EteeEncodingManager.h"

class ICommunicationManager {
 public:
  virtual bool Connect() = 0;
  virtual void BeginListener(const std::function<void(const VRCommInputData_t&)> inputCallback, const std::function<void(const VRStateEvent_t&)> stateCallback) = 0;
  virtual bool IsConnected() = 0;
  virtual void Disconnect() = 0;

 private:
  std::unique_ptr<EteeEncodingManager> m_encodingManager;
};