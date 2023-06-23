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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_GATT_CALLBACKS_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_GATT_CALLBACKS_H_

#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/future.h"

namespace nearby {
namespace fastpair {

// Fake GATT characteristic callbacks for tests.
// Register the callback returned by `GetGattCallback()` with the GattServer and
// then communicate with the GATT client via `characteristics_`.
class FakeGattCallbacks {
  using GattCharacteristic = ::nearby::api::ble_v2::GattCharacteristic;

 public:
  struct CharacteristicData {
    // Value written to the characteristic by the gatt client
    Future<std::string> write_value;
    // Write result returned to the gatt client.
    absl::Status write_result = absl::OkStatus();
    // Value returned to the gatt client when they try to read the
    // characteristic
    absl::StatusOr<std::string> read_value =
        absl::FailedPreconditionError("characteristic not set");
    absl::AnyInvocable<absl::Status(absl::string_view)> write_callback;
    absl::AnyInvocable<absl::StatusOr<std::string>()> read_callback;

    absl::Status WriteCallback(absl::string_view data) {
      if (write_callback) {
        return write_callback(data);
      }
      write_value.Set(std::string(data));
      return write_result;
    }
    absl::StatusOr<std::string> ReadCallback() {
      if (read_callback) {
        return read_callback();
      }
      return read_value;
    }
  };

  BleV2Medium::ServerGattConnectionCallback GetGattCallback() {
    return BleV2Medium::ServerGattConnectionCallback{
        .on_characteristic_read_cb =
            [&](const api::ble_v2::BlePeripheral& remote_device,
                const GattCharacteristic& characteristic, int offset,
                BleV2Medium::ServerGattConnectionCallback::ReadValueCallback
                    callback) {
              auto it = characteristics_.find(characteristic);
              if (it == characteristics_.end()) {
                callback(absl::NotFoundError("characteristic not found"));
                return;
              }
              callback(it->second.ReadCallback());
            },
        .on_characteristic_write_cb =
            [&](const api::ble_v2::BlePeripheral& remote_device,
                const GattCharacteristic& characteristic, int offset,
                absl::string_view data,
                BleV2Medium::ServerGattConnectionCallback::WriteValueCallback
                    callback) {
              auto it = characteristics_.find(characteristic);
              if (it == characteristics_.end()) {
                callback(absl::NotFoundError("characteristic not found"));
                return;
              }
              callback(it->second.WriteCallback(data));
            }};
  }

  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_GATT_CALLBACKS_H_
