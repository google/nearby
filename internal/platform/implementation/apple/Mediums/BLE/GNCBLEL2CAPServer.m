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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEL2CAPServerQueueLabel = "com.google.nearby.GNCBLEL2CAPServer";

@interface GNCBLEL2CAPServer () <GNCPeripheralManagerDelegate>
@property(atomic, readwrite) CBL2CAPPSM PSM;
@end

@implementation GNCBLEL2CAPServer {
  dispatch_queue_t _queue;
  id<GNCPeripheralManager> _peripheralManager;
  NSString *_serviceID;
  GNCOpenL2CAPServerPSMPublishedCompletionHandler _psmPublishedCompletionHandler;
  GNCOpenL2CAPServerChannelOpendCompletionHandler _channelOpenedCompletionHandler;

  CBL2CAPChannel *_l2CAPChannel;
  GNCBLEL2CAPStream *_l2CAPStream;
  /// Whether start call has been performed when the peripheral was off.
  BOOL _alreadyStartedWhenPeripheralPoweredOff;
}


- (instancetype)initWithPeripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager
                                    queue:(nullable dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_queue_create(kGNCBLEL2CAPServerQueueLabel, DISPATCH_QUEUE_SERIAL);
    if (GNCFeatureFlags.sharedPeripheralManagerEnabled) {
      if (!peripheralManager) {
        // In shared mode, the peripheral manager must be injected.
        [NSException raise:NSInvalidArgumentException
                    format:@"Peripheral manager cannot be nil when shared manager is enabled."];
      }
      _peripheralManager = peripheralManager;
      // In shared mode, do NOT set the delegate. The Multiplexer handles callbacks.
    } else {
       // Legacy mode: Create a new manager if one isn't provided.
       // Note: L2CAP Server didn't create its own manager in the same way GATT server did in
       // very older versions, but for consistency in "Safe Rollout", we allow it to take nil
       // and create one.
       if (!peripheralManager) {
         peripheralManager = [[CBPeripheralManager alloc]
             initWithDelegate:self
                        queue:_queue
                      options:@{CBPeripheralManagerOptionShowPowerAlertKey : @NO}];
       }
      _peripheralManager = peripheralManager;
      // Set for @c GNCPeripheralManager to be able to forward callbacks.
      _peripheralManager.peripheralDelegate = self;
    }
  }
  return self;
}

- (void)startListeningChannelWithPSMPublishedCompletionHandler:
            (GNCOpenL2CAPServerPSMPublishedCompletionHandler)psmPublishedCompletionHandler
                                channelOpenedCompletionHandler:
                                    (GNCOpenL2CAPServerChannelOpendCompletionHandler)
                                        channelOpenedCompletionHandler {
  _psmPublishedCompletionHandler = [psmPublishedCompletionHandler copy];
  _channelOpenedCompletionHandler = [channelOpenedCompletionHandler copy];
  if (!_queue) {
    if (_psmPublishedCompletionHandler) {
      _psmPublishedCompletionHandler(0, [NSError errorWithDomain:GNCBLEErrorDomain
                                                            code:GNCBLEErrorL2CAPListeningOnQueueNil
                                                        userInfo:nil]);
    }
    return;
  }
  if (!_peripheralManager) {
    GNCLoggerError(@"[NEARBY] Peripheral manager must not be nil.");
    return;
  }

  if (_peripheralManager.state == CBManagerStatePoweredOn) {
    // Bluetooth link is already encrypted, however encryption is not required here to avoid getting
    // insufficient authentication errors due to initialization order.
    [_peripheralManager publishL2CAPChannelWithEncryption:NO];
  } else {
    GNCLoggerInfo(@"[NEARBY] Peripheral must be on to start, waiting.");
    _alreadyStartedWhenPeripheralPoweredOff = YES;
  }
}

- (void)close {
  [self shutDown];
}

#pragma mark - GNCPeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  dispatch_assert_queue(_queue);
  if (_peripheralManager.state == CBManagerStatePoweredOn) {
    if (_alreadyStartedWhenPeripheralPoweredOff) {
      // Only setup once so that toggling Bluetooth does not cause an L2CAP channel to be
      // published every time since a new instance is created for each new channel.
      _alreadyStartedWhenPeripheralPoweredOff = NO;

      // Bluetooth link is already encrypted, however encryption is not required here to avoid
      // getting insufficient authentication errors due to initialization order.
      [_peripheralManager publishL2CAPChannelWithEncryption:NO];
    }
  }
  if (_peripheralManager.state == CBManagerStatePoweredOff) {
    [self shutDown];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
       didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GNCLoggerDebug(@"[NEARBY] didPublishL2CAPChannel with PSM: %@", @(PSM));
  if (error) {
    GNCLoggerError(@"[NEARBY] Failed to publish L2CAP channel: %@", error);
    if (_psmPublishedCompletionHandler) {
      _psmPublishedCompletionHandler(0, error);
    }
    return;
  }
  _PSM = PSM;
  if (_psmPublishedCompletionHandler) {
    _psmPublishedCompletionHandler(PSM, nil);
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
  dispatch_assert_queue(_queue);
  GNCLoggerDebug(@"[NEARBY] didUnpublishL2CAPChannel on PSM %@", @(PSM));
  if (error) {
    GNCLoggerError(@"[NEARBY] Failed to unpublish L2CAP channel: %@", error);
  }
  [_l2CAPStream tearDown];
  _l2CAPStream = nil;
  _PSM = 0;
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GNCLoggerDebug(
      @"[NEARBY] didOpenL2CAPChannel, channel: %@, inputStream: %@, outputStream: %@, error: %@",
      channel, channel.inputStream, channel.outputStream, error);
  // TODO: edwinwu - channel.inputStream is null when doing testing. Refactor tests in the future.
  if (error || (channel && (!channel.inputStream || !channel.outputStream))) {
    if (_channelOpenedCompletionHandler) {
      _channelOpenedCompletionHandler(nil, error);
    }
    return;
  }

  // Cleanup older references.
  if (_l2CAPStream || _l2CAPChannel) {
    // The device may establish a new L2CAP socket connection while the old socket is still
    // connected if sysproxy stopped for a reason other than Bluetooth disconnection. Closing the
    // channel here ensures the server and the client state is reset between the two
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
  if (_channelOpenedCompletionHandler) {
    _channelOpenedCompletionHandler(_l2CAPStream, nil);
  }
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  [self gnc_peripheralManagerDidUpdateState:peripheral];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                     error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didPublishL2CAPChannel:PSM error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                       error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didUnpublishL2CAPChannel:PSM error:error];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                    error:(nullable NSError *)error {
  [self gnc_peripheralManager:peripheral didOpenL2CAPChannel:channel error:error];
}

#pragma mark Private

- (void)shutDown {
  if (_PSM > 0) {
    [_peripheralManager unpublishL2CAPChannel:_PSM];
  }
  _peripheralManager.peripheralDelegate = nil;
  _peripheralManager = nil;
}

- (void)closeL2CAPChannel {
  [_l2CAPStream tearDown];
  _l2CAPStream = nil;
  _l2CAPChannel = nil;
}

@end

NS_ASSUME_NONNULL_END
