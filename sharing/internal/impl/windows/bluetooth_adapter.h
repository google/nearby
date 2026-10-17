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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/mac_address.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Radios.h"

namespace nearby::sharing::windows {

// The default WinRT Bluetooth adapter and its radio; radio state changes are
// reported to observers as power changes.
class BluetoothAdapter : public sharing::api::BluetoothAdapter {
 public:
  BluetoothAdapter();
  ~BluetoothAdapter() override;

  bool IsPresent() const override;
  bool IsPowered() const override;
  bool IsLowEnergySupported() const override;
  // ponytail: WinRT exposes no scan-offload query; reported unsupported.
  bool IsScanOffloadSupported() const override { return false; }
  bool IsAdvertisementOffloadSupported() const override;
  bool IsExtendedAdvertisingSupported() const override;
  bool IsPeripheralRoleSupported() const override;
  PermissionStatus GetOsPermissionStatus() const override;
  void SetPowered(bool powered, std::function<void()> success_callback,
                  std::function<void()> error_callback) override;
  std::optional<std::string> GetAdapterId() const override;
  MacAddress GetAddress() const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(Observer* observer) override;

 private:
  void OnRadioStateChanged();

  winrt::Windows::Devices::Bluetooth::BluetoothAdapter adapter_ = nullptr;
  winrt::Windows::Devices::Radios::Radio radio_ = nullptr;
  winrt::event_token radio_state_token_;

  mutable absl::Mutex mutex_;
  bool powered_ ABSL_GUARDED_BY(mutex_) = false;
  absl::flat_hash_set<Observer*> observers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_BLUETOOTH_ADAPTER_H_
