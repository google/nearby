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

// clang-format off
#include <windows.h>   // These two headers must be defined
#include <winioctl.h>  // first and in this order, otherwise
                       // the MSVC compiler emits a warning
                       // 'CTL_CODE undefined. Include winioctl.h or wdm.h'
                       // See https://screenshot.googleplex.com/BYB9QnoHT6eP6oc
// clang-format on

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
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/json/json.hpp"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

typedef std::basic_string<TCHAR> tstring;

// IOCTL to get local radio information
#define BTH_GET_DEVICE_INFO_IOCTL 0x411008

#define REGISTRY_QUERY_FORMAT \
  "SYSTEM\\ControlSet001\\Enum\\%s\\Device Parameters"

#define BLUETOOTH_RADIO_REGISTRY_NAME_KEY "Local Name"

namespace nearby {
namespace windows {
namespace {
struct LocalSettings {
  std::string original_radio_name;
  std::string nearby_radio_name;
};

using json = ::nlohmann::json;

constexpr absl::string_view kLocalSettingsFileName = "settings_file.json";
constexpr char kOriginalRadioName[] = "OriginalRadioName";
constexpr char kNearbyRadioName[] = "NearbyRadioName";

void to_json(json &json_output, const LocalSettings &local_settings) {
  json_output = json{{kOriginalRadioName, local_settings.original_radio_name},
                     {kNearbyRadioName, local_settings.nearby_radio_name}};
}

void from_json(const json &json_input, LocalSettings &local_settings) {
  json_input.at(kOriginalRadioName).get_to(local_settings.original_radio_name);
  json_input.at(kNearbyRadioName).get_to(local_settings.nearby_radio_name);
}
}  // namespace

constexpr uint8_t kAndroidDiscoverableBluetoothNameMaxLength = 37;  // bytes

BluetoothAdapter::BluetoothAdapter() : windows_bluetooth_adapter_(nullptr) {
  windows_bluetooth_adapter_ =
      winrt::Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync()
          .get();
  if (windows_bluetooth_adapter_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
  } else {
    // Gets the radio represented by this Bluetooth adapter.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.getradioasync?view=winrt-20348
    windows_bluetooth_radio_ = windows_bluetooth_adapter_.GetRadioAsync().get();
  }
}

// Synchronously sets the status of the BluetoothAdapter to 'status', and
// returns true if the operation was a success.
bool BluetoothAdapter::SetStatus(Status status) {
  NEARBY_LOGS(ERROR) << __func__ << ": Set Bluetooth radio status to "
                     << (status == Status::kEnabled ? "On" : "Off");
  if (windows_bluetooth_radio_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth radio on this device.";
    return false;
  }

  auto radio_state = windows_bluetooth_radio_.State();

  if (status == Status::kDisabled &&
      (radio_state == RadioState::Unknown || radio_state == RadioState::Off ||
       radio_state == RadioState::Disabled)) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": Skip set radio status kDisabled due to requested state is "
           "already kDisabled.";
    return true;
  }

  if (status == Status::kEnabled && radio_state == RadioState::On) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": Skip set radio status kEnabled due to requested state is "
           "already kEnabled.";
    return true;
  }

  if (!FeatureFlags::GetInstance().GetFlags().enable_set_radio_state) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Attempt to set the radio state while "
                         "FeatureFlags::enable_set_radio_state is false.";
    return false;
  }

  try {
    // An asynchronous operation that attempts to set the state of the radio
    // represented by this object.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.setstateasync?view=winrt-20348
    if (status == Status::kDisabled) {
      windows_bluetooth_radio_.SetStateAsync(RadioState::Off).get();
    } else {
      windows_bluetooth_radio_.SetStateAsync(RadioState::On).get();
    }
  } catch (const winrt::hresult_error &ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set Bluetooth radio state to "
                       << (status == Status::kDisabled ? "kDisabled."
                                                       : "kEnabled.")
                       << "Exception: " << ex.code() << ": "
                       << winrt::to_string(ex.message());

    return false;
  }

  NEARBY_LOGS(INFO) << __func__ << ": Successfully set the radio state to "
                    << (status == Status::kDisabled ? "kDisabled."
                                                    : "kEnabled.");
  return true;
}

// Returns true if the BluetoothAdapter's current status is
// Status::Value::kEnabled.
bool BluetoothAdapter::IsEnabled() const {
  if (windows_bluetooth_radio_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth radio on this device.";
    return false;
  }
  // Gets the current state of the radio represented by this object.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.state?view=winrt-20348
  return windows_bluetooth_radio_.State() == RadioState::On;
}

// Returns true if the Bluetooth hardware supports Bluetooth 5.0 Extended
// Advertising
bool BluetoothAdapter::IsExtendedAdvertisingSupported() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return false;
  }
  // Indicates whether the adapter supports the 5.0 Extended Advertising format.
  // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.isextendedadvertisingsupported?view=winrt-22621
  return windows_bluetooth_adapter_.IsExtendedAdvertisingSupported();
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

void BluetoothAdapter::RestoreRadioNameIfNecessary() {
  std::string nearby_radio_name;
  std::string current_radio_name = GetName();

  std::string settings_path(kLocalSettingsFileName);

  auto full_path =
      nearby::api::ImplementationPlatform::GetAppDataPath(settings_path);

  auto settings_file =
      nearby::api::ImplementationPlatform::CreateInputFile(full_path, 0);
  if (settings_file == nullptr) {
    return;
  }

  auto total_size = settings_file->GetTotalSize();
  nearby::ExceptionOr<ByteArray> raw_local_settings;

  raw_local_settings = settings_file->Read(total_size);
  settings_file->Close();

  if (!raw_local_settings.ok()) {
    return;
  }

  auto local_settings =
      json::parse(raw_local_settings.GetResult().data(), nullptr, false);

  if (local_settings.is_discarded()) {
    return;
  }

  LocalSettings settings = local_settings.get<LocalSettings>();

  if (current_radio_name == settings.nearby_radio_name) {
    SetName(settings.original_radio_name,
            /* persist= */ true);
  }
}

void BluetoothAdapter::StoreRadioNames(absl::string_view original_radio_name,
                                       absl::string_view nearby_radio_name) {
  if (original_radio_name == nullptr || original_radio_name.size() == 0) {
    return;
  }
  if (nearby_radio_name == nullptr || nearby_radio_name.size() == 0) {
    return;
  }

  std::string settings_path(kLocalSettingsFileName);
  auto full_path =
      nearby::api::ImplementationPlatform::GetAppDataPath(settings_path);

  auto settings_file =
      nearby::api::ImplementationPlatform::CreateOutputFile(full_path);

  if (settings_file == nullptr) {
    return;
  }

  LocalSettings local_settings = {std::string(original_radio_name),
                                  std::string(nearby_radio_name)};

  json encoded_local_settings = local_settings;

  ByteArray data(encoded_local_settings.dump());

  settings_file->Write(data);
  settings_file->Close();
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
// Returns an empty string on error
std::string BluetoothAdapter::GetName() const {
  if (device_name_.has_value()) {
    return *device_name_;
  }

  char *_instance_id = GetGenericBluetoothAdapterInstanceID();
  if (_instance_id == nullptr) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get Generic Bluetooth Adapter InstanceID";
    return std::string();
  }

  std::string instance_id(_instance_id);

  // Change radio module local name in registry
  HKEY hKey;

  // Retrieve the size required
  size_t registry_query_size = absl::SNPrintF(nullptr,                // output
                                              0,                      // size
                                              REGISTRY_QUERY_FORMAT,  // format
                                              instance_id.c_str());   // args

  std::string local_name_key;
  local_name_key.reserve(registry_query_size + 1);

  absl::SNPrintF(local_name_key.data(),    // output
                 registry_query_size + 1,  // size
                 REGISTRY_QUERY_FORMAT,    // format
                 instance_id.c_str());     // args

  LSTATUS status;

  // Opens the specified registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa
  status = RegOpenKeyExA(
      HKEY_LOCAL_MACHINE,      // A handle to an open registry key, using
                               // predefined key for local machine
      local_name_key.c_str(),  // The name of the registry subkey to be opened.
      0L,               // Specifies the option to apply when opening the key.
      KEY_QUERY_VALUE,  // A mask that specifies the desired access rights to
                        // the key to be opened.
      &hKey);  // A pointer to a variable that receives a handle to the opened
               // key

  std::string adapter_name;

  if (status == ERROR_SUCCESS) {
    adapter_name = GetNameFromRegistry(&hKey);
  }

  // Closes a handle to the specified registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
  RegCloseKey(hKey);

  // The local name is not in the registry, return the machine name
  if (adapter_name.empty()) {
    return GetNameFromComputerName();
  }
  return adapter_name;
}

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
bool BluetoothAdapter::SetName(absl::string_view name) {
  return SetName(name,
                 /* persist= */ true);
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  if (!persist) {
    StoreRadioNames(GetName(), name);
  }
  if (name.size() > 248 * sizeof(char)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set name for bluetooth adapter because "
                          "the name exceeded the 248 bytes limit for Windows.";
    return false;
  }

  if (name.size() > kAndroidDiscoverableBluetoothNameMaxLength * sizeof(char)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set name for bluetooth adapter because "
                          "Android cannot discover Windows bluetooth device "
                          "name that exceeded the 37 bytes limit (11 "
                          "characters in EndpointInfo).";
    device_name_ = std::string(name);
    return true;
  }

  device_name_ = std::nullopt;

  if (registry_bluetooth_adapter_name_ == name) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Tried to set name for bluetooth adapter to the "
                         "same name again.";
    return true;
  }

  std::string instance_id(GetGenericBluetoothAdapterInstanceID());

  if (instance_id.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Failed to get Generic Bluetooth Adapter InstanceID";
    return false;
  }
  // defined in usbiodef.h
  const GUID guid = GUID_DEVINTERFACE_USB_DEVICE;

// Conversion of a 16 byte guid to a format of
// "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" results in
// a required buffer of
// (32 digits + 4 '-'s + 2 braces + 1 '\0') * sizeof(OLECHAR) for
// a total of 39 OLECHARs.
#define OLE_GUID_STRING_BUFFER_SIZE 39
  OLECHAR guid_ole_str[OLE_GUID_STRING_BUFFER_SIZE];
  size_t guid_ole_str_size = OLE_GUID_STRING_BUFFER_SIZE;

  // Converts a globally unique identifier (GUID) into a string of printable
  // characters.
  // https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-stringfromguid2
  auto conversionResult =
      StringFromGUID2(guid, guid_ole_str, guid_ole_str_size);

  if (conversionResult == 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to convert guid to string";
    return false;
  }
  std::string guid_str;
  int guid_str_size = 0;
  BOOL defaultCharUsed;

  guid_str_size = WideCharToMultiByte(
      CP_UTF8,            // Code page to use in performing the conversion.
      0,                  // Flags indicating the conversion type.
      guid_ole_str,       // Pointer to the Unicode string to convert.
      guid_ole_str_size,  // Size, in characters, of the string indicated by
                          // lpWideCharStr.
      nullptr,  // Pointer to a buffer that receives the converted string.
      0,        // Size, in bytes, of the buffer indicated by lpMultiByteStr.
      NULL,     // Pointer to the character to use if a character cannot be
                // represented in the specified code page.
      &defaultCharUsed);  // Pointer to a flag that indicates if the function
                          // has used a default character in the conversion.

  if (guid_str_size == 0) {
    process_error();
    return false;
  }

  guid_str.reserve(guid_str_size);

  // Maps a UTF-16 (wide character) string to a new character string. The new
  // character string is not necessarily from a multibyte character set.
  // https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  conversionResult = WideCharToMultiByte(
      CP_UTF8,            // Code page to use in performing the conversion.
      0,                  // Flags indicating the conversion type.
      guid_ole_str,       // Pointer to the Unicode string to convert.
      guid_ole_str_size,  // Size, in characters, of the string indicated by
      // lpWideCharStr.
      guid_str
          .data(),    // Pointer to a buffer that receives the converted string.
      guid_str_size,  // Size, in bytes, of the buffer indicated by
                      // lpMultiByteStr.
      NULL,  // // Pointer to the character to use if a character cannot be
             // represented in the specified code page.
      &defaultCharUsed);  // // Pointer to a flag that indicates if the
                          // function has used a default character in the
                          // conversion.

  if (conversionResult == 0) {
    process_error();
    return false;
  }

  // Add 1 to length to get size (including null)
  std::string instance_id_modified;
  instance_id_modified.reserve((instance_id.size() + 1) * sizeof(char));

  absl::SNPrintF(instance_id_modified.data(),
                 size_t((instance_id.size() + 1) * sizeof(char)), "%s",
                 instance_id.c_str());

  // Convert the P&P instance id, to one that CreateFileA expects
  find_and_replace(instance_id_modified.data(), "\\", "#");

  size_t file_name_size =
      absl::SNPrintF(nullptr, 0, "\\\\.\\%s#%s", instance_id_modified.c_str(),
                     guid_str.c_str());

  std::string file_name;
  file_name.reserve(file_name_size + 1);

  // Add the guid to the P&P instance id forming the complete symbolic
  // path to the device.
  absl::SNPrintF(file_name.data(), file_name_size + 1, "\\\\.\\%s#%s",
                 instance_id_modified.c_str(), guid_str.c_str());

  HANDLE hDevice;

  // Creates or opens a file or I/O device.
  // https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
  hDevice = CreateFileA(
      file_name.c_str(),  // The name of the file or device to be created or
                          // opened.
      GENERIC_WRITE,      // The requested access to the file or device.
      0,                  // The requested sharing mode of the file or device.
      NULL,               // A pointer to a SECURITY_ATTRIBUTES structure.
      OPEN_EXISTING,  // An action to take on a file or device that exists or
                      // does not exist.
      0,              // The file or device attributes and flags.
      NULL);          // A valid handle to a template file with the GENERIC_READ
                      // access right. This parameter can be NULL.

  if (hDevice == INVALID_HANDLE_VALUE) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to open device. Error code: "
                       << GetLastError();
    return false;
  }

  // Change radio module local name in registry
  HKEY hKey;
  size_t buffer_size =
      absl::SNPrintF(nullptr, 0, REGISTRY_QUERY_FORMAT, instance_id);

  std::string local_name_key;
  local_name_key.reserve(buffer_size + 1);

  absl::SNPrintF(local_name_key.data(), buffer_size + 1, REGISTRY_QUERY_FORMAT,
                 instance_id.c_str());

  LSTATUS status;
  // Opens the specified registry key. Note that key names are not case
  // sensitive.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa
  status = RegOpenKeyExA(
      HKEY_LOCAL_MACHINE,      // A handle to an open registry key.
      local_name_key.c_str(),  // The name of the registry subkey to be opened.
      0L,             // Specifies the option to apply when opening the key.
      KEY_SET_VALUE,  // A mask that specifies the desired access rights to
                      // the key to be opened.
      &hKey);  // A pointer to a variable that receives a handle to the opened
               // key.

  if (status != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to open registry key. Error code: "
                       << status;
    return false;
  }

  if (name != "") {
    // Sets the data and type of a specified value under a registry key.
    // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexa
    status = RegSetValueExA(
        hKey,                               // A handle to an open registry key
        BLUETOOTH_RADIO_REGISTRY_NAME_KEY,  // The name of the value to be
                                            // set.
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
    status = RegDeleteValueA(hKey, BLUETOOTH_RADIO_REGISTRY_NAME_KEY);
  }

  if (status != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set/delete registry key. Error code: "
                       << status;
    return false;
  }

  // Closes a handle to the specified registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
  RegCloseKey(hKey);

  // tells the control function to reset or reload or similar...
  int32_t reload = 4;
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
          &bytes,  // A pointer to a variable that receives the size of the
                   // data stored in the output buffer, in bytes.
          NULL)) {  // A pointer to an OVERLAPPED structure.
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to update radio module local name. Error code: "
        << GetLastError();

    return false;
  }

  registry_bluetooth_adapter_name_ = std::string(name);

  return true;
}

void BluetoothAdapter::process_error() {
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
    // that specifies a device information element in a device information
    // set.
    // https://docs.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdienumdeviceinfo
    if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData)) break;

    // The CM_Get_Device_ID function retrieves the device instance ID for a
    // specified device instance on the local machine.
    // https://docs.microsoft.com/en-us/windows/win32/api/cfgmgr32/nf-cfgmgr32-cm_get_device_ida
    r = CM_Get_Device_IDA(DeviceInfoData.DevInst, deviceInstanceID,
                          MAX_DEVICE_ID_LEN, 0);

    if (r != CR_SUCCESS) continue;

    // With Windows, a Bluetooth radio can be packaged as an external dongle
    // or embedded inside a computer but it must be connected to one of the
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
  if (windows_bluetooth_adapter_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return "";
  }
  return uint64_to_mac_address_string(
      windows_bluetooth_adapter_.BluetoothAddress());
}

std::string BluetoothAdapter::GetNameFromRegistry(PHKEY hKey) const {
  DWORD local_name_size = 0;
  DWORD value_type;

  // Retrieves the size of the data for the specified value name associated
  // with an open registry key.
  // https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
  LSTATUS status = RegQueryValueExA(
      *hKey,                              // A handle to an open registry key.
      BLUETOOTH_RADIO_REGISTRY_NAME_KEY,  // The name of the registry
                                          // value.
      nullptr,      // This parameter is reserved and must be NULL.
      &value_type,  // A pointer to a variable that receives a code
                    // indicating the type of data stored in the
                    // specified value.
      nullptr,      // null tells the function to just get the buffer size.
                    // value's data.
      &local_name_size);  // A pointer to a variable that specifies the
                          // size of the buffer pointed to by the lpData
                          // parameter, in bytes.
  if (status != ERROR_SUCCESS) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to get the required size of the local name buffer";
    return "";
  }
  unsigned char *local_name = new unsigned char[local_name_size];
  memset(local_name, '\0', local_name_size);

  status = RegQueryValueExA(
      *hKey,                              // A handle to an open registry key.
      BLUETOOTH_RADIO_REGISTRY_NAME_KEY,  // The name of the registry
                                          // value.
      nullptr,            // This parameter is reserved and must be NULL.
      &value_type,        // A pointer to a variable that receives a code
                          // indicating the type of data stored in the
                          // specified value.
      local_name,         // A pointer to a buffer that
                          // receives the value's data.
      &local_name_size);  // A pointer to a variable that specifies the
                          // size of the buffer pointed to by the lpData
                          // parameter, in bytes.

  if (status == ERROR_SUCCESS) {
    std::string local_name_return = std::string(
        local_name, local_name + local_name_size / sizeof local_name[0]);

    delete[] local_name;
    return local_name_return;
  }
  delete[] local_name;
  return "";
}

std::string BluetoothAdapter::GetNameFromComputerName() const {
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  CHAR computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  if (GetComputerNameA(computer_name, &size)) {
    return std::string(computer_name);
  }

  NEARBY_LOGS(ERROR) << __func__ << ": Failed to get any computer name";
  return "";
}

}  // namespace windows
}  // namespace nearby
