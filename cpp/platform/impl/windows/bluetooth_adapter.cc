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

#include "platform/impl/windows/bluetooth_adapter.h"

#include <windows.h>
#include <stdio.h>
#include <winioctl.h>
#include <bthdef.h>
#include <bthioctl.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <usbiodef.h>

#include <string>

#include "absl/strings/str_format.h"
#include "platform/impl/windows/generated/winrt/Windows.Foundation.h"
#include "platform/impl/windows/utils.h"
#include "platform/public/logging.h"

typedef std::basic_string<TCHAR> tstring;

// IOCTL to get local radio information
#define BTH_GET_DEVICE_INFO_IOCTL 0x411008

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
// TODO(jcarroll): Setup an event for this and hook the bluetooth medium into
// the event to allow for updates.
bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  scan_mode_ = scan_mode;
  return true;
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
// Returns an empty string on error
// TODO(b/184975123): replace with real implementation.
std::string BluetoothAdapter::GetName() const { return "Un-implemented"; }

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
  char fileName[1024] = {0};

  // defined in usbiodef.h
  const GUID guid = GUID_DEVINTERFACE_USB_DEVICE;

  OLECHAR guidOleStr[64];
  int oleBufferLen = 64;

  char guidStr[64];
  int bufferLen = 64;
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

  if (conversionResult != 0) {
    const char *errorResult = {};

    switch (conversionResult) {
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
    }

    NEARBY_LOGS(ERROR) << __func__ << ": Failed to convert guid to string "
                       << errorResult;
  }

  absl::SNPrintF(fileName, sizeof(fileName), "\\\\.\\%s%s#%s", fileName,
                 instanceIDModified, guidStr);

  // Creates or opens a file or I/O device.
  // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
  hDevice =
      CreateFileA(fileName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

  delete[] instanceIDModified;

  if (hDevice == INVALID_HANDLE_VALUE) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to open device. Error code: "
                       << GetLastError();
    return false;
  }

  // Change radio module local name in registry
  HKEY hKey;
  char rmLocalNameKey[1024] = {0};
  LSTATUS ret;

  absl::SNPrintF(rmLocalNameKey, sizeof(rmLocalNameKey),
                 "SYSTEM\\ControlSet001\\Enum\\%s\\Device Parameters",
                 instanceID);

  // Opens the specified registry key. Note that key names are not case
  // sensitive.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa
  ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, rmLocalNameKey, 0L, KEY_SET_VALUE,
                      &hKey);

  if (ret != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to open registry key. Error code: " << ret;
    return false;
  }

  // Sets the data and type of a specified value under a registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexa
  ret = RegSetValueExA(hKey, "Local Name", 0, REG_BINARY,
                       (LPBYTE)std::string(name).c_str(),
                       strlen(std::string(name).c_str()));

  if (ret != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set registry key. Error code: " << ret;
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
  if (!DeviceIoControl(hDevice, BTH_GET_DEVICE_INFO_IOCTL, &reload, 4, NULL, 0,
                       &bytes, NULL)) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to update radio module local name. Error code: "
        << GetLastError();

    return false;
  }

  return true;
}

void BluetoothAdapter::find_and_replace(char *source, const char *strFind,
                                        const char *strReplace) {
  std::string s = std::string(source);
  std::string f = std::string(strFind);
  std::string r = std::string(strReplace);
  size_t j;

  for (; (j = s.find(f)) != std::string::npos;) {
    s.replace(j, f.length(), r);
  }

  memcpy(source, s.c_str(), s.size());
}

char *BluetoothAdapter::GetGenericBluetoothAdapterInstanceID(void) {
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
