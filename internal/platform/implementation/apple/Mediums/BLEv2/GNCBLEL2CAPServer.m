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
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheralManager.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

static char *const kGNCBLEL2CAPServerQueueLabel = "com.google.nearby.GNCBLEL2CAPServer";

@interface GNCBLEL2CAPServer () <GNCPeripheralManagerDelegate>
@property(atomic, readwrite) CBL2CAPPSM PSM;
@property(nonatomic, readonly) NSMutableArray<GNCBLEL2CAPStream *> *readyStreams;
@end

@implementation GNCBLEL2CAPServer {
  dispatch_queue_t _queue;
  id<GNCPeripheralManager> _peripheralManager;
  NSString *_serviceID;
  GNCStartListeningL2CAPChannelCompletionHandler _startListeningL2CAPChannelcompletionHandler;

  CBL2CAPChannel *_l2CAPChannel;
  BOOL _alreadyStartedWhenPeripheralPoweredOff;

  NSCondition *_condition;
  GNCBLEReceivedDataHandler _receivedDataHandler;
  NSError *_openChannelError;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _condition = [[NSCondition alloc] init];
    _receivedDataHandler = nil;
    _startListeningL2CAPChannelcompletionHandler = nil;
    _queue = dispatch_queue_create(kGNCBLEL2CAPServerQueueLabel, DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

// This is private and should only be used for tests. The provided peripheral manager must call
// delegate methods on the main queue.
- (instancetype)initWithPeripheralManager:(nullable id<GNCPeripheralManager>)peripheralManager {
  self = [super init];
  if (self) {
    _condition = [[NSCondition alloc] init];
    _receivedDataHandler = nil;
    _startListeningL2CAPChannelcompletionHandler = nil;
    _queue = dispatch_get_main_queue();
    if (peripheralManager) {
      _peripheralManager = peripheralManager;
      // Set for @c GNCPeripheralManager to be able to forward callbacks.
      _peripheralManager.peripheralDelegate = self;
    }
  }
  return self;
}

- (void)startListeningChannelWithCompletionHandler:
    (GNCStartListeningL2CAPChannelCompletionHandler)completionHandler {
  _startListeningL2CAPChannelcompletionHandler = completionHandler;
  if (!_queue) {
    _startListeningL2CAPChannelcompletionHandler([NSError
        errorWithDomain:GNCBLEErrorDomain
                   code:GNCBLEErrorL2CAPListeningOnQueueNil
               userInfo:nil]);
    return;
  }
  if (!_peripheralManager) {
    // Lazy initialization to avoid system dialog on app startup before pairing.
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self
                                                                 queue:_queue
                                                               options:nil];

    // Set for @c GNCPeripheralManager to be able to forward callbacks.
    _peripheralManager.peripheralDelegate = self;
  }

  if (_peripheralManager.state == CBManagerStatePoweredOn) {
    // Bluetooth link is already encrypted, however encryption is not required here to avoid getting
    // insufficient authentication errors due to initialization order.
    [_peripheralManager publishL2CAPChannelWithEncryption:NO];
  } else {
    GTMLoggerInfo(@"[NEARBY] Peripheral must be on to start, waiting.");
    _alreadyStartedWhenPeripheralPoweredOff = YES;
  }
}

@synthesize readyStreams = _readyStreams;

- (NSMutableArray<GNCBLEL2CAPStream *> *)readyStreams {
  if (!_readyStreams) {
    _readyStreams = [[NSMutableArray alloc] init];
  }
  return _readyStreams;
}

- (nullable GNCBLEL2CAPStream *)acceptWithReceivedDataHandler:
                                    (GNCBLEReceivedDataHandler)receivedDataHandler
                                                        error:(NSError **)error {
  GTMLoggerInfo(@"[NEARBY] acceptWithReceivedCallback");
  // Wait until we have a ready stream.
  _receivedDataHandler = receivedDataHandler;
  [_condition lock];
  GNCBLEL2CAPStream* l2CAPStream = [self.readyStreams lastObject];
  while (!l2CAPStream && !_openChannelError) {
    [_condition wait];
    l2CAPStream = [self.readyStreams lastObject];
  }
  if (l2CAPStream != nil) {
    [self.readyStreams removeObject:l2CAPStream];
  }
  if (error != nil) {
    *error = [_openChannelError copy];
  }
  [_condition unlock];

  return l2CAPStream;
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
  GTMLoggerDebug(@"[NEARBY] didPublishL2CAPChannel with PSM: %@", @(PSM));
  if (error) {
    GTMLoggerError(@"[NEARBY] Failed to publish L2CAP channel: %@", error);
    if (_startListeningL2CAPChannelcompletionHandler) {
      _startListeningL2CAPChannelcompletionHandler(error);
    }
    return;
  }
  _PSM = PSM;
  if (_startListeningL2CAPChannelcompletionHandler) {
    _startListeningL2CAPChannelcompletionHandler(nil);
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
  dispatch_assert_queue(_queue);
  GTMLoggerDebug(@"[NEARBY] didUnpublishL2CAPChannel on PSM %@", @(PSM));
  if (error) {
    GTMLoggerError(@"[NEARBY] Failed to unpublish L2CAP channel: %@", error);
  }
  if (self.readyStreams.count > 0) {
    GNCBLEL2CAPStream *l2CAPStream = [self.readyStreams lastObject];
    [l2CAPStream tearDown];
    [self.readyStreams removeAllObjects];
  }
  _PSM = 0;
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
  dispatch_assert_queue(_queue);
  GTMLoggerDebug(@"[NEARBY] didOpenL2CAPChannel");
  if (error) {
    [_condition lock];
    _openChannelError =
        [NSError errorWithDomain:GNCBLEErrorDomain
                            code:GNCBLEErrorL2CAPOpeningChannel
                        userInfo:@{NSLocalizedDescriptionKey : [error localizedDescription]}];
    [_condition signal];
    [_condition unlock];
    return;
  }

  // Cleanup older references.
  if (_readyStreams.count > 0 || _l2CAPChannel) {
    // The device may establish a new L2CAP socket connection while the old socket is still
    // connected if sysproxy stopped for a reason other than Bluetooth disconnection. Closing the
    // channel here ensures the server and the client state is reset between the two
    // connection sessions.
    [self closeL2CAPChannel];
    _openChannelError = nil;
  }

  _l2CAPChannel = channel;
  [_condition lock];
  __weak __typeof__(self) weakSelf = self;
  GNCBLEL2CAPStream *l2CAPStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
        __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf) return;
        // Indicates the L2CAP socket is closed. Clean up the resources used for the old
        // connection so that a new one can be established.
        [strongSelf closeL2CAPChannel];
      }
      receivedDataBlock:^(NSData *data) {
        __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf preProcessData:data];
      }
      inputStream:_l2CAPChannel.inputStream
      outputStream:_l2CAPChannel.outputStream];
  [self.readyStreams addObject:l2CAPStream];
  [_condition signal];
  [_condition unlock];
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

- (void)preProcessData:(NSData*) data {
  GTMLoggerDebug(@"[NEARBY] preProcessData, data: %@", [data description]);

  // TODO: b/399815436 - Handle parsing ble l2cap packets before deliver upstream.
  if (_receivedDataHandler) {
    _receivedDataHandler(data);
  }
}

- (void)shutDown {
  if (_PSM > 0) {
    [_peripheralManager unpublishL2CAPChannel:_PSM];
  }
  _peripheralManager.peripheralDelegate = nil;
  _peripheralManager = nil;
}

- (void)closeL2CAPChannel {
  if (self.readyStreams.count > 0) {
    GNCBLEL2CAPStream *l2CAPStream = [self.readyStreams lastObject];
    [l2CAPStream tearDown];
    [self.readyStreams removeAllObjects];
  }
  _l2CAPChannel = nil;
}

@end

NS_ASSUME_NONNULL_END
