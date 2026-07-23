// Copyright 2020-2023 Google LLC
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
#include <windows.h>

#include <bthdef.h>
#include <objbase.h>
// clang-format on

#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"

namespace nearby::windows {

BluetoothAdapter::BluetoothAdapter() : windows_bluetooth_adapter_(nullptr) {
  try {
    windows_bluetooth_adapter_ =
        winrt::Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync()
            .get();
    if (windows_bluetooth_adapter_ == nullptr) {
      LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    } else {
      // Gets the radio represented by this Bluetooth adapter.
      // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.getradioasync?view=winrt-20348
      windows_bluetooth_radio_ =
          windows_bluetooth_adapter_.GetRadioAsync().get();
    }
  } catch (const winrt::hresult_error &error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
  }
}

// Synchronously sets the status of the BluetoothAdapter to 'status', and
// returns true if the operation was a success.
bool BluetoothAdapter::SetStatus(Status status) {
  LOG(INFO) << __func__ << ": Set Bluetooth radio status to "
            << (status == Status::kEnabled ? "On" : "Off");
  if (windows_bluetooth_radio_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth radio on this device.";
    return false;
  }

  auto radio_state = windows_bluetooth_radio_.State();

  if (status == Status::kDisabled &&
      (radio_state == RadioState::Unknown || radio_state == RadioState::Off ||
       radio_state == RadioState::Disabled)) {
    VLOG(1) << __func__
            << ": Skip set radio status kDisabled due to requested state is "
               "already kDisabled.";
    return true;
  }

  if (status == Status::kEnabled && radio_state == RadioState::On) {
    VLOG(1) << __func__
            << ": Skip set radio status kEnabled due to requested state is "
               "already kEnabled.";
    return true;
  }

  if (!FeatureFlags::GetInstance().GetFlags().enable_set_radio_state) {
    VLOG(1) << __func__
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
    LOG(ERROR) << __func__ << ": Failed to set Bluetooth radio state to "
               << (status == Status::kDisabled ? "kDisabled." : "kEnabled.")
               << "Exception: " << ex.code() << ": "
               << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }

  LOG(INFO) << __func__ << ": Successfully set the radio state to "
            << (status == Status::kDisabled ? "kDisabled." : "kEnabled.");
  return true;
}

// Returns true if the BluetoothAdapter's current status is
// Status::Value::kEnabled.
bool BluetoothAdapter::IsEnabled() const {
  if (windows_bluetooth_radio_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth radio on this device.";
    return false;
  }
  try {
    // Gets the current state of the radio represented by this object.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.state?view=winrt-20348
    return windows_bluetooth_radio_.State() == RadioState::On;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
    return false;
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }
}

// Returns true if the Bluetooth hardware supports Bluetooth 5.0 Extended
// Advertising
bool BluetoothAdapter::IsExtendedAdvertisingSupported() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return false;
  }
  try {
    // Indicates whether the adapter supports the 5.0 Extended Advertising
    // format.
    // https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.isextendedadvertisingsupported?view=winrt-22621
    return windows_bluetooth_adapter_.IsExtendedAdvertisingSupported();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
    return false;
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }
}

// Returns true if the Bluetooth hardware supports BLE Central Role
bool BluetoothAdapter::IsCentralRoleSupported() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return false;
  }
  try {
    // Indicates whether the adapter supports the BLE Central Role
    // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.iscentralrolesupported?view=winrt-22621
    return windows_bluetooth_adapter_.IsCentralRoleSupported();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
    return false;
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }
}

// Returns true if the Bluetooth hardware supports BLE Peripheral Role
bool BluetoothAdapter::IsPeripheralRoleSupported() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return false;
  }
  try {
    // Indicates whether the adapter supports the BLE Peripheral Role
    // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.isperipheralrolesupported?view=winrt-22621
    return windows_bluetooth_adapter_.IsPeripheralRoleSupported();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
    return false;
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }
}

// Returns true if the Bluetooth hardware supports BLE
bool BluetoothAdapter::IsLowEnergySupported() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return false;
  }
  try {
    // Indicates whether the adapter supports BLE
    // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter.islowenergysupported?view=winrt-22621
    return windows_bluetooth_adapter_.IsLowEnergySupported();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
    return false;
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
    return false;
  }
}

// Returns ScanMode passed to SetScanMode.
BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  return scan_mode_;
}

// No-op.  We do not change scan mode on Windows.
bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  scan_mode_ = scan_mode;
  return true;
}

// Returns the name passed to SetName, or the computer name if SetName has not
// been called.
std::string BluetoothAdapter::GetName() const {
  if (device_name_.has_value()) {
    return *device_name_;
  }
  return GetNameFromComputerName();
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  // Skip setting the name as Android cannot discover Windows bluetooth device
  // name that exceeded the 37 bytes limit (11 characters in EndpointInfo)
  device_name_ = std::string(name);
  return true;
}

// Returns BT MAC address assigned to this adapter.
MacAddress BluetoothAdapter::GetMacAddress() const {
  if (windows_bluetooth_adapter_ == nullptr) {
    LOG(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
    return MacAddress();
  }
  MacAddress mac_address;
  try {
    MacAddress::FromUint64(
            windows_bluetooth_adapter_.BluetoothAddress(), mac_address);
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": exception:" << exception.what();
  } catch (const winrt::hresult_error &ex) {
    LOG(ERROR) << __func__ << ": exception:" << ex.code() << ": "
               << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": unknown error.";
  }
  return mac_address;
}

std::string BluetoothAdapter::GetNameFromComputerName() const {
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  CHAR computer_name[MAX_COMPUTERNAME_LENGTH + 1];
  if (GetComputerNameA(computer_name, &size)) {
    return std::string(computer_name);
  }

  LOG(ERROR) << __func__ << ": Failed to get any computer name";
  return "";
}

}  // namespace nearby::windows
