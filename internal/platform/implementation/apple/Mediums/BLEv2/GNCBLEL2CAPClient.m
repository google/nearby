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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"
#import "GoogleToolboxForMac/GTMLogger.h"

static char *const kGNCBLEL2CAPClientQueueLabel = "com.googlenearby.GNCBLEL2CAPClient";

@interface GNCBLEL2CAPClient () <GNCPeripheralDelegate>
@end

@implementation GNCBLEL2CAPClient {
  dispatch_queue_t _queue;
  id<GNCPeripheral> _peripheral;
  GNCRequestDisconnectionHandler _requestDisconnectionHandler;

  // The L2CAP channel that is used to send and receive data.
  CBL2CAPChannel *_channel;
}

- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
       requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  return [self initWithPeripheral:peripheral
                            queue:dispatch_queue_create(kGNCBLEL2CAPClientQueueLabel,
                                                        DISPATCH_QUEUE_SERIAL)
      requestDisconnectionHandler:requestDisconnectionHandler];
};

// This is private and should only be used for tests. The provided peripheral must call
// delegate methods on the main queue.
- (instancetype)initWithPeripheral:(id<GNCPeripheral>)peripheral
                             queue:(nullable dispatch_queue_t)queue
       requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_get_main_queue();
    _peripheral = peripheral;
    _peripheral.peripheralDelegate = self;
    _requestDisconnectionHandler = requestDisconnectionHandler;
  }
  return self;
};

- (void)disconnect {
  dispatch_async(_queue, ^{
    _requestDisconnectionHandler(_peripheral);
  });
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                                                                  error:(NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"[NEARBY] Failed to open L2CAP channel: %@", error);
    return;
  }
  GTMLoggerInfo(@"[NEARBY] Opened L2CAP channel: %@", channel);
  _channel = channel;
  // TODO: b/399815436 - Implement to wrap up l2cap channel.
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(NSError *)error {
  if ([self respondsToSelector:@selector(gnc_peripheral:didOpenL2CAPChannel:error:)]) {
    [self gnc_peripheral:peripheral didOpenL2CAPChannel:channel error:error];
  }
}

@end
