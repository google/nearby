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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INIT_BLE_BEACON_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INIT_BLE_BEACON_H_

#include "gmock/gmock.h"
#include "sharing/internal/api/fast_init_ble_beacon.h"

namespace nearby::sharing::api {

class MockFastInitBleBeacon : public nearby::api::FastInitBleBeacon {
 public:
  MockFastInitBleBeacon() = default;
  MockFastInitBleBeacon(const MockFastInitBleBeacon&) = delete;
  MockFastInitBleBeacon& operator=(const MockFastInitBleBeacon&) = delete;
  ~MockFastInitBleBeacon() override = default;

  MOCK_METHOD(void, SerializeToByteArray, (), (override));
  MOCK_METHOD(void, ParseFromByteArray, (), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_FAST_INIT_BLE_BEACON_H_
