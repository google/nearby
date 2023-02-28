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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

class BleV2Socket : public api::ble_v2::BleSocket {
 public:
  BleV2Socket() = default;
  ~BleV2Socket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;

  api::ble_v2::BlePeripheral* GetRemotePeripheral() override;

  bool Connect(api::ble_v2::BlePeripheral* ble_peripheral);

 private:
  class BleInputStream : public InputStream {
   public:
    ~BleInputStream() override = default;
    ExceptionOr<ByteArray> Read(std::int64_t size) override;
    Exception Close() override;
  };

  class BleOutputStream : public OutputStream {
   public:
    ~BleOutputStream() override = default;

    Exception Write(const ByteArray& data) override;
    Exception Flush() override;
    Exception Close() override;
  };

  BleInputStream input_stream_{};
  BleOutputStream output_stream_{};
  api::ble_v2::BlePeripheral* ble_peripheral_ = nullptr;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_SOCKET_H_
