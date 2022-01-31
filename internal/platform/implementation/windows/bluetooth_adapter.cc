// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/windows/bluetooth_adapter.h"

#include <windows.h>
#include <winioctl.h>
#include <bthdef.h>
#include <bthioctl.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <stdio.h>
#include <usbiodef.h>

#include <string>

#include "absl/strings/str_format.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

typedef std::basic_string<TCHAR> tstring;

// IOCTL to get local radio information
#define BTH_GET_DEVICE_INFO_IOCTL 0x411008

#define BUFFER_SIZE 64
#define FILE_NAME_SIZE 1024

#define REGISTRY_QUERY_FORMAT \
  "SYSTEM\\ControlSet001\\Enum\\%s\\Device Parameters"

#define BLUETOOTH_RADIO_REGISTRY_NAME_KEY "Local Name"

namespace location {
namespace nearby {
namespace windows {

BluetoothAdapter::BluetoothAdapter()
    : windows_bluetooth_adapter_(winrt::Windows::Devices::Bluetooth::
                                     BluetoothAdapter::GetDefaultAsync()
                                         .get()) {
  // Gets the radio represented by this Bluetooth adapter.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.getradioasync?view=winrt-20348
  windows_bluetooth_radio_ = windows_bluetooth_adapter_.GetRadioAsync().get();
}

// Synchronously sets the status of the BluetoothAdapter to 'status', and
// returns true if the operation was a success.
bool BluetoothAdapter::SetStatus(Status status) {
  if (status == Status::kDisabled) {
    // An asynchronous operation that attempts to set the state of the radio
    // represented by this object.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.setstateasync?view=winrt-20348
    windows_bluetooth_radio_.SetStateAsync(RadioState::Off).get();
  } else {
    windows_bluetooth_radio_.SetStateAsync(RadioState::On).get();
  }
  return true;
}

// Returns true if the BluetoothAdapter's current status is
// Status::Value::kEnabled.
bool BluetoothAdapter::IsEnabled() const {
  // Gets the current state of the radio represented by this object.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.state?view=winrt-20348
  return windows_bluetooth_radio_.State() == RadioState::On;
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
//
// Returns ScanMode::kUnknown on error.
BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  return scan_mode_;
}

// Synchronously sets the scan mode of the adapter, and returns true if the
// operation was a success.
bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  scan_mode_ = scan_mode;

  if (scan_mode_changed_ != nullptr) {
    scan_mode_changed_(scan_mode);
  }

  return true;
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
// Returns an empty string on error
std::string BluetoothAdapter::GetName() const {
  char *instanceID = GetGenericBluetoothAdapterInstanceID();

  if (instanceID == NULL) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get Generic Bluetooth Adapter InstanceID";
    return std::string();
  }
  // Add 1 to length to get size (including null)
  // Modify the InstanceID to the format that the Registry expects
  char *instanceIDModified = new char[(strlen(instanceID) + 1) * sizeof(char)];
  absl::SNPrintF(instanceIDModified,
                 size_t((strlen(instanceID) + 1) * sizeof(char)), "%s",
                 instanceID);
  find_and_replace(instanceIDModified, "\\", "#");

  // Change radio module local name in registry
  HKEY hKey;
  char rmLocalNameKey[FILE_NAME_SIZE] = {0};
  LSTATUS ret;

  absl::SNPrintF(rmLocalNameKey, sizeof(rmLocalNameKey), REGISTRY_QUERY_FORMAT,
                 instanceID);

  // Opens the specified registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa
  ret = RegOpenKeyExA(
      HKEY_LOCAL_MACHINE,  // A handle to an open registry key, using
                           // predefined key for local machine
      rmLocalNameKey,      // The name of the registry subkey to be opened.
      0L,               // Specifies the option to apply when opening the key.
      KEY_QUERY_VALUE,  // A mask that specifies the desired access rights to
                        // the key to be opened.
      &hKey);  // A pointer to a variable that receives a handle to the opened
               // key

  if (ret == ERROR_SUCCESS) {
    BYTE rmLocalName[FILE_NAME_SIZE] = {0};
    DWORD rmLocalNameSize = FILE_NAME_SIZE;
    DWORD valueType;

    LSTATUS status =
        // Retrieves the type and data for the specified value name associated
        // with an open registry key.
        // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
        RegQueryValueExA(
            hKey,  // A handle to an open registry key.
            BLUETOOTH_RADIO_REGISTRY_NAME_KEY,  // The name of the registry
                                                // value.
            nullptr,            // This parameter is reserved and must be NULL.
            &valueType,         // A pointer to a variable that receives a code
                                // indicating the type of data stored in the
                                // specified value.
            &rmLocalName[0],    // A pointer to a buffer that receives the
                                // value's data.
            &rmLocalNameSize);  // A pointer to a variable that specifies the
                                // size of the buffer pointed to by the lpData
                                // parameter, in bytes.

    // Closes a handle to the specified registry key.
    // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
      char str[FILE_NAME_SIZE]{0};
      memcpy(str, rmLocalName, rmLocalNameSize);
      return std::string(str);
    }
  }

  // The local name is not in the registry, return the machine name
  char localName[FILE_NAME_SIZE];
  DWORD nameSize = FILE_NAME_SIZE;

  // Retrieves the NetBIOS name of the local computer.
  // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getcomputernamea
  if (GetComputerNameA(localName, &nameSize)) {
    return std::string(localName);
  }

  // If we're here, we couldn't get a local name, this should never happen
  NEARBY_LOGS(ERROR) << __func__ << ": Failed to get any radio name";
  return std::string();
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
bool BluetoothAdapter::SetName(absl::string_view name) {
  char *instanceID = GetGenericBluetoothAdapterInstanceID();

  if (instanceID == NULL) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get Generic Bluetooth Adapter InstanceID";
    return false;
  }

  // Add 1 to length to get size (including null)
  char *instanceIDModified = new char[(strlen(instanceID) + 1) * sizeof(char)];

  absl::SNPrintF(instanceIDModified,
                 size_t((strlen(instanceID) + 1) * sizeof(char)), "%s",
                 instanceID);

  find_and_replace(instanceIDModified, "\\", "#");

  HANDLE hDevice;
  char fileName[FILE_NAME_SIZE] = {0};

  // defined in usbiodef.h
  const GUID guid = GUID_DEVINTERFACE_USB_DEVICE;

  OLECHAR guidOleStr[BUFFER_SIZE];
  int oleBufferLen = BUFFER_SIZE;

  char guidStr[BUFFER_SIZE];
  int bufferLen = BUFFER_SIZE;
  BOOL defaultCharUsed;

  // Converts a globally unique identifier (GUID) into a string of printable
  // characters.
  // https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-stringfromguid2
  auto conversionResult = StringFromGUID2(guid, guidOleStr, bufferLen);

  if (conversionResult == 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to convert guid to string";
    return false;
  }

  // Maps a UTF-16 (wide character) string to a new character string. The new
  // character string is not necessarily from a multibyte character set.
  // https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  conversionResult =
      WideCharToMultiByte(CP_UTF8, 0, guidOleStr, oleBufferLen, guidStr,
                          bufferLen, NULL, &defaultCharUsed);

  if (conversionResult == 0) {
    const char *errorResult = {};
    int errorMessageID = GetLastError();

    switch (errorMessageID) {
      case ERROR_INSUFFICIENT_BUFFER:
        errorResult =
            "A supplied buffer size was not large enough, or it was "
            "incorrectly set to NULL.";
        break;
      case ERROR_INVALID_FLAGS:
        errorResult = "The values supplied for flags were not valid.";
        break;
      case ERROR_INVALID_PARAMETER:
        errorResult = "Any of the parameter values was invalid.";
        break;
      case ERROR_NO_UNICODE_TRANSLATION:
        errorResult = "Invalid Unicode was found in a string.";
        break;
      default:
        errorResult = "Unknown error.";
        break;
    }

    NEARBY_LOGS(ERROR) << __func__ << ": Failed to convert guid to string "
                       << errorResult << " Error code:" << errorMessageID;
  }

  absl::SNPrintF(fileName, sizeof(fileName), "\\\\.\\%s%s#%s", fileName,
                 instanceIDModified, guidStr);

  // Creates or opens a file or I/O device.
  // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
  hDevice = CreateFileA(
      fileName,       // The name of the file or device to be created or opened.
      GENERIC_WRITE,  // The requested access to the file or device.
      0,              // The requested sharing mode of the file or device.
      NULL,           // A pointer to a SECURITY_ATTRIBUTES structure.
      OPEN_EXISTING,  // An action to take on a file or device that exists or
                      // does not exist.
      0,              // The file or device attributes and flags.
      NULL);  // A valid handle to a template file with the GENERIC_READ access
              // right. This parameter can be NULL.

  delete[] instanceIDModified;

  if (hDevice == INVALID_HANDLE_VALUE) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to open device. Error code: "
                       << GetLastError();
    return false;
  }

  // Change radio module local name in registry
  HKEY hKey;
  char rmLocalNameKey[FILE_NAME_SIZE] = {0};
  LSTATUS ret;

  absl::SNPrintF(rmLocalNameKey, sizeof(rmLocalNameKey), REGISTRY_QUERY_FORMAT,
                 instanceID);

  // Opens the specified registry key. Note that key names are not case
  // sensitive.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa
  ret = RegOpenKeyExA(
      HKEY_LOCAL_MACHINE,  // A handle to an open registry key.
      rmLocalNameKey,      // The name of the registry subkey to be opened.
      0L,             // Specifies the option to apply when opening the key.
      KEY_SET_VALUE,  // A mask that specifies the desired access rights to the
                      // key to be opened.
      &hKey);  // A pointer to a variable that receives a handle to the opened
               // key.

  if (ret != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to open registry key. Error code: " << ret;
    return false;
  }

  if (name != "") {
    // Sets the data and type of a specified value under a registry key.
    // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    ret = RegSetValueExA(
        hKey,                               // A handle to an open registry key
        BLUETOOTH_RADIO_REGISTRY_NAME_KEY,  // The name of the value to be set.
        0,           // This parameter is reserved and must be zero.
        REG_BINARY,  // The type of data pointed to by the lpData parameter.
        (LPBYTE)std::string(name).c_str(),  // The data to be stored.
        strlen(std::string(name)
                   .c_str()));  // The size of the information pointed
                                // to by the lpData parameter, in bytes.
  } else {
    // If we are told to set the key to "", we treat this as a reset
    // If we delete the key value the OS will default to the system
    // name. If we just set it to blank, it will show as blank in all
    // system dialogs, this is likely undesireable
    ret = RegDeleteValueA(hKey, BLUETOOTH_RADIO_REGISTRY_NAME_KEY);
  }

  if (ret != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set/delete registry key. Error code: "
                       << ret;
    return false;
  }

  // Closes a handle to the specified registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
  RegCloseKey(hKey);

  // tells the control function to reset or reload or similar...
  int32 reload = 4;
  // merely a placeholder
  DWORD bytes = 0;

  // Send radio module driver command to update device information
  // Sends a control code directly to a specified device driver, causing the
  // corresponding device to perform the corresponding operation.
  // https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol
  if (!DeviceIoControl(
          hDevice,  // A handle to the device on which the operation is to be
                    // performed.
          BTH_GET_DEVICE_INFO_IOCTL,  // The control code for the operation.
          &reload,  // A pointer to the input buffer that contains the data
                    // required to perform the operation.
          sizeof(reload),  // The size of the input buffer, in bytes.
          NULL,    // A pointer to the output buffer that is to receive the data
                   // returned by the operation.
          0,       // The size of the output buffer, in bytes.
          &bytes,  // A pointer to a variable that receives the size of the data
                   // stored in the output buffer, in bytes.
          NULL)) {  // A pointer to an OVERLAPPED structure.
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to update radio module local name. Error code: "
        << GetLastError();

    return false;
  }

  return true;
}

void BluetoothAdapter::find_and_replace(char *source, const char *strFind,
                                        const char *strReplace) const {
  std::string s = std::string(source);
  std::string f = std::string(strFind);
  std::string r = std::string(strReplace);
  size_t j;

  for (; (j = s.find(f)) != std::string::npos;) {
    s.replace(j, f.length(), r);
  }

  memcpy(source, s.c_str(), s.size());
}

char *BluetoothAdapter::GetGenericBluetoothAdapterInstanceID(void) const {
  unsigned i;
  CONFIGRET r;
  HDEVINFO hDevInfo;
  SP_DEVINFO_DATA DeviceInfoData;
  char *deviceInstanceID = new char[MAX_DEVICE_ID_LEN];

  // Find all bluetooth radio modules
  // The SetupDiGetClassDevs function returns a handle to a device information
  // set that contains requested device information elements for a local
  // computer.
  // https://docs.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetclassdevsa
  hDevInfo =
      SetupDiGetClassDevsA(&GUID_DEVCLASS_BLUETOOTH, NULL, NULL, DIGCF_PRESENT);

  if (hDevInfo == INVALID_HANDLE_VALUE) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Could not find BluetoothDevice on this machine";
    return NULL;
  }

  // Get first Generic Bluetooth Adapter InstanceID
  for (i = 0;; i++) {
    DeviceInfoData.cbSize = sizeof(DeviceInfoData);

    // The SetupDiEnumDeviceInfo function returns a SP_DEVINFO_DATA structure
    // that specifies a device information element in a device information set.
    // https://docs.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdienumdeviceinfo
    if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData)) break;

    // The CM_Get_Device_ID function retrieves the device instance ID for a
    // specified device instance on the local machine.
    // https://docs.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_get_device_ida
    r = CM_Get_Device_IDA(DeviceInfoData.DevInst, deviceInstanceID,
                          MAX_DEVICE_ID_LEN, 0);

    if (r != CR_SUCCESS) continue;

    // With Windows, a Bluetooth radio can be packaged as an external dongle or
    // embedded inside a computer but it must be connected to one of the
    // computer's USB ports.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-host-radio-support
    if (strncmp("USB", deviceInstanceID, 3) == 0) {
      return deviceInstanceID;
    }
  }

  NEARBY_LOGS(ERROR) << __func__
                     << ": Failed to get the generic bluetooth adapter id";

  return NULL;
}

// Returns BT MAC address assigned to this adapter.
std::string BluetoothAdapter::GetMacAddress() const {
  return uint64_to_mac_address_string(
      windows_bluetooth_adapter_.BluetoothAddress());
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
