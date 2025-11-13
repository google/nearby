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

#ifndef CORE_INTERNAL_BLE_L2CAP_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_BLE_L2CAP_ENDPOINT_CHANNEL_H_

#include <cstdint>
#include <memory>
#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#include "connections/implementation/mediums/ble/ble_socket.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {

class BleL2capEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Ble channels.
  BleL2capEndpointChannel(const std::string& service_id,
                          const std::string& channel_name,
                          BleL2capSocket socket);

  // A refactor version of the above constructor.
  BleL2capEndpointChannel(const std::string& service_id,
                          const std::string& channel_name,
                          std::unique_ptr<mediums::BleSocket> socket);

  // Returns the medium of this endpoint channel.
  location::nearby::proto::connections::Medium GetMedium() const override;

  // Returns the maximum transmit packet size of this endpoint channel.
  int GetMaxTransmitPacketSize() const override;

  ExceptionOr<ByteArray> DispatchPacket() override;
  ExceptionOr<std::int32_t> ReadPayloadLength() override;
  Exception WritePayloadLength(int payload_length) override;

 private:
  void CloseImpl() override;

  BleL2capSocket ble_l2cap_socket_;
  std::unique_ptr<mediums::BleSocket> ble_l2cap_socket_2_ = nullptr;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BLE_L2CAP_ENDPOINT_CHANNEL_H_
