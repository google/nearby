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

#include <string>

#include "connections/implementation/base_endpoint_channel.h"
#include "internal/platform/ble.h"

namespace nearby {
namespace connections {

class BleL2capEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming Ble channels.
  BleL2capEndpointChannel(const std::string& service_id,
                          const std::string& channel_name,
                          BleL2capSocket socket);

  // Returns the medium of this endpoint channel.
  location::nearby::proto::connections::Medium GetMedium() const override;

  // Returns the maximum transmit packet size of this endpoint channel.
  int GetMaxTransmitPacketSize() const override;

 private:
  void CloseImpl() override;

  BleL2capSocket ble_l2cap_socket_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BLE_L2CAP_ENDPOINT_CHANNEL_H_
