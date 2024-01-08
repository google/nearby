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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INITIATION_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INITIATION_MANAGER_H_

#include <functional>

#include "sharing/internal/api/fast_init_ble_beacon.h"

namespace nearby {
namespace api {

class FastInitiationManager {
 public:
  enum class Error : int {
    kUnknown = 0,
    kBluetoothRadioUnavailable,
    kResourceInUse,
    kDisabledByPolicy,
    kDisabledByUser,
    kHardwareNotSupported,
    kTransportNotSupported,
    kConsentRequired
  };

  FastInitiationManager() = default;
  virtual ~FastInitiationManager() = default;

  virtual void StartAdvertising(
      api::FastInitBleBeacon::FastInitType type,
      std::function<void()> callback,
      std::function<void(api::FastInitiationManager::Error)>
          error_callback) = 0;

  virtual void StopAdvertising(std::function<void()> callback) = 0;

  virtual void StartScanning(
      std::function<void()> devices_discovered_callback,
      std::function<void()> devices_not_discovered_callback,
      std::function<void(api::FastInitiationManager::Error)>
          error_callback) = 0;

  virtual void StopScanning(std::function<void()> callback) = 0;

  virtual bool IsAdvertising() = 0;

  virtual bool IsScanning() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_FAST_INITIATION_MANAGER_H_
