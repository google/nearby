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

#ifndef CONNECTIONS_IMPLEMENTATION_BLE_V2_ENDPOINT_CHANNEL_H_
#define CONNECTIONS_IMPLEMENTATION_BLE_V2_ENDPOINT_CHANNEL_H_

#include <memory>
#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#include "connections/implementation/mediums/ble_v2/ble_socket.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {

class BleV2EndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Ble channels.
  BleV2EndpointChannel(const std::string& service_id,
                       const std::string& channel_name, BleV2Socket socket);

  // A refactor version of the above constructor.
  BleV2EndpointChannel(const std::string& service_id,
                       const std::string& channel_name,
                       std::unique_ptr<mediums::BleSocket> socket);

  location::nearby::proto::connections::Medium GetMedium() const override;

  int GetMaxTransmitPacketSize() const override;

  ExceptionOr<ByteArray> DispatchPacket() override;
  Exception WritePayloadLength(int payload_length) override;

 private:
  static constexpr int kDefaultBleMaxTransmitPacketSize = 512;  // 512 bytes

  void CloseImpl() override;

  BleV2Socket ble_socket_;
  std::unique_ptr<mediums::BleSocket> ble_socket_2_ = nullptr;
};

}  // namespace connections
}  // namespace nearby

#endif  // CONNECTIONS_IMPLEMENTATION_BLE_V2_ENDPOINT_CHANNEL_H_
