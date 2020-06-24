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

#include "core_v2/internal/bluetooth_endpoint_channel.h"

#include <string>

#include "platform_v2/public/bluetooth_classic.h"
#include "platform_v2/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

OutputStream* GetOutputStreamOrNull(BluetoothSocket& socket) {
  if (socket.GetRemoteDevice().IsValid()) return &socket.GetOutputStream();
  return nullptr;
}

InputStream* GetInputStreamOrNull(BluetoothSocket& socket) {
  if (socket.GetRemoteDevice().IsValid()) return &socket.GetInputStream();
  return nullptr;
}

}  // namespace

BluetoothEndpointChannel::BluetoothEndpointChannel(
    const std::string& channel_name, BluetoothSocket socket)
    : BaseEndpointChannel(channel_name, GetInputStreamOrNull(socket),
                          GetOutputStreamOrNull(socket)),
      bluetooth_socket_(std::move(socket)) {}

proto::connections::Medium BluetoothEndpointChannel::GetMedium() const {
  return proto::connections::Medium::BLUETOOTH;
}

void BluetoothEndpointChannel::CloseImpl() {
  auto status = bluetooth_socket_.Close();
  if (!status.Ok()) {
    NEARBY_LOG(INFO, "Failed to close BT socket: exception=%d", status.value);
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
