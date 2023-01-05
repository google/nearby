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

#include "connections/implementation/ble_v2_endpoint_channel.h"

#include <string>
#include <utility>

#include "internal/platform/ble_v2.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(BleV2Socket& socket) {
  if (socket.GetRemotePeripheral().IsValid()) {
    return &socket.GetOutputStream();
  }
  return nullptr;
}

InputStream* GetInputStreamOrNull(BleV2Socket& socket) {
  if (socket.GetRemotePeripheral().IsValid()) {
    return &socket.GetInputStream();
  }
  return nullptr;
}

}  // namespace

BleV2EndpointChannel::BleV2EndpointChannel(const std::string& service_id,
                                           const std::string& channel_name,
                                           BleV2Socket socket)
    : BaseEndpointChannel(service_id, channel_name,
                          GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      ble_socket_(std::move(socket)) {}

location::nearby::proto::connections::Medium BleV2EndpointChannel::GetMedium()
    const {
  return location::nearby::proto::connections::Medium::BLE;
}

int BleV2EndpointChannel::GetMaxTransmitPacketSize() const {
  return kDefaultBleMaxTransmitPacketSize;
}

void BleV2EndpointChannel::CloseImpl() {
  Exception status = ble_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOGS(WARNING)
        << "Failed to close underlying socket for BleEndpointChannel "
        << GetName() << ": exception=" << status.value;
  }
}

}  // namespace connections
}  // namespace nearby
