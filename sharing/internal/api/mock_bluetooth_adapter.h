// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_BLUETOOTH_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_BLUETOOTH_ADAPTER_H_

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "sharing/internal/api/bluetooth_adapter.h"

namespace nearby::sharing::api {

class MockBluetoothAdapter : public nearby::sharing::api::BluetoothAdapter {
 public:
  MockBluetoothAdapter() = default;
  MockBluetoothAdapter(const MockBluetoothAdapter&) = delete;
  MockBluetoothAdapter& operator=(const MockBluetoothAdapter&) = delete;
  ~MockBluetoothAdapter() override = default;

  MOCK_METHOD(bool, IsPresent, (), (const, override));
  MOCK_METHOD(bool, IsPowered, (), (const, override));
  MOCK_METHOD(bool, IsLowEnergySupported, (), (const, override));
  MOCK_METHOD(bool, IsScanOffloadSupported, (), (const, override));
  MOCK_METHOD(bool, IsAdvertisementOffloadSupported, (), (const, override));
  MOCK_METHOD(bool, IsExtendedAdvertisingSupported, (), (const, override));
  MOCK_METHOD(bool, IsPeripheralRoleSupported, (), (const, override));
  MOCK_METHOD(PermissionStatus, GetOsPermissionStatus, (), (const, override));
  MOCK_METHOD(void, SetPowered,
              (bool powered, std::function<void()> success_callback,
               std::function<void()> error_callback),
              (override));
  MOCK_METHOD(std::optional<std::string>, GetAdapterId, (), (const, override));
  MOCK_METHOD((std::optional<std::array<uint8_t, 6>>), GetAddress, (),
              (const, override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(bool, HasObserver, (Observer * observer), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_BLUETOOTH_ADAPTER_H_
