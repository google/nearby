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

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPConnection.h"
#import "internal/platform/implementation/apple/utils.h"
#include "internal/platform/implementation/ble.h"

namespace nearby {
namespace apple {

#pragma mark - BleL2capInputStream

BleL2capInputStream::BleL2capInputStream(GNCBLEL2CAPConnection *connection)
    : connection_(connection),
      newDataPackets_([NSMutableArray array]),
      accumulatedData_([NSMutableData data]),
      condition_([[NSCondition alloc] init]) {
  // Create the handlers of incoming data from the remote endpoint.
  connection.connectionHandlers = [GNCMConnectionHandlers
      payloadHandler:^(NSData *data) {
        [condition_ lock];
        // Add the incoming data to the data packet array to be processed in read() below.
        [newDataPackets_ addObject:data];
        [condition_ broadcast];
        [condition_ unlock];
      }
      disconnectedHandler:^{
        [condition_ lock];
        // Release the data packet array, meaning the stream has been closed or severed.
        newDataPackets_ = nil;
        [condition_ broadcast];
        [condition_ unlock];
      }];
}

BleL2capInputStream::~BleL2capInputStream() {
  NSCAssert(!newDataPackets_, @"BleInputStream not closed before destruction");
}

ExceptionOr<ByteArray> BleL2capInputStream::Read(std::int64_t size) {
  // Block until either (a) the connection has been closed, (b) we have enough data to return.
  NSData *dataToReturn;
  [condition_ lock];
  while (true) {
    // Check if the stream has been closed or severed.
    if (!newDataPackets_) break;

    if (newDataPackets_.count > 0) {
      // Add the packet data to the accumulated data.
      for (NSData *data in newDataPackets_) {
        if (data.length > 0) {
          [accumulatedData_ appendData:data];
        }
      }
      [newDataPackets_ removeAllObjects];
    }

    if (accumulatedData_.length > 0) {
      // Return up to |size| bytes of the data.
      std::int64_t sizeToReturn = (accumulatedData_.length < size) ? accumulatedData_.length : size;
      NSRange range = NSMakeRange(0, (NSUInteger)sizeToReturn);
      dataToReturn = [accumulatedData_ subdataWithRange:range];
      [accumulatedData_ replaceBytesInRange:range withBytes:nil length:0];
      break;
    }

    [condition_ wait];
  }
  [condition_ unlock];

  if (dataToReturn) {
    return ExceptionOr<ByteArray>{ByteArray((const char *)dataToReturn.bytes, dataToReturn.length)};
  } else {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
}

Exception BleL2capInputStream::Close() {
  // Unblock pending read operation.
  [condition_ lock];
  newDataPackets_ = nil;
  [condition_ broadcast];
  [condition_ unlock];
  return {Exception::kSuccess};
}

#pragma mark - BleL2capOutputStream

BleL2capOutputStream::~BleL2capOutputStream() {
  NSCAssert(!connection_, @"BleL2capOutputStream not closed before destruction");
}

Exception BleL2capOutputStream::Write(absl::string_view data) {
  [condition_ lock];
  if (!connection_) {
    [condition_ unlock];
    return {Exception::kIo};
  }

  NSMutableData *packet = [NSMutableData dataWithBytes:data.data() length:data.size()];

  // Send the data, blocking until the completion handler is called.
  __block BOOL isComplete = NO;
  __block BOOL sendResult = NO;
  NSCondition *condition = condition_;  // don't capture |this| in completion

  [connection_ sendData:packet
             completion:^(BOOL result) {
               [condition lock];
               if (isComplete) {
                 [condition unlock];
                 return;
               }
               isComplete = YES;
               sendResult = result;
               [condition broadcast];
               [condition unlock];
             }];

  while (connection_ && !isComplete) {
    [condition_ wait];
  }

  if (sendResult == YES) {
    [condition_ unlock];
    return {Exception::kSuccess};
  } else {
    [condition_ unlock];
    return {Exception::kIo};
  }
}

Exception BleL2capOutputStream::Flush() {
  // The write() function blocks until the data is received by the remote endpoint, so there's
  // nothing to do here.
  return {Exception::kSuccess};
}

Exception BleL2capOutputStream::Close() {
  GNCLoggerInfo(@"[NEARBY] BleL2capOutputStream Closing");
  // Unblock pending write operation.
  [condition_ lock];
  connection_ = nil;
  [condition_ broadcast];
  [condition_ unlock];
  return {Exception::kSuccess};
}

#pragma mark - BleL2capSocket

BleL2capSocket::BleL2capSocket(GNCBLEL2CAPConnection *connection)
    : BleL2capSocket(connection, BlePeripheral::DefaultBlePeripheral().GetUniqueId()) {}

BleL2capSocket::BleL2capSocket(GNCBLEL2CAPConnection *connection,
                               api::ble::BlePeripheral::UniqueId peripheral_id)
    : input_stream_(std::make_unique<BleL2capInputStream>(connection)),
      output_stream_(std::make_unique<BleL2capOutputStream>(connection)),
      peripheral_id_(peripheral_id) {}

BleL2capSocket::~BleL2capSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

bool BleL2capSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception BleL2capSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void BleL2capSocket::DoClose() {
  GNCLoggerInfo(@"[NEARBY] BleL2capSocket DoClose");
  if (!closed_) {
    input_stream_->Close();
    output_stream_->Close();
    closed_ = true;
  }
}

}  // namespace apple
}  // namespace nearby
