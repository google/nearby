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

#import "internal/platform/implementation/apple/ble_socket.h"

#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleConnection.h"
#import "internal/platform/implementation/apple/ble_peripheral.h"
#import "internal/platform/implementation/apple/ble_utils.h"
#import "internal/platform/implementation/apple/utils.h"

// TODO(b/293336684): Remove this file when shared Weave is complete.

namespace nearby {
namespace apple {

#pragma mark - BleInputStream

BleInputStream::BleInputStream()
    : newDataPackets_([NSMutableArray array]),
      accumulatedData_([NSMutableData data]),
      condition_([[NSCondition alloc] init]) {
  // Create the handlers of incoming data from the remote endpoint.
  connectionHandlers_ = [GNCMConnectionHandlers
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

BleInputStream::~BleInputStream() {
  NSCAssert(!newDataPackets_, @"BleInputStream not closed before destruction");
}

ExceptionOr<ByteArray> BleInputStream::Read(std::int64_t size) {
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

    if ((size == -1) && (accumulatedData_.length > 0)) {
      // Return all of the data.
      dataToReturn = accumulatedData_;
      accumulatedData_ = [NSMutableData data];
      break;
    } else if (accumulatedData_.length > 0) {
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
    NSLog(@"[NEARBY] Input stream: Received data of size: %lu", (unsigned long)dataToReturn.length);
    return ExceptionOr<ByteArray>(ByteArrayFromNSData(dataToReturn));
  } else {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
}

Exception BleInputStream::Close() {
  // Unblock pending read operation.
  [condition_ lock];
  newDataPackets_ = nil;
  [condition_ broadcast];
  [condition_ unlock];
  return {Exception::kSuccess};
}

#pragma mark - BleOutputStream

BleOutputStream::~BleOutputStream() {
  NSCAssert(!connection_, @"BleOutputStream not closed before destruction");
}

Exception BleOutputStream::Write(const ByteArray &data) {
  [condition_ lock];
  NSLog(@"[NEARBY] Sending data of size: %lu", NSDataFromByteArray(data).length);

  if (!connection_) {
    [condition_ unlock];
    return {Exception::kIo};
  }

  NSMutableData *packet = [NSMutableData dataWithData:NSDataFromByteArray(data)];

  // Send the data, blocking until the completion handler is called.
  __block bool isComplete = NO;
  __block GNCMPayloadResult sendResult = GNCMPayloadResultFailure;
  NSCondition *condition = condition_;  // don't capture |this| in completion

  [connection_ sendData:packet
      progressHandler:^(size_t count) {
      }
      completion:^(GNCMPayloadResult result) {
        [condition lock];
        // Make sure we haven't already reported completion before. This prevents a crash
        // where we try leaving a dispatch group more times than we entered it.
        // b/79095653.
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

  if (sendResult == GNCMPayloadResultSuccess) {
    [condition_ unlock];
    return {Exception::kSuccess};
  } else {
    [condition_ unlock];
    return {Exception::kIo};
  }
}

Exception BleOutputStream::Flush() {
  // The write() function blocks until the data is received by the remote endpoint, so there's
  // nothing to do here.
  return {Exception::kSuccess};
}

Exception BleOutputStream::Close() {
  // Unblock pending write operation.
  [condition_ lock];
  connection_ = nil;
  [condition_ broadcast];
  [condition_ unlock];
  return {Exception::kSuccess};
}

#pragma mark - BleSocket

BleSocket::BleSocket(id<GNCMConnection> connection)
    : input_stream_(new BleInputStream()),
      output_stream_(new BleOutputStream(connection)),
      peripheral_(new EmptyBlePeripheral()) {}

BleSocket::BleSocket(id<GNCMConnection> connection, api::ble_v2::BlePeripheral *peripheral)
    : input_stream_(new BleInputStream()),
      output_stream_(new BleOutputStream(connection)),
      peripheral_(peripheral) {}

BleSocket::~BleSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

bool BleSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception BleSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void BleSocket::DoClose() {
  if (!closed_) {
    input_stream_->Close();
    output_stream_->Close();
    closed_ = true;
  }
}

}  // namespace apple
}  // namespace nearby
