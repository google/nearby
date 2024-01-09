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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_IMPL_H_

#include <functional>
#include <memory>

#include "internal/base/observer_list.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/public/context.h"

namespace nearby {
namespace sharing {

class NearbyFastInitiationImpl : public NearbyFastInitiation {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyFastInitiation> Create(Context* context);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory() = default;
    virtual std::unique_ptr<NearbyFastInitiation> CreateInstance(
        Context* context) = 0;

   private:
    static Factory* test_factory_;
  };

  explicit NearbyFastInitiationImpl(Context* context);

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

  bool IsScanning() const override;

  bool IsAdvertising() const override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(Observer* observer) override;

 private:
  // Handle the scanning error codes and print out to logs
  void ScanningErrorCodeCallbackHandler(
      nearby::api::FastInitiationManager::Error error);

  // Handle the advertising error codes and print out to logs
  void AdvertisingErrorCodeCallbackHandler(
      nearby::api::FastInitiationManager::Error error);

  Context* const context_;
  ObserverList<NearbyFastInitiation::Observer> observer_list_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_IMPL_H_
