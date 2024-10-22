// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INITIATION_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INITIATION_MANAGER_H_

#include <functional>

#include "gmock/gmock.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"
#include "sharing/internal/api/fast_initiation_manager.h"

namespace nearby::sharing::api {

class MockFastInitiationManager : public nearby::api::FastInitiationManager {
 public:
  MockFastInitiationManager() = default;
  MockFastInitiationManager(const MockFastInitiationManager&) = delete;
  MockFastInitiationManager& operator=(const MockFastInitiationManager&) =
      delete;
  ~MockFastInitiationManager() override = default;

  MOCK_METHOD(void, StartAdvertising,
              (nearby::api::FastInitBleBeacon::FastInitType type,
               std::function<void()> callback,
               std::function<void(nearby::api::FastInitiationManager::Error)>
                   error_callback),
              (override));

  MOCK_METHOD(void, StopAdvertising, (std::function<void()> callback),
              (override));

  MOCK_METHOD(void, StartScanning,
              (std::function<void()> devices_discovered_callback,
               std::function<void()> devices_not_discovered_callback,
               std::function<void(nearby::api::FastInitiationManager::Error)>
                   error_callback),
              (override));

  MOCK_METHOD(void, StopScanning, (std::function<void()> callback), (override));

  MOCK_METHOD(bool, IsAdvertising, (), (override));

  MOCK_METHOD(bool, IsScanning, (), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INITIATION_MANAGER_H_
