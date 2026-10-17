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
#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_FAST_INITIATION_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_FAST_INITIATION_MANAGER_H_

#include <functional>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"

namespace nearby::sharing::windows {

// The 26-byte Fast Initiation service data, laid out as the official Quick
// Share for Windows client serializes it.
class FastInitBleBeacon : public nearby::api::FastInitBleBeacon {
 public:
  void SerializeToByteArray() override;
  // Only advertising is implemented on Windows, so beacons are never parsed.
  void ParseFromByteArray() override {}
};

// Advertises the Fast Initiation beacon with a BluetoothLEAdvertisement
// publisher. Scanning is not implemented on Windows.
class FastInitiationManager : public nearby::api::FastInitiationManager {
 public:
  explicit FastInitiationManager(const sharing::api::BluetoothAdapter& adapter)
      : adapter_(adapter) {}
  ~FastInitiationManager() override;

  void StartAdvertising(nearby::api::FastInitBleBeacon::FastInitType type,
                        std::function<void()> callback,
                        std::function<void(Error)> error_callback) override;
  void StopAdvertising(std::function<void()> callback) override;
  void StartScanning(std::function<void()> devices_discovered_callback,
                     std::function<void()> devices_not_discovered_callback,
                     std::function<void(Error)> error_callback) override {
    error_callback(Error::kHardwareNotSupported);
  }
  void StopScanning(std::function<void()> callback) override { callback(); }
  bool IsAdvertising() override;
  bool IsScanning() override { return false; }

 private:
  using Publisher = winrt::Windows::Devices::Bluetooth::Advertisement::
      BluetoothLEAdvertisementPublisher;

  void OnStatusChanged(
      const Publisher& publisher,
      const winrt::Windows::Devices::Bluetooth::Advertisement::
          BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args);
  void ReleasePublisher() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const sharing::api::BluetoothAdapter& adapter_;
  absl::Mutex mutex_;
  Publisher publisher_ ABSL_GUARDED_BY(mutex_) = nullptr;
  winrt::event_token status_token_ ABSL_GUARDED_BY(mutex_);
  bool is_advertising_ ABSL_GUARDED_BY(mutex_) = false;
  std::function<void()> start_callback_ ABSL_GUARDED_BY(mutex_);
  std::function<void(Error)> error_callback_ ABSL_GUARDED_BY(mutex_);
  std::function<void()> stop_callback_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::sharing::windows

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_WINDOWS_FAST_INITIATION_MANAGER_H_
