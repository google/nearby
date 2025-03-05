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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheralManager.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEL2CAPServerQueueLabel = "com.nearby.GNCBLEL2CAPServer";

@interface GNCBLEL2CAPServer () <GNCPeripheralManagerDelegate>
@property(atomic, readwrite) CBL2CAPPSM PSM;
@end

@implementation GNCBLEL2CAPServer {
  dispatch_queue_t _queue;
  id<GNCPeripheralManager> _peripheralManager;

  // The L2CAP channel that is used to send and receive data.
  CBL2CAPChannel *_channel;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _queue = dispatch_queue_create(kGNCBLEL2CAPServerQueueLabel, DISPATCH_QUEUE_SERIAL);
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:nil queue:_queue];
    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
  }
  return self;
}

// This is private and should only be used for tests. The provided peripheral manager must call
// delegate methods on the main queue.
- (instancetype)initWithPeripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager {
  self = [super init];
  if (self) {
    _queue = dispatch_get_main_queue();
    _peripheralManager = peripheralManager;
    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
  }
  return self;
}

- (nullable CBL2CAPChannel *)getChannel {
  return _channel;
}

#pragma mark - GNCPeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  dispatch_assert_queue(_queue);
  if (_peripheralManager.state == CBManagerStatePoweredOn) {
    [_peripheralManager publishL2CAPChannelWithEncryption:NO];
  }
  if (_peripheralManager.state == CBManagerStatePoweredOff) {
    [_peripheralManager unpublishL2CAPChannel:_PSM];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
       didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"Failed to publish L2CAP channel: %@", error);
  }
  GTMLoggerInfo(@"Published L2CAP channel with PSM: %d", PSM);
  _PSM = PSM;
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"Failed to unpublish L2CAP channel: %@", error);
  }
  _PSM = 0;
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  if (error) {
    GTMLoggerError(@"Failed to open L2CAP channel: %@", error);
    return;
  }
  GTMLoggerInfo(@"Opened L2CAP channel with PSM: %d", channel.PSM);
  _channel = channel;
  // TODO: b/399815436 - Implement to wrap up l2cap channel.
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManagerDidUpdateState:peripheral];
  });
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                     error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManager:peripheral didPublishL2CAPChannel:PSM error:error];
  });
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                       error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManager:peripheral didUnpublishL2CAPChannel:PSM error:error];
  });
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                    error:(nullable NSError *)error {
  dispatch_async(_queue, ^{
    [self gnc_peripheralManager:peripheral didOpenL2CAPChannel:channel error:error];
  });
}

#pragma mark Private

- (void)tearDown {
  if (_PSM > 0) {
    [_peripheralManager unpublishL2CAPChannel:_PSM];
  }
  _peripheralManager.peripheralDelegate = nil;
  _peripheralManager = nil;
}

@end

NS_ASSUME_NONNULL_END
