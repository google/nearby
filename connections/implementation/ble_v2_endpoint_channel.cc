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

#include <memory>
#include <string>
#include <utility>

#include "connections/implementation/base_endpoint_channel.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble_v2/ble_socket.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(BleV2Socket& socket) {
  if (socket.IsValid()) {
    return &socket.GetOutputStream();
  }
  return nullptr;
}

InputStream* GetInputStreamOrNull(BleV2Socket& socket) {
  if (socket.IsValid()) {
    return &socket.GetInputStream();
  }
  return nullptr;
}

OutputStream* GetOutputStreamOrNull(mediums::BleSocket* socket) {
  if (socket != nullptr && socket->IsValid()) {
    return &socket->GetOutputStream();
  }
  return nullptr;
}

InputStream* GetInputStreamOrNull(mediums::BleSocket* socket) {
  if (socket != nullptr && socket->IsValid()) {
    return &socket->GetInputStream();
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

BleV2EndpointChannel::BleV2EndpointChannel(
    const std::string& service_id, const std::string& channel_name,
    std::unique_ptr<mediums::BleSocket> socket)
    : BaseEndpointChannel(service_id, channel_name,
                          GetInputStreamOrNull(socket.get()),
                          GetOutputStreamOrNull(socket.get())),
      ble_socket_2_(std::move(socket)) {}

location::nearby::proto::connections::Medium BleV2EndpointChannel::GetMedium()
    const {
  return location::nearby::proto::connections::Medium::BLE;
}

int BleV2EndpointChannel::GetMaxTransmitPacketSize() const {
  return kDefaultBleMaxTransmitPacketSize;
}

void BleV2EndpointChannel::CloseImpl() {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kRefactorBleL2cap)) {
    if (ble_socket_2_ == nullptr || !ble_socket_2_->IsValid()) {
      LOG(WARNING) << "BleV2EndpointChannel " << GetName()
                   << " is already closed.";
      return;
    }
    Exception status = ble_socket_2_->Close();
    if (!status.Ok()) {
      LOG(WARNING)
          << "Failed to close underlying socket for BleEndpointChannel "
          << GetName() << ": exception=" << status.value;
    }
  } else {
    Exception status = ble_socket_.Close();
    if (!status.Ok()) {
      LOG(WARNING)
          << "Failed to close underlying socket for BleEndpointChannel "
          << GetName() << ": exception=" << status.value;
    }
  }

  LOG(INFO) << "BleV2EndpointChannel " << GetName() << " is already closed.";
}

ExceptionOr<ByteArray> BleV2EndpointChannel::DispatchPacket() {
  return ble_socket_2_->DispatchPacket();
}

Exception BleV2EndpointChannel::WritePayloadLength(int payload_length) {
  return ble_socket_2_->WritePayloadLength(payload_length);
}

}  // namespace connections
}  // namespace nearby
