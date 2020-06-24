// Copyright 2020 Google LLC
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

#ifndef CORE_V2_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
#define CORE_V2_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_

#include <string>

#include "core_v2/internal/base_endpoint_channel.h"
#include "platform_v2/public/bluetooth_classic.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

class BluetoothEndpointChannel final : public BaseEndpointChannel {
 public:
  // Creates both outgoing and incoming BT channels.
  BluetoothEndpointChannel(const std::string& channel_name,
                           BluetoothSocket bluetooth_socket);

  proto::connections::Medium GetMedium() const override;

 private:
  void CloseImpl() override;

  BluetoothSocket bluetooth_socket_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_BLUETOOTH_ENDPOINT_CHANNEL_H_
