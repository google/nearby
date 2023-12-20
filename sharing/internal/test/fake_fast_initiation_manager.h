// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_FAST_INITIATION_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_FAST_INITIATION_MANAGER_H_

#include <functional>
#include <utility>

#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"

namespace nearby {

class FakeFastInitiationManager : public api::FastInitiationManager {
 public:
  FakeFastInitiationManager() : api::FastInitiationManager() {}
  void StartAdvertising(api::FastInitBleBeacon::FastInitType type,
                        std::function<void()> callback,
                        std::function<void(api::FastInitiationManager::Error)>
                            error_callback) override {
    advertising_started_callback_ = std::move(callback);
    advertising_error_callback_ = std::move(error_callback);
    is_advertising_ = true;
  }

  void StopAdvertising(std::function<void()> callback) override {
    advertising_stopped_callback_ = std::move(callback);
    is_advertising_ = false;
  }

  void StartScanning(std::function<void()> devices_discovered_callback,
                     std::function<void()> devices_not_discovered_callback,
                     std::function<void(api::FastInitiationManager::Error)>
                         error_callback) override {
    scanning_discovered_callback_ = std::move(devices_discovered_callback);
    scanning_not_discovered_callback_ =
        std::move(devices_not_discovered_callback);
    scanning_error_callback_ = std::move(error_callback);
    is_scanning_ = true;
  }

  void StopScanning(std::function<void()> callback) override {
    scanning_stopped_callback_ = std::move(callback);
    is_scanning_ = false;
  }

  bool IsAdvertising() override { return is_advertising_; }

  bool IsScanning() override { return is_scanning_; }

  // Mock methods to simulate OS advertising/scanning event callbacks
  void SetAdvertisingStarted() { advertising_started_callback_(); }

  void SetAdvertisingError(api::FastInitiationManager::Error error) {
    advertising_error_callback_(error);
  }

  void SetAdvertisingStopped() { advertising_stopped_callback_(); }

  void SetScanningDiscovered() { scanning_discovered_callback_(); }

  void SetScanningNotDiscovered() { scanning_not_discovered_callback_(); }

  void SetScanningError(api::FastInitiationManager::Error error) {
    scanning_error_callback_(error);
  }

  void SetScanningStopped() { scanning_stopped_callback_(); }

 private:
  bool is_advertising_ = false;
  bool is_scanning_ = false;
  std::function<void()> advertising_started_callback_;
  std::function<void(api::FastInitiationManager::Error)>
      advertising_error_callback_;
  std::function<void()> advertising_stopped_callback_;
  std::function<void()> scanning_discovered_callback_;
  std::function<void()> scanning_not_discovered_callback_;
  std::function<void(api::FastInitiationManager::Error)>
      scanning_error_callback_;
  std::function<void()> scanning_stopped_callback_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_FAST_INITIATION_MANAGER_H_
