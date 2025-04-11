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

#import "internal/platform/implementation/apple/ble_l2cap_socket.h"

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#import "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

#pragma mark - BleL2capInputStream

BleL2capInputStream::BleL2capInputStream(GNCBLEL2CAPStream* stream) : stream_(stream) {
  GTMLoggerInfo(@"BleL2capInputStream::BleL2capInputStream");
}

ExceptionOr<ByteArray> BleL2capInputStream::Read(std::int64_t size) {
  // TODO: edwinwu - Implement to read data from l2cap channel.
  return {Exception::kIo};
}

Exception BleL2capInputStream::Close() {
  // The input stream reads directly from the connection. It can not be closed without closing the
  // connection itself. A call to `BleL2capSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - BleL2capOutputStream

BleL2capOutputStream::BleL2capOutputStream(GNCBLEL2CAPStream* stream) : stream_(stream) {}

Exception BleL2capOutputStream::Write(const ByteArray& data) {
  // TODO: edwinwu - Implement to write data to l2cap channel.
  return {Exception::kIo};
}

Exception BleL2capOutputStream::Flush() { return {Exception::kSuccess}; }

Exception BleL2capOutputStream::Close() { return {Exception::kSuccess}; }

#pragma mark - BleL2capSocket

BleL2capSocket::BleL2capSocket(GNCBLEL2CAPStream* stream)
    : BleL2capSocket(stream, new EmptyBlePeripheral()) {}

BleL2capSocket::BleL2capSocket(GNCBLEL2CAPStream* stream, api::ble_v2::BlePeripheral* peripheral)
    : stream_(stream),
      input_stream_(std::make_unique<BleL2capInputStream>(stream)),
      output_stream_(std::make_unique<BleL2capOutputStream>(stream)),
      peripheral_(peripheral) {}

InputStream& BleL2capSocket::GetInputStream() { return *input_stream_; }

OutputStream& BleL2capSocket::GetOutputStream() { return *output_stream_; }

Exception BleL2capSocket::Close() {
  [stream_ close];
  return {Exception::kSuccess};
}

void BleL2capSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {}

}  // namespace apple
}  // namespace nearby
