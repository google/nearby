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

int BleL2capServerSocket::GetPSM() const { return PSM_; }

void BleL2capServerSocket::SetPSM(int PSM) { PSM_ = PSM; }

std::unique_ptr<api::ble_v2::BleL2capSocket> BleL2capServerSocket::Accept() {
  // TODO: b/399815436 - Implement to accept incoming l2cap connection.
  return nullptr;
}

bool BleL2capServerSocket::Connect(std::unique_ptr<BleL2capSocket> socket) {
  // TODO: b/399815436 - Implement to connect to l2cap server socket.
  return false;
}

Exception BleL2capServerSocket::Close() {
  return {Exception::kSuccess};
}

}  // namespace apple
}  // namespace nearby