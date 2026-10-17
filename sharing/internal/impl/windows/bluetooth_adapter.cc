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

#include "sharing/internal/impl/windows/bluetooth_adapter.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "winrt/Windows.Foundation.h"

namespace nearby::sharing::windows {
namespace {

using ::winrt::Windows::Devices::Radios::RadioAccessStatus;
using ::winrt::Windows::Foundation::AsyncStatus;
using ::winrt::Windows::Devices::Radios::RadioState;

void LogWinRtError(const char* operation, const winrt::hresult_error& error) {
  LOG(WARNING) << operation << " failed: " << error.code() << ": "
               << winrt::to_string(error.message());
}

}  // namespace

BluetoothAdapter::BluetoothAdapter() {
  try {
    adapter_ = winrt::Windows::Devices::Bluetooth::BluetoothAdapter::
                   GetDefaultAsync()
                       .get();
    if (adapter_ == nullptr) {
      LOG(WARNING) << "No Bluetooth adapter on this device.";
      return;
    }
    radio_ = adapter_.GetRadioAsync().get();
    if (radio_ == nullptr) return;
    {
      absl::MutexLock lock(mutex_);
      powered_ = radio_.State() == RadioState::On;
    }
    radio_state_token_ = radio_.StateChanged(
        [this](const auto&, const auto&) { OnRadioStateChanged(); });
  } catch (const winrt::hresult_error& error) {
    LogWinRtError("Bluetooth adapter initialization", error);
  }
}

BluetoothAdapter::~BluetoothAdapter() {
  if (radio_ != nullptr) radio_.StateChanged(radio_state_token_);
}

bool BluetoothAdapter::IsPresent() const { return adapter_ != nullptr; }

bool BluetoothAdapter::IsPowered() const {
  absl::MutexLock lock(mutex_);
  return powered_;
}

bool BluetoothAdapter::IsLowEnergySupported() const {
  return adapter_ != nullptr && adapter_.IsLowEnergySupported();
}

bool BluetoothAdapter::IsAdvertisementOffloadSupported() const {
  return adapter_ != nullptr && adapter_.IsAdvertisementOffloadSupported();
}

bool BluetoothAdapter::IsExtendedAdvertisingSupported() const {
  return adapter_ != nullptr && adapter_.IsExtendedAdvertisingSupported();
}

bool BluetoothAdapter::IsPeripheralRoleSupported() const {
  return adapter_ != nullptr && adapter_.IsPeripheralRoleSupported();
}

sharing::api::BluetoothAdapter::PermissionStatus
BluetoothAdapter::GetOsPermissionStatus() const {
  if (radio_ == nullptr) return PermissionStatus::kUndetermined;
  try {
    switch (winrt::Windows::Devices::Radios::Radio::RequestAccessAsync().get()) {
      case RadioAccessStatus::Allowed:
        return PermissionStatus::kAllowed;
      case RadioAccessStatus::DeniedByUser:
        return PermissionStatus::kUserDenied;
      case RadioAccessStatus::DeniedBySystem:
        return PermissionStatus::kSystemDenied;
      default:
        return PermissionStatus::kUndetermined;
    }
  } catch (const winrt::hresult_error& error) {
    LogWinRtError("Radio access request", error);
    return PermissionStatus::kUndetermined;
  }
}

void BluetoothAdapter::SetPowered(bool powered,
                                  std::function<void()> success_callback,
                                  std::function<void()> error_callback) {
  if (radio_ == nullptr) {
    if (error_callback) error_callback();
    return;
  }
  try {
    radio_.SetStateAsync(powered ? RadioState::On : RadioState::Off)
        .Completed([success = std::move(success_callback),
                    error = std::move(error_callback)](
                       const auto& operation, AsyncStatus status) {
          bool ok = status == AsyncStatus::Completed &&
                    operation.GetResults() == RadioAccessStatus::Allowed;
          if (ok && success) success();
          if (!ok && error) error();
        });
  } catch (const winrt::hresult_error& winrt_error) {
    LogWinRtError("Radio state change", winrt_error);
    if (error_callback) error_callback();
  }
}

std::optional<std::string> BluetoothAdapter::GetAdapterId() const {
  if (adapter_ == nullptr) return std::nullopt;
  return winrt::to_string(adapter_.DeviceId());
}

MacAddress BluetoothAdapter::GetAddress() const {
  MacAddress address;
  if (adapter_ != nullptr) {
    MacAddress::FromUint64(adapter_.BluetoothAddress(), address);
  }
  return address;
}

void BluetoothAdapter::AddObserver(Observer* observer) {
  absl::MutexLock lock(mutex_);
  observers_.insert(observer);
}

void BluetoothAdapter::RemoveObserver(Observer* observer) {
  absl::MutexLock lock(mutex_);
  observers_.erase(observer);
}

bool BluetoothAdapter::HasObserver(Observer* observer) {
  absl::MutexLock lock(mutex_);
  return observers_.contains(observer);
}

void BluetoothAdapter::OnRadioStateChanged() {
  bool powered = radio_.State() == RadioState::On;
  std::vector<Observer*> observers;
  {
    absl::MutexLock lock(mutex_);
    if (powered_ == powered) return;
    powered_ = powered;
    observers.assign(observers_.begin(), observers_.end());
  }
  for (Observer* observer : observers) {
    observer->AdapterPoweredChanged(this, powered);
  }
}

}  // namespace nearby::sharing::windows
