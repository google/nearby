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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

static char *const kGNCBLEL2CAPClientQueueLabel = "com.google.nearby.GNCBLEL2CAPClient";

@interface GNCBLEL2CAPClient () <GNCPeripheralDelegate>
@end

@implementation GNCBLEL2CAPClient {
  dispatch_queue_t _queue;
  id<GNCPeripheral> _peripheral;
  GNCOpenL2CAPStreamCompletionHandler _completionHandler;
  GNCRequestDisconnectionHandler _requestDisconnectionHandler;

  CBL2CAPChannel *_l2CAPChannel;
  GNCBLEL2CAPStream *_l2CAPStream;
}

- (instancetype)initWithRequestDisconnectionHandler:
    (GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  return
      [self initWithQueue:dispatch_queue_create(kGNCBLEL2CAPClientQueueLabel, DISPATCH_QUEUE_SERIAL)
          requestDisconnectionHandler:requestDisconnectionHandler];
};

// This is private and should only be used for tests. The provided peripheral must call
// delegate methods on the main queue.
- (instancetype)initWithQueue:(nullable dispatch_queue_t)queue
    requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_get_main_queue();
    _requestDisconnectionHandler = requestDisconnectionHandler;
  }
  return self;
};

- (void)openL2CAPChannelWithPSM:(uint16_t)PSM
                     peripheral:(id<GNCPeripheral>)peripheral
              completionHandler:(GNCOpenL2CAPStreamCompletionHandler)completionHandler {
  GNCLoggerInfo(@"[NEARBY] openL2CAPChannelWithPSM = %d", PSM);
  _peripheral = peripheral;
  _peripheral.peripheralDelegate = self;
  _completionHandler = [completionHandler copy];
  [_peripheral openL2CAPChannel:(CBL2CAPPSM)PSM];
}

- (void)disconnect {
  dispatch_async(_queue, ^{
    id<GNCPeripheral> localPeripheral = _peripheral;
    _requestDisconnectionHandler(localPeripheral);
    _peripheral = nil;
    _peripheral.peripheralDelegate = nil;
  });
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(NSError *)error {
  dispatch_assert_queue(_queue);
  GNCLoggerDebug(
      @"[NEARBY] didOpenL2CAPChannel, channel: %@, inputStream: %@, outputStream: %@, error: %@",
      channel, channel.inputStream, channel.outputStream, error);
  // TODO: b/399815436 - channel.inputStream is null when doing testing. Refactor tests in the
  // future.
  if (error || (channel && (!channel.inputStream || !channel.outputStream))) {
    if (_completionHandler) {
      _completionHandler(nil, error);
    }
    return;
  }

  // Cleanup older references.
  if (_l2CAPStream || _l2CAPChannel) {
    // The watch may establish a new L2CAP socket connection while the old socket is still
    // connected if sysproxy stopped for a reason other than Bluetooth disconnection. Closing the
    // channel here ensures the controller and the client state is reset between the two
    // connection sessions.
    [self closeL2CAPChannel];
  }

  _l2CAPChannel = channel;
  __weak __typeof__(self) weakSelf = self;
  _l2CAPStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
        __typeof__(self) strongSelf = weakSelf;
        // Indicates the L2CAP socket is closed. Clean up the resources used for the old
        // connection so that a new one can be established.
        [strongSelf closeL2CAPChannel];
      }
              inputStream:_l2CAPChannel.inputStream
             outputStream:_l2CAPChannel.outputStream];
  if (_completionHandler) {
    _completionHandler(_l2CAPStream, nil);
  }
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(NSError *)error {
  dispatch_async(_queue, ^{
    if ([self respondsToSelector:@selector(gnc_peripheral:didOpenL2CAPChannel:error:)]) {
      [self gnc_peripheral:peripheral didOpenL2CAPChannel:channel error:error];
    }
  });
}

#pragma mark Private

- (void)closeL2CAPChannel {
  [_l2CAPStream tearDown];
  _l2CAPStream = nil;
  _l2CAPChannel = nil;
  [self disconnect];
}

@end
