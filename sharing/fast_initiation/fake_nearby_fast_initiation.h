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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_H_
#define THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_H_

#include <functional>
#include <memory>

#include "internal/base/observer_list.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/fast_initiation/nearby_fast_initiation_impl.h"
#include "sharing/internal/public/context.h"

namespace nearby {
namespace sharing {

// FakeNearbyFastInitiation is a fake implementation of NearbyFastInitiation.
// In the implementation, developers can simulate different output by calling
// faked methods, such as setting offload supports on scanning or advertising.
class FakeNearbyFastInitiation : public NearbyFastInitiation {
 public:
  class Factory : public NearbyFastInitiationImpl::Factory {
   public:
    Factory() = default;
    ~Factory() override = default;

    void SetStartScanningError(bool is_start_scanning_error);

    void SetStartAdvertisingError(bool is_start_advertising_error);

    void SetLowEnergySupported(bool is_low_energy_supported);

    void SetScanOffloadSupported(bool is_scan_offload_supported);

    void SetAdvertisementOffloadSupported(
        bool is_advertisement_offload_supported);

    FakeNearbyFastInitiation* GetNearbyFastInitiation();

   private:
    std::unique_ptr<NearbyFastInitiation> CreateInstance(
        Context* context) override;

    bool is_start_scanning_error_ = false;
    bool is_start_advertising_error_ = false;
    bool is_low_energy_supported_ = true;
    bool is_scan_offload_supported_ = true;
    bool is_advertisement_offload_supported_ = true;
    FakeNearbyFastInitiation* fake_nearby_fast_initiation_ = nullptr;
  };

  explicit FakeNearbyFastInitiation(Context* context);
  ~FakeNearbyFastInitiation() override = default;

  bool IsLowEnergySupported() override;
  bool IsScanOffloadSupported() override;
  bool IsAdvertisementOffloadSupported() override;

  void StartScanning(std::function<void()> devices_discovered_callback,
                     std::function<void()> devices_not_discovered_callback,
                     std::function<void()> error_callback) override;

  void StopScanning(std::function<void()> callback) override;

  void StartAdvertising(FastInitType type, std::function<void()> callback,
                        std::function<void()> error_callback) override;

  void StopAdvertising(std::function<void()> callback) override;

  bool IsScanning() const override { return is_scanning_; }
  bool IsAdvertising() const override { return is_advertising_; }

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(Observer* observer) override;

  // Fake methods
  void SetStartScanningError(bool is_start_scanning_error);
  void SetStartAdvertisingError(bool is_start_advertising_error);
  void SetLowEnergySupported(bool is_low_energy_supported);
  void SetScanOffloadSupported(bool is_scan_offload_supported);
  void SetAdvertisementOffloadSupported(
      bool is_advertisement_offload_supported);

  int StartScanningCount() const;
  int StopScanningCount() const;
  int StartAdvertisingCount() const;
  int StopAdvertisingCount() const;

  void FireDevicesDetected();
  void FireDevicesNotDetected();

 private:
  Context* context_;
  bool is_scanning_ = false;
  bool is_advertising_ = false;
  bool is_start_scanning_error_ = false;
  bool is_start_advertising_error_ = false;
  bool is_low_energy_supported_ = true;
  bool is_scan_offload_supported_ = true;
  bool is_advertisement_offload_supported_ = true;
  int start_scanning_call_count_ = 0;
  int stop_scanning_call_count_ = 0;
  int start_advertising_call_count_ = 0;
  int stop_advertising_call_count_ = 0;

  std::function<void()> scanning_devices_discovered_callback_ = nullptr;
  std::function<void()> scanning_devices_not_discovered_callback_ = nullptr;

  ObserverList<NearbyFastInitiation::Observer> observer_list_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_FAKE_NEARBY_FAST_INITIATION_H_
