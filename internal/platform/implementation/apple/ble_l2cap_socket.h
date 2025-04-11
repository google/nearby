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

#include <memory>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/ble_peripheral.h"
#include "internal/platform/implementation/ble_v2.h"

namespace nearby {
namespace apple {

/** A readable stream of bytes. */
class BleL2capInputStream : public InputStream {
 public:
  // Creates a BleL2capInputStream.
  //
  // @param stream The underlying stream to use for reading and writing.
  explicit BleL2capInputStream(GNCBLEL2CAPStream* stream);
  ~BleL2capInputStream() override = default;

  // Reads at most `size` bytes from the input stream.
  //
  // Returns an empty byte array on end of file, or Exception::kIo on error.
  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  // Closes the stream preventing further reads.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override;

 private:
  GNCBLEL2CAPStream* stream_;
};

/** A writable stream of bytes. */
class BleL2capOutputStream : public OutputStream {
 public:
  // Creates a BleL2capOutputStream.
  //
  // @param stream The underlying stream to use for reading and writing.
  explicit BleL2capOutputStream(GNCBLEL2CAPStream* stream);
  ~BleL2capOutputStream() override = default;

  // Write the provided bytes to the output stream.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Write(const ByteArray& data) override;

  // no-op
  //
  // Always returns Exception::kSuccess.
  Exception Flush() override;

  // Closes the stream preventing further writes.
  //
  // Returns Exception::kIo on error, otherwise Exception::kSuccess.
  Exception Close() override;

 private:
  GNCBLEL2CAPStream* stream_;
};

/**
 * Concrete BleL2capSocket implementation.
 */
class BleL2capSocket : public api::ble_v2::BleL2capSocket {
 public:
  explicit BleL2capSocket(GNCBLEL2CAPStream* stream);

  // The peripheral used to create the socket must outlive the socket or
  // undefined behavior will occur.
  BleL2capSocket(GNCBLEL2CAPStream* stream,
                 api::ble_v2::BlePeripheral* peripheral);
  ~BleL2capSocket() override = default;

  // Returns the InputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  InputStream& GetInputStream() override;

  // Returns the OutputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  OutputStream& GetOutputStream() override;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Sets the close notifier by client side.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) override;

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  api::ble_v2::BlePeripheral* GetRemotePeripheral() override {
    return peripheral_;
  }

 private:
  GNCBLEL2CAPStream* stream_;
  std::unique_ptr<BleL2capInputStream> input_stream_;
  std::unique_ptr<BleL2capOutputStream> output_stream_;
  api::ble_v2::BlePeripheral* peripheral_;
};

}  // namespace apple
}  // namespace nearby