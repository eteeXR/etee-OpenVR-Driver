#pragma once

#include <windows.h>

#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "CommunicationManager.h"
#include "DeviceConfiguration.h"
#include "Encode/EteeEncodingManager.h"

class SerialCommunicationManager : public ICommunicationManager {
 public:
  SerialCommunicationManager(VRSerialConfiguration configuration, std::unique_ptr<EteeEncodingManager> encodingManager)
      : m_configuration(configuration), m_encodingManager(std::move(encodingManager)), m_isConnected(false), m_hSerial(0), m_errors(0), m_writeMutex(), m_queuedWrite("") {};

  void BeginListener(const std::function<void(const VRCommInputData_t&)> inputCallback, const std::function<void(const VRStateEvent_t&)> stateCallback);
  bool IsConnected();
  void Disconnect();
  void WriteActivate();
  void WriteCommand(const std::string& command);
  void WriteQueryDevices();

 private:
  bool Connect();
  bool SetCommunicationTimeout(
      unsigned long readIntervalTimeout,
      unsigned long readTotalTimeoutMultiplier,
      unsigned long readTotalTimeoutConstant,
      unsigned long writeTotalTimeoutMultiplier,
      unsigned long WriteTotalTimeoutConstant);
  void ListenerThread();
  bool ReceiveNextPacket(std::string& buff);
  bool PurgeBuffer();
  void WaitAttemptConnection();
  bool DisconnectFromDevice(bool writeDeactivate = true);
  bool WriteQueued();
  int GetComPort();
  void UpdateDongleState(VRDongleState state);

  void LogMessage(const char* message);
  void LogError(const char* message);

  std::atomic<bool> m_isConnected;
  // Serial comm handler
  HANDLE m_hSerial;
  // Error tracking
  DWORD m_errors;

  std::string m_port;

  std::atomic<bool> m_threadActive;
  std::thread m_serialThread;

  std::unique_ptr<EteeEncodingManager> m_encodingManager;

  std::mutex m_writeMutex;

  std::string m_queuedWrite;

  VRSerialConfiguration m_configuration;

  std::function<void(const VRStateEvent_t&)> m_stateCallback;
  std::function<void(const VRCommInputData_t&)> m_inputCallback;
};