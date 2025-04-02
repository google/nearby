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

#import "internal/platform/implementation/apple/ble_l2cap_server_socket.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/ble_l2cap_socket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

BleL2capServerSocket::BleL2capServerSocket(GNCBLEL2CAPServer* l2cap_server)
    : l2cap_server_(l2cap_server) {}

int BleL2capServerSocket::GetPSM() const { return [l2cap_server_ PSM]; }

std::unique_ptr<api::ble_v2::BleL2capSocket> BleL2capServerSocket::Accept() {
  // TODO: edwinwu - Implement to wrap up l2cap channel with |GNCBLEL2CAPStream|.
  return nullptr;
}

Exception BleL2capServerSocket::Close() {
  [l2cap_server_ close];
  return {Exception::kSuccess};
}

}  // namespace apple
}  // namespace nearby