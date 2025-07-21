// Copyright 2025 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_MOCK_BLE_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_MOCK_BLE_SOCKET_H_

#include "gmock/gmock.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace testing {

class MockBleSocket : public api::ble_v2::BleSocket {
 public:
  MOCK_METHOD(InputStream&, GetInputStream, (), (override));
  MOCK_METHOD(OutputStream&, GetOutputStream, (), (override));
  MOCK_METHOD(Exception, Close, (), (override));
  MOCK_METHOD(api::ble_v2::BlePeripheral::UniqueId, GetRemotePeripheralId, (),
              (override));
};

class MockBleL2capSocket : public api::ble_v2::BleL2capSocket {
 public:
  MOCK_METHOD(InputStream&, GetInputStream, (), (override));
  MOCK_METHOD(OutputStream&, GetOutputStream, (), (override));
  MOCK_METHOD(Exception, Close, (), (override));
  MOCK_METHOD(api::ble_v2::BlePeripheral::UniqueId, GetRemotePeripheralId, (),
              (override));
};

}  // namespace testing
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_MOCK_BLE_SOCKET_H_
