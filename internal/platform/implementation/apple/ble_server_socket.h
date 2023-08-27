// Copyright 2023 Google LLC
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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

// TODO(b/293336684): Remove this file when shared Weave is complete.

#import <Foundation/Foundation.h>

#include <memory>

#include "absl/functional/any_invocable.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/ble_socket.h"

namespace nearby {
namespace apple {

// A BLE server socket for listening for incoming Weave sockets.
class BleServerSocket : public api::ble_v2::BleServerSocket {
 public:
  BleServerSocket() = default;
  ~BleServerSocket() override;

  // Wait for an available socket.
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  //
  // On success, returns connected socket, ready to exchange data or nullptr on
  // error. Once error is reported, it is permanent, and ServerSocket must be
  // closed.
  std::unique_ptr<api::ble_v2::BleSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Close the server socket.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool Connect(std::unique_ptr<BleSocket> socket) ABSL_LOCKS_EXCLUDED(mutex_);
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;
  absl::flat_hash_set<std::unique_ptr<BleSocket>> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace apple
}  // namespace nearby
