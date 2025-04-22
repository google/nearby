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

// Note: File language is detected using heuristics. Many Objective-C++ headers
// are incorrectly classified as C++ resulting in invalid linter errors. The use
// of "NSArray" and other Foundation classes like "NSData", "NSDictionary" and
// "NSUUID" are highly weighted for Objective-C and Objective-C++ scores. Oddly,
// "#import <Foundation/Foundation.h>" does not contribute any points. This
// comment alone should be enough to trick the IDE in to believing this is
// actually some sort of Objective-C file. See:
// cs/google3/devtools/search/lang/recognize_language_classifiers_data

#include <memory>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/ble_l2cap_socket.h"

namespace nearby {
namespace apple {

// A BLE L2CAP server socket for listening incoming L2CAP socket.
class BleL2capServerSocket : public api::ble_v2::BleL2capServerSocket {
 public:
  BleL2capServerSocket() = default;
  ~BleL2capServerSocket() override = default;

  // Gets PSM value has been published by the server.
  int GetPSM() const override;

  // Sets PSM value has been published by the server.
  void SetPSM(int PSM);

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and L2CAP ServerSocket has to be
  // closed.
  std::unique_ptr<api::ble_v2::BleL2capSocket> Accept() override;

  // Closes the L2CAP server socket.
  Exception Close() override;

  // Connects to the L2CAP server socket.
  bool Connect(std::unique_ptr<BleL2capSocket> socket);

 private:
  // The PSM value of the L2CAP server socket.
  int PSM_ = 0;
};

}  // namespace apple
}  // namespace nearby