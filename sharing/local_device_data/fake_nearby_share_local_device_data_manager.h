// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

namespace nearby::sharing {

// A fake implementation of NearbyShareLocalDeviceDataManager, along with a fake
// factory, to be used in tests.
class FakeNearbyShareLocalDeviceDataManager
    : public NearbyShareLocalDeviceDataManager {
 public:
  // Factory that creates FakeNearbyShareLocalDeviceDataManager instances. Use
  // in NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public NearbyShareLocalDeviceDataManagerImpl::Factory {
   public:
    Factory() = default;
    ~Factory() override = default;

    // Returns all FakeNearbyShareLocalDeviceDataManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareLocalDeviceDataManager*>& instances() {
      return instances_;
    }

   protected:
    std::unique_ptr<NearbyShareLocalDeviceDataManager> CreateInstance()
        override;

   private:
    std::vector<FakeNearbyShareLocalDeviceDataManager*> instances_;
  };

  explicit FakeNearbyShareLocalDeviceDataManager(
      absl::string_view default_device_name);
  ~FakeNearbyShareLocalDeviceDataManager() override = default;

  // NearbyShareLocalDeviceDataManager:
  std::string GetDeviceName() const override;
  DeviceNameValidationResult SetDeviceName(absl::string_view name) override;

  // Make protected observer-notification methods from the base class public in
  // this fake class.
  using NearbyShareLocalDeviceDataManager::NotifyLocalDeviceDataChanged;

  void set_next_validation_result(DeviceNameValidationResult result) {
    next_validation_result_ = result;
  }

 private:
  std::string device_name_;
  DeviceNameValidationResult next_validation_result_ =
      DeviceNameValidationResult::kValid;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
