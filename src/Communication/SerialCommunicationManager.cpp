#include <Communication/SerialCommunicationManager.h>
#include <SetupAPI.h>

#include <chrono>

#include "Util/DriverLog.h"

static const std::string c_serialDeviceId = "VID_239A&PID_8029";
static const uint32_t c_listenerWaitTime = 1000;

int SerialCommunicationManager::GetComPort() {
  if (m_configuration.overridePort) {
    return m_configuration.port;
  }

  HDEVINFO DeviceInfoSet;
  DWORD DeviceIndex = 0;
  SP_DEVINFO_DATA DeviceInfoData;
  std::string DevEnum = "USB";
  char szBuffer[1024] = {0};
  DEVPROPTYPE ulPropertyType;
  DWORD dwSize = 0;
  DWORD Error = 0;

  DeviceInfoSet = SetupDiGetClassDevs(NULL, DevEnum.c_str(), NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

  if (DeviceInfoSet == INVALID_HANDLE_VALUE) return -1;

  ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
  DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  // Receive information about an enumerated device

  while (SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData)) {
    DeviceIndex++;

    // Retrieves a specified Plug and Play device property
    if (SetupDiGetDeviceRegistryProperty(
            DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &ulPropertyType,
            (BYTE*)szBuffer,
            sizeof(szBuffer),  // The size, in bytes
            &dwSize)) {
      HKEY hDeviceRegistryKey;
      if (std::string(szBuffer).find(c_serialDeviceId) == std::string::npos) continue;
      hDeviceRegistryKey = SetupDiOpenDevRegKey(DeviceInfoSet, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      if (hDeviceRegistryKey == INVALID_HANDLE_VALUE) {
        Error = GetLastError();
        break;
      } else {
        char pszPortName[20];
        DWORD dwSize = sizeof(pszPortName);
        DWORD dwType = 0;

        if ((RegQueryValueEx(hDeviceRegistryKey, "PortName", NULL, &dwType, (LPBYTE)pszPortName, &dwSize) == ERROR_SUCCESS) && (dwType == REG_SZ)) {
          std::string sPortName = pszPortName;
          try {
            if (sPortName.substr(0, 3) == "COM") {
              int nPortNr = std::stoi(pszPortName + 3);
              if (nPortNr != 0) {
                return nPortNr;
              }
            }
          } catch (...) {
            DriverLog("Parsing failed for a port");
          }
        }
        RegCloseKey(hDeviceRegistryKey);
      }
    }
  }
  if (DeviceInfoSet) {
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
  }

  return -1;
}

static std::string GetLastErrorAsString() {
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) {
    return std::string();
  }

  LPSTR messageBuffer = nullptr;

  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      errorMessageID,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer,
      0,
      NULL);

  std::string message(messageBuffer, size);

  LocalFree(messageBuffer);

  return message;
}

bool SerialCommunicationManager::SetCommunicationTimeout(
    unsigned long ReadIntervalTimeout,
    unsigned long ReadTotalTimeoutMultiplier,
    unsigned long ReadTotalTimeoutConstant,
    unsigned long WriteTotalTimeoutMultiplier,
    unsigned long WriteTotalTimeoutConstant) {
  COMMTIMEOUTS timeout;

  timeout.ReadIntervalTimeout = MAXDWORD;
  timeout.ReadTotalTimeoutConstant = 10;
  timeout.ReadTotalTimeoutMultiplier = MAXDWORD;
  timeout.WriteTotalTimeoutConstant = WriteTotalTimeoutConstant;
  timeout.WriteTotalTimeoutMultiplier = WriteTotalTimeoutMultiplier;

  if (!SetCommTimeouts(m_hSerial, &timeout)) return false;

  return true;
}

bool SerialCommunicationManager::Connect() {
  // We're not yet connected
  m_isConnected = false;

  LogMessage("Attempting connection to dongle...");

  short port = GetComPort();
  m_port = "\\\\.\\COM" + std::to_string(port);

  // Try to connect to the given port throuh CreateFile
  m_hSerial = CreateFile(m_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  // Set the size of the buffers. 100 bytes holds 4 controller SteamVR packets, I guess it could be smaller if needed, but this is fine for now.
  if (!SetupComm(m_hSerial, 100, 300)) {
    LogError("Failed to setup comm");

    return false;
  }

  if (this->m_hSerial == INVALID_HANDLE_VALUE) {
    LogError("Received error connecting to port");
    return false;
  }

  // If connected we try to set the comm parameters
  DCB dcbSerialParams = {0};

  if (!GetCommState(m_hSerial, &dcbSerialParams)) {
    LogError("Failed to get current port parameters");
    return false;
  }

  // Define serial connection parameters for the arduino board
  dcbSerialParams.BaudRate = CBR_115200;
  dcbSerialParams.ByteSize = 8;
  dcbSerialParams.StopBits = ONESTOPBIT;
  dcbSerialParams.Parity = NOPARITY;

  // reset upon establishing a connection
  dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

  // set the parameters and check for their proper application
  if (!SetCommState(m_hSerial, &dcbSerialParams)) {
    LogError("Failed to set serial parameters");
    return false;
  }

  if (!SetCommunicationTimeout(MAXDWORD, MAXDWORD, 1000, 5, 0)) {
    LogError("Failed to set communication timeout");
    return false;
  }

  COMMTIMEOUTS timeout;
  timeout.ReadIntervalTimeout = MAXDWORD;
  timeout.ReadTotalTimeoutConstant = 1000;
  timeout.ReadTotalTimeoutMultiplier = MAXDWORD;
  timeout.WriteTotalTimeoutConstant = 50;
  timeout.WriteTotalTimeoutMultiplier = 1;
  if (!SetCommTimeouts(m_hSerial, &timeout)) {
    LogError("Failed to set comm timeouts");

    return false;
  }

  // If everything went fine we're connected
  m_isConnected = true;

  LogMessage("Successfully connected to dongle");
  return true;
};

void SerialCommunicationManager::WaitAttemptConnection() {
  LogMessage("Attempting to connect to dongle");
  while (m_threadActive && !IsConnected() && !Connect()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(c_listenerWaitTime));
  }
  if (!m_threadActive) return;

  // std::this_thread::sleep_for(std::chrono::milliseconds(c_listenerWaitTime));
  // WriteCommand("BP+VS");
  UpdateDongleState(VRDongleState::connected);
}

void SerialCommunicationManager::BeginListener(
    const std::function<void(const VRCommInputData_t&)> inputCallback, const std::function<void(const VRStateEvent_t&)> stateCallback) {
  m_threadActive = true;
  m_stateCallback = std::move(stateCallback);
  m_inputCallback = std::move(inputCallback);
  m_serialThread = std::thread(&SerialCommunicationManager::ListenerThread, this);
}

void SerialCommunicationManager::ListenerThread() {
  WaitAttemptConnection();

  PurgeBuffer();

  while (m_threadActive) {
    std::string receivedString;

    if (!ReceiveNextPacket(receivedString)) {
      LogMessage("Detected device error. Disconnecting device and attempting reconnection...");
      UpdateDongleState(VRDongleState::disconnected);

      if (DisconnectFromDevice(false)) {
        WaitAttemptConnection();
        LogMessage("Successfully reconnected to device");
        continue;
      }

      LogMessage("Could not disconnect from device. Closing listener...");
      Disconnect();

      return;
    }

    // If we haven't received anything don't bother with trying to decode anything
    if (receivedString.empty()) {
      goto finish;
    }

    try {
      VRCommInputData_t commData = m_encodingManager->DecodeInputPacket(receivedString);
      if (!commData.isValid) {
        LogMessage("Received Bad Packet.");
        PurgeBuffer();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        continue;
      }

      m_inputCallback(commData);
    } catch (const std::invalid_argument& ia) {
      LogMessage((std::string("Received error from encoding: ") + ia.what()).c_str());
    } catch (...) {
      LogMessage("Received unknown error attempting to decode packet.");
    }

  finish:
    // write anything we need to
    WriteQueued();
  }
}

bool SerialCommunicationManager::ReceiveNextPacket(std::string& buff) {
  DWORD dwRead = 0;

  char thisChar = 0x00;
  char lastChar = 0x00;

  do {
    lastChar = thisChar;

    if (!ReadFile(m_hSerial, &thisChar, 1, &dwRead, NULL)) {
      LogError("Error reading from file");
      return false;
    }

    if (dwRead == 0) {
      DebugDriverLog("No packet received within timeout");
      break;
    }

    buff += thisChar;

    if ((thisChar == '\n' && lastChar == '\r')) {
      VRStateEventData_t data;

      VRStateEvent_t stateEvent = m_encodingManager->DecodeStatePacket(buff);
      // DebugDriverLog("Read state message %s", buff.c_str());
      if (stateEvent.type != VRStateEventType::unknown) m_stateCallback(stateEvent);

      buff = "";

      break;
    }

    if (buff.size() > 200) {
      LogError("Overflowed controller input. Resetting...");
      buff = "";
      break;
    }

  } while ((!(thisChar == -1 && lastChar == -1)) && m_threadActive);

  return true;
}

void SerialCommunicationManager::WriteQueryDevices() {
  WriteCommand("BP+LR");
}

void SerialCommunicationManager::WriteActivate() {
  DebugDriverLog("Starting streaming");
  WriteCommand("BP+VG");
}

void SerialCommunicationManager::WriteCommand(const std::string& command) {
  std::scoped_lock lock(m_writeMutex);

  if (!m_isConnected) {
    LogMessage("Cannot write to dongle as it is not connected.");

    return;
  }

  m_queuedWrite = command + "\r\n";
}

bool SerialCommunicationManager::WriteQueued() {
  std::scoped_lock lock(m_writeMutex);

  if (!m_isConnected) return false;

  if (m_queuedWrite.empty()) return true;

  const char* buf = m_queuedWrite.c_str();
  DWORD bytesSend;
  if (!WriteFile(this->m_hSerial, (void*)buf, (DWORD)m_queuedWrite.size(), &bytesSend, 0)) {
    LogError("Error writing to port");
    return false;
  }

  DebugDriverLog("Wrote: %s", m_queuedWrite.c_str());

  m_queuedWrite.clear();

  return true;
}

bool SerialCommunicationManager::PurgeBuffer() {
  return PurgeComm(m_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

void SerialCommunicationManager::Disconnect() {
  DriverLog("Attempting to disconnect serial");
  if (m_threadActive.exchange(false)) {
    CancelIoEx(m_hSerial, nullptr);
    m_serialThread.join();

    DriverLog("Serial joined");
  }

  if (IsConnected()) DisconnectFromDevice(true);

  DriverLog("Serial finished disconnecting");
}

bool SerialCommunicationManager::DisconnectFromDevice(bool writeDeactivate) {
  if (writeDeactivate) {
    WriteCommand("BP+VS");
  } else {
    LogMessage("Not deactivating Input API as dongle was forcibly disconnected");
  }

  if (!CloseHandle(m_hSerial)) {
    LogError("Error disconnecting from device");
    return false;
  }

  m_isConnected = false;

  LogMessage("Successfully disconnected from device");
  return true;
};

void SerialCommunicationManager::UpdateDongleState(VRDongleState state) {
  VRStateEventData_t data;
  data.dongleState = state;

  VRStateEvent_t stateEvent(VRStateEventType::dongle_state, data);
  m_stateCallback(stateEvent);
}

bool SerialCommunicationManager::IsConnected() {
  return m_isConnected;
};

void SerialCommunicationManager::LogError(const char* message) {
  // message with port name and last error
  DriverLog("%s (%s) - Error: %s", message, m_port.c_str(), GetLastErrorAsString().c_str());
}

void SerialCommunicationManager::LogMessage(const char* message) {
  // message with port name
  DriverLog("%s (%s)", message, m_port.c_str());
}