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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#import <Foundation/Foundation.h>  // IWYU pragma: keep

#include <memory>

#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/ble_peripheral.h"

@class GNCMConnectionHandlers;
@protocol GNCMConnection;

namespace nearby {
namespace apple {

// A readable stream of bytes.
class BleL2capInputStream : public InputStream {
 public:
  BleL2capInputStream();
  ~BleL2capInputStream() override;

  // Reads at most `size` bytes from the input stream.
  //
  // Returns an empty byte array on end of file, or Exception::kIo on error.
  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  // Closes the stream preventing further reads.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override;

  GNCMConnectionHandlers *GetConnectionHandlers() { return connectionHandlers_; }

 private:
  GNCMConnectionHandlers *connectionHandlers_;
  NSMutableArray<NSData *> *newDataPackets_;
  NSMutableData *accumulatedData_;
  NSCondition *condition_;
};

// A writable stream of bytes.
class BleL2capOutputStream : public OutputStream {
 public:
  explicit BleL2capOutputStream(id<GNCMConnection> connection)
      : connection_(connection), condition_([[NSCondition alloc] init]) {}
  ~BleL2capOutputStream() override;

  // Write the provided bytes to the output stream.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Write(const ByteArray &data) override;

  // no-op
  //
  // Always returns Exception::kSuccess.
  Exception Flush() override;

  // Closes the stream preventing further writes.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override;

 private:
  id<GNCMConnection> connection_;
  NSCondition *condition_;
};

// A BLE L2CAP socket.
class BleL2capSocket : public api::ble_v2::BleL2capSocket {
 public:
  explicit BleL2capSocket(id<GNCMConnection> connection);

  // The peripheral used to create the socket must outlive the socket or undefined behavior will
  // occur.
  BleL2capSocket(id<GNCMConnection> connection, api::ble_v2::BlePeripheral *peripheral);
  ~BleL2capSocket() override;

  // Returns the InputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  BleL2capInputStream &GetInputStream() override { return *input_stream_; }

  // Returns the OutputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  BleL2capOutputStream &GetOutputStream() override { return *output_stream_; }

  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  api::ble_v2::BlePeripheral *GetRemotePeripheral() override { return peripheral_; }

  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  std::unique_ptr<BleL2capInputStream> input_stream_;
  std::unique_ptr<BleL2capOutputStream> output_stream_;
  api::ble_v2::BlePeripheral *peripheral_;
};

}  // namespace apple
}  // namespace nearby
