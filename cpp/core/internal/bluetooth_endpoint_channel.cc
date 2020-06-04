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

#include "core/internal/bluetooth_endpoint_channel.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

Ptr<BluetoothEndpointChannel> BluetoothEndpointChannel::createOutgoing(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(new BluetoothEndpointChannel(channel_name, bluetooth_socket));
}

Ptr<BluetoothEndpointChannel> BluetoothEndpointChannel::createIncoming(
    Ptr<MediumManager<Platform> > medium_manager, const string& channel_name,
    Ptr<BluetoothSocket> bluetooth_socket) {
  return MakePtr(new BluetoothEndpointChannel(channel_name, bluetooth_socket));
}

BluetoothEndpointChannel::BluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket)
    : BaseEndpointChannel(channel_name, bluetooth_socket->getInputStream(),
                          bluetooth_socket->getOutputStream()),
      bluetooth_socket_(bluetooth_socket) {}

BluetoothEndpointChannel::~BluetoothEndpointChannel() {}

proto::connections::Medium BluetoothEndpointChannel::getMedium() {
  return proto::connections::Medium::BLUETOOTH;
}

void BluetoothEndpointChannel::closeImpl() {
  Exception::Value exception = bluetooth_socket_->close();
  if (exception != Exception::NONE) {
    if (exception == Exception::IO) {
      // TODO(tracyzhou): Add logging.
    }
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
