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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_

// clang-format off
#include <windows.h>
#include <guiddef.h>
// clang-format on

#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Radios.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {

// Represents a Bluetooth adapter.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter?view=winrt-20348
using WindowsBluetoothAdapter =
    winrt::Windows::Devices::Bluetooth::BluetoothAdapter;

// Represents a radio device on the system.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radio?view=winrt-20348
using winrt::Windows::Devices::Radios::IRadio;

// Enumeration that describes possible radio states.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.radios.radiostate?view=winrt-20348
using winrt::Windows::Devices::Radios::RadioState;

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  BluetoothAdapter();

  ~BluetoothAdapter() override = default;

  typedef absl::AnyInvocable<void(api::BluetoothAdapter::ScanMode)>
      ScanModeCallback;

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  bool SetStatus(Status status) override;

  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  bool IsEnabled() const override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  // Returns ScanMode::kUnknown on error.
  ScanMode GetScanMode() const override;

  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(ScanMode scan_mode) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  std::string GetName() const override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  bool SetName(absl::string_view name) override;
  bool SetName(absl::string_view name, bool persist) override;

  // Returns BT MAC address assigned to this adapter.
  std::string GetMacAddress() const override;

  // Returns bluetooth device name from registry
  std::string GetNameFromRegistry(PHKEY hKey) const;

  // Returns computer name
  std::string GetNameFromComputerName() const;

  void SetOnScanModeChanged(ScanModeCallback callback) {
    if (scan_mode_changed_) {
      scan_mode_changed_ = std::move(callback);
    }
  }

  // Returns true if the Bluetooth hardware supports Bluetooth 5.0 Extended
  // Advertising
  bool IsExtendedAdvertisingSupported() const;
  void RestoreRadioNameIfNecessary();

 private:
  void process_error();
  void StoreRadioNames(absl::string_view original_radio_name,
                       absl::string_view nearby_radio_name);

  WindowsBluetoothAdapter windows_bluetooth_adapter_;
  std::string registry_bluetooth_adapter_name_;

  IRadio windows_bluetooth_radio_;
  char *GetGenericBluetoothAdapterInstanceID() const;
  void find_and_replace(char *source, const char *strFind,
                        const char *strReplace) const;
  ScanMode scan_mode_ = ScanMode::kNone;
  ScanModeCallback scan_mode_changed_;

  // Used to fake the device name when the device name is longer than android
  // limitation.
  std::optional<std::string> device_name_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
